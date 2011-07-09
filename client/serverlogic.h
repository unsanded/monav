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
			qint64 size;
			QString dir;

			bool operator==( const PackageInfo& other ) const
			{
				return this->path == other.path && this->size == other.size && this->dir == other.dir;
			}

		};

		ServerLogic();
		~ServerLogic();

		void addPackagesToLoad( const QList< PackageInfo >& packageLocations );
		const QDomDocument& packageList() const;

	signals:

		void loadedList();
		void loadedPackage( QString info );
		void error();

	public slots:

		void connectNetworkManager();
		bool loadPackage();
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
};

#endif // SERVERLOGIC_H
