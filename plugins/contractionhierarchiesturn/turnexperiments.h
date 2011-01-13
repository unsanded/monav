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
#include "../contractionhierarchies/contractor.h"
#include "../contractionhierarchies/query.h"

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
	 std::string DebugStringSearchGraphMemory( const Graph& graph ) const {
		double numNodes = graph.GetNumberOfNodes();
		unsigned nodes = graph.GetNumberOfNodes() * 12;  // firstEdge:4, firstPenalty:4, firstOriginalEdge: 4
		unsigned edges =  graph.GetNumberOfEdges() * 12; // target:4, originalEdgeSource:2, originalEdgeTarget:2, distance:4
		unsigned penalties = graph.GetNumberOfStoredPenalties() *  1;  // penalty:1;
		std::stringstream ss;
		ss << "nodes: " << nodes/1048576. << " MiB, edges: " << edges/1048576.<< " MiB, penalties: " << penalties/1048576. << " MiB (total " << (nodes+edges+penalties)/numNodes << " Byte/node)";
		return ss.str();
	 }
	 std::string DebugStringSearchGraphMemory( const Contractor::_DynamicGraph& graph ) const {
		double numNodes = graph.GetNumberOfNodes();
		unsigned nodes = graph.GetNumberOfNodes() * 4;  // firstEdge:4
		unsigned edges =  graph.GetNumberOfEdges() * 8; // target:4, distance:4
		std::stringstream ss;
		ss << "nodes: " << nodes/1048576. << " MiB, edges: " << edges/1048576.<< " MiB (total " << (nodes+edges)/numNodes << " Byte/node)";
		return ss.str();
	 }

	 std::vector< Demand > _demands;

public:

	bool PlainUnidirDijkstra(QString dir, const TurnContractor& contractor) {
		const Graph& graph = contractor.GetGraph();
		unsigned numNodes = graph.GetNumberOfNodes();
		qDebug() << DebugStringSearchGraphMemory( graph ).c_str();

		unsigned numQueries = 1000;
		srand48(176);
		_demands.clear();
		_demands.reserve( numQueries );
//		{
//			Demand demand;
//			demand.source = 405446;
//			demand.source2 = 405442;
//			demand.target2 = 405440;
//			demand.target = 405441;
//			_demands.push_back(demand);
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
			_demands.push_back( demand );
		}


//		omp_set_num_threads(1);
		int maxThreads = omp_get_max_threads();
		qDebug( "using %d threads", maxThreads );
		qDebug() << _demands.size() << "queries";

		double duration = _Timestamp();
		long long progress = 0;
		long long lastProgress = 0;
		Timer time;
		#pragma omp parallel
		{
			TurnQuery<Graph, false /*stall on demand*/> query(graph);
			#pragma omp for schedule ( dynamic )
			for ( int i = 0; i < (int)_demands.size(); ++i )
			{
				Demand& demand = _demands[i];
				demand.distance = query.UnidirSearch( demand.source, demand.target );
				query.Clear();
				#ifndef NDEBUG
				qDebug() << i << demand.DebugString().c_str();
				#endif

	#pragma omp critical
				{
					progress++;
					if ( time.elapsed() >= 10000 ) {
						qDebug() << progress * 100.0 / _demands.size() << "%" << "ETA:" << time.restart() / 1000.0 / ( progress - lastProgress ) * ( _demands.size() - progress ) << "s";
						lastProgress = progress;
					}
				}
			}
			if (maxThreads == 1) {
				duration = _Timestamp() - duration;
				qDebug() << "T queries done in" << duration << "seconds.";
				qDebug() << "query time:" << duration * 1000 / _demands.size() << " ms";
				#ifdef QUERY_COUNT
				qDebug() << query.GetCounter().DebugString().c_str();
				#endif
			}
		}

		QString filename = fileInDirectory( dir, "CHT Demands");
		FileStream data( filename );

		if ( !data.open( QIODevice::WriteOnly ) )
			return false;

		data << unsigned(_demands.size());
		for ( unsigned i = 0; i < _demands.size(); ++i )
		{
			const Demand& demand = _demands[i];
			data << demand.source.source << demand.source.target << demand.source.edgeID;
			data << demand.target.source << demand.target.target << demand.target.edgeID;
			data << demand.distance;
		}
		return true;
	}

	bool ReadDemands(QString dir) {
		qDebug() << "ReadDemands";
		QString filename = fileInDirectory( dir, "CHT Demands");
		FileStream data( filename );

		if ( !data.open( QIODevice::ReadOnly ) ) {
			qCritical() << "TurnExperiments::ReadDemands(): File not found";
			return false;
		}


		unsigned numQueries;
		data >> numQueries;

		if ( data.status() == QDataStream::ReadPastEnd ) {
			qCritical() << "TurnExperiments::ReadDemands(): Corrupted Demands Data";
			return false;
		}

		qDebug() << numQueries << "demands";
		_demands.clear();
		_demands.reserve( numQueries );

		for ( unsigned i = 0; i < numQueries; ++i )
		{
			Demand demand;
			data >> demand.source.source >> demand.source.target >> demand.source.edgeID;
			data >> demand.target.source >> demand.target.target >> demand.target.edgeID;
			data >> demand.distance;

			if ( data.status() == QDataStream::ReadPastEnd ) {
				qCritical() << "TurnExperiments::ReadDemands(): Corrupted Demands Data";
				return false;
			}

			_demands.push_back( demand );
		}
		return true;
	}

	bool chtQuery(IImporter*
			#ifndef NDEBUG
			importer
			#endif
			, QString dir) {
		const Graph* graph = Graph::ReadFromFile( fileInDirectory( dir, "CHT Dynamic Graph") );
		qDebug() << DebugStringSearchGraphMemory( *graph ).c_str();


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


		ReadDemands(dir);

		  #ifndef NDEBUG
		if ( false ) {
			_demands.clear();
			Demand demand;
				demand.source = TurnQueryEdge( 66635, 66633, 75484);
				demand.target = TurnQueryEdge( 13327, 13326, 89990 );
				demand.distance = 28625;

				demand.source = TurnQueryEdge( 45002, 11622,  49612 );
				demand.target = TurnQueryEdge( 13327, 13326, 89990 );
				demand.distance = 27990;

				_demands.push_back(demand);
		}
		  #endif

		typedef TurnQuery<Graph, true/*stall on demand*/> Query;
		Query query(*graph);
		double duration = _Timestamp();
		#ifdef QUERY_COUNT
		double maxRelError = 0;
		double maxAbsError = 0;
		double sumError = 0;
		#endif

		for ( int i = 0; i < (int)_demands.size(); ++i )
		{
			const Demand& demand = _demands[i];
			int distance = query.BidirSearch( demand.source, demand.target );
			if (distance != demand.distance ) {
				#ifdef QUERY_COUNT
				if ( demand.distance != 0 ) {
					//qDebug() << distance << demand.DebugString().c_str();
					double relError = fabs( ( double ) distance / demand.distance - 1 );
					if ( relError > maxRelError ) {
						maxRelError = relError;
					}
					sumError += relError;
				}
				double absError = fabs( distance - demand.distance );
				if ( absError > maxAbsError )
					maxAbsError = absError;
				#endif

#ifndef NDEBUG
				qDebug() << i << demand.source.DebugString().c_str() << "..." << demand.target.DebugString().c_str()
						<< ": CHT" << distance << "plain" << demand.distance;

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



				TurnContractor* contractor = new TurnContractor( numNodes, &inputEdges, inDegree, outDegree, penalties );
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
							int testDistance = plainQuery.UnidirSearch( demand.source, TurnQueryEdge( plainGraph.GetTarget( oEdge ), graph->GetTarget(edge), plainGraph.GetEdgeData( oEdge ).id ) );
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
				exit(1);
#endif
			}
			query.Clear();
		}
		duration = _Timestamp() - duration;
		qDebug() << "CHT queries done in" << duration << "seconds.";
		qDebug() << "query time:" << duration * 1000 / _demands.size() << " ms";
		#ifdef QUERY_COUNT
		qDebug() << "max error:" << maxAbsError / 10.0 << "s";
		qDebug() << "max error:" << maxRelError * 100.0 << "%";
		qDebug() << "avg error:" << sumError / _demands.size() * 100.0 << "%";
		qDebug() << query.GetCounter().DebugString().c_str();
		#endif


		delete graph;
		return true;
	}

	struct EdgeMapItem {
		unsigned first;
		unsigned second;
		unsigned distance;
		EdgeMapItem(unsigned d) : first(-1), second(-1), distance(d) {}
		EdgeMapItem(unsigned f, unsigned s, unsigned d) : first(f), second(s), distance(d) {}
	};
	QHash< unsigned, EdgeMapItem > _edgeMap;
	Contractor * _contractor;

	bool CreateEdgeBasedGraph(IImporter* importer, QString dir) {
		std::vector< IImporter::RoutingEdge > inputTurnEdges;
		_edgeMap.clear();
		unsigned numNodes = 0;
		{
			qDebug() << "Read graph with penalties ...";
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

			TurnContractor contractor( inputNodes.size(), &inputEdges, inDegree, outDegree, penalties );
			std::vector< IImporter::RoutingEdge >().swap( inputEdges );
			std::vector< char >().swap( inDegree );
			std::vector< char >().swap( outDegree );
			std::vector< double >().swap( penalties );

			const Graph& graph = contractor.GetGraph();

			qDebug() << "Create edge-based graph ...";
			for ( unsigned source = 0; source < graph.GetNumberOfNodes(); ++source ) {
				for ( unsigned edge = graph.BeginEdges( source ); edge < graph.EndEdges( source ); ++edge ) {
					const EdgeData& edgeData = graph.GetEdgeData( edge );
					if ( !edgeData.forward )
						continue;
					unsigned target = graph.GetTarget( edge );
					assert( !edgeData.shortcut );
					QHash< unsigned, EdgeMapItem >::iterator it = _edgeMap.find( edgeData.id );
					if ( it == _edgeMap.end() ) {
						it = _edgeMap.insert( edgeData.id, EdgeMapItem( edgeData.distance ) );
					}

					if ( source <= target ) {
						assert( it->first == (unsigned)-1 );
						it->first = numNodes;
					} else {
						assert( it->second == (unsigned)-1 );
						it->second = numNodes;
					}
					++numNodes;
				}
			}

			for ( unsigned source = 0; source < graph.GetNumberOfNodes(); ++source ) {
				for ( unsigned edge1 = graph.BeginEdges( source ) ; edge1 < graph.EndEdges( source ); ++edge1 ) {
					const EdgeData& edge1Data = graph.GetEdgeData( edge1 );
					if ( !edge1Data.forward )
						continue;
					unsigned via = graph.GetTarget( edge1 );
					Graph::PenaltyTable penaltyTable = graph.GetPenaltyTable( via );
					assert( !edge1Data.shortcut );

					QHash< unsigned, EdgeMapItem >::iterator it = _edgeMap.find( edge1Data.id );
					assert( it != _edgeMap.end() );
					unsigned edgeSource = source <= via ? it->first : it->second;
					for ( unsigned edge2 = graph.BeginEdges( via ); edge2 < graph.EndEdges( via ); ++edge2 ) {
						const EdgeData& edge2Data = graph.GetEdgeData( edge2 );
						if ( !edge2Data.forward )
							continue;
						assert( !edge2Data.shortcut );
						unsigned target = graph.GetTarget( edge2 );
						Graph::PenaltyData penalty = penaltyTable.GetData( graph.GetOriginalEdgeTarget( edge1 ), graph.GetOriginalEdgeSource( edge2 ) );
						if ( penalty == TurnContractor::RESTRICTED_TURN )
							continue;

						it = _edgeMap.find( edge2Data.id );
						assert( it != _edgeMap.end() );
						unsigned edgeTarget = via <= target ? it->first : it->second;

						unsigned turnDistance = edge1Data.distance + penalty;

						if (source == 5382 && via == 7029) {
							qDebug() << graph.DebugStringEdge( edge1 ).c_str() << graph.DebugStringEdge( edge2 ).c_str();
							qDebug() << edgeSource << edgeTarget << turnDistance;
						}

						IImporter::RoutingEdge turnEdge;
						turnEdge.source = edgeSource;
						turnEdge.target = edgeTarget;
						turnEdge.distance = turnDistance/10.;
						turnEdge.bidirectional = false;
						inputTurnEdges.push_back( turnEdge );
					}
				}
			}
		}

		qDebug() << "Write edge-based graph to file ...";
		QString filename =  fileInDirectory( dir, "Edge Based Graph");

		FileStream data( filename );

		if ( !data.open( QIODevice::WriteOnly ) )
			return false;
		unsigned numEdges = inputTurnEdges.size();
		unsigned numMaps = _edgeMap.size();
		data << numNodes << numEdges << numMaps;

		for ( QHash< unsigned, EdgeMapItem >::iterator it = _edgeMap.begin(); it != _edgeMap.end(); ++it ) {
			data << it.key() << it->first << it->second << it->distance;
		}

		for ( unsigned i = 0; i < inputTurnEdges.size(); ++i ) {
			unsigned source = inputTurnEdges[i].source;
			unsigned target = inputTurnEdges[i].target;
			double distance = inputTurnEdges[i].distance;
			bool bidirectional = inputTurnEdges[i].bidirectional;
			data << source << target << distance << bidirectional;
		}

		return true;
	}

	bool ReadEdgeMap(QString dir) {
		qDebug() << "ReadEdgeMap";
		QHash< unsigned, std::pair<unsigned, unsigned> > edgeMap;
		unsigned numNodes = 0;
		unsigned numEdges = 0;
		unsigned numMaps = 0;


		QString filename =  fileInDirectory( dir, "Edge Based Graph");
		FileStream data( filename );

		if ( !data.open( QIODevice::ReadOnly ) ) {
			qCritical() << "TurnExperiments::ReadEdgeMap(): File not found";
			exit(1);
			return false;
		}

		data >> numNodes >> numEdges >> numMaps;

		if ( data.status() == QDataStream::ReadPastEnd ) {
			qCritical() << "TurnExperiments::ReadEdgeMap(): Corrupted Graph Data";
			exit(1);
		}

		assert( _edgeMap.empty() );
		_edgeMap.reserve( numMaps );

		for ( unsigned i = 0; i < numMaps; ++i ) {
			unsigned id, first, second, distance;
			data >> id >> first >> second >> distance;
			_edgeMap.insert( id, EdgeMapItem( first, second, distance ) );
			if ( data.status() == QDataStream::ReadPastEnd ) {
				qCritical() << "TurnExperiments::GetEdgeContractor(): Corrupted Graph Data";
				exit(1);
			}
		}

		return true;
	}


	bool ReadEdgeContractor(QString dir) {
		qDebug() << "ReadEdgeContractor";
		std::vector< IImporter::RoutingEdge > inputTurnEdges;
		unsigned numNodes = 0;
		unsigned numEdges = 0;
		unsigned numMaps = 0;


		QString filename =  fileInDirectory( dir, "Edge Based Graph");
		FileStream data( filename );

		if ( !data.open( QIODevice::ReadOnly ) ) {
			qCritical() << "TurnExperiments::ReadEdgeContractor(): File not found";
			exit(1);
			return false;
		}

		data >> numNodes >> numEdges >> numMaps;

		if ( data.status() == QDataStream::ReadPastEnd ) {
			qCritical() << "TurnExperiments::GetEdgeContractor(): Corrupted Graph Data";
			exit(1);
		}

		assert( _edgeMap.empty() );
		_edgeMap.reserve( numMaps );
		inputTurnEdges.reserve( numEdges );

		for ( unsigned i = 0; i < numMaps; ++i ) {
			unsigned id, first, second, distance;
			data >> id >> first >> second >> distance;
			_edgeMap.insert( id, EdgeMapItem( first, second, distance ) );
			if ( data.status() == QDataStream::ReadPastEnd ) {
				qCritical() << "TurnExperiments::GetEdgeContractor(): Corrupted Graph Data";
				exit(1);
			}
		}

		inputTurnEdges.reserve( numEdges );
		for ( unsigned i = 0; i < numEdges; ++i ) {
			unsigned source, target;
			double distance;
			bool bidirectional;
			data >> source >> target >> distance >> bidirectional;

			IImporter::RoutingEdge turnEdge;
			turnEdge.source = source;
			turnEdge.target = target;
			turnEdge.distance = distance;
			turnEdge.bidirectional = bidirectional;
			inputTurnEdges.push_back( turnEdge );
		}

		_contractor = new Contractor( numNodes, inputTurnEdges );

		return true;
	}

	void ReadEdgeDemands(std::vector< Demand > *edgeDemands) {
		edgeDemands->reserve( _demands.size() );
		for ( unsigned i = 0; i < _demands.size(); ++i )
		{
			const Demand &demand = _demands[i];
			QHash< unsigned, EdgeMapItem >::iterator it = _edgeMap.find( demand.source.edgeID );
			assert( it != _edgeMap.end() );
			Demand edgeDemand;
			edgeDemand.source.source = demand.source.source <= demand.source.target ? it->first : it->second;
			assert( edgeDemand.source.source != (unsigned)-1 );
			it = _edgeMap.find( demand.target.edgeID );
			assert( it != _edgeMap.end() );
			edgeDemand.target.target = demand.target.source <= demand.target.target ? it->first : it->second;
			assert( edgeDemand.target.target != (unsigned)-1 );
			if ( demand.distance != std::numeric_limits<int>::max() )
				edgeDemand.distance = demand.distance - it->distance;
			else
				edgeDemand.distance = demand.distance;
			edgeDemands->push_back( edgeDemand );
		}
	}

	bool EdgePlainUnidirDijkstra() {
		const Contractor::_DynamicGraph & graph = _contractor->GetGraph();
		qDebug() << DebugStringSearchGraphMemory( graph ).c_str();

		std::vector< Demand > edgeDemands;
		ReadEdgeDemands( &edgeDemands );

		Query<Contractor::_DynamicGraph, false /*stall on demand*/> query(graph);

		double duration = _Timestamp();
		for ( int i = 0; i < (int)edgeDemands.size(); ++i ) {
			const Demand& demand = edgeDemands[i];
			qDebug() << demand.source.source << demand.target.target;
			int distance = query.UnidirSearch( demand.source.source, demand.target.target );
			qDebug() << i << _demands[i].DebugString().c_str() << "||" << distance;
			if (distance != demand.distance) {
				qDebug() << i << "edge" << distance << "node" << demand.distance;

				qDebug() << graph.DebugStringEdgesOf( demand.source.source ).c_str();
				qDebug() << graph.DebugStringEdgesOf( demand.target.target ).c_str();

				exit(1);
			}

			query.Clear();
		}
		duration = _Timestamp() - duration;
		qDebug() << "edge-based graph queries done in" << duration << "seconds.";
		qDebug() << "query time:" << duration * 1000 / _demands.size() << " ms";
		#ifdef QUERY_COUNT
		qDebug() << query.GetCounter().DebugString().c_str();
		#endif
		return true;
	}

	bool EdgeContract(QString dir) {
		_contractor->Run();
		qDebug() << "Writing graph to file ...";
		_contractor->GetGraph().WriteToFile( fileInDirectory( dir, "Edge Based CH Dynamic Graph") );
		return true;
	}

	bool EdgeCHQuery(IImporter*, QString dir) {
		const Contractor::_DynamicGraph* graph = Contractor::_DynamicGraph::ReadFromFile( fileInDirectory( dir, "Edge Based CH Dynamic Graph") );
		qDebug() << DebugStringSearchGraphMemory( *graph ).c_str();


		std::vector< Demand > edgeDemands;
		ReadEdgeDemands( &edgeDemands );

		typedef Query<Contractor::_DynamicGraph, true /*stall on demand*/> Query;
		Query query(*graph);
		double duration = _Timestamp();
		#ifdef QUERY_COUNT
		double maxRelError = 0;
		double maxAbsError = 0;
		double sumError = 0;
		#endif
		for ( int i = 0; i < (int)edgeDemands.size(); ++i )
		{
			const Demand& demand = edgeDemands[i];
			int distance = query.BidirSearch( demand.source.source, demand.target.target );
			if (distance != demand.distance) {

				#ifdef QUERY_COUNT
				if ( demand.distance != 0 ) {
					//qDebug() << distance << demand.DebugString().c_str();
					double relError = fabs( ( double ) distance / demand.distance - 1 );
					if ( relError > maxRelError ) {
						maxRelError = relError;
					}
					sumError += relError;
				}
				double absError = fabs( distance - demand.distance );
				if ( absError > maxAbsError )
					maxAbsError = absError;
				#endif

				#ifndef NDEBUG
				qDebug() << i << "edge" << distance << "node" << demand.distance;

				qDebug() << graph->DebugStringEdgesOf( demand.source.source ).c_str();
				qDebug() << graph->DebugStringEdgesOf( demand.target.target ).c_str();

				exit(1);
				#endif

			}

			query.Clear();
		}
		duration = _Timestamp() - duration;
		qDebug() << "edge-based CH queries done in" << duration << "seconds.";
		qDebug() << "query time:" << duration * 1000 / _demands.size() << " ms";
		#ifdef QUERY_COUNT
		qDebug() << "max error:" << maxAbsError / 10.0 << "s";
		qDebug() << "max error:" << maxRelError * 100.0 << "%";
		qDebug() << "avg error:" << sumError / _demands.size() * 100.0 << "%";
		qDebug() << query.GetCounter().DebugString().c_str();
		#endif

		delete graph;
		return true;
	}

};

#endif // TURNEXPERIMENTS_H_INCLUDED
