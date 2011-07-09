#include <QStringList>
#include <QFile>
#include <QDir>
#include <QNetworkReply>
#include <QDebug>

#include "serverlogic.h"
#include "utils/directoryunpacker.h"

ServerLogic::ServerLogic()
{
	m_localDir = "";
	m_network = NULL;
	m_unpacker = NULL;

	connect(this, SIGNAL(error()), this, SLOT(cleanUp()) );
}

ServerLogic::~ServerLogic()
{
	if ( m_network != NULL )
		delete m_network;

	if( m_unpacker != NULL )
		delete m_unpacker;
}

void ServerLogic::addPackagesToLoad( const QList< ServerLogic::PackageInfo > &packageLocations )
{
	m_packagesToLoad.append( packageLocations );
}

const QDomDocument& ServerLogic::packageList() const
{
	return m_packageList;
}

void ServerLogic::connectNetworkManager()
{
	if(m_network == NULL)
	{
		m_network = new QNetworkAccessManager( this );
		connect( m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)) );
	}
}

bool ServerLogic::loadPackageList( const QUrl &url )
{
	qDebug() << "loading package list from:" << url << ", isvalid =" << url.isValid();

	m_network->get( QNetworkRequest( url ) );

	return true;
}

bool ServerLogic::loadPackage()
{
	if( m_packagesToLoad.isEmpty() )
		return false;

	ServerLogic::PackageInfo package = m_packagesToLoad.takeFirst();

	m_localDir = package.dir;

	qDebug() << "loading package from:" << package.path;

	QNetworkRequest request( package.path );

	QNetworkReply * reply = m_network->get( request );
	reply->setReadBufferSize( 2 * 16 * 1024 );

	if( package.path.endsWith(".mmm") )
	{
		QString filename = package.path;
		filename.remove( 0, filename.lastIndexOf( "/" ) + 1 );

		m_unpacker = new DirectoryUnpacker( m_localDir + filename, reply );
		connect( m_unpacker, SIGNAL( error() ), this, SIGNAL( error() ) );
	}

	return true;
}

void ServerLogic::cleanUp()
{
	m_packagesToLoad.clear();

	if( m_unpacker != NULL )
		m_unpacker->deleteLater();
	m_unpacker = NULL;
}

void ServerLogic::finished( QNetworkReply* reply )
{
	if( reply->error() != QNetworkReply::NoError )
	{
		qCritical( reply->errorString().toUtf8() );
		emit error();
		return;
	}

	if( reply->url().path().endsWith( ".mmm" ) )
	{
		while ( reply->bytesAvailable() > 0 )
			m_unpacker->processNetworkData();

		m_unpacker->deleteLater();
		m_unpacker = NULL;

		if( !m_packagesToLoad.isEmpty() )
			emit loadedPackage( m_packagesToLoad.first().path );
		else
			emit loadedPackage( "Finished." );
	}

	if( reply->url().path().endsWith( "packageList.xml" ) )
	{
		if ( !m_packageList.setContent( reply ) )
		{
			qCritical( "error parsing package list\n" );
			emit error();
			return;
		}

		emit loadedList();
	}

	if( reply->url().path().endsWith( "MoNav.ini" ) )
	{
		QDir dir( m_localDir );
		if( !dir.exists() && !dir.mkpath( m_localDir ) )
		{
			qCritical( "Couldn't open target file");
			emit error();
			return;
		}

		QFile file( m_localDir + "MoNav.ini" );
		if ( !file.open( QIODevice::WriteOnly ) )
		{
			qCritical( "Couldn't open target file");
			emit error();
			return;
		}

		file.write( reply->readAll() );
		file.close();

		if( !m_packagesToLoad.isEmpty() )
			emit loadedPackage( m_packagesToLoad.first().path );
		else
			emit loadedPackage( "Finished." );
	}

	reply->deleteLater();
}
