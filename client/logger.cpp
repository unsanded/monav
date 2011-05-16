/*
Copyright 2010 Christoph Eckert ce@christeck.de

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

#include "logger.h"
#include "routinglogic.h"
#include "utils/qthelpers.h"
#include <QXmlStreamReader>

Logger* Logger::instance()
{
	static Logger logger;
	return &logger;
}


Logger::Logger( QObject* parent ) :
	QObject( parent )
{
	initialize();
	readGpxLog();
}


Logger::~Logger()
{
}

void Logger::cleanup()
{
	writeGpxLog();
}

void Logger::initialize()
{
	m_lastFlushTime = QDateTime::currentDateTime();

	QSettings settings( "MoNavClient" );
	m_loggingEnabled = settings.value( "LoggingEnabled", false ).toBool();
	m_tracklogPath = settings.value( "LogFilePath", QDir::homePath() ).toString();
	m_maxSpeed = 0.0;
	m_sumSpeeds = 0.0;
	m_validPoints = 0;
	m_trackDistance = 0.0;
	m_trackMinElevation = -20000.0;
	m_trackMaxElevation = -20000.0;
	m_trackElevations.clear();

	m_tracklogPrefix = tr( "MoNav Track" );
	QString tracklogFilename = m_tracklogPrefix;

	QDateTime currentDateTime = QDateTime::currentDateTime();
	tracklogFilename.append( currentDateTime.toString( " yyyy-MM-dd" ) );
	tracklogFilename.append( ".gpx" );
	m_logFile.setFileName( fileInDirectory( m_tracklogPath, tracklogFilename ) );
}


QVector< int > Logger::polygonEndpointsTracklog()
{
	QVector<int> endpoints;
	bool append = false;
	int invalidElements = 0;
	for (int i = 0; i < m_gpsInfoBuffer.size(); i++)
	{
		if( !m_gpsInfoBuffer.at(i).position.IsValid() )
		{
			invalidElements++;
			append = true;
			continue;
		}
		if (append == true || endpoints.size() == 0)
		{
			endpoints.append(i+1-invalidElements);
			append = false;
			continue;
		}
		endpoints.pop_back();
		endpoints.append(i+1-invalidElements);
	}

	return endpoints;
}


QVector< UnsignedCoordinate > Logger::polygonCoordsTracklog()
{
	QVector<UnsignedCoordinate> coordinates;
	for (int i = 0; i < m_gpsInfoBuffer.size(); i++)
	{
		if( m_gpsInfoBuffer.at(i).position.IsValid() )
			coordinates.append(m_gpsInfoBuffer.at(i).position);
	}
	return coordinates;
}


void Logger::readGpsInfo( RoutingLogic::GPSInfo gpsInfo )
{
	// Filling local cache variables
	if( m_gpsInfoBuffer.size() > 0 && m_gpsInfoBuffer.last().position.IsValid() && gpsInfo.position.IsValid() ){
		m_trackDistance += gpsInfo.position.ToGPSCoordinate().Distance( m_gpsInfoBuffer.last().position.ToGPSCoordinate() );
	}

	if( m_trackMinElevation == -20000.0 || gpsInfo.altitude < m_trackMinElevation ){
		if( QString::number( gpsInfo.altitude ) != "nan" ){
			m_trackMinElevation = gpsInfo.altitude;
		}
	}
	if( m_trackMaxElevation == -20000.0 || gpsInfo.altitude > m_trackMaxElevation ){
		if( QString::number( gpsInfo.altitude ) != "nan" ){
			m_trackMaxElevation = gpsInfo.altitude;
		}
	}
	if( QString::number( gpsInfo.altitude ) != "nan" && gpsInfo.verticalAccuracy < 50 ){
		m_trackElevations.append( gpsInfo.altitude );
	}
	if( gpsInfo.groundSpeed > m_maxSpeed ){
		m_maxSpeed = gpsInfo.groundSpeed;
	}
	m_sumSpeeds += gpsInfo.groundSpeed;
	m_validPoints++;
	m_gpsInfoBuffer.append(gpsInfo);
}


void Logger::positionChanged()
{
	if ( !m_loggingEnabled )
		return;

	const RoutingLogic::GPSInfo& gpsInfo = RoutingLogic::instance()->gpsInfo();
	if ( !gpsInfo.position.IsValid() )
		return;

	// Filter inaccurate data.
	// Position and/or timestamp equal the last received event
	if ( m_gpsInfoBuffer.last().position == gpsInfo.position )
		return;

	// Precision is worse than x meters.
	// Indoor results: Horizontal accuracy: 652.64 , Vertical accuracy: 32767.5
	// Outdoor results: Horizontal accuracy: Between 27 and 37 , Vertical accuracy: Between 35 and 50
	if ( gpsInfo.horizontalAccuracy > 50 )
		return;

	// TODO: Call by reference, not value
	readGpsInfo( gpsInfo );

	int flushSecondsPassed = m_lastFlushTime.secsTo( QDateTime::currentDateTime() );
	if ( flushSecondsPassed >= 300 )
		writeGpxLog();
	emit trackChanged();
}


bool Logger::readGpxLog()
{
	m_gpsInfoBuffer.clear();

	if ( !m_logFile.open( QIODevice::ReadOnly | QIODevice::Text ) ){
		return false;
	}

	QXmlStreamReader gpxReader( &m_logFile );
	QXmlStreamAttributes currentAttributes;
	QXmlStreamAttribute currentAttribute;
	QString currentValue;
	double lat = 200.0;
	double lon = 200.0;
	RoutingLogic::GPSInfo gpsInfo;
	GPSCoordinate gpsCoordinate;

	enum FileStatus{ insideFile = 0, insideGpx = 1, insideTrack = 2, insideTracksegment = 3, insideTrackpoint = 4, insideWaypoint = 5 };
	FileStatus fileStatus = insideFile;

	// Reading the file "line by line"
	while ( !gpxReader.atEnd() )
	{
		gpxReader.readNext();
		if( gpxReader.hasError() )
		{
			qCritical() << tr( "There was an error reading the tracklog file" ) << m_logFile.fileName();
			m_logFile.close();
			return false;
		}

		if ( gpxReader.isStartElement() && gpxReader.name().toString() == "trkseg" )
		{
			fileStatus = insideTracksegment;
			continue;
		}
		if ( gpxReader.isEndElement() && gpxReader.name().toString() == "trkseg" )
		{
			fileStatus = insideTrack;
			// Append an/the currently invalid element to the list. Better save than sorry:
			gpsInfo = RoutingLogic::GPSInfo();
			m_gpsInfoBuffer.append( gpsInfo );
			continue;
		}

		if ( gpxReader.isStartElement() && gpxReader.name().toString() == "trkpt" )
		{
			fileStatus = insideTrackpoint;
			currentAttributes = gpxReader.attributes();
			for( int i = 0; i < currentAttributes.size(); i++ ){
				if( currentAttributes[i].name() == "lat" )
					lat = currentAttributes[i].value().toString().toDouble();
				if( currentAttributes[i].name() == "lon" )
					lon = currentAttributes[i].value().toString().toDouble();
			}
			gpsCoordinate.latitude = lat;
			gpsCoordinate.longitude = lon;
			gpsInfo.position = UnsignedCoordinate( gpsCoordinate );
			continue;
		}
		if ( gpxReader.isStartElement() && fileStatus == insideTrackpoint && gpxReader.name().toString() == "ele")
		{
			gpsInfo.altitude = gpxReader.readElementText().toDouble();
			continue;
		}
		if ( gpxReader.isStartElement() && fileStatus == insideTrackpoint && gpxReader.name().toString() == "time")
		{
			gpsInfo.timestamp = QDateTime::fromString( gpxReader.readElementText() );
			continue;
		}
		if ( gpxReader.isStartElement() && fileStatus == insideTrackpoint && gpxReader.name().toString() == "speed")
		{
			gpsInfo.groundSpeed = gpxReader.readElementText().toDouble();
			continue;
		}
		if ( gpxReader.isEndElement() && gpxReader.name().toString() == "trkpt" )
		{
			fileStatus = insideTracksegment;
			if( gpsInfo.position.IsValid() ){
				m_gpsInfoBuffer.append( gpsInfo );
			}
			// Reinitialize the object to make it invalid.
			gpsInfo = RoutingLogic::GPSInfo();
			continue;
		}
	}
	m_logFile.close();
	emit trackChanged();
	return true;
}


bool Logger::writeGpxLog()
{
	return true;
}

/*
bool Logger::writeGpxLog()
{
	QDateTime currentDateTime = QDateTime::currentDateTime();
	// Create a backup while writing to avoid data loss in case the battery drained etc.
	QString backupFilename;
	backupFilename = m_logFile.fileName().remove( m_logFile.fileName().size() -4, 4 );
	backupFilename.append( currentDateTime.toString( "-hhmmss" ) );
	backupFilename = backupFilename.append( tr( "-bck" ) );
	backupFilename = backupFilename.append( ".gpx" );
	// Always do this before opening the logfile, as the logfile got closed otherwise.
	m_logFile.copy( backupFilename );

	if ( !m_logFile.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ){
		m_loggingEnabled = false;
		qCritical() << "Logger: Cannot write " << m_logFile.fileName() << ". Logging disabled.";
		return false;
	}

	QString trackName = m_tracklogPrefix;
	trackName.append( currentDateTime.toString( " yyyy-MM-dd" ) );
	trackName.prepend("  <name>");
	trackName.append("</name>\n");

	QTextStream gpxStream(&m_logFile);
	gpxStream << QString("<?xml version=\"1.0\"?>\n").toUtf8();
	gpxStream << QString("<gpx version=\"1.0\" creator=\"MoNav Tracklogger\" xmlns=\"http://www.topografix.com/GPX/1/0\">\n").toUtf8();
	gpxStream << QString("  <trk>\n").toUtf8();
	gpxStream << trackName;

	bool insideTracksegment = false;
	for (int i = 0; i < m_gpsInfoBuffer.size(); i++)
	{
			if (!m_gpsInfoBuffer.at(i).position.IsValid() && insideTracksegment)
		{
			gpxStream << "    </trkseg>\n";
			insideTracksegment = false;
			continue;
		}
		if (!m_gpsInfoBuffer.at(i).position.IsValid() && !insideTracksegment)
		{
			continue;
		}
		if (m_gpsInfoBuffer.at(i).position.IsValid() && !insideTracksegment)
		{
			gpxStream << "    <trkseg>\n";
			insideTracksegment = true;
		}
		if (m_gpsInfoBuffer.at(i).position.IsValid() && insideTracksegment)
		{
			double latitude = m_gpsInfoBuffer.at(i).position.ToGPSCoordinate().latitude;
			double longitude = m_gpsInfoBuffer.at(i).position.ToGPSCoordinate().longitude;
			double elevation = m_gpsInfoBuffer.at(i).altitude;
			QString lat = QString::number( latitude, 'f', 6 ).prepend("      <trkpt lat=\"").append("\"");
			QString lon = QString::number( longitude, 'f', 6 ).prepend(" lon=\"").append("\">\n");
			QString ele = QString::number( elevation, 'f', 2 ).prepend("        <ele>").append("</ele>\n");
			QString time = m_gpsInfoBuffer.at(i).timestamp.toString( "yyyy-MM-ddThh:mm:ss" ).prepend("        <time>").append("</time>\n");
			gpxStream << lat.toUtf8();
			gpxStream << lon.toUtf8();
			if ( !ele.contains("nan") && m_gpsInfoBuffer.at(i).verticalAccuracy < 50 )
				gpxStream << ele.toUtf8();
			gpxStream << time.toUtf8();
			gpxStream << QString("      </trkpt>\n").toUtf8();
		}
	}
	if (insideTracksegment)
	{
		gpxStream << QString("    </trkseg>\n").toUtf8();
	}
	gpxStream << QString("  </trk>\n").toUtf8();
	gpxStream << QString("</gpx>\n").toUtf8();
	m_logFile.close();
	if( QFile::exists ( backupFilename ) )
		m_logFile.remove( backupFilename );

	m_lastFlushTime = QDateTime::currentDateTime();
	return true;
}
*/


void Logger::clearTracklog()
{
	m_gpsInfoBuffer.clear();
	writeGpxLog();
	initialize();
	readGpxLog();
}


double Logger::trackDistance()
{
	return m_trackDistance;
}


double Logger::maxSpeed()
{
	return m_maxSpeed;
}


double Logger::trackMinElevation()
{
	if( m_trackMinElevation == -20000 )
		return 0;
	else
		return m_trackMinElevation;
}


double Logger::trackMaxElevation()
{
	if( m_trackMinElevation == -20000 )
		return 0;
	else
		return m_trackMaxElevation;
}


double Logger::averageSpeed()
{
	double speed = 0.0;
	if( m_sumSpeeds > 0 && m_validPoints > 0 )
		speed = m_sumSpeeds / m_validPoints;
	return speed;
}


QDateTime Logger::trackStartTime()
{
	for( int i = 0; i < m_gpsInfoBuffer.size(); i++ ){
		if( m_gpsInfoBuffer.at(i).position.IsValid() ){
			return m_gpsInfoBuffer.at(i).timestamp;
		}
	}
	// In case thre aren't any valid trackpoints, return an invalid date object
	return QDateTime();
}


int Logger::trackDuration()
{
	QDateTime startTime;
	QDateTime endTime;

	for (int i = 0; i < m_gpsInfoBuffer.size(); i++)
	{
		if( !m_gpsInfoBuffer.at(i).position.IsValid() ){
			continue;
		}
		else{
			startTime = m_gpsInfoBuffer.at(i).timestamp;
			break;
		}
	}
	for (int i = m_gpsInfoBuffer.size() -1; i >= 0; i--)
	{
		if( !m_gpsInfoBuffer.at(i).position.IsValid() ){
			continue;
		}
		else{
			endTime = m_gpsInfoBuffer.at(i).timestamp;
			break;
		}
	}
	return startTime.secsTo( endTime );
}


const QVector<double>& Logger::trackElevations()
{
	return m_trackElevations;
}


bool Logger::loggingEnabled()
{
	return m_loggingEnabled;
}


void Logger::setLoggingEnabled(bool enable)
{
	// Avoid a new logfile is created in case nothing changed.
	if (m_loggingEnabled == enable)
		return;
	QSettings settings( "MoNavClient" );
	settings.setValue("LoggingEnabled", enable);
	writeGpxLog();
	initialize();
	readGpxLog();
}


QString Logger::directory()
{
	return m_tracklogPath;
}


void Logger::setDirectory(QString directory)
{
	// Avoid a new logfile is created in case nothing changed.
	if (m_tracklogPath == directory)
		return;
	QSettings settings( "MoNavClient" );
	settings.setValue("LogFilePath", directory);
	writeGpxLog();
	initialize();
	readGpxLog();
}

