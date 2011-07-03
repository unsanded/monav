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

		struct Server {
			QString name;
			QUrl url;
		};

		ServerLogic();
		~ServerLogic();

		void setLocalDir( const QString &dir );
		const QDomDocument& packageList() const;

	signals:
		void loadedList();
		void loadedPackage();

	public slots:
		void connectNetworkManager();
		bool loadPackage( const QUrl &url );
		bool loadPackages( QProgressDialog *progress = NULL, QList< QUrl > packageURLs = QList< QUrl >() );
		bool loadPackageList( const QUrl &url );
		void finished( QNetworkReply* reply );

	protected slots:
		void handleUnpackError();

	protected:

		QString m_localDir;
		QDomDocument m_packageList;
		DirectoryUnpacker *m_unpacker;
		QNetworkAccessManager* m_network;
		QProgressDialog* m_progress;

		QList< QUrl > m_packagesToLoad;
};

#endif // SERVERLOGIC_H
