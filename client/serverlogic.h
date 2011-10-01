#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#include <QString>
#include <QList>
#include <QUrl>
#include <QDomDocument>

class QNetworkAccessManager;
class QNetworkReply;
class DirectoryUnpacker;
class QProgressDialog;

class ServerLogic : public QObject
{
	Q_OBJECT

	public:

		struct Server
		{
			QString name;
			QUrl url;
		};

		struct PackageInfo
		{
			QString path;
			QString server;
			QString dir;
			qint64 size;
			unsigned timestamp;

			bool operator==( const PackageInfo& other ) const
			{
				return this->path == other.path;
			}

			bool operator<( const PackageInfo& other ) const
			{
				return this->server < other.server;
			}

		};

		ServerLogic();
		~ServerLogic();

		void addPackagesToLoad( const QList< PackageInfo >& packageLocations );
		const QList< PackageInfo >& packagesToLoad() const;
		const QDomDocument& packageList() const;

	signals:

		void loadedList();
		void checkedPackage( QString info );
		void loadedPackage( QString info );
		void error();

	public slots:

		void connectNetworkManager();
		bool loadPackage();
		bool checkPackage();
		bool loadPackageList( const QUrl &url );

	protected slots:

		void finished( QNetworkReply* reply );
		void cleanUp();

	protected:

		QString m_localDir;
		QDomDocument m_packageList;
		DirectoryUnpacker *m_unpacker;
		QNetworkAccessManager* m_network;

		QList< PackageInfo > m_packagesToLoad;
		int m_packageIndex;

		QDomElement findElement( QString packageType, QString packageName, QString mapName = "" );
};

#endif // SERVERLOGIC_H
