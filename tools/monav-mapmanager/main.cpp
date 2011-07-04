#include <stdio.h>

#include <QStringList>
#include <QSettings>
#include <QFile>
#include <QtXml/QDomDocument>

const int VERSION = 2;

enum OPERATION { CREATE_LIST, DELETE_LIST, NEW_MAP, DELETE_MAP, ADD_PACKAGE, DELETE_PACKAGE, DEFAULT };

QDomElement findPackageElement( QDomDocument &list, QString packageType, QString packageName, QString mapName = "" )
{
	QDomNodeList packageNodeList = list.elementsByTagName( packageType );

	for( unsigned int i=0; i < packageNodeList.length(); i++ )
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

int main( int argc, char *argv[] )
{
	if( argc != 3 )
	{
		printf( "usage:\n" );
		printf( "-c server_url : create list\n" );
		printf( "-r server_url : delete list\n" );
		printf( "-n map_path_and_name : add map to list\n" );
		printf( "-e map_path_and_name : delete map to list\n" );
		printf( "-a module_path_and_name : add module to list\n" );
		printf( "-d module_path_and_name : delete module from list\n" );
		return 0;
	}

	QString serverPath = "";
	QString packagePath = "";
	OPERATION listOperation = DEFAULT;

	int c;
	while ( ( c = getopt( argc, argv, "c:r:n:e:a:d:" ) ) != -1 )
	{
		switch(c)
		{
			case 'c':
				listOperation = CREATE_LIST;
				serverPath = optarg;
				break;
			case 'r':
				listOperation = DELETE_LIST;
				break;
			case 'a':
				listOperation = ADD_PACKAGE;
				packagePath = optarg;
				break;
			case 'd':
				listOperation = DELETE_PACKAGE;
				packagePath = optarg;
				break;
			default:
				printf( "operation not recognized\n" );
				return 1;
		}
	}

	QString packageName = packagePath;
	packageName.remove( 0, packageName.lastIndexOf( '/' ) + 1 );
	packageName.truncate( packageName.lastIndexOf( '.' ) );

	if( packagePath.startsWith( "." ) )
		packagePath.remove( 0, 1 );

	if( packagePath.startsWith( "/" ) )
		packagePath.remove( 0, 1 );


	QDomDocument list;
	QFile listFile( "packageList.xml" );

	if( listOperation == CREATE_LIST )
	{
		if (listFile.exists())
		{
			printf( "a previous list file exists on the server, delete it first using -r server_path\n" );
			return 1;
		}

		if( !serverPath.endsWith( "/" ) )
			serverPath.append( "/" );

		QDomElement rootElement = list.createElement( "root" );
		rootElement.setAttribute( "path", serverPath );
		rootElement.setAttribute( "timestamp", 0 );
		rootElement.setAttribute( "version", VERSION );
		list.appendChild(rootElement);

		if ( !listFile.open( QIODevice::ReadWrite | QIODevice::Truncate ) )
		{
			printf( "failed to open list file\n" );
			return 1;
		}
		listFile.write( list.toString().toUtf8() );
		listFile.close();

		printf( "created list\n" );
		return 0;
	}

	if ( !listFile.exists() )
	{
		printf( "create list file first using -c server_path\n" );
		return 1;
	}

	if( listOperation == DELETE_LIST )
	{
		if ( !listFile.remove() )
		{
			printf( "error deleting list file\n" );
			return 1;
		}

		printf( "deleted list\n" );
		return 0;
	}

	if ( !listFile.open( QIODevice::ReadOnly ) )
	{
		printf( "failed to open list file\n" );
		return 1;
	}

	if ( !list.setContent( &listFile ) )
	{
		printf("error parsing list file\n");
		listFile.close();
		return 1;
	}
	listFile.close();

	QDomElement rootElement = list.documentElement();

	// map package
	if( packagePath.endsWith( ".ini" ) )
	{
		if( !QFile( packagePath ).exists() )
		{
			printf( "map file does not exist (required for parsing)\n" );
			printf( "path: " + packagePath.toUtf8() + "\n" );
			return 1;
		}

		QSettings mapFile( packagePath, QSettings::IniFormat );
		QString mapName = mapFile.value( "name" ).toString();
		QDomElement mapElement = findPackageElement( list, "map", mapName );

		if( listOperation == DELETE_PACKAGE )
		{
			if( mapElement.isNull() )
			{
				printf( "map not in list\n" );
				return 1;
			}

			rootElement.removeChild( mapElement );

			printf( "deleted map entry\n" );
		}

		if( listOperation == ADD_PACKAGE )
		{

			if( !mapElement.isNull() )
			{
				printf( "map already in list\n" );
				return 1;
			}

			mapElement = list.createElement( "map" );
			mapElement.setAttribute( "name" , mapName );
			mapElement.setAttribute( "path" , packagePath );
			rootElement.appendChild( mapElement );

			printf( "created map entry\n" );
		}
	}

	// module package
	else if( packagePath.endsWith( ".mmm" ) )
	{
		QStringList packageAttributes = packageName.split("_");
		QString type = packageAttributes[0];
		QString mapName =  packageAttributes[1];
		QString name = packageAttributes[2];
		QDomElement packageElement = findPackageElement( list, "module", name, mapName );

		if( listOperation == DELETE_PACKAGE )
		{
			if( packageElement.isNull() )
			{
				printf("module package not in list\n");
				return 1;
			}

			QDomElement parentElement = packageElement.parentNode().toElement();
			parentElement.removeChild( packageElement );

			if( !parentElement.hasChildNodes() )
				parentElement.parentNode().removeChild( parentElement );

			printf( "deleted module entry\n" );
		}

		if( listOperation == ADD_PACKAGE )
		{
			QDomElement mapElement = findPackageElement( list, "map", mapName ).toElement();
			if( mapElement.isNull() )
			{
				printf("corresponding map not in list, add it first\n");
				return 1;
			}

			if( !packageElement.isNull() )
			{
				printf( "module package already in list\n" );
				return 1;
			}

			int timestamp = rootElement.attribute( "timestamp" ).toInt() + 1;
			rootElement.setAttribute( "timestamp", timestamp );
			mapElement.setAttribute( "timestamp", timestamp );

			QDomElement typeElement = mapElement.firstChildElement( type );
			if( typeElement.isNull() )
			{
				typeElement = list.createElement( type );
				mapElement.appendChild(typeElement);
			}
			typeElement.setAttribute( "timestamp", timestamp );

			QDomElement moduleElement = list.createElement( "module" );
			moduleElement.setAttribute( "name", name );
			moduleElement.setAttribute( "timestamp", timestamp );
			typeElement.appendChild( moduleElement );

			QDomText pathNode = list.createTextNode(packagePath);
			moduleElement.appendChild(pathNode);

			printf( "added module entry\n" );
		}
	}

	else
	{
		printf( "unrecognized package format\n" );
		return 1;
	}

	if (!listFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) return 1;
	listFile.write(list.toString().toUtf8());
	listFile.close();

	return 0;
}
