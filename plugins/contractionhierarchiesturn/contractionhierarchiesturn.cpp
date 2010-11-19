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

#include <qcoreapplication.h>
#include "contractionhierarchiesturn.h"
#include "compressedturngraphbuilder.h"
#include "turncontractor.h"
#include "contractionturncleanup.h"
#include "utils/qthelpers.h"
#include "turnexperiments.h"

ContractionHierarchiesTurn::ContractionHierarchiesTurn()
{
	m_settingsDialog = NULL;
}

ContractionHierarchiesTurn::~ContractionHierarchiesTurn()
{
	if ( m_settingsDialog != NULL )
		delete m_settingsDialog;
}

QString ContractionHierarchiesTurn::GetName()
{
	return "Contraction Hierarchies Turn";
}

bool ContractionHierarchiesTurn:: LoadSettings( QSettings* settings )
{
	if ( m_settingsDialog == NULL )
		m_settingsDialog = new CHTSettingsDialog();
	return m_settingsDialog->loadSettings( settings );
}

bool ContractionHierarchiesTurn::SaveSettings( QSettings* settings )
{
	if ( m_settingsDialog == NULL )
		m_settingsDialog = new CHTSettingsDialog();
	return m_settingsDialog->saveSettings( settings );
}

int ContractionHierarchiesTurn::GetFileFormatVersion()
{
	return 1;
}

ContractionHierarchiesTurn::Type ContractionHierarchiesTurn::GetType()
{
	return Router;
}

QWidget* ContractionHierarchiesTurn::GetSettings()
{
	if ( m_settingsDialog == NULL )
		m_settingsDialog = new CHTSettingsDialog();
	return m_settingsDialog;
}

bool ContractionHierarchiesTurn::Preprocess( IImporter* importer, QString dir )
{
	if ( m_settingsDialog == NULL )
		m_settingsDialog = new CHTSettingsDialog();
	m_settingsDialog->getSettings( &m_settings );

	const QStringList& args = QCoreApplication::arguments();

	QString filename = fileInDirectory( dir, "Contraction Hierarchies Turn" );

	bool doContract = args.indexOf("--cht-contract") != -1;
	bool doPlainQuery = args.indexOf("--cht-plain-query") != -1;
	typedef TurnContractor::DynamicGraph Graph;

	if ( doContract || doPlainQuery ) {

		std::vector< IImporter::RoutingNode > inputNodes;
		std::vector< IImporter::RoutingEdge > inputEdges;
		std::vector< char > inDegree, outDegree;
		std::vector< double > penalties;

		if ( !importer->GetRoutingNodes( &inputNodes ) )
			return false;
		if ( !importer->GetRoutingEdges( &inputEdges ) )
			return false;
		if ( !importer->GetRoutingPenalties( &inDegree, &outDegree, &penalties ) )
			return false;

		unsigned numNodes = inputNodes.size();

		TurnContractor* contractor = new TurnContractor( numNodes, inputEdges, inDegree, outDegree, penalties );
		std::vector< IImporter::RoutingEdge >().swap( inputEdges );
		std::vector< char >().swap( inDegree );
		std::vector< char >().swap( outDegree );
		std::vector< double >().swap( penalties );

		if (doPlainQuery) {
			TurnExperiments experiments;
			experiments.plainDijkstra( dir, *contractor );
		}


		if ( doContract ) {
			contractor->Run();
			qDebug() << "Writing graph to file ...";
			contractor->GetGraph().WriteToFile( fileInDirectory( dir, "CHT Dynamic Graph") );
		}
		delete contractor;
	}

	if (args.indexOf("--cht-query") != -1) {
		TurnExperiments experiments;
		experiments.chtQuery( importer, dir );
	}



    exit(1);


//	std::vector< TurnContractor::Witness > witnessList;
//	contractor->GetWitnessList( witnessList );
//
//	std::vector< ContractionTurnCleanup::Edge > contractedEdges;
//	std::vector< ContractionTurnCleanup::Edge > contractedLoops;
//	contractor->GetEdges( &contractedEdges );
//	contractor->GetLoops( &contractedLoops );
//	delete contractor;
//
//	ContractionTurnCleanup* cleanup = new ContractionTurnCleanup( inputNodes.size(), contractedEdges, contractedLoops, witnessList );
//	std::vector< ContractionTurnCleanup::Edge >().swap( contractedEdges );
//	std::vector< ContractionTurnCleanup::Edge >().swap( contractedLoops );
//	std::vector< TurnContractor::Witness >().swap( witnessList );
//	cleanup->Run();
//
//	qDebug() << "Contraction finished.";
//

//	std::vector< CompressedTurnGraph::Edge > edges;
//	std::vector< NodeID > map;
//	cleanup->GetData( &edges, &map );
//	delete cleanup;
//
//	{
//		std::vector< unsigned > edgeIDs( numEdges );
//		for ( unsigned edge = 0; edge < edges.size(); edge++ ) {
//			if ( edges[edge].data.shortcut )
//				continue;
//			unsigned id = 0;
//			unsigned otherEdge = edge;
//			while ( true ) {
//				if ( otherEdge == 0 )
//					break;
//				otherEdge--;
//				if ( edges[otherEdge].source != edges[edge].source )
//					break;
//				if ( edges[otherEdge].target != edges[edge].target )
//					continue;
//				if ( edges[otherEdge].data.shortcut )
//					continue;
//				id++;
//			}
//			edgeIDs[edges[edge].data.id] = id;
//		}
//		importer->SetEdgeIDMap( edgeIDs );
//	}
//
//	std::vector< IRouter::Node > nodes( numNodes );
//	for ( std::vector< IImporter::RoutingNode >::const_iterator i = inputNodes.begin(), iend = inputNodes.end(); i != iend; i++ )
//		nodes[map[i - inputNodes.begin()]].coordinate = i->coordinate;
//	std::vector< IImporter::RoutingNode >().swap( inputNodes );
//
//	std::vector< IRouter::Node > pathNodes;
//	{
//		std::vector< IImporter::RoutingNode > edgePaths;
//		if ( !importer->GetRoutingEdgePaths( &edgePaths ) )
//			return false;
//		pathNodes.resize( edgePaths.size() );
//		for ( unsigned i = 0; i < edgePaths.size(); i++ )
//			pathNodes[i].coordinate = edgePaths[i].coordinate;
//	}
//
//	if ( !importer->GetRoutingEdges( &inputEdges ) )
//		return false;
//
//	{
//		std::vector< QString > inputNames;
//		if ( !importer->GetRoutingWayNames( &inputNames ) )
//			return false;
//
//		QFile nameFile( filename + "_names" );
//		if ( !openQFile( &nameFile, QIODevice::WriteOnly ) )
//			return false;
//
//		std::vector< unsigned > nameMap( inputNames.size() );
//		for ( unsigned name = 0; name < inputNames.size(); name++ ) {
//			nameMap[name] = nameFile.pos();
//			QByteArray buffer = inputNames[name].toUtf8();
//			buffer.push_back( ( char ) 0 );
//			nameFile.write( buffer );
//		}
//
//		nameFile.close();
//		nameFile.open( QIODevice::ReadOnly );
//		const char* test = ( const char* ) nameFile.map( 0, nameFile.size() );
//		for ( unsigned name = 0; name < inputNames.size(); name++ ) {
//			QString testName = QString::fromUtf8( test + nameMap[name] );
//			assert( testName == inputNames[name] );
//		}
//
//		for ( unsigned edge = 0; edge < numEdges; edge++ )
//			inputEdges[edge].nameID = nameMap[inputEdges[edge].nameID];
//	}
//
//	{
//		std::vector< QString > inputTypes;
//		if ( !importer->GetRoutingWayTypes( &inputTypes ) )
//			return false;
//
//		QFile typeFile( filename + "_types" );
//		if ( !openQFile( &typeFile, QIODevice::WriteOnly ) )
//			return false;
//
//		QStringList typeList;
//		for ( unsigned type = 0; type < inputTypes.size(); type++ )
//			typeList.push_back( inputTypes[type] );
//
//		typeFile.write( typeList.join( ";" ).toUtf8() );
//	}
//
//	for ( std::vector< IImporter::RoutingEdge >::iterator i = inputEdges.begin(), iend = inputEdges.end(); i != iend; i++ ) {
//		i->source = map[i->source];
//		i->target = map[i->target];
//	}
//
//	CompressedTurnGraphBuilder* builder = new CompressedTurnGraphBuilder( 1u << m_settings.blockSize, nodes, edges, inputEdges, pathNodes );
//	if ( !builder->run( filename, &map ) )
//		return false;
//	delete builder;
//
//	importer->SetIDMap( map );

	return true;
}

Q_EXPORT_PLUGIN2( contractionhierarchiesturn, ContractionHierarchiesTurn )
