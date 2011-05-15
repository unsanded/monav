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

#include "interfaces/irouter.h"
#include "mapdata.h"
#include "routinglogic.h"
#include "logger.h"

#include "tripinfodialog.h"
#include "ui_tripinfodialog.h"

#define DIALOGUPDATEINTERVAL 5


TripinfoDialog::TripinfoDialog( QWidget* parent ) :
		QWidget( parent ),
		m_ui( new Ui::TripinfoDialog )
{
	m_ui->setupUi( this );
qDebug() << "Konstruktor start";
	QStringList itemNames;
	itemNames << tr( "Ground Speed:" );
	itemNames << tr( "Max. Speed:" );
	itemNames << tr( "Average Speed:" );
	itemNames << tr( "Remaining Distance:" );
	itemNames << tr( "Track Length:" );
	itemNames << tr( "Track Time:" );
	itemNames << tr( "Max. Elevation:" );
	itemNames << tr( "Min. Elevation:" );
	for( int i = 0; i < itemNames.size(); i++){
		m_listItems.append( new QTreeWidgetItem(0) );
	}
	for( int i = 0; i < m_listItems.size(); i++){
		m_listItems[i]->setText( 0, itemNames[i] );
		m_listItems[i]->setText( 1, "-" );
	}
	m_ui->treeWidget->setColumnCount( 2 );
	m_ui->treeWidget->addTopLevelItems( m_listItems );
	m_ui->treeWidget->resizeColumnToContents( 0 );
	m_ui->treeWidget->setHeaderHidden ( true );
qDebug() << "Konstruktor end";
	m_lastUpdateTime = QDateTime::currentDateTime().addSecs( - DIALOGUPDATEINTERVAL );
	connect( m_ui->cancel, SIGNAL(clicked()), this, SIGNAL(cancelled()) );
	updateInformation();
}

TripinfoDialog::~TripinfoDialog()
{
	delete m_ui;
}

void TripinfoDialog::updateInformation()
{
	// Limit the amount of recalculations
	if( m_lastUpdateTime.secsTo( QDateTime::currentDateTime() ) < DIALOGUPDATEINTERVAL ){
		return;
	}
	m_lastUpdateTime = QDateTime::currentDateTime();

	double theValue = 0.0;
	QStringList theValues;

	theValue = RoutingLogic::instance()->groundSpeed();
	theValues.append( QString::number( theValue /1000, 'f', 1 ).append( "km/h" ) );

	theValue = Logger::instance()->maxSpeed();
	theValues.append( QString::number( theValue /1000, 'f', 1 ).append( "km/h" ) );

	theValue = Logger::instance()->averageSpeed();
	theValues.append( QString::number( theValue /1000, 'f', 1 ).append( "km/h" ) );

	theValue = RoutingLogic::instance()->routeDistance();
	theValues.append( QString::number( theValue /1000, 'f', 1 ).append( "km" ) );

	theValue = Logger::instance()->trackDistance();
	theValues.append( QString::number( theValue /1000, 'f', 1 ).append( "km" ) );

	theValue = Logger::instance()->trackDuration();
	theValues.append( QString::number( theValue /1000, 'f', 0 ).append( "min" ) );

	theValue = Logger::instance()->trackMaxElevation();
	theValues.append( QString::number( theValue /1000, 'f', 0 ).append( "m" ) );

	theValue = Logger::instance()->trackMinElevation();
	theValues.append( QString::number( theValue /1000, 'f', 0 ).append( "m" ) );


qDebug() << "Sizes:" << m_listItems.size() << theValues.size();
	if( m_listItems.size() == theValues.size() ){
		for( int i = 0; i < m_listItems.size(); i++ ){
			m_listItems[i]->setText( 1, theValues[i] );
		}
	}


/*
	theValue = RoutingLogic::instance()->routeDistance();
	if( 0 < m_listItems.size() )
		m_listItems[0]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "km" ) );

	theValue = RoutingLogic::instance()->groundSpeed();
	if( 1 < m_listItems.size() )
		m_listItems[1]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "km/h" ) );

	theValue = Logger::instance()->averageSpeed();
	if( 2 < m_listItems.size() )
		m_listItems[2]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "km/h" ) );

	theValue = Logger::instance()->trackDistance();
	if( 3 < m_listItems.size() )
		m_listItems[3]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "km" ) );

	theValue = Logger::instance()->trackDuration();
	if( 4 < m_listItems.size() )
		m_listItems[4]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "min" ) );

	theValue = Logger::instance()->maxSpeed();
	if( 5 < m_listItems.size() )
		m_listItems[5]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "km/h" ) );

	theValue = Logger::instance()->trackMaxElevation();
	if( 6 < m_listItems.size() )
		m_listItems[6]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "m" ) );

	theValue = Logger::instance()->trackMinElevation();
	if( 7 < m_listItems.size() )
		m_listItems[7]->setText( 1, QString::number( theValue /1000, 'f', 1 ).append( "m" ) );
*/

	m_ui->treeWidget->resizeColumnToContents( 1 );
	m_ui->displayTrackProfile->setMinimumSize( 300, 150 );

/*
	double routeDistance = 0;
	routeDistance = RoutingLogic::instance()->routeDistance();
	m_ui->displayRemainingDistance->setText( QString::number( routeDistance /1000, 'f', 1 ).append( "km" ) );

	double averageSpeed = 0.0;
	averageSpeed = Logger::instance()->averageSpeed();
	m_ui->displayAverageSpeed->setText( QString::number( averageSpeed, 'f', 1 ).append( tr( "km/h" ) ) );

	double trackDistance = 0;
	trackDistance = Logger::instance()->trackDistance();
	m_ui->displayTraveledDistance->setText( QString::number( trackDistance /1000, 'f', 1 ).append( "km" ) );

	int trackTime = 0;
	trackTime = Logger::instance()->trackDuration();
	int hours = trackTime / 3600;
	int minutes = trackTime % 3600 / 60;
	QString hourstring;
	QString minutestring;
	minutestring = QString::number( minutes );
	if( minutes < 10 ){
		minutestring.prepend( "0" );
	}
	if( hours > 0 ){
		hourstring = QString::number( hours ).append( tr( ":" ) );
		minutestring.append( "h" );
	}
	else{
		minutestring.append( "min" );
	}
	m_ui->displayTraveledTime->setText( hourstring + minutestring );
*/

	double maxSpeed = 0.0;
	double minElevation = 0.0;
	double maxElevation = 0.0;

	maxSpeed = Logger::instance()->maxSpeed();
	minElevation = Logger::instance()->trackMinElevation();
	maxElevation = Logger::instance()->trackMaxElevation();

	// m_ui->displayMaxSpeed->setText( QString::number( maxSpeed, 'f', 1 ).append( tr( "km/h" ) ) );
	// m_ui->displayMinElevation->setText( QString::number( minElevation ).append( tr( "m" ) ) );
	// m_ui->displayMaxElevation->setText( QString::number( maxElevation ).append( tr( "m" ) ) );

	const QVector<double>& trackElevations = Logger::instance()->trackElevations();

	// Drawing the track's height profile
	int marginLeft = 5;
	int marginRight = 5;
	int marginTop = 5;
	int marginBottom = 5;

	QPixmap pixmap( m_ui->displayTrackProfile->width(), m_ui->displayTrackProfile->height());
	pixmap.fill( QColor( 255, 255, 255, 128 ) );
	m_ui->displayTrackProfile->setPixmap( pixmap );

	double scaleX = 1.0;
	scaleX = double( m_ui->displayTrackProfile->pixmap()->width() - marginLeft - marginRight ) / double( trackElevations.size() );

	double metersDelta = maxElevation - minElevation;
	double scaleY = 1.0;
	scaleY = ( m_ui->displayTrackProfile->pixmap()->height() - marginTop - marginBottom) / metersDelta;

	QPolygonF polygon;
	for( int i = 0; i < trackElevations.size(); i++ ){
		polygon << QPointF( (i * scaleX) + marginLeft, (trackElevations.at(i) * scaleY) -marginBottom );
	}

	QPainter painter;
	painter.begin( &pixmap );
	painter.setPen( QColor( 178, 034, 034 ) );
	painter.drawPolyline( polygon );
	painter.end();

	// Ugly hack to flip the image
	QImage mirroredImage = pixmap.toImage().mirrored( false, true );
	pixmap = QPixmap::fromImage( mirroredImage );
	m_ui->displayTrackProfile->setPixmap( pixmap );
}

