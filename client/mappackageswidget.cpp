/*
Copyright 2010  Christian Vetter veaac.fdirct@gmail.com

This file is part of MoNav.

MoNav is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoNav is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoNav.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mappackageswidget.h"
#include "ui_mappackageswidget.h"
#include "serverinputdialog.h"
#include "mapdata.h"
#include "serverlogic.h"

#include <QResizeEvent>
#include <QShowEvent>
#include <QSettings>
#include <QStringList>
#include <QDir>
#include <QProgressDialog>
#include <QFileDialog>
#include <QtDebug>
#include <QMessageBox>

struct MapPackagesWidget::PrivateImplementation {

	int selected;

	QString path;
	QVector< MapData::MapPackage > maps;
	QVector< ServerLogic::Server > servers;
	ServerLogic *serverLogic;
	QProgressDialog *progress;

	void populateInstalled( QListWidget* list );
	void highlightButton( QPushButton* button, bool highlight );
};

MapPackagesWidget::MapPackagesWidget( QWidget* parent ) :
	QWidget( parent ),
	m_ui( new Ui::MapPackagesWidget )
{
	m_ui->setupUi( this );
	d = new PrivateImplementation;

	d->serverLogic = NULL;
	d->progress = NULL;

	QSettings settings( "MoNavClient" );
	settings.beginGroup( "MapPackages" );
	d->path = settings.value( "path" ).toString();
	bool worldMap = settings.value( "worldmap", true ).toBool();
	m_ui->installedList->setVisible( !worldMap );
	m_ui->worldMap->setVisible( worldMap );
	m_ui->switchSelection->setChecked( worldMap );

	int entries = settings.beginReadArray( "servers" );
	for ( int i = 0; i < entries; i++ ) {
		settings.setArrayIndex( i );
		ServerLogic::Server server;
		server.name = settings.value( "name" ).toString();
		server.url = settings.value( "url" ).toUrl();
		d->servers.push_back( server );
		m_ui->server->addItem( server.name, server.url );
	}
	settings.endArray();
	m_ui->server->setCurrentIndex( settings.value( "server", -1 ).toInt() );
	// TODO: INSERT DEFAULT SERVER

	connect( m_ui->changeDirectory, SIGNAL(clicked()), this, SLOT(directory()) );
	connect( m_ui->load, SIGNAL(clicked()), this, SLOT(load()) );
	connect( m_ui->check, SIGNAL(clicked()), this, SLOT(check()) );
	connect( m_ui->update, SIGNAL(clicked()), this, SLOT(update()) );
	connect( m_ui->download, SIGNAL(clicked()), this, SLOT(download()) );
	connect( m_ui->addServer, SIGNAL(clicked()), this, SLOT(editServerList()) );
	// BACK
	// CLICK
	connect( m_ui->installedList, SIGNAL(itemSelectionChanged()), this, SLOT(mapSelectionChanged()) );
	connect( m_ui->updateList, SIGNAL(itemSelectionChanged()), this, SLOT(updateSelectionChanged()) );
	connect( m_ui->downloadList, SIGNAL(itemSelectionChanged()), this, SLOT(downloadSelectionChanged()) );
	connect( m_ui->worldMap, SIGNAL(clicked(int)), this, SLOT(selected(int)) );

	d->populateInstalled( m_ui->installedList );
	d->highlightButton( m_ui->changeDirectory, m_ui->installedList->count() == 0 );
	m_ui->worldMap->setMaps( d->maps );
	m_ui->worldMap->setHighlight( d->selected );
	m_ui->downloadList->setHeaderLabel("Server Package List");
}

MapPackagesWidget::~MapPackagesWidget()
{
	QSettings settings( "MoNavClient" );
	settings.beginGroup( "MapPackages" );
	settings.setValue( "path", d->path );
	settings.setValue( "worldmap", m_ui->switchSelection->isChecked() );
	settings.setValue( "server", m_ui->server->currentIndex());
	settings.beginWriteArray( "servers", d->servers.size() );
	for ( int i = 0; i < d->servers.size(); i++ ) {
		settings.setArrayIndex( i );
		settings.setValue( "name", d->servers[i].name );
		settings.setValue( "url", d->servers[i].url );
	}
	settings.endArray();

	if(d->serverLogic != NULL)
		delete d->serverLogic;
	if(d->progress != NULL)
		delete d->progress;
	delete d;
	delete m_ui;
}

void MapPackagesWidget::resizeEvent ( QResizeEvent* /*event*/ )
{
	// TODO CHANGE ORIENTATION
}

void MapPackagesWidget::showEvent( QShowEvent* /*event*/ )
{
	if ( !QFile::exists( d->path ) ) {
		//QMessageBox::information( this, "Data Directory", "Before proceeding be sure to select a valid data directory", "Ok" );
	}
}

void MapPackagesWidget::selected( int id )
{
	m_ui->installedList->item( id )->setSelected( true );
}

void MapPackagesWidget::mapSelectionChanged()
{
	bool selected = m_ui->installedList->selectedItems().size() == 1;
	if ( selected )
		m_ui->worldMap->setHighlight( m_ui->installedList->selectedItems().first()->data( Qt::UserRole ).toInt() );
	m_ui->load->setEnabled( selected );
	m_ui->deleteMap->setEnabled( selected );
}

void MapPackagesWidget::updateSelectionChanged()
{
	bool selected = m_ui->updateList->selectedItems().size() > 0;
	m_ui->update->setEnabled( selected );
}

void MapPackagesWidget::downloadSelectionChanged()
{
	bool selected = m_ui->downloadList->selectedItems().size() > 0;
	m_ui->download->setEnabled( selected );
}

void MapPackagesWidget::load()
{
	QList< QListWidgetItem* > items = m_ui->installedList->selectedItems();
	if ( items.size() != 1 ) {
		qDebug() << "Error: only one map should be selected";
		return;
	}

	int index = items.first()->data( Qt::UserRole ).toInt();

	MapData* mapData = MapData::instance();
	mapData->setPath( d->maps[index].path );
	if ( !mapData->loadInformation() )
		return;

	emit mapChanged();
}

void MapPackagesWidget::directory()
{
	QString newDir = QFileDialog::getExistingDirectory( this, "MoNav Data Directory", d->path );
	if ( newDir.isEmpty() || newDir == d->path )
		return;

	d->path = newDir;
	d->populateInstalled( m_ui->installedList );
	d->highlightButton( m_ui->changeDirectory, m_ui->installedList->count() == 0 );
	m_ui->worldMap->setMaps( d->maps );
	m_ui->worldMap->setHighlight( d->selected );
}

void MapPackagesWidget::setupNetworkAccess()
{
	d->serverLogic = new ServerLogic();
	connect( d->serverLogic, SIGNAL( loadedList() ), this, SLOT( populateServerPackageList() ) );
	d->serverLogic->connectNetworkManager();

}
void MapPackagesWidget::check()
{
	QString name = d->servers[m_ui->server->currentIndex()].name;
	QUrl url = d->servers[m_ui->server->currentIndex()].url;

	QMessageBox dlMsg;
	dlMsg.setText( "Downloading package list from " + name + " (" + url.toString() + ")." );
	dlMsg.setInformativeText( "Downloading the package list requires an active internet connection. Proceed?" );
	dlMsg.setStandardButtons( QMessageBox::No | QMessageBox::Yes );
	dlMsg.setDefaultButton( QMessageBox::Yes );

	if( dlMsg.exec() != QMessageBox::Yes )
		return;

	if(d->serverLogic == NULL)
		setupNetworkAccess();

	d->serverLogic->setLocalDir( d->path );
	d->serverLogic->loadPackageList( url );
}

void MapPackagesWidget::update()
{

}

void MapPackagesWidget::download()
{
	if(d->serverLogic == NULL)
		setupNetworkAccess();

	d->serverLogic->setLocalDir( d->path );

	QList< QTreeWidgetItem* > selected = m_ui->downloadList->selectedItems();
	QList< QUrl > packagesToLoad;

	foreach ( QTreeWidgetItem* item, selected )
	{
		QDomNodeList packages = d->serverLogic->packageList().elementsByTagName( item->data( 0, Qt::UserRole ).toString() );

		QDomElement element;
		for( int i=0; i < packages.size(); i++ )
		{
			if( item->text( 0 ) == packages.item(i).toElement().attribute( "name" ) )
			{
				element = packages.item(i).toElement();
				break;
			}
		}

		if( element.isNull() )
		{
			qDebug() << "package not found:" << item->data( 0, Qt::UserRole ).toString() << item->text( 0 );
			return;
		}

		QStringList modules = element.text().split(".mmm", QString::SkipEmptyParts);

		for( int i=0; i < modules.size(); i++ )
			packagesToLoad.append( QUrl( modules.at(i) + ".mmm" ) );

		item->setSelected( false );
	}

	d->progress = new QProgressDialog( "Starting package download", "Abort download", 0, packagesToLoad.size(), this );
	d->progress->setWindowModality( Qt::WindowModal );
	d->progress->setValue( 0 );
	d->progress->show();

	d->serverLogic->loadPackages( d->progress, packagesToLoad );
}

void MapPackagesWidget::editServerList()
{
	ServerInputDialog* dialog = new ServerInputDialog( d->servers, this );

	if( dialog->exec() == QDialog::Accepted )
	{
		dialog->writeServerSettings( &d->servers );

		m_ui->server->clear();
		for(int i=0; i < d->servers.size(); i++)
			m_ui->server->addItem( d->servers[i].name, d->servers[i].url );
	}

	delete dialog;
}

void MapPackagesWidget::populateServerPackageList()
{
	m_ui->downloadList->clear();

	QDomElement element = d->serverLogic->packageList().documentElement().firstChildElement();
	QTreeWidgetItem *parent = NULL;

	while(!element.isNull())
	{
		QString name = element.hasAttribute( "name" ) ? element.attribute( "name" ) : element.tagName();

		QTreeWidgetItem *item = new QTreeWidgetItem( parent, QStringList( name ) );
		item->setData(0, Qt::UserRole, element.tagName());
		m_ui->downloadList->addTopLevelItem( item );

		if(!element.firstChildElement().isNull())
		{
			element = element.firstChildElement();
			parent = item;
		}

		else
		{
			while(element.nextSiblingElement().isNull() && !element.parentNode().isNull())
			{
				element = element.parentNode().toElement();
				parent = parent != NULL ? parent->parent() : NULL;
			}

			element = element.nextSiblingElement();
		}
	}
}

void MapPackagesWidget::PrivateImplementation::populateInstalled( QListWidget* list )
{
	list->clear();
	maps.clear();
	selected = -1;

	MapData* mapData = MapData::instance();
	if ( !mapData->searchForMapPackages( path, &maps, 2 ) )
		return;

	for ( int i = 0; i < maps.size(); i++ ) {
		QListWidgetItem* item = new QListWidgetItem( maps[i].name );
		item->setData( Qt::UserRole, i );
		list->addItem( item );
		if ( maps[i].path == mapData->path() ) {
			item->setSelected( true );
			selected = i;
		}
	}
}

void MapPackagesWidget::PrivateImplementation::highlightButton( QPushButton* button, bool highlight )
{
	QFont font = button->font();
	font.setBold( highlight );
	font.setUnderline( highlight );
	button->setFont( font );
}
