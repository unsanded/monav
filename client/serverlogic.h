#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#include <QString>
#include <QUrl>
#include <QFile>
#include <QNetworkAccessManager>
#include <QDomDocument>

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

		const QDomDocument& packageList() const;

	signals:
		void loadedList();

	public slots:
		void connectNetworkManager();
		bool loadPackageList( const QString &dir, const QUrl &url );
		void finished( QNetworkReply* reply );

	protected:

		QString m_localDir;
		QDomDocument m_packageList;
		QNetworkAccessManager* m_network;
};

#endif // SERVERLOGIC_H
