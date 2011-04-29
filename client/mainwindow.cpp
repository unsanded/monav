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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mapdata.h"
#include "mappackageswidget.h"
#include "mapmoduleswidget.h"
#include "overlaywidget.h"
#include "generalsettingsdialog.h"

#include "addressdialog.h"
#include "bookmarksdialog.h"

#include "routinglogic.h"
#include "routedescriptiondialog.h"
#include "gpsdialog.h"
#include "globalsettings.h"

#include <QtDebug>
#include <QInputDialog>
#include <QSettings>
#include <QGridLayout>
#include <QResizeEvent>
#include <QMessageBox>
#include <QStyle>
#include <QScrollArea>
// #include <QSignalMapper>
#include <QUrl>
#include <QDesktopServices>

#ifdef Q_WS_MAEMO_5
	#include "fullscreenexitbutton.h"
	#include <QtGui/QX11Info>
	#include <X11/Xlib.h>
	#include <X11/Xatom.h>
#endif

struct MainWindow::PrivateImplementation {

	enum Mode {
		Modeless, Source, Target, Via, Instructions
	};

	OverlayWidget* targetOverlay;
	OverlayWidget* sourceOverlay;
	OverlayWidget* gotoOverlay;
	OverlayWidget* settingsOverlay;

	QMenuBar* menuBar;

	QMenu* menuFile;
	QMenu* menuRouting;
	QMenu* menuMethods;
	QMenu* menuView;
	QMenu* menuPreferences;
	QMenu* menuHelp;

	QToolBar* toolBarFile;
	QToolBar* toolBarRouting;
	QToolBar* toolBarMethods;
	QToolBar* toolBarView;
	QToolBar* toolBarPreferences;
	QToolBar* toolBarHelp;

	QAction* actionSource;
	QAction* actionVia;
	QAction* actionTarget;
	QAction* actionInstructions;

	QAction* actionBookmark;
	QAction* actionAddress;
	QAction* actionGpsCoordinate;
	QAction* actionRemove;

	QAction* actionZoomIn;
	QAction* actionZoomOut;

	QAction* actionHideControls;
	QAction* actionPackages;
	QAction* actionModules;
	QAction* actionPreferencesGeneral;
	QAction* actionPreferencesRenderer;
	QAction* actionPreferencesRouter;
	QAction* actionPreferencesGpsLookup;
	QAction* actionPreferencesAddressLookup;
	QAction* actionPreferencesGpsReceiver;

	QAction* actionHelpAbout;
	QAction* actionHelpProjectPage;

	int currentWaypoint;

	int maxZoom;
	bool fixed;
	Mode mode;

	void resizeIcons();
};

MainWindow::MainWindow( QWidget* parent ) :
		QMainWindow( parent ),
		m_ui( new Ui::MainWindow )
{
	m_ui->setupUi( this );
	d = new PrivateImplementation;

	createActions();
	populateMenus();
	populateToolbars();
	m_ui->infoWidget->hide();
	m_ui->paintArea->setKeepPositionVisible( true );

	// ensure that we're painting our background
	setAutoFillBackground(true);

	d->mode = PrivateImplementation::Modeless;
	d->fixed = false;

	QSettings settings( "MoNavClient" );
	// explicitly look for geometry as out own might not be initialized properly yet.
	if ( settings.contains( "geometry" ) )
		setGeometry( settings.value( "geometry" ).toRect() );
	GlobalSettings::loadSettings( &settings );

	resizeIcons();
	connectSlots();
	// waypointsChanged();

#ifdef Q_WS_MAEMO_5
	grabZoomKeys( true );
	new FullScreenExitButton(this);
	showFullScreen();
#endif

	MapData* mapData = MapData::instance();

	if ( mapData->loadInformation() )
		mapData->loadLast();

	if ( !mapData->informationLoaded() ) {
		displayMapChooser();
	} else if ( !mapData->loaded() ) {
		displayModuleChooser();
	}
}

MainWindow::~MainWindow()
{
	QSettings settings( "MoNavClient" );
	settings.setValue( "geometry", geometry() );
	GlobalSettings::saveSettings( &settings );

	delete d;
	delete m_ui;
}

void MainWindow::connectSlots()
{
	connect( d->actionSource, SIGNAL( triggered() ), this, SLOT( setModeSourceSelection()));
	connect( d->actionVia, SIGNAL( triggered() ), this, SLOT( setModeViaSelection()));
	connect( d->actionTarget, SIGNAL( triggered() ), this, SLOT( setModeTargetSelection()));
	connect( d->actionInstructions, SIGNAL( triggered() ), this, SLOT( setModeInstructions()));

	connect( d->actionBookmark, SIGNAL( triggered() ), this, SLOT( bookmarks()));
	connect( d->actionAddress, SIGNAL( triggered() ), this, SLOT( addresses()));
	connect( d->actionGpsCoordinate, SIGNAL( triggered() ), this, SLOT( gpsCoordinate()));
	connect( d->actionRemove, SIGNAL( triggered() ), this, SLOT( remove()));

	connect( d->actionZoomIn, SIGNAL( triggered() ), this, SLOT( addZoom()));
	connect( d->actionZoomOut, SIGNAL( triggered() ), this, SLOT( subtractZoom()));

	connect( d->actionHideControls, SIGNAL( triggered() ), this, SLOT( hideControls()));
	connect( d->actionPackages, SIGNAL( triggered() ), this, SLOT( displayMapChooser()));
	connect( d->actionModules, SIGNAL( triggered() ), this, SLOT( displayModuleChooser()));
	connect( d->actionPreferencesGeneral, SIGNAL( triggered() ), this, SLOT( settingsGeneral()));
	connect( d->actionPreferencesRenderer, SIGNAL( triggered() ), this, SLOT( settingsRenderer()));
	connect( d->actionPreferencesRouter, SIGNAL( triggered() ), this, SLOT( settingsRouter()));
	connect( d->actionPreferencesGpsLookup, SIGNAL( triggered() ), this, SLOT( settingsGPSLookup()));
	connect( d->actionPreferencesAddressLookup, SIGNAL( triggered() ), this, SLOT( settingsAddressLookup()));
	connect( d->actionPreferencesGpsReceiver, SIGNAL( triggered() ), this, SLOT( settingsGPS()));

	connect( d->actionHelpAbout, SIGNAL( triggered() ), this, SLOT( about()));
	connect( d->actionHelpProjectPage, SIGNAL( triggered() ), this, SLOT( projectPage()));

	// TODO: Clean up old stuff which is obsolete since the new user interface

	MapData* mapData = MapData::instance();
	connect( m_ui->paintArea, SIGNAL(zoomChanged(int)), this, SLOT(setZoom(int)) );
	connect( m_ui->paintArea, SIGNAL(mouseClicked(ProjectedCoordinate)), this, SLOT(mouseClicked(ProjectedCoordinate)) );

	connect( m_ui->infoIcon1, SIGNAL(clicked()), this, SLOT(showInstructions()) );
	connect( m_ui->infoIcon2, SIGNAL(clicked()), this, SLOT(showInstructions()) );

	connect( mapData, SIGNAL(informationChanged()), this, SLOT(informationLoaded()) );
	connect( mapData, SIGNAL(dataLoaded()), this, SLOT(dataLoaded()) );
	connect( RoutingLogic::instance(), SIGNAL(instructionsChanged()), this, SLOT(instructionsChanged()) );
	// connect( RoutingLogic::instance(), SIGNAL(waypointsChanged()), this, SLOT(waypointsChanged()) );
}

void MainWindow::createActions()
{
	d->actionSource = new QAction( QIcon( ":/images/source.png" ), tr( "Source" ), this );
	d->actionVia = new QAction( QIcon( ":/images/viapoint.png" ), tr( "Via" ), this );
	d->actionTarget = new QAction( QIcon( ":/images/target.png" ), tr( "Target" ), this );
	d->actionInstructions = new QAction( QIcon( ":/images/oxygen/emblem-unlocked.png" ), tr( "Instructions" ), this );

	d->actionBookmark = new QAction( QIcon( ":/images/oxygen/bookmarks.png" ), tr( "Bookmark" ), this );
	d->actionAddress = new QAction( QIcon( ":/images/address.png" ), tr( "Address" ), this );
	d->actionGpsCoordinate = new QAction( QIcon( ":/images/oxygen/network-wireless.png" ), tr( "GPS-Coordinate" ), this );
	d->actionRemove = new QAction( QIcon( ":/images/notok.png" ), tr( "Delete" ), this );

	d->actionZoomIn = new QAction( QIcon( ":/images/oxygen/zoom-in.png" ), tr( "Zoom In" ), this );
	d->actionZoomOut = new QAction( QIcon( ":/images/oxygen/zoom-out.png" ), tr( "Zoom Out" ), this );

	d->actionHideControls = new QAction( QIcon( ":/images/map.png" ), tr( "Hide Controls" ), this );
	d->actionPackages = new QAction( QIcon( ":/images/oxygen/folder-tar.png" ), tr( "Map Packages" ), this );
	d->actionModules = new QAction( QIcon( ":/images/oxygen/map-modules.png" ), tr( "Map Modules" ), this );
	d->actionPreferencesGeneral = new QAction( QIcon( ":/images/oxygen/preferences-system.png" ), tr( "General" ), this );
	d->actionPreferencesRenderer = new QAction( QIcon( ":/images/map.png" ), tr( "Renderer" ), this );
	d->actionPreferencesRouter = new QAction( QIcon( ":/images/route.png" ), tr( "Router" ), this );
	d->actionPreferencesGpsLookup = new QAction( QIcon( ":/images/satellite.png" ), tr( "GPS-Lookup" ), this );
	d->actionPreferencesAddressLookup = new QAction( QIcon( ":/images/address.png" ), tr( "Address Lookup" ), this );
	d->actionPreferencesGpsReceiver = new QAction( QIcon( ":/images/oxygen/hwinfo.png" ), tr( "GPS Receiver" ), this );

	d->actionHelpAbout = new QAction( QIcon( ":/images/about.png" ), tr( "About" ), this );
	d->actionHelpProjectPage = new QAction( QIcon( ":/images/home.png" ), tr( "Homepage" ), this );

	d->actionSource->setCheckable( true );
	d->actionTarget->setCheckable( true );
	d->actionVia->setCheckable( true );
	d->actionInstructions->setCheckable( true );

	d->actionHideControls->setCheckable( true );

	d->actionSource->setShortcut( Qt::Key_S );
	d->actionVia->setShortcut( Qt::Key_V );
	d->actionTarget->setShortcut( Qt::Key_T );
	d->actionInstructions->setShortcut( Qt::Key_I );

	d->actionBookmark->setShortcut( Qt::Key_D );
	d->actionAddress->setShortcut( Qt::Key_A );
	d->actionGpsCoordinate->setShortcut( Qt::Key_G );
	d->actionRemove->setShortcut( Qt::Key_Backspace );

	d->actionZoomIn->setShortcut( Qt::Key_Plus );
	d->actionZoomOut->setShortcut( Qt::Key_Minus );

	d->actionHideControls->setShortcut( Qt::Key_H );
	d->actionPackages->setShortcut( Qt::Key_P );
	d->actionModules->setShortcut( Qt::Key_M );
	d->actionPreferencesGeneral->setShortcut( Qt::Key_P );

	d->actionHelpProjectPage->setShortcut( Qt::Key_H );
}

void MainWindow::populateMenus()
{
	d->menuBar = menuBar();

	d->menuFile = new QMenu( tr( "File" ), this );
	d->menuRouting = new QMenu( tr( "Routing" ), this );
	d->menuMethods = new QMenu( tr( "Methods" ), this );
	d->menuView = new QMenu( tr( "View" ), this );
	d->menuPreferences = new QMenu( tr( "Settings" ), this );
	d->menuHelp = new QMenu( tr( "Help" ), this );

	d->menuFile->addAction( d->actionPackages );
	d->menuFile->addAction( d->actionModules );

	d->menuRouting->addAction( d->actionSource );
	d->menuRouting->addAction( d->actionVia );
	d->menuRouting->addAction( d->actionTarget );
	d->menuRouting->addAction( d->actionInstructions );

	d->menuMethods->addAction( d->actionBookmark );
	d->menuMethods->addAction( d->actionAddress );
	d->menuMethods->addAction( d->actionGpsCoordinate );
	d->menuMethods->addAction( d->actionRemove );

	d->menuView->addAction( d->actionHideControls );
	d->menuView->addAction( d->actionZoomIn );
	d->menuView->addAction( d->actionZoomOut );

	d->menuPreferences->addAction( d->actionPreferencesGeneral );
	d->menuPreferences->addAction( d->actionPreferencesRenderer );
	d->menuPreferences->addAction( d->actionPreferencesRouter );
	d->menuPreferences->addAction( d->actionPreferencesGpsLookup );
	d->menuPreferences->addAction( d->actionPreferencesAddressLookup );
	d->menuPreferences->addAction( d->actionPreferencesGpsReceiver );
#ifdef Q_WS_MAEMO_5
	d->menuPreferences->addAction( d->actionGpsCoordinate );
#endif

	d->menuHelp->addAction( d->actionHelpAbout );
	d->menuHelp->addAction( d->actionHelpProjectPage );

#ifndef Q_WS_MAEMO_5
	d->menuBar->addMenu( d->menuFile );
	d->menuBar->addMenu( d->menuRouting );
	d->menuBar->addMenu( d->menuMethods );
	d->menuBar->addMenu( d->menuView );
#endif
	d->menuBar->addMenu( d->menuPreferences );
	d->menuBar->addMenu( d->menuHelp );
}

void MainWindow::populateToolbars()
{
	d->toolBarFile = addToolBar( tr( "File" ) );
	d->toolBarRouting = addToolBar( tr( "Routing" ) );
	d->toolBarMethods = addToolBar( tr( "Method" ) );
	d->toolBarView = addToolBar( tr( "Zoom" ) );
	d->toolBarPreferences = addToolBar( tr( "Preferences" ) );
	d->toolBarHelp = addToolBar( tr( "Help" ) );
#ifdef Q_WS_MAEMO_5
	d->toolBarPreferences->setVisible( false );
	d->toolBarHelp->setVisible( false );
#endif

	d->toolBarFile->addAction( d->actionPackages );
	d->toolBarFile->addAction( d->actionModules );

	d->toolBarRouting->addAction( d->actionSource );
	d->toolBarRouting->addAction( d->actionVia );
	d->toolBarRouting->addAction( d->actionTarget );
	d->toolBarRouting->addAction( d->actionInstructions );

	d->toolBarMethods->addAction( d->actionBookmark );
	d->toolBarMethods->addAction( d->actionAddress );
#ifndef Q_WS_MAEMO_5
	d->toolBarMethods->addAction( d->actionGpsCoordinate );
#endif
	d->toolBarMethods->addAction( d->actionRemove );

	d->toolBarView->addAction( d->actionHideControls );
	d->toolBarView->addAction( d->actionZoomIn );
	d->toolBarView->addAction( d->actionZoomOut );

	d->toolBarPreferences->addAction( d->actionPreferencesGeneral );
	d->toolBarPreferences->addAction( d->actionPreferencesRenderer );
	d->toolBarPreferences->addAction( d->actionPreferencesRouter );
	d->toolBarPreferences->addAction( d->actionPreferencesGpsLookup );
	d->toolBarPreferences->addAction( d->actionPreferencesAddressLookup );
	d->toolBarPreferences->addAction( d->actionPreferencesGpsReceiver );

	d->toolBarHelp->addAction( d->actionHelpAbout );
	d->toolBarHelp->addAction( d->actionHelpProjectPage );
}

void MainWindow::hideControls()
{
	d->toolBarFile->setVisible( !d->actionHideControls->isChecked() );
	d->toolBarRouting->setVisible( !d->actionHideControls->isChecked() );
	d->toolBarMethods->setVisible( !d->actionHideControls->isChecked() );
	d->toolBarView->setVisible( !d->actionHideControls->isChecked() );
	d->toolBarPreferences->setVisible( !d->actionHideControls->isChecked() );
	d->toolBarHelp->setVisible( !d->actionHideControls->isChecked() );
}

void MainWindow::resizeIcons()
{
	// a bit hackish right now
	// find all suitable children and resize their icons
	// TODO find cleaner way
	int iconSize = GlobalSettings::iconSize();
	foreach ( QToolButton* button, this->findChildren< QToolButton* >() )
		button->setIconSize( QSize( iconSize, iconSize ) );
	foreach ( QToolBar* button, this->findChildren< QToolBar* >() )
		button->setIconSize( QSize( iconSize, iconSize ) );
}

void MainWindow::showInstructions()
{
	RouteDescriptionWidget* widget = new RouteDescriptionWidget( this );
	int index = m_ui->stacked->addWidget( widget );
	m_ui->stacked->setCurrentIndex( index );
	connect( widget, SIGNAL(closed()), widget, SLOT(deleteLater()) );
}

void MainWindow::displayMapChooser()
{
	MapPackagesWidget* widget = new MapPackagesWidget();
	int index = m_ui->stacked->addWidget( widget );
	m_ui->stacked->setCurrentIndex( index );
	widget->show();
	setWindowTitle( "MoNav - Map Packages" );
	connect( widget, SIGNAL(mapChanged()), this, SLOT(mapLoaded()) );
}

void MainWindow::displayModuleChooser()
{
	MapModulesWidget* widget = new MapModulesWidget();
	int index = m_ui->stacked->addWidget( widget );
	m_ui->stacked->setCurrentIndex( index );
	widget->show();
	setWindowTitle( "MoNav - Map Modules" );
	connect( widget, SIGNAL(selected()), this, SLOT(modulesLoaded()) );
	connect( widget, SIGNAL(cancelled()), this, SLOT(modulesCancelled()) );
}

void MainWindow::mapLoaded()
{
	MapData* mapData = MapData::instance();
	if ( !mapData->informationLoaded() )
		return;

	if ( m_ui->stacked->count() <= 1 )
		return;

	QWidget* widget = m_ui->stacked->currentWidget();
	widget->deleteLater();

	if ( !mapData->loadLast() )
		displayModuleChooser();
}

void MainWindow::modulesCancelled()
{
	if ( m_ui->stacked->count() <= 1 )
		return;

	QWidget* widget = m_ui->stacked->currentWidget();
	widget->deleteLater();

	MapData* mapData = MapData::instance();
	if ( !mapData->loaded() ) {
		if ( !mapData->loadLast() )
			displayMapChooser();
	}
}

void MainWindow::modulesLoaded()
{
	if ( m_ui->stacked->count() <= 1 )
		return;

	QWidget* widget = m_ui->stacked->currentWidget();
	widget->deleteLater();
}

void MainWindow::informationLoaded()
{
	MapData* mapData = MapData::instance();
	if ( !mapData->informationLoaded() )
		return;

	this->setWindowTitle( "MoNav - " + mapData->information().name );
}

void MainWindow::dataLoaded()
{
	IRenderer* renderer = MapData::instance()->renderer();
	if ( renderer == NULL )
		return;

	d->maxZoom = renderer->GetMaxZoom();
	m_ui->paintArea->setMaxZoom( d->maxZoom );
	setZoom( GlobalSettings::zoomMainMap());
	m_ui->paintArea->setVirtualZoom( GlobalSettings::magnification() );
	m_ui->paintArea->setCenter( RoutingLogic::instance()->source().ToProjectedCoordinate() );
	m_ui->paintArea->setKeepPositionVisible( true );

	this->setWindowTitle( "MoNav - " + MapData::instance()->information().name );
}

void MainWindow::instructionsChanged()
{
	if ( !d->fixed )
		return;

	QStringList label;
	QStringList icon;

	RoutingLogic::instance()->instructions( &label, &icon, 60 );

	m_ui->infoWidget->setHidden( label.empty() );

	if ( label.isEmpty() )
		return;

	m_ui->infoLabel1->setText( label[0] );
	m_ui->infoIcon1->setIcon( QIcon( icon[0] ) );

	m_ui->infoIcon2->setHidden( label.size() == 1 );
	m_ui->infoLabel2->setHidden( label.size() == 1 );

	if ( label.size() == 1 )
		return;

	m_ui->infoLabel2->setText( label[1] );
	m_ui->infoIcon2->setIcon( QIcon( icon[1] ) );
}

void MainWindow::settingsGeneral()
{
	GeneralSettingsDialog* window = new GeneralSettingsDialog;
	window->exec();
	delete window;
	resizeIcons();
	m_ui->paintArea->setVirtualZoom( GlobalSettings::magnification() );
}

void MainWindow::settingsRenderer()
{
	IRenderer* renderer = MapData::instance()->renderer();
	if ( renderer != NULL )
		renderer->ShowSettings();
}

void MainWindow::settingsRouter()
{
	IRouter* router = MapData::instance()->router();
	if ( router != NULL )
		router->ShowSettings();
}

void MainWindow::settingsGPSLookup()
{
	IGPSLookup* gpsLookup = MapData::instance()->gpsLookup();
	if( gpsLookup != NULL )
		gpsLookup->ShowSettings();
}

void MainWindow::settingsAddressLookup()
{
	IAddressLookup* addressLookup = MapData::instance()->addressLookup();
	if ( addressLookup != NULL )
		addressLookup->ShowSettings();
}

void MainWindow::settingsGPS()
{
	GPSDialog* window = new GPSDialog( this );
	window->exec();
	delete window;
}

void MainWindow::resizeEvent( QResizeEvent* event )
{
	QBoxLayout* box = qobject_cast< QBoxLayout* >( m_ui->infoWidget->layout() );
	assert ( box != NULL );
	if ( event->size().width() > event->size().height() )
		box->setDirection( QBoxLayout::LeftToRight );
	else
		box->setDirection( QBoxLayout::TopToBottom );
}

#ifdef Q_WS_MAEMO_5
void MainWindow::grabZoomKeys( bool grab )
{
	if ( !winId() ) {
		qWarning() << "Can't grab keys unless we have a window id";
		return;
	}

	unsigned long val = ( grab ) ? 1 : 0;
	Atom atom = XInternAtom( QX11Info::display(), "_HILDON_ZOOM_KEY_ATOM", False );
	if ( !atom ) {
		qWarning() << "Unable to obtain _HILDON_ZOOM_KEY_ATOM. This will only work on a Maemo 5 device!";
		return;
	}

	XChangeProperty ( QX11Info::display(), winId(), atom, XA_INTEGER, 32, PropModeReplace, reinterpret_cast< unsigned char* >( &val ), 1 );
}

void MainWindow::keyPressEvent( QKeyEvent* event )
{
	switch (event->key()) {
	case Qt::Key_F7:
		addZoom();
		event->accept();
		break;

	case Qt::Key_F8:
		this->subtractZoom();
		event->accept();
		break;
	}

	QWidget::keyPressEvent(event);
}
#endif

void MainWindow::setModeSourceSelection()
{
	if ( !d->actionSource->isChecked() ){
		setModeless();
		return;
	}

	d->mode = PrivateImplementation::Source;
	d->fixed = false;
	m_ui->paintArea->setFixed( false );
	m_ui->paintArea->setKeepPositionVisible( false );
	m_ui->infoWidget->hide();

	d->actionVia->setChecked( false );
	d->actionTarget->setChecked( false );
	d->actionInstructions->setChecked( false );

	d->toolBarMethods->setDisabled( false );
}

void MainWindow::setModeViaSelection()
{
	if ( !d->actionVia->isChecked() ){
		setModeless();
		return;
	}

	d->mode = PrivateImplementation::Via;
	d->fixed = false;
	m_ui->paintArea->setFixed( false );
	m_ui->paintArea->setKeepPositionVisible( false );
	m_ui->infoWidget->hide();

	d->actionSource->setChecked( false );
	d->actionTarget->setChecked( false );
	d->actionInstructions->setChecked( false );

	d->toolBarMethods->setDisabled( false );
}

void MainWindow::setModeTargetSelection()
{
	if ( !d->actionTarget->isChecked() ){
		setModeless();
		return;
	}

	d->mode = PrivateImplementation::Target;
	d->fixed = false;
	m_ui->paintArea->setFixed( false );
	m_ui->paintArea->setKeepPositionVisible( false );
	m_ui->infoWidget->hide();

	d->actionSource->setChecked( false );
	d->actionVia->setChecked( false );
	d->actionInstructions->setChecked( false );

	d->toolBarMethods->setDisabled( false );
}

void MainWindow::setModeInstructions()
{
	if ( !d->actionInstructions->isChecked() ){
		setModeless();
		return;
	}

	d->mode = PrivateImplementation::Instructions;
	d->fixed = true;
	m_ui->paintArea->setFixed( true );
	m_ui->paintArea->setKeepPositionVisible( true );
	m_ui->infoWidget->show();

	d->actionSource->setChecked( false );
	d->actionTarget->setChecked( false );
	d->actionVia->setChecked( false );

	d->toolBarMethods->setDisabled( true );
	instructionsChanged();
}

void MainWindow::setModeless()
{
	d->mode = PrivateImplementation::Modeless;
	d->fixed = false;
	m_ui->paintArea->setFixed( false );
	m_ui->infoWidget->hide();

	d->actionSource->setChecked( false );
	d->actionTarget->setChecked( false );
	d->actionVia->setChecked( false );
	d->actionInstructions->setChecked( false );

	d->toolBarMethods->setDisabled( false );
}

void MainWindow::mouseClicked( ProjectedCoordinate clickPos )
{
	UnsignedCoordinate coordinate( clickPos );
	if ( d->mode == PrivateImplementation::Source ) {
		RoutingLogic::instance()->setSource( coordinate );
	}
	if ( d->mode == PrivateImplementation::Via ) {
		RoutingLogic::instance()->setWaypoint( d->currentWaypoint, coordinate );
	}
	if ( d->mode == PrivateImplementation::Target ){
		RoutingLogic::instance()->setTarget( coordinate );
	}
}

void MainWindow::subductRoutepoint()
{
	RoutingLogic* routingLogic = RoutingLogic::instance();
	QVector< UnsignedCoordinate > waypoints = routingLogic->waypoints();
	if ( d->currentWaypoint >= waypoints.size() )
		return;
	waypoints.remove( d->currentWaypoint );
	routingLogic->setWaypoints( waypoints );
}

void MainWindow::addViapoint( UnsignedCoordinate coordinate )
{
	RoutingLogic* routingLogic = RoutingLogic::instance();
	QVector< UnsignedCoordinate > waypoints = routingLogic->waypoints();
	waypoints.clear();

	routingLogic->setWaypoints( waypoints );
}

void MainWindow::bookmarks()
{
	UnsignedCoordinate result;
	if ( !BookmarksDialog::showBookmarks( &result, this ) )
		return;

	if ( d->mode == d->Source )
		RoutingLogic::instance()->setSource( result );
	if ( d->mode == d->Target )
		RoutingLogic::instance()->setTarget( result );
	if ( d->mode == d->Via )
		addViapoint( result );
	m_ui->paintArea->setKeepPositionVisible( false );
	m_ui->paintArea->setCenter( result.ToProjectedCoordinate() );
}

void MainWindow::addresses()
{
	if ( MapData::instance()->addressLookup() == NULL )
		return;
	UnsignedCoordinate result;
	if ( !AddressDialog::getAddress( &result, this ) )
		return;

	if ( d->mode == d->Source )
		RoutingLogic::instance()->setSource( result );
	if ( d->mode == d->Target )
		RoutingLogic::instance()->setTarget( result );
	if ( d->mode == d->Via )
		addViapoint( result );
	m_ui->paintArea->setKeepPositionVisible( false );
	m_ui->paintArea->setCenter( result.ToProjectedCoordinate() );
}

void MainWindow::gpsLocation()
{
	const RoutingLogic::GPSInfo& gpsInfo = RoutingLogic::instance()->gpsInfo();
	if ( !gpsInfo.position.IsValid() )
		return;
	GPSCoordinate gps( gpsInfo.position.ToGPSCoordinate().latitude, gpsInfo.position.ToGPSCoordinate().longitude );
	UnsignedCoordinate result = UnsignedCoordinate( gps );
	if ( d->mode == d->Source )
		RoutingLogic::instance()->setSource( result );
	if ( d->mode == d->Target )
		RoutingLogic::instance()->setTarget( result );
	if ( d->mode == d->Via )
		addViapoint( result );

	m_ui->paintArea->setCenter( ProjectedCoordinate( gps ) );
	m_ui->paintArea->setKeepPositionVisible( true );

	IRenderer* renderer = MapData::instance()->renderer();
	if ( renderer == NULL )
		return;
	setZoom( renderer->GetMaxZoom() - 5 );
}

void MainWindow::gpsCoordinate()
{
	bool ok = false;
	double latitude = QInputDialog::getDouble( this, "Enter Coordinate", "Enter Latitude", 0, -90, 90, 5, &ok );
	if ( !ok )
		return;
	double longitude = QInputDialog::getDouble( this, "Enter Coordinate", "Enter Longitude", 0, -180, 180, 5, &ok );
	if ( !ok )
		return;
	GPSCoordinate gps( latitude, longitude );

	UnsignedCoordinate result = UnsignedCoordinate( gps );
	if ( d->mode == d->Source )
		RoutingLogic::instance()->setSource( result );
	if ( d->mode == d->Target )
		RoutingLogic::instance()->setTarget( result );
	if ( d->mode == d->Via )
		addViapoint( result );

	m_ui->paintArea->setCenter( ProjectedCoordinate( gps ) );
	m_ui->paintArea->setKeepPositionVisible( false );

	IRenderer* renderer = MapData::instance()->renderer();
	if ( renderer == NULL )
		return;
	setZoom( renderer->GetMaxZoom() - 5 );
}

void MainWindow::remove()
{
	// TODO: RoutingLogic::instance()->clear() does not remove rendering waypoints and target.
	if ( d->mode == d->Via )
		subductRoutepoint();
	else
		RoutingLogic::instance()->clear();

	// m_ui->paintArea->setCenter( result.ToProjectedCoordinate() );
}

void MainWindow::addZoom()
{
	setZoom( GlobalSettings::zoomMainMap() + 1 );
}

void MainWindow::subtractZoom()
{
	setZoom( GlobalSettings::zoomMainMap() - 1 );
}

void MainWindow::setZoom( int zoom )
{
	IRenderer* renderer = MapData::instance()->renderer();
	if ( renderer == NULL )
		return;
	if( zoom > renderer->GetMaxZoom() )
		zoom = renderer->GetMaxZoom();
	if( zoom < 0 )
		zoom = 0;

	m_ui->paintArea->setZoom( zoom );
	GlobalSettings::setZoomMainMap( zoom );
}

void MainWindow::about()
{
	// TODO: Create a better dialog displaying better information, and read some of the data from an include file or define.
	QMessageBox::about( this, tr("About MoNav"), "MoNav 0.4 is (c) 2011 by Christian Vetter and was released under the GNU GPL v3." );
}

void MainWindow::projectPage()
{
	QDesktopServices::openUrl( QUrl( tr("http://code.google.com/p/monav/") ) );
}

