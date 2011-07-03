#include <QStringList>
#include <QFile>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QDebug>

#include "serverlogic.h"
#include "utils/directoryunpacker.h"

ServerLogic::ServerLogic()
{
	m_localDir = "";
	m_network = NULL;
	m_unpacker = NULL;
	m_progress = NULL;

	connect( this, SIGNAL(loadedPackage()), this, SLOT(loadPackages()) );
}

ServerLogic::~ServerLogic()
{
	if ( m_network != NULL )
		delete m_network;

	if( m_unpacker != NULL )
		delete m_unpacker;

	if( m_progress != NULL )
		delete m_progress;
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
	connect( m_unpacker, SIGNAL( error() ), this, SLOT( handleUnpackError() ) );

	connect( reply, SIGNAL( readyRead() ), m_unpacker, SLOT( processNetworkData() ) );
	connect( reply, SIGNAL( destroyed() ), m_unpacker, SLOT( deleteLater() ) );

	return true;
}

bool ServerLogic::loadPackages( QProgressDialog *progress, QList<QUrl> packageURLs )
{
	if( progress != NULL)
		m_progress = progress;

	if( m_progress->wasCanceled() )
		return false;

	if( !packageURLs.isEmpty() )
		m_packagesToLoad = packageURLs;
	else
		m_progress->setValue( m_progress->value() + 1 );

	if( m_packagesToLoad.isEmpty() )
	{
		m_progress->setLabelText( "Finished" );
		return true;
	}

	m_progress->setLabelText( "Downloading " + m_packagesToLoad.first().toString() );

	return loadPackage( m_packagesToLoad.takeFirst() );
}

void ServerLogic::handleUnpackError()
{
	m_network->deleteLater();
	m_unpacker->deleteLater();
	m_progress->deleteLater();
}

void ServerLogic::finished( QNetworkReply* reply )
{
	if( reply->error() != QNetworkReply::NoError )
	{
		qCritical( reply->errorString().toUtf8() );
		return;
	}

	if( reply->url().path().endsWith( ".mmm" ) )
	{
		while ( reply->bytesAvailable() > 0 )
			m_unpacker->processNetworkData();

		emit loadedPackage();
	}

	if( reply->url().path().endsWith( "packageList.xml" ) )
	{
		if ( !m_packageList.setContent( reply ) )
			qCritical( "error parsing package list\n" );

		emit loadedList();
	}

	reply->deleteLater();
}
