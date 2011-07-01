#include <QNetworkReply>
#include <QFile>
#include <QDebug>

#include "serverlogic.h"
#include "utils/directoryunpacker.h"

ServerLogic::ServerLogic()
{
	m_localDir = "";
	m_network = NULL;
	m_unpacker = NULL;
}

ServerLogic::~ServerLogic()
{
	if ( m_network != NULL )
		delete m_network;

	if( m_unpacker != NULL )
		delete m_unpacker;
}

void ServerLogic::setLocalDir( const QString &dir )
{
	m_localDir = dir;
}

const QDomDocument& ServerLogic::packageList() const
{
	return m_packageList;
}

void ServerLogic::connectNetworkManager()
{
	if(m_network == NULL)
		m_network = new QNetworkAccessManager( this );

	connect( m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)) );
}

bool ServerLogic::loadPackageList( const QUrl &url )
{
	qDebug() << "loading package list from:" << url << ", isvalid =" << url.isValid();

	m_network->get( QNetworkRequest( url ) );

	return true;
}

bool ServerLogic::loadPackage( const QUrl &url )
{
	qDebug() << "loading package from:" << url << ", isvalid =" << url.isValid();

	QNetworkRequest request( url );

	QNetworkReply * reply = m_network->get( request );
	reply->setReadBufferSize( 2 * 16 * 1024 );

	m_unpacker = new DirectoryUnpacker( m_localDir, reply );
	connect( reply, SIGNAL( readyRead() ), m_unpacker, SLOT( processNetworkData() ) );
	connect( reply, SIGNAL( destroyed() ), m_unpacker, SLOT( deleteLater() ) );
	connect( m_unpacker, SIGNAL( error() ), this, SLOT( handleUnpackError() ) );

	return true;
}

void ServerLogic::handleUnpackError()
{
	/*
	delete m_network;
	m_network = NULL;

	delete m_unpacker;
	m_unpacker = NULL;
	*/
}

void ServerLogic::finished( QNetworkReply* reply )
{
	if( reply->error() != QNetworkReply::NoError )
		qCritical( reply->errorString().toUtf8() );

	if( reply->url().path().endsWith( ".mmm" ) )
	{
		while ( reply->bytesAvailable() > 0 )
			m_unpacker->processNetworkData();

		delete m_unpacker;
		m_unpacker = NULL;
	}

	if( reply->url().path().endsWith( "packageList.xml" ) )
	{
		if ( !m_packageList.setContent( reply ) )
			qCritical( "error parsing package list\n" );

		emit loadedList();
	}

	reply->deleteLater();
}
