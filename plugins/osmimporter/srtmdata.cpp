/*
Copyright 2011  Christian Vetter veaac.fdirct@gmail.com

This file is part of MoNav.

MoNav is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoNav is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoNav.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
Part of this file originated from srtm2wayinfo: http://wiki.openstreetmap.org/wiki/Srtm2wayinfo
Copyright (c) 2009 Hermann Kraus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
 */

#include "srtmdata.h"
#include "utils/qthelpers.h"
#include <QCache>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QStringList>
#include <QRegExp>
#include <QtDebug>
#include <QtEndian>
#include <QDir>
#include <cmath>
#include <zzip/lib.h>

struct SRTMData::PrivateImplementation
{
	struct Tile
	{
		Tile()
		{
			buffer = NULL;
		}

		~Tile()
		{
			//qDebug() << "Deleted:" << this;
			delete buffer;
		}

		int getPixelValue(int x, int y)
		{
			Q_ASSERT( x >= 0 && x < size && y >= 0 && y < size );
			int offset = x + size * ( size - y - 1 );
			qint16 value;
			value = qFromBigEndian( buffer[offset] );
			return value;
		}

		qint16* buffer;
		int size;
	};

	static float avg( float a, float b, float weight ) {
		if ( a == SRTM_DATA_VOID )
			return b;
		if ( b == SRTM_DATA_VOID )
			return a;
		return b * weight + a * ( 1 - weight );
	}

	Tile errorTile;

	static quint64 coordinateToTile( double latitude, double longitude );
	Tile* getTile( double latitude, double longitude );
	void createTileList();
	int unzip( QString filename, qint16** buffer );

	QHash< quint64, QString > filenameHash;
	QDir cacheDir;
	QCache< quint64, Tile > cache;
	QNetworkAccessManager network;

	long long loads;
};

SRTMData::SRTMData()
{
	d = new PrivateImplementation;
	d->createTileList();
	d->loads = 0;
	d->cache.setMaxCost( 1000 );
}

SRTMData::~SRTMData()
{
	qDebug() << "OSM Importer: SRTM tile loads:" << d->loads;
	delete d;
}

void SRTMData::setCacheDir( QString dir )
{
	d->cacheDir.setPath( dir );
}

QString SRTMData::cacheDir()
{
	return d->cacheDir.path();
}

void SRTMData::setMemCacheSize( long long tiles )
{
	// set size in tiles
	d->cache.setMaxCost( tiles );
}

long long SRTMData::memCacheSize()
{
	return d->cache.maxCost() * 1024;
}

double SRTMData::getAltitude( double latitude, double longitude )
{
	PrivateImplementation::Tile* tile = d->getTile( latitude, longitude );

	// tile valid?
	if ( tile || !tile->buffer )
		return SRTM_DATA_VOID;

	latitude -= floor( latitude );
	longitude -= floor( longitude );
	Q_ASSERT(lat >= 0.0 && lat < 1.0 && lon >= 0.0 && lon < 1.0);
	float x = longitude * ( tile->size - 1 );
	float y = latitude * ( tile->size - 1 );
	float value00 = tile->getPixelValue( x, y );
	float value10 = tile->getPixelValue( x + 1, y );
	float value01 = tile->getPixelValue( x, y + 1 );
	float value11 = tile->getPixelValue( x + 1, y + 1 );
	float value_0 = d->avg( value00, value10, x - int( x ) );
	float value_1 = d->avg( value01, value11, x - int( x ) );
	float value = d->avg( value_0, value_1, y - int( y ) );
	return value;
}

SRTMData::PrivateImplementation::Tile* SRTMData::PrivateImplementation::getTile( double latitude, double longitude )
{
	quint64 id = coordinateToTile( latitude, longitude );
	// look into mem cache
	if ( cache.contains( id ) )
		return cache[id];

	// file available on the server?
	if ( !filenameHash.contains( id ) )
		return &errorTile;

	// look into disk cache
	QString filename = filenameHash[id];
	QStringList filenameParts = filename.split( "/" );
	if ( filenameParts.size() != 2 )
	{
		qCritical() << "OSM Importer: Illegal tile name:" << filename;
		return &errorTile;
	}
	QString cacheName = cacheDir.filePath( filenameParts[1] );

	// load from the internet
	if ( !QFile::exists( cacheName ) )
	{
		Timer timer;
		QUrl url( "http://dds.cr.usgs.gov/srtm/version2_1/SRTM3/" + filename );
		QEventLoop loop;
		loop.connect( &network, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()) );
		QNetworkReply* reply = network.get( QNetworkRequest( url ) );
		loop.exec();
		if ( reply->error() )
		{
			qCritical() << "OSM Importer: Failed to download tile:" << reply->url() << reply->errorString();
			return &errorTile;
		}
		QFile file( cacheName );
		if ( !file.open( QIODevice::WriteOnly ) )
		{
			qCritical() << "OSM Importer: Failed to write tile:" << cacheName << file.errorString();
			return &errorTile;
		}
		file.write( reply->readAll() );
		qDebug() << "OSM Importer: Finished downloading tile:" << reply->url() << timer.elapsed() << "ms";
	}

	// load from disk cache and
	// insert into mem cache
	Tile* tile = new Tile;
	tile->size = unzip( cacheName, &tile->buffer );
	cache.insert( id, tile, 1 );
	loads++;
	//if ( loads < 100 )
	//	qDebug() << "Insert:" << tile << ":" << id << tile->size * tile->size * sizeof( qint16 ) / 1024 + 1 << "/" << cache.maxCost();

	return tile;
}

quint64 SRTMData::PrivateImplementation::coordinateToTile( double latitude, double longitude )
{
	latitude = floor( latitude );
	longitude = floor( longitude );
	return ( ( ( quint64 ) latitude ) << 32 ) | ( ( quint64 ) longitude );
}

void SRTMData::PrivateImplementation::createTileList()
{
	// read from cache
	QString listFilename = cacheDir.filePath( "tileList" );
	if ( QFile::exists( listFilename ) )
	{
		FileStream stream( listFilename );
		if ( stream.open( QIODevice::ReadOnly ) )
		{
			stream >> filenameHash;
			return;
		}
	}

	// download from the internet
	QStringList subDirs;
	subDirs << "Africa" << "Australia" << "Eurasia" << "Islands" << "North_America" << "South_America";
	Timer timer;
	foreach( QString dir, subDirs )
	{
		QEventLoop loop;
		loop.connect( &network, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()) );
		QNetworkReply* reply = network.get( QNetworkRequest( QUrl( "http://dds.cr.usgs.gov/srtm/version2_1/SRTM3/" + dir + "/" ) ) );
		loop.exec();

		if ( reply->error() )
		{
			qCritical() << "OSM Importer: Failed to get SRTM file list:" << reply->url() << reply->errorString();
			return;
		}
		QString list = reply->readAll();
		//qDebug() << reply->url() << "\n" << list;
		QRegExp regex( "<a href=\"([NS])(\\d{2})([EW])(\\d{3})\\.hgt\\.zip" );
		int index = -1;
		while ( ( index = list.indexOf( regex, index + 1 ) ) != -1 ) {
			int lat = regex.cap( 2 ).toInt();
			int lon = regex.cap( 4 ).toInt();
			if ( regex.cap( 1 ) == "S" )
				lat = -lat;
			if ( regex.cap( 3 ) == "W" )
				lon = - lon;
			//S00E000.hgt.zip
			//123456789012345 => 15 bytes long
			filenameHash[coordinateToTile( lat, lon )] = dir + "/" + regex.cap().right( 15 );
		}
	}
	qDebug() << "OSM Importer: Finished downloading SRTM index:" << timer.elapsed() << "ms";
	//qDebug() << filenameHash;

	// write to cache
	FileStream stream( listFilename );
	if ( stream.open( QIODevice::WriteOnly ) )
	{
		stream << filenameHash;
		return;
	}
}

int SRTMData::PrivateImplementation::unzip( QString filename, qint16 **buffer )
{
	*buffer = 0;

	QFileInfo fi( filename );
	int size = 0;
	ZZIP_DIR* dir = zzip_dir_open( filename.toAscii(), 0);
	if ( !dir ) {
		qCritical() << "OSM Importer: ZIP: Could not open zip file" << filename;
		return 0;
	}
	ZZIP_FILE* fp = zzip_file_open( dir, fi.completeBaseName().toAscii(), 0);
	if (!fp) {
		qCritical() << "ZIP: Could not find" <<  fi.completeBaseName() << "in" << filename;
		return 0;
	}
	ZZIP_STAT stat;
	if ( zzip_file_stat( fp, &stat) == -1 ) {
		qCritical() << "OSM Importer: ZIP: Could not get info about" << fi.completeBaseName();
		return 0;
	}

	size = sqrt( stat.st_size / 2 );
	if ( size * size * 2 != stat.st_size ) {
		qCritical() << "OSM Importer: ZIP: Invalid data: Not a square!";
		return 0;
	}
	*buffer = new qint16[stat.st_size / 2];

	if ( zzip_file_read( fp, *buffer, stat.st_size ) != stat.st_size ) {
		qCritical() << "OSM Importer: ZIP: Could not read all bytes.";
		delete *buffer;
		*buffer = 0;
		return 0;
	}

	zzip_file_close( fp );
	zzip_dir_close( dir );

	return size;
}
