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
	m_currentOp = INACTIVE;
	m_network = NULL;
	m_unpacker = NULL;

	connect( this, SIGNAL( error( ServerLogic::ERROR_TYPE, QString ) ), this, SLOT( cleanUp( ServerLogic::ERROR_TYPE, QString ) ) );
}

ServerLogic::~ServerLogic()
{
	if ( m_network != NULL )
		delete m_network;

	if( m_unpacker != NULL )
		delete m_unpacker;
}

void ServerLogic::setOp( ServerLogic::OPERATION operation )
{
	m_currentOp = operation;
}

const ServerLogic::OPERATION& ServerLogic::getOp()
{
	return m_currentOp;
}

void ServerLogic::clearPackagesToLoad()
{
	m_packagesToLoad.clear();
	m_packageIndex = 0;
}

void ServerLogic::addPackagesToLoad( const QList< ServerLogic::PackageInfo > &packageLocations )
{
	m_packagesToLoad.append( packageLocations );
}

void ServerLogic::removePackagesToLoad( const QList< ServerLogic::PackageInfo > &packageLocations )
{
	foreach ( const ServerLogic::PackageInfo &package, packageLocations )
		m_packagesToLoad.removeOne( package );
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

void ServerLogic::loadPackageList( const QUrl &url )
{
	m_packageList.clear();

	qDebug() << "loading package list from:" << url << ", isvalid =" << url.isValid();

	m_network->get( QNetworkRequest( url ) );
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
	if( m_packageIndex >= m_packagesToLoad.size() )
		return true;

	QString server = m_packagesToLoad[ m_packageIndex ].server;
	if( m_packageList.documentElement().isNull() )
	{
		m_packagesToLoad.removeAt( m_packageIndex );

		if( m_packageIndex >= m_packagesToLoad.size() )
			emit checkedPackage( "Finished Checking For Updates." );
		else if( m_packagesToLoad[ m_packageIndex ].server != server )
			loadPackageList( m_packagesToLoad[ m_packageIndex ].server + "packageList.xml" );
		else
			emit checkedPackage( m_packagesToLoad[ m_packageIndex ].path );
		return true;
	}

	if( server != m_packageList.documentElement().attribute( "path" ) )
	{
		QString listPath = server + "packageList.xml";
		loadPackageList( listPath );
		return false;
	}

	PackageInfo package = m_packagesToLoad.at( m_packageIndex );

	QString type = package.dir.endsWith( "MoNav.ini" ) ? "map" : "module";
	QString map = package.path;
	QString name = ( type == "map" ) ? map : package.dir.right( package.dir.size() - package.dir.lastIndexOf( '_') - 1);

	QDomElement packageElement = findElement( type, name, map);

	if( packageElement.isNull() )
	{
		qDebug() << "Package not found in list: " << map << type << name;
		m_packagesToLoad.removeAt( m_packageIndex );
	}

	else if( packageElement.attribute( "timestamp" ).toUInt() > package.timestamp )
	{
		qDebug() << "Update found: " << map << type << name;

		if( type == "map")
			m_packagesToLoad[ m_packageIndex ].path = packageElement.attribute( "path" );
		else
			m_packagesToLoad[ m_packageIndex ].path = packageElement.text();
		m_packagesToLoad[ m_packageIndex ].dir.truncate( m_packagesToLoad[ m_packageIndex ].dir.lastIndexOf('/') +1 );
		m_packagesToLoad[ m_packageIndex ].size = packageElement.attribute( "size" ).toLongLong();
		m_packagesToLoad[ m_packageIndex ].timestamp = packageElement.attribute( "timestamp" ).toUInt();

		++m_packageIndex;
	}

	else
	{
		qDebug() << "Up to date: " << map << type << name;
		m_packagesToLoad.removeAt( m_packageIndex );
	}

	if( m_packageIndex >= m_packagesToLoad.size() )
		emit checkedPackage( "Finished Checking For Updates." );

	else
		emit checkedPackage( m_packagesToLoad[ m_packageIndex ].path );

	return true;
}

QDomElement ServerLogic::findElement( QString packageType, QString packageName, QString mapName )
{
	QDomNodeList packageNodeList = m_packageList.elementsByTagName( packageType );

	for( int i=0; i < packageNodeList.size(); i++ )
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

void ServerLogic::cleanUp( ERROR_TYPE type, QString message )
{
	if( type == LIST_DL_ERROR )
		return;

	m_packagesToLoad.clear();

	if( m_unpacker != NULL )
		m_unpacker->deleteLater();
	m_unpacker = NULL;
}

void ServerLogic::finished( QNetworkReply* reply )
{
	if( reply->error() != QNetworkReply::NoError )
	{
		qDebug( "Error loading " + reply->url().toString().toUtf8() + " : " + reply->errorString().toUtf8() );

		if( reply->url().path().endsWith( "packageList.xml" ) )
		{
			emit error( ServerLogic::LIST_DL_ERROR, reply->url().toString().toUtf8() + ":\n" + reply->errorString().toUtf8() + '\n' );
			emit loadedList();
		}
		else
			emit error( ServerLogic::PACKAGE_DL_ERROR, reply->url().toString().toUtf8() + ":\n" + reply->errorString().toUtf8() + '\n' );
	}

	else if( reply->url().path().endsWith( ".mmm" ) )
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

	else if( reply->url().path().endsWith( "packageList.xml" ) )
	{
		if ( !m_packageList.setContent( reply ) )
			qCritical( "error parsing package list\n" );
		emit loadedList();
	}

	else if( reply->url().path().endsWith( "MoNav.ini" ) )
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
