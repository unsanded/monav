#include <stdio.h>

#include <QStringList>
#include <QSettings>
#include <QFile>
#include <QtXml/QDomDocument>

const int VERSION = 1;

enum OPERATION { NEW_MAP, DELETE_MAP, ADD_PACKAGE, DELETE_PACKAGE, CREATE_LIST, DELETE_LIST };

QDomNode findPackageNode(QDomDocument &list, QString packageType, QString packageName)
{
	QDomNodeList packageNodeList = list.elementsByTagName(packageType);

	for(unsigned int i=0; i<packageNodeList.length(); i++)
	{
		QDomNode curPackage = packageNodeList.at(i);

		if(packageType == "map")
		{
			if(curPackage.toElement().attribute("name") == packageName)
				return curPackage;
		}

		else
		{
			if(curPackage.toElement().text() == packageName)
				return curPackage;
		}
	}

	return QDomNode();
}

QDomNode findMapVersionNode(QDomNode &mapNode, QString &mapVersion)
{
	QDomNodeList versionNodes = mapNode.childNodes();

	for(int i=0; i<versionNodes.size(); i++)
	{
		if(versionNodes.at(i).toElement().attribute("name") == mapVersion)
			return versionNodes.at(i);
	}

	return QDomNode();
}

int main( int argc, char *argv[] )
{
	QSettings settings("MonavMapManager");

	if(argc == 1)
	{
		printf("usage:\n");
		printf("-c server_path : create list\n");
		printf("-r delete list\n");
		printf("-n [map_path_and_name] : add map to list\n");
		printf("-e [map_path_and_name] : delete map to list\n");
		printf("-a [module_path_and_name] : add module to list\n");
		printf("-d [module_path_and_name] : delete module from list\n");
		return 0;
	}

	QString listPath = settings.value("ListPath","").toString();
	QString packagePath = "";

	OPERATION listOperation;

	int c;
	while ((c = getopt(argc, argv, "c:d:n:a:e:r")) != -1)
	{
		switch(c)
		{
			case 'r':
				listOperation = DELETE_LIST;
				break;

			case 'c':
				listOperation = CREATE_LIST;
				listPath = optarg;
				break;
			case 'd':
				listOperation = DELETE_PACKAGE;
				packagePath = optarg;
				break;
			case 'e':
				listOperation = DELETE_MAP;
				packagePath = optarg;
				break;
			case 'n':
				listOperation = NEW_MAP;
				packagePath = optarg;
				break;
			case 'a':
				listOperation = ADD_PACKAGE;
				packagePath = optarg;
				break;
		}
	}

	if(!listPath.endsWith("/")) listPath.append("/");

	QString packageName = packagePath;
	packageName.remove(0, packageName.lastIndexOf('/')+1);
	packageName.truncate(packageName.lastIndexOf('.'));

	QDomDocument list;
	QFile listFile(listPath+("packageList.xml"));

	if(listOperation == CREATE_LIST)
	{
		if (listFile.exists())
		{
			printf("a previous list file exists on the server, delete it first using -r server_path\n");
			return 1;
		}

		QDomElement rootElement = list.createElement("root");
		rootElement.setAttribute("path", listPath);
		rootElement.setAttribute("timestamp", 0);
		list.appendChild(rootElement);

		if (!listFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) return 1;
		listFile.write(list.toString().toUtf8());
		listFile.close();

		settings.setValue("ListPath", listPath);
		return 0;
	}

	if (!listFile.exists())
	{
		printf(listPath.toUtf8());
		printf("no list file exists on the server, create one first using -c server_path\n");
		return 1;
	}

	if(listOperation == DELETE_LIST)
	{
		if (!listFile.remove())
		{
			printf("error deleting list file\n");
			return 1;
		}

		settings.setValue("ListPath", "");
		return 0;
	}

	if (!listFile.open(QIODevice::ReadOnly)) return 1;
	if (!list.setContent(&listFile))
	{
		printf("error parsing list file\n");
		listFile.close();
		return 1;
	}
	listFile.close();

	QDomElement rootElement = list.documentElement();

	if(listOperation == NEW_MAP)
	{
		if(!QFile(packagePath).exists())
		{
			printf("could not open map file for parsing\n");
			printf("path: "+packagePath.toUtf8()+"\n");
			return 1;
		}

		QSettings mapFile( packagePath, QSettings::IniFormat );

		QString mapName = mapFile.value("name").toString();
		QString mapVersion = mapFile.value("configVersion").toString();

		QDomNode mapNode = findPackageNode(list, "map", mapName);

		if(mapNode.isNull())
		{
			mapNode = list.createElement("map");
			mapNode.toElement().setAttribute("name", mapName);
			rootElement.appendChild(mapNode);

			printf("creating new map entry\n");
		}

		else
		{
			if(!findMapVersionNode(mapNode, mapVersion).isNull())
			{
				printf("map version already in list\n");
				return 1;
			}
		}

		QDomElement versionElement = list.createElement("version");
		versionElement.setAttribute("name", mapVersion);
		versionElement.setAttribute("complete", "no");
		versionElement.setAttribute("path", packagePath);
		mapNode.appendChild(versionElement);
	}

	else if(listOperation == DELETE_MAP)
	{
		if(!QFile(packagePath).exists())
		{
			printf("could not open map file for parsing\n");
			printf("path: "+packagePath.toUtf8()+"\n");
			return 1;
		}

		QSettings mapFile( packagePath, QSettings::IniFormat );

		QString mapName = mapFile.value("name").toString();
		QString mapVersion = mapFile.value("configVersion").toString();

		QDomNode mapNode = findPackageNode(list,"map", mapName);

		if(mapNode.isNull())
		{
			printf("map not in list\n");
			return 1;
		}

		QDomNode versionNode = findMapVersionNode(mapNode, mapVersion);

		if(versionNode.isNull())
		{
			printf("map version not in list\n");
			return 1;
		}

		mapNode.removeChild(versionNode);

		if(mapNode.firstChildElement("version").isNull()) rootElement.removeChild(mapNode);
	}

	else if(listOperation == ADD_PACKAGE)
	{
		if(!findPackageNode(list, "module", packagePath).isNull())
		{
			printf("module package already in list\n");
			return 1;
		}

		/* packageName : type_mapname_version_subtype */
		QStringList packageAttributes = packageName.split("_");

		QDomNode mapNode = findPackageNode(list, "map", packageAttributes[1]);
		if(mapNode.isNull())
		{
			printf("corresponding map not in list, add it first\n");
			return 1;
		}

		QDomElement	versionElement = findMapVersionNode(mapNode, packageAttributes[2]).toElement();

		if(versionElement.isNull())
		{
			printf("no corresponding map version in list, add it first\n");
			return 1;
		}

		QDomElement typeElement = versionElement.firstChildElement(packageAttributes[0]);
		if(typeElement.isNull()) typeElement = list.createElement(packageAttributes[0]);
		versionElement.appendChild(typeElement);

		QDomElement moduleElement = list.createElement("module");
		moduleElement.setAttribute("name", packageAttributes[3]);
		moduleElement.setAttribute("timestamp", rootElement.attribute("timestamp").toInt()+1);
		rootElement.setAttribute("timestamp", moduleElement.attribute("timestamp").toInt());
		typeElement.appendChild(moduleElement);

		QDomText pathNode = list.createTextNode(packagePath);
		moduleElement.appendChild(pathNode);
	}

	else if(listOperation == DELETE_PACKAGE)
	{
		QDomNode packageNode = findPackageNode(list, "module", packagePath);

		if(packageNode.isNull())
		{
			printf("module package not in list\n");
			return 1;
		}

		QStringList packageAttributes = packageName.split("_");

		QDomNode mapNode = findPackageNode(list, "map", packageAttributes[1]);
		if(mapNode.isNull())
		{
			printf("corresponding map not in list, add it first\n");
			return 1;
		}

		QDomElement	versionElement = findMapVersionNode(mapNode, packageAttributes[2]).toElement();

		if(versionElement.isNull())
		{
			printf("no corresponding map version in list, add it first\n");
			return 1;
		}

		versionElement.removeChild(packageNode);
	}

	if (!listFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) return 1;
	listFile.write(list.toString().toUtf8());
	listFile.close();

	return 0;
}
