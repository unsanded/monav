#include <QFile>
#include <QDir>
#include <QSettings>
#include <QNetworkReply>
#include <QDebug>

#include "serverlogic.h"
#include "utils/directoryunpacker.h"

ServerLogic::ServerLogic()
{
	m_localDir = "";
	m_packageIndex = 0;
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

const QList< ServerLogic::PackageInfo >& ServerLogic::packagesToLoad() const
{
	return m_packagesToLoad;
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

	ServerLogic::PackageInfo package = m_packagesToLoad.first();
	QUrl url( package.server + package.path );

	m_localDir = package.dir;

	qDebug() << "loading package from:" << url;

	QNetworkRequest request( url );

	QNetworkReply * reply = m_network->get( request );
	reply->setReadBufferSize( 2 * 16 * 1024 );

	if( package.path.endsWith(".mmm") )
	{
		QString filename = package.path;
		filename.remove( 0, filename.lastIndexOf( "/" ) + 1 ).replace( ".mmm", "" );

		m_unpacker = new DirectoryUnpacker( m_localDir.append( filename + '/' ), reply );
		connect( m_unpacker, SIGNAL( error() ), this, SIGNAL( error() ) );
	}

	return true;
}

bool ServerLogic::checkPackage()
{
	while( m_packageIndex < m_packagesToLoad.size() &&
		   m_packagesToLoad[ m_packageIndex ].server != m_packageList.documentElement().attribute( "path" ) )
	{
		QUrl listPath(m_packagesToLoad[ m_packageIndex ].server + "packageList.xml" );
		if( !loadPackageList( listPath ) )
		{
			qDebug() << "Unable to load server package list: " << listPath << '\n';
			m_packagesToLoad.removeAt( m_packageIndex );
		}
	}

	if( m_packageIndex >= m_packagesToLoad.size() )
	{
		emit checkedPackage( "Finished Checking For Updates." );
		return true;
	}

	PackageInfo* package = &m_packagesToLoad[ m_packageIndex ];

	QString type = package->dir.endsWith( "MoNav.ini" ) ? "map" : "module";
	QString map = package->path;
	QString name = ( type == "map" ) ? map : package->dir.right( package->dir.size() - package->dir.lastIndexOf( '_') - 1);

	QDomElement packageElement = findElement( type, name, map);

	if( packageElement.isNull() )
	{
		qDebug() << "Package not found in list: " << name << '\n';
		m_packagesToLoad.removeAt( m_packageIndex );
	}

	else if( packageElement.attribute( "timestamp" ).toUInt() > package->timestamp )
	{
		qDebug() << "Update found: " << name << '\n';
		++m_packageIndex;
	}

	else
	{
		qDebug() << "Up to date: " << name << '\n';
		m_packagesToLoad.removeAt( m_packageIndex );
	}

	emit checkedPackage( name );
	return true;
}

QDomElement ServerLogic::findElement( QString packageType, QString packageName, QString mapName )
{
	QDomNodeList packageNodeList = m_packageList.elementsByTagName( packageType );

	for( unsigned int i=0; i < packageNodeList.length(); i++ )
	{
		QDomElement package = packageNodeList.at( i ).toElement();

		if( packageType == "map" && package.attribute( "name" ) == packageName )
				return package;

		if( packageType == "module" && package.attribute( "name" ) == packageName )
		{
			if( package.parentNode().parentNode().toElement().attribute( "name" ) == mapName )
				return package;
		}
	}

	return QDomElement();
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

		QSettings serverConfig( m_localDir + "Server.ini", QSettings::IniFormat );
		serverConfig.setValue( "server", m_packagesToLoad.first().server );
		serverConfig.setValue( "timestamp", m_packagesToLoad.first().timestamp );
		m_packagesToLoad.removeFirst();

		if( !m_packagesToLoad.isEmpty() )
			emit loadedPackage( m_packagesToLoad.first().path );
		else
			emit loadedPackage( "Finished Download & Extract." );
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

		QSettings serverConfig( m_localDir + "Server.ini", QSettings::IniFormat );
		serverConfig.setValue( "server", m_packagesToLoad.first().server );
		serverConfig.setValue( "timestamp", m_packagesToLoad.first().timestamp );
		m_packagesToLoad.removeFirst();

		if( !m_packagesToLoad.isEmpty() )
			emit loadedPackage( m_packagesToLoad.first().path );
		else
			emit loadedPackage( "Finished Download & Extract." );
	}

	reply->deleteLater();
}
