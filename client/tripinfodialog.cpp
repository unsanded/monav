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
	QStringList itemNames;
	itemNames << tr( "Ground Speed:" );
	itemNames << tr( "Max. Speed:" );
	itemNames << tr( "Average Speed:" );
	itemNames << tr( "Remaining Distance:" );
	itemNames << tr( "Track Length:" );
	itemNames << tr( "Departure:" );
	itemNames << tr( "Track Time:" );
	itemNames << tr( "Remaining Time:" );
	itemNames << tr( "Arrival:" );
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
	double averageSpeed = 0.0;
	double routeDistance = 0.0;
	double remainingTime = 0.0;
	QStringList theValues;

	theValue = RoutingLogic::instance()->groundSpeed();
	// Snails have bad luck:
	if( theValue < 1 )
		theValues.append( "-" );
	else
		theValues.append( QString::number( theValue, 'f', 1 ).append( "km/h" ) );

	theValue = Logger::instance()->maxSpeed();
	if( theValue < 1 )
		theValues.append( "-" );
	else
		theValues.append( QString::number( theValue, 'f', 1 ).append( "km/h" ) );

	averageSpeed = Logger::instance()->averageSpeed();
	if( averageSpeed < 1 )
		theValues.append( "-" );
	else
		theValues.append( QString::number( averageSpeed, 'f', 1 ).append( "km/h" ) );

	routeDistance = RoutingLogic::instance()->routeDistance();
	if( routeDistance < 1 )
		theValues.append( "-" );
	else
		theValues.append( QString::number( routeDistance/1000, 'f', 1 ).append( "km" ) );

	theValue = Logger::instance()->trackDistance();
	if( theValue < 1 )
		theValues.append( "-" );
	else
		theValues.append( QString::number( theValue/1000, 'f', 1 ).append( "km" ) );

	QDateTime startTime = Logger::instance()->trackStartTime();
	if( !startTime.isValid() )
		theValues.append( "-" );
	else
		theValues.append( startTime.toString( "yyyyMMdd hh:mm" ) );

	theValue = Logger::instance()->trackDuration();
	if( theValue < 1 )
		theValues.append( "-" );
	else if( theValue < 600 ){
		theValues.append( QString::number( theValue/60, 'f', 1 ).append( "min" ) );
	}
	else if( theValue < 3600 ){
		theValues.append( QString::number( theValue/60, 'f', 0 ).append( "min" ) );
	}
	else{
		int hours = theValue / 3600;
		int minutes = (int)theValue % 3600 / 60;
		theValues.append( QString::number( hours ).append( ":" ) );
		theValues.last().append( QString::number( minutes ).append( "h" ) );
	}

	// Avoid an division by zero
	if( averageSpeed == 0 ){
		remainingTime = 0;
	}
	else{
	remainingTime = ((routeDistance*60)/(averageSpeed*1000));
	}

	if( remainingTime < 1 ){
		theValues.append( "-" );
	}
	else if( remainingTime < 60 ){
		theValues.append( QString::number( remainingTime, 'f', 0 ).append( "min" ) );
	}
	else{
		int hours = remainingTime / 60;
		int minutes = (int)remainingTime % 60;
		theValues.append( QString::number( hours ).append( ":" ) );
		theValues.last().append( QString::number( minutes ).append( "h" ) );
	}


	if( remainingTime < 1 ){
		theValues.append( "-" );
	}
	else{
		QDateTime arrivalTime = QDateTime::currentDateTime().addSecs( (int)remainingTime*60 );
		theValues.append( arrivalTime.toString( "yyyyMMdd hh:mm" ) );
	}

	theValue = Logger::instance()->trackMaxElevation();
	theValues.append( QString::number( theValue, 'f', 0 ).append( "m" ) );

	theValue = Logger::instance()->trackMinElevation();
	theValues.append( QString::number( theValue, 'f', 0 ).append( "m" ) );

	if( m_listItems.size() == theValues.size() ){
		for( int i = 0; i < m_listItems.size(); i++ ){
			m_listItems[i]->setText( 1, theValues[i] );
		}
	}

	m_ui->treeWidget->resizeColumnToContents( 1 );
	m_ui->displayTrackProfile->setMinimumSize( 300, 150 );

	double maxSpeed = 0.0;
	double minElevation = 0.0;
	double maxElevation = 0.0;

	maxSpeed = Logger::instance()->maxSpeed();
	minElevation = Logger::instance()->trackMinElevation();
	maxElevation = Logger::instance()->trackMaxElevation();

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

