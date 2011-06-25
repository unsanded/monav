#include <QNetworkReply>
#include <QFile>
#include <QDebug>

#include "serverlogic.h"

ServerLogic::ServerLogic()
{
	m_localDir = "";
	m_network = NULL;
}

ServerLogic::~ServerLogic()
{
	if ( m_network != NULL )
		delete m_network;
}

const QDomDocument& ServerLogic::packageList() const
{
	return m_packageList;
}

void ServerLogic::connectNetworkManager()
{
	m_network = new QNetworkAccessManager( this );
	connect( m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)) );
}

bool ServerLogic::loadPackageList( const QString &dir, const QUrl &url )
{
	QUrl listURL( url );
	listURL.setPath( url.path() + "packageList.xml" );

	qDebug() << "loading package list from:" << listURL << ", isvalid =" << listURL.isValid();

	m_localDir = dir;
	m_network->get( QNetworkRequest( listURL ) );

	return true;
}

void ServerLogic::finished( QNetworkReply* reply )
{
	QFile localFile( m_localDir + reply->url().host() + "/" + "packageList.xml" );

	qDebug() << "saving package list to:" << localFile.fileName();

	localFile.open( QIODevice::WriteOnly );
	localFile.write( reply->readAll() );
	localFile.close();

	reply->deleteLater();

	localFile.open( QIODevice::ReadOnly);
	if ( !m_packageList.setContent(&localFile) )
		qDebug() << "error parsing package list\n";
	localFile.close();

	emit loadedList();
}
