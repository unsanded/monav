#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#include <QString>
#include <QUrl>
#include <QDomDocument>

class QNetworkAccessManager;
class QNetworkReply;
class DirectoryUnpacker;

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

	public slots:
		void connectNetworkManager();
		bool loadPackage( const QUrl &url );
		bool loadPackageList( const QUrl &url );
		void finished( QNetworkReply* reply );

	protected slots:
		void handleUnpackError();

	protected:

		QString m_localDir;
		QDomDocument m_packageList;
		DirectoryUnpacker *m_unpacker;
		QNetworkAccessManager* m_network;
};

#endif // SERVERLOGIC_H
