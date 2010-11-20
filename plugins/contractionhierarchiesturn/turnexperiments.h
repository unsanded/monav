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

#ifndef TURNEXPERIMENTS_H_INCLUDED
#define TURNEXPERIMENTS_H_INCLUDED
#include <vector>
#include <omp.h>
#include <limits.h>
#include <limits>
#include <sstream>
#include "utils/qthelpers.h"
#include "dynamicturngraph.h"
#include "../contractionhierarchies/binaryheap.h"
#include "utils/config.h"
#include "turnquery.h"
#include "turncontractor.h"

class TurnExperiments {
public:
	typedef TurnContractor::DynamicGraph Graph;
	typedef Graph::EdgeData EdgeData;
	typedef Graph::EdgeIterator EdgeIterator;
	typedef Graph::NodeIterator NodeIterator;

	struct Demand {
		TurnQueryEdge source;
		TurnQueryEdge target;
		int distance;
		std::string DebugString() const {
			std::stringstream ss;
			ss << source.DebugString() << " --- " << distance << " --- " << target.DebugString();
			return ss.str();
		}
	};

	TurnExperiments() {}

protected:
	double _Timestamp() {
		static Timer timer;
		return ( double ) timer.elapsed() / 1000;
	}
public:

	bool plainDijkstra(QString dir, const TurnContractor& contractor) {
		const Graph& graph = contractor.GetGraph();
		unsigned numNodes = graph.GetNumberOfNodes();

		unsigned numQueries = 1000;
		srand48(176);
		std::vector< Demand > demands;
		demands.reserve( numQueries );
//		{
//			Demand demand;
//			demand.source = 405446;
//			demand.source2 = 405442;
//			demand.target2 = 405440;
//			demand.target = 405441;
//			demands.push_back(demand);
//		}
		std::vector< std::pair< unsigned, unsigned > > temp;
		for ( unsigned i = 0; i < numQueries; ++i ) {
			Demand demand;
			while (temp.empty()) {
				demand.source.source = lrand48() % numNodes;
				for ( unsigned e = graph.BeginEdges( demand.source.source ); e < graph.EndEdges( demand.source.source ); ++ e) {
					const EdgeData& edgeData = graph.GetEdgeData( e );
					if ( !edgeData.shortcut && edgeData.forward ) {
						temp.push_back( std::make_pair( graph.GetTarget( e ), edgeData.id ) );
					}
				}
			}
			{
				unsigned i = lrand48() % temp.size();
				demand.source.target = temp[i].first;
				demand.source.edgeID = temp[i].second;
			}
			temp.clear();

			while (temp.empty()) {
				demand.target.target = lrand48() % numNodes;
				for ( unsigned e = graph.BeginEdges( demand.target.target ); e < graph.EndEdges( demand.target.target ); ++ e) {
					const EdgeData& edgeData = graph.GetEdgeData( e );
					if ( !edgeData.shortcut && edgeData.backward ) {
						temp.push_back( std::make_pair( graph.GetTarget( e ), edgeData.id ) );
					}
				}
			}
			{
				unsigned i = lrand48() % temp.size();
				demand.target.source = temp[i].first;
				demand.target.edgeID = temp[i].second;
			}
			temp.clear();

			demand.distance = -1;
			demands.push_back( demand );
		}


//		omp_set_num_threads(1);
		int maxThreads = omp_get_max_threads();
		qDebug( "using %d threads", maxThreads );
		qDebug() << demands.size() << "queries";

		#pragma omp parallel
		{
			TurnQuery<Graph, false /*stall on demand*/> query(graph);
			#pragma omp for schedule ( dynamic )
			for ( int i = 0; i < (int)demands.size(); ++i )
			{
				Demand& demand = demands[i];
				demand.distance = query.UnidirSearch( demand.source, demand.target );
				query.Clear();
				qDebug() << i << demand.DebugString().c_str();
			}
		}

		QString filename = fileInDirectory( dir, "CHT Demands");
		FileStream data( filename );

		if ( !data.open( QIODevice::WriteOnly ) )
			return false;

		data << unsigned(demands.size());
		for ( unsigned i = 0; i < demands.size(); ++i )
		{
			const Demand& demand = demands[i];
			data << demand.source.source << demand.source.target << demand.source.edgeID;
			data << demand.target.source << demand.target.target << demand.target.edgeID;
			data << demand.distance;
		}
		return true;
	}

	bool chtQuery(IImporter*
			#ifndef NDEBUG
			importer
			#endif
			, QString dir) {
		const Graph* graph = Graph::ReadFromFile( fileInDirectory( dir, "CHT Dynamic Graph") );

//		if ( false ) {
//			std::vector< IImporter::RoutingNode > inputNodes;
//			std::vector< IImporter::RoutingEdge > inputEdges;
//			std::vector< char > inDegree, outDegree;
//			std::vector< double > penalties;
//
//			if ( !importer->GetRoutingNodes( &inputNodes ) )
//				return false;
//			if ( !importer->GetRoutingEdges( &inputEdges ) )
//				return false;
//			if ( !importer->GetRoutingPenalties( &inDegree, &outDegree, &penalties ) )
//				return false;
//
//			unsigned numNodes = inputNodes.size();
//
//			TurnContractor* contractor = new TurnContractor( numNodes, inputEdges, inDegree, outDegree, penalties );
//			std::vector< IImporter::RoutingEdge >().swap( inputEdges );
//			std::vector< char >().swap( inDegree );
//			std::vector< char >().swap( outDegree );
//			std::vector< double >().swap( penalties );
//			const Graph& plainGraph = contractor->GetGraph();
//			TurnQuery<Graph, false> plainQuery(plainGraph);
//
//			qDebug() << "check shortcuts";
//			unsigned numUnnecessary = 0;
//			for (NodeIterator node = 0; node < graph->GetNumberOfNodes(); ++node)
//			{
//				for (EdgeIterator edge = graph->BeginEdges( node ); edge != graph->EndEdges( node ); ++edge )
//				{
//					bool isUnnecessary = true;
//					const EdgeData& edgeData = graph->GetEdgeData( edge );
//					if ( edgeData.forward )
//					{
//						isUnnecessary = false;
//						NodeIterator source = node;
//						NodeIterator target = graph->GetTarget( edge );
//						EdgeIterator eSource = plainGraph.FindOriginalEdge( source, graph->GetOriginalEdgeSource( edge ), true );
//						assert( eSource != plainGraph.EndEdges( source ) );
//						NodeIterator source2 = plainGraph.GetTarget( eSource );
//						EdgeIterator eTarget = plainGraph.FindOriginalEdge( target, graph->GetOriginalEdgeTarget( edge ), false );
//						assert( eTarget != plainGraph.EndEdges( target ) );
//						NodeIterator target2 = plainGraph.GetTarget( eTarget );
//
//						int distance = plainQuery.UnidirSearch( source, source2, target, target2 );
//						if ( distance > (int)edgeData.distance )
//						{
//							qDebug() << plainGraph.DebugStringEdge( eSource ).c_str();
//							qDebug() << plainGraph.DebugStringEdge( eTarget ).c_str();
//							qDebug() << node << graph->DebugStringEdge( edge ).c_str() << "\t" << distance;
//							exit(1);
//						} else if (distance < (int)edgeData.distance) {
//							isUnnecessary = true;
//						}
//
//						plainQuery.Clear();
//					}
//
//					if ( edgeData.backward )
//					{
//						isUnnecessary = false;
//						NodeIterator source = graph->GetTarget( edge );
//						NodeIterator target = node;
//						EdgeIterator eSource = plainGraph.FindOriginalEdge( source, graph->GetOriginalEdgeTarget( edge ), true );
//						assert( eSource != plainGraph.EndEdges( source ) );
//						NodeIterator source2 = plainGraph.GetTarget( eSource );
//						EdgeIterator eTarget = plainGraph.FindOriginalEdge( target, graph->GetOriginalEdgeSource( edge ), false );
//						assert( eTarget != plainGraph.EndEdges( target ) );
//						NodeIterator target2 = plainGraph.GetTarget( eTarget );
//
//						int distance = plainQuery.UnidirSearch( source, source2, target, target2 );
//						if ( distance > (int)edgeData.distance )
//						{
//							qDebug() << plainGraph.DebugStringEdge( eSource ).c_str();
//							qDebug() << plainGraph.DebugStringEdge( eTarget ).c_str();
//							qDebug() << node << graph->DebugStringEdge( edge ).c_str() << "\t" << distance;
//							exit(1);
//						} else if (distance < (int)edgeData.distance) {
//							isUnnecessary = true;
//						}
//						plainQuery.Clear();
//					}
//					numUnnecessary += isUnnecessary;
//				}
//			}
//			qDebug() << "all shortcuts done," << numUnnecessary << " unnecessary";
//
//		}

		unsigned numQueries;
		std::vector< Demand > demands;
		{
			QString filename = fileInDirectory( dir, "CHT Demands");
			FileStream data( filename );

			if ( !data.open( QIODevice::ReadOnly ) )
				return false;

			data >> numQueries;

			if ( data.status() == QDataStream::ReadPastEnd ) {
				qCritical() << "TurnExperiments::cthQuery(): Corrupted Demands Data";
				return false;
			}

			qDebug() << numQueries << "queries";
			demands.reserve( numQueries );

			for ( unsigned i = 0; i < numQueries; ++i )
			{
				Demand demand;
				data >> demand.source.source >> demand.source.target >> demand.source.edgeID;
				data >> demand.target.source >> demand.target.target >> demand.target.edgeID;
				data >> demand.distance;

				if ( data.status() == QDataStream::ReadPastEnd ) {
					qCritical() << "TurnExperiments::cthQuery(): Corrupted Demands Data";
					return false;
				}

				demands.push_back( demand );


			}
		}

        #ifndef NDEBUG
		if ( false ) {
			demands.clear();
			Demand demand;
			demand.source = TurnQueryEdge( 443808, 443809, 503662 );
			demand.target = TurnQueryEdge( 483398, 240897, 126592 );
            demand.distance = 121476;

            demand.source = TurnQueryEdge( 443808, 443809, 503662 );
            demand.target = TurnQueryEdge( 410, 356230, 606291 );
            demand.distance = 27749;
			demands.push_back(demand);
		}
        #endif

		typedef TurnQuery<Graph, true /*stall on demand*/> Query;
		Query query(*graph);
		double duration = _Timestamp();
		for ( int i = 0; i < (int)demands.size(); ++i )
		{
			const Demand& demand = demands[i];
			int distance = query.BidirSearch( demand.source, demand.target );
			if (distance != demand.distance) {
				qDebug() << i << demand.source.DebugString().c_str() << "..." << demand.target.DebugString().c_str()
						<< ": CHT" << distance << "plain" << demand.distance;

#ifndef NDEBUG
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
				const Graph& plainGraph = contractor->GetGraph();
				qDebug() << plainGraph.DebugStringEdgesOf( demand.target.target ).c_str();
				qDebug() << plainGraph.DebugStringPenaltyTable( demand.target.target ).c_str();
				TurnQuery<Graph, false> plainQuery(plainGraph);
				int plainDistanceUnidir = plainQuery.UnidirSearch( demand.source, demand.target );

		        qDebug() << "up";
		        {
		            unsigned origUp = plainQuery.m_middle.in;
		            do {
		                std::stringstream ss;
		                assert( plainQuery.m_heapForward.WasInserted( origUp ) );
		                const TurnQuery< Graph, false >::HeapData& heapData = plainQuery.m_heapForward.GetData( origUp );
		                EdgeIterator edge = heapData.parentEdge;

		                ss << "\t" << plainQuery.m_heapForward.GetKey( origUp ) << "\t" << plainGraph.DebugStringEdge(edge);
		                if ( query.m_heapForward.WasInserted( origUp ) ) {
		                    ss << "\t||\t" << query.m_heapForward.GetKey( origUp ) << "\t" << query.m_heapForward.GetData( origUp ).DebugString().c_str();
		                }
		                qDebug() << ss.str().c_str();

		                origUp = heapData.parentOrig;
		            } while ( origUp != (unsigned)-1 );
		        }
		        qDebug();


				plainQuery.Clear();
				int plainDistanceBidir = plainQuery.BidirSearch( demand.source, demand.target );
				qDebug() << plainQuery.DebugStringPath().c_str();
				plainQuery.Clear();
				qDebug() << "unidir" << plainDistanceUnidir << "bidir" << plainDistanceBidir;


				qDebug() << "up";
				{
					unsigned origUp = query.m_middle.in;
					do {
						std::stringstream ss;
						assert( query.m_heapForward.WasInserted( origUp ) );
						const Query::HeapData& heapData = query.m_heapForward.GetData( origUp );
						EdgeIterator edge = heapData.parentEdge;
						EdgeIterator oEdge = plainGraph.FindOriginalEdge( graph->GetTarget( edge ), graph->GetOriginalEdgeTarget( edge ), false );

						ss << "\t" << query.m_heapForward.GetKey( origUp ) << "\t" << graph->DebugStringEdge(edge);
						ss << "\t||\t" << plainGraph.DebugStringEdge( oEdge );
						if ( heapData.parentOrig != (unsigned)-1 ) {
							int testDistance = plainQuery.UnidirSearch( demand.source, TurnQueryEdge( graph->GetTarget(edge), plainGraph.GetTarget( oEdge ), plainGraph.GetEdgeData( oEdge ).id ) );
							plainQuery.Clear();
							ss << "\t||\t" << testDistance;
						}
						qDebug() << ss.str().c_str();

						origUp = heapData.parentOrig;
					} while ( origUp != (unsigned)-1 );
				}
				qDebug();

				qDebug() << "down";
				{
					unsigned origDown = query.m_middle.out;
					do {
						std::stringstream ss;
						assert( query.m_heapBackward.WasInserted( origDown ) );
						const Query::HeapData& heapData = query.m_heapBackward.GetData( origDown );
						EdgeIterator edge = heapData.parentEdge;
						EdgeIterator oEdge = plainGraph.FindOriginalEdge( graph->GetTarget( edge ), graph->GetOriginalEdgeTarget( edge ), true );

						ss << "\t" << query.m_heapBackward.GetKey( origDown ) << "\t" << graph->DebugStringEdge(edge);
						ss << "\t||\t" << plainGraph.DebugStringEdge( oEdge );
						if ( heapData.parentOrig != (unsigned)-1 ) {
							int testDistance = plainQuery.UnidirSearch( TurnQueryEdge( graph->GetTarget(edge), plainGraph.GetTarget( oEdge ), plainGraph.GetEdgeData( oEdge ).id ), demand.target );
							plainQuery.Clear();
							ss << "\t||\t" << testDistance;
						}
						qDebug() << ss.str().c_str();

						origDown = heapData.parentOrig;
					} while ( origDown != (unsigned)-1 );
				}
				qDebug();


				delete contractor;
#endif
				exit(1);
			}
			query.Clear();
		}
		duration = _Timestamp() - duration;
		qDebug() << "CHT queries done in" << duration << "seconds.";

		delete graph;
		return true;
	}

};

#endif // TURNEXPERIMENTS_H_INCLUDED
