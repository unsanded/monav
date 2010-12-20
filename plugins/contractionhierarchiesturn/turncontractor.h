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

#ifndef TURNCONTRACTOR_H_INCLUDED
#define TURNCONTRACTOR_H_INCLUDED
#include <vector>
#include <omp.h>
#include <limits.h>
#include <limits>
#include <sstream>
#include "utils/qthelpers.h"
#include "dynamicturngraph.h"
#include "../contractionhierarchies/binaryheap.h"
#include "utils/config.h"
#include "utils/bithelpers.h"
#include "turnquery.h"
#include "penaltyclassificator.h"

class TurnContractor {

	public:

		struct Witness {
			NodeID source;
			NodeID target;
			NodeID middle;
		};

	private:

		bool m_aggressive;
		long long m_savedShortcuts;

		struct _EdgeData {
			unsigned distance;
			unsigned originalEdges : 29;
			bool shortcut : 1;
			bool forward : 1;
			bool backward : 1;
			union {
				NodeID middle; // shortcut
				unsigned id; // original edge
			};
			std::string DebugString() const {
				std::stringstream ss;
				ss << distance << " ";
				if (backward) ss << "<";
				ss << "-";
				if (forward) ss << ">";
				if (shortcut) ss << " shortcut|" << middle;
				else ss << " orig=" << id;
				return ss.str();
			}

			void Serialize( FileStream *data ) const {
				unsigned t1; bool t2;
				t1 = distance;  *data << t1;
				t2 = shortcut;  *data << t2;
				t2 = forward;   *data << t2;
				t2 = backward;  *data << t2;
				t1 = id;        *data << t1;
			}
			void Deserialize( FileStream *data ) {
				unsigned t1; bool t2;
				*data >> t1;  distance = t1;
				*data >> t2;  shortcut = t2;
				*data >> t2;  forward = t2;
				*data >> t2;  backward = t2;
				*data >> t1;  id = t1;
			}

		} data;

		struct _HeapData {
			NodeID node;
				unsigned numOriginalEdges : 23;
			unsigned char originalEdge : 8;
			bool onlyViaContracted : 1;
				_HeapData(NodeID _node, unsigned _numOriginalEdges, unsigned char _originalEdge, bool _onlyViaContracted)
				: node(_node), numOriginalEdges(_numOriginalEdges), originalEdge(_originalEdge), onlyViaContracted(_onlyViaContracted) {}
				std::string DebugString() const {
					 std::stringstream ss;
					 ss << (unsigned)originalEdge << " -> " << node;
					 if (onlyViaContracted)
						  ss << " ONLY";
					 return ss.str();
				}
		};

		  typedef unsigned char _PenaltyData;
	public:
		  static const _PenaltyData RESTRICTED_TURN = 255;
	private:
		  typedef short int GammaValue;
		  static const GammaValue RESTRICTED_NEIGHBOUR = SHRT_MIN;
		#ifndef NDEBUG
		  static const unsigned SPECIAL_NODE = -1;
		#endif
		typedef DynamicTurnGraph< _EdgeData, _PenaltyData > _DynamicGraph;
		typedef BinaryHeap< NodeID, NodeID, int, _HeapData > _Heap;
		typedef _DynamicGraph::InputNode _ImportNode;
		typedef _DynamicGraph::InputEdge _ImportEdge;
		typedef _DynamicGraph::InputPenalty _ImportPenalty;

		struct _ThreadData {
			_Heap heap;
			_Heap aggressiveHeap;
			std::vector< _ImportEdge > insertedEdges;
			std::vector< Witness > witnessList;
			std::vector< NodeID > neighbours;
			int deletedEdges;
			_ThreadData( NodeID numOriginalEdges ): heap( numOriginalEdges ), aggressiveHeap( numOriginalEdges ) {
			}
		};

		struct _PriorityData {
			int depth;
			NodeID bias;
			_PriorityData() {
				depth = 0;
			}
		};

		struct _ContractionInformation {
			int edgesDeleted;
			int edgesAdded;
			int originalEdgesDeleted;
			int originalEdgesAdded;
			_ContractionInformation() {
				edgesAdded = edgesDeleted = originalEdgesAdded = originalEdgesDeleted = 0;
			}
		};

		struct _NodePartitionor {
			bool operator()( std::pair< NodeID, bool > nodeData ) {
				return !nodeData.second;
			}
		};

		struct _LogItem {
			unsigned iteration;
			NodeID nodes;
			double contraction;
			double independent;
			double inserting;
			double removing;
			double updating;
			unsigned remainingNodes;
			unsigned remainingEdges;

			_LogItem() {
				iteration = nodes = contraction = independent = inserting = removing = updating = 0;
			}

			double GetTotalTime() const {
				return contraction + independent + inserting + removing + updating;
			}

			void PrintStatistics() const {
				double remainingDegree = 0;
				if (remainingNodes > 0)
					remainingDegree = (double)remainingEdges/remainingNodes;
				qDebug( "%d\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t%d\t%d\t%lf", iteration, nodes, independent, contraction, inserting, removing, updating, remainingNodes, remainingEdges, remainingDegree );
			}
		};

		struct GammaIndexItem {
			unsigned forward;
			unsigned backward;
			GammaIndexItem( unsigned f, unsigned b ) : forward( f ), backward ( b ) {}
		};
		struct OriginalEdgePair {
			unsigned node;
			unsigned originalEdge;
			bool operator==(const OriginalEdgePair& right) const {
				return node == right.node && originalEdge == right.originalEdge;
			}
			bool operator<(const OriginalEdgePair& right) const {
				if (node != right.node) return node < right.node;
				return originalEdge < right.originalEdge;
			}
		};


		class _LogData {
			public:

				std::vector < _LogItem > iterations;

				unsigned GetNIterations() {
					return ( unsigned ) iterations.size();
				}

				_LogItem GetSum() const {
					_LogItem sum;
					sum.iteration = ( unsigned ) iterations.size();

					for ( int i = 0, e = ( int ) iterations.size(); i < e; ++i ) {
						sum.nodes += iterations[i].nodes;
						sum.contraction += iterations[i].contraction;
						sum.independent += iterations[i].independent;
						sum.inserting += iterations[i].inserting;
						sum.removing += iterations[i].removing;
						sum.updating += iterations[i].updating;
					}

					return sum;
				}

				void PrintHeader() const {
					qDebug( "Iteration\tNodes\tIndependent\tContraction\tInserting\tRemoving\tUpdating\tRNodes\tREdges\tRDegree" );
				}

				void PrintSummary() const {
					PrintHeader();
					GetSum().PrintStatistics();
				}

				void Print() const {
					PrintHeader();
					for ( int i = 0, e = ( int ) iterations.size(); i < e; ++i )
						iterations[i].PrintStatistics();
				}

				void Insert( const _LogItem& data ) {
					iterations.push_back( data );
				}

		};

	public:

		typedef _DynamicGraph DynamicGraph;

		template< class InputEdge, class DegreeType, class PenaltyType >
		TurnContractor( NodeID numNodes, std::vector< InputEdge >* inputEdges,
				const std::vector< DegreeType >& inDegree, const std::vector< DegreeType >& outDegree,
				const std::vector< PenaltyType >& inputPenalties ) {
			std::vector< _ImportEdge > edges;
			edges.reserve( 2 * inputEdges->size() );
			int skippedLargeEdges = 0;

			m_aggressive = false;
			m_savedShortcuts = 0;

			if ( QCoreApplication::arguments().indexOf( "--aggressive" ) != -1 ) {
				m_aggressive = true;
				qDebug() << "aggressive contraction";
			}

			#ifndef NDEBUG
			if (SPECIAL_NODE < numNodes) qDebug() << SPECIAL_NODE << (int)inDegree[SPECIAL_NODE] << (int)outDegree[SPECIAL_NODE];
			#endif

			std::vector< unsigned > classMapping;
			std::vector< PenaltyClassificator::PenaltyClass > penaltyClasses;
			std::vector< int > quantizedPenalties;
			/************Compress Penalties**************/
			{
				std::vector< int > encoderTable;
				encoderTable.push_back( -1 );
				encoderTable.push_back( 0 );
				PenaltyType maxPenalty = 0;
				for ( unsigned penalty = 0; penalty < inputPenalties.size(); penalty++ )
					maxPenalty = std::max( maxPenalty, inputPenalties[penalty] );
				double error = 0.2;
				QStringList args = QCoreApplication::arguments();
				int errorIndex = args.indexOf( QRegExp( "--error=.*" ) );
				if ( errorIndex != -1 )
					error = args[errorIndex].section( '=', 1 ).toDouble();

				qDebug() << "Contraction Hierarchies: penalties error:" << error * 100 << "%";

				int encoderBits = compute_encoder_table( &encoderTable, maxPenalty * 10 + 1, error );

				qDebug() << "Contraction Hierarchies: Penalty Bits:" << encoderBits << ", Entries:" << encoderTable.size();
				for ( unsigned penalty = 0; penalty < inputPenalties.size(); penalty++ ) {
					int value = inputPenalties[penalty] < 0 ? -1 : inputPenalties[penalty] * 10 + 0.5;
					int encodedValue = table_encode( value, encoderTable );
					quantizedPenalties.push_back( encoderTable[encodedValue] == -1 ? RESTRICTED_TURN : encoderTable[encodedValue] );
					if ( ( double ) abs( value - encoderTable[encodedValue] ) / value > error ) {
						qDebug() << "Wrong Encoding:" << value << encodedValue << encoderTable[encodedValue];
						for ( unsigned i = 0; i < encoderTable.size(); i++ )
							qDebug() << i << ":" << encoderTable[i];
					}
					if ( value != 0 )
						assert( ( double ) abs( value - encoderTable[encodedValue] ) / value <= error );
					else
						assert( encoderTable[encodedValue] == 0 );
				}

				PenaltyClassificator* classificator = new PenaltyClassificator( inDegree, outDegree, &quantizedPenalties, *inputEdges );
				classificator->run();
				quantizedPenalties.clear();
				std::vector< char > edgeMapping;

				classificator->getClasses( &penaltyClasses, &quantizedPenalties, &classMapping, &edgeMapping );
				assert( classMapping.size() == numNodes );
				delete classificator;

				std::vector< unsigned > edgeMappingIndex;
				unsigned position = 0;
				for ( unsigned node = 0; node < classMapping.size(); node++ ) {
					assert( penaltyClasses[classMapping[node]].in == inDegree[node] );
					assert( penaltyClasses[classMapping[node]].out == outDegree[node] );
					edgeMappingIndex.push_back( position );
					for ( unsigned in = 0; in < ( unsigned ) inDegree[node]; in++ )
						assert( edgeMapping[position + in] < inDegree[node] );
					for ( unsigned out = 0; out < ( unsigned ) outDegree[node]; out++ )
						assert( edgeMapping[position + inDegree[node] + out] < outDegree[node] );
					position += ( unsigned ) inDegree[node] + outDegree[node];
				}
				assert( position == edgeMapping.size() );

				for ( unsigned edge = 0; edge < inputEdges->size(); edge++ ) {
					IImporter::RoutingEdge& edgeData = ( *inputEdges )[edge];
					assert( edgeData.edgeIDAtSource < outDegree[edgeData.source] );
					assert( edgeData.edgeIDAtTarget < inDegree[edgeData.target] );
					if ( edgeData.bidirectional ) {
						assert( edgeData.edgeIDAtSource < inDegree[edgeData.source] );
						assert( edgeData.edgeIDAtTarget < outDegree[edgeData.target] );
					}
					edgeData.edgeIDAtTarget = edgeMapping[edgeMappingIndex[edgeData.target] + edgeData.edgeIDAtTarget];
					edgeData.edgeIDAtSource = edgeMapping[edgeMappingIndex[edgeData.source] + inDegree[edgeData.source] + edgeData.edgeIDAtSource];
					assert( edgeData.edgeIDAtTarget < inDegree[edgeData.target] );
					assert( edgeData.edgeIDAtSource < outDegree[edgeData.source] );
					if ( edgeData.bidirectional ) {
						if ( edgeData.edgeIDAtSource >= inDegree[edgeData.source] ) {
							unsigned source = edgeData.source;
							qDebug() << edgeData.edgeIDAtSource << edgeData.source << ( unsigned ) inDegree[source] << ( unsigned ) outDegree[edgeData.source];
							for ( unsigned in = 0; in < ( unsigned ) inDegree[source]; in++ )
								qDebug() << ( unsigned ) edgeMapping[edgeMappingIndex[source] + in];
							qDebug() << "===";
							for ( unsigned out = 0; out < ( unsigned ) outDegree[source]; out++ )
								qDebug() << ( unsigned ) edgeMapping[edgeMappingIndex[source] + inDegree[source] + out];
						}
						assert( edgeData.edgeIDAtSource < inDegree[edgeData.source] );
						assert( edgeData.edgeIDAtTarget < outDegree[edgeData.target] );
					}
				}
			}
			/************Compress Penalties**************/

			for ( typename std::vector< InputEdge >::const_iterator i = inputEdges->begin(), e = inputEdges->end(); i != e; ++i ) {
				_ImportEdge edge;
				edge.source = i->source;
				edge.target = i->target;

				assert( i->edgeIDAtSource < outDegree[i->source] );
				assert( i->edgeIDAtTarget < inDegree[i->target] );
				#ifndef NDEBUG
				if (i->bidirectional) {
					assert( i->edgeIDAtSource < inDegree[i->source] );
					assert( i->edgeIDAtTarget < outDegree[i->target] );
				}
				#endif
				edge.originalEdgeSource = i->edgeIDAtSource;
				edge.originalEdgeTarget = i->edgeIDAtTarget;
				edge.data.distance = std::max( i->distance * 10.0 + 0.5, 1.0 );
				if ( edge.data.distance > 24 * 60 * 60 * 10 ) {
					skippedLargeEdges++;
					continue;
				}
				edge.data.shortcut = false;
				edge.data.id = i - inputEdges->begin();
				edge.data.forward = true;
				edge.data.backward = i->bidirectional;
				edge.data.originalEdges = 1;

				if ( edge.data.distance < 1 ) {
					qDebug() << edge.source << edge.target << edge.data.forward << edge.data.backward << edge.data.distance << edge.data.id << i->distance;
				}

				if ( edge.source == edge.target ) {
					_loops.push_back( edge );
					//continue;
				}

				#ifndef NDEBUG
				if (edge.target == SPECIAL_NODE || edge.source == SPECIAL_NODE) {
					qDebug() << edge.DebugString().c_str();
				}
				#endif

				edges.push_back( edge );
				// Add reverse edge, unless it is a bidirectional loop.
				if ( edge.source != edge.target || !edge.data.backward ) {
					std::swap( edge.source, edge.target );
					std::swap( edge.originalEdgeSource, edge.originalEdgeTarget );
					edge.data.forward = i->bidirectional;
					edge.data.backward = true;
					edges.push_back( edge );
				}
			}
			if ( skippedLargeEdges != 0 )
				qDebug( "Skipped %d edges with too large edge weight", skippedLargeEdges );
			std::sort( edges.begin(), edges.end() );

			std::vector< _ImportPenalty > penalties;
			std::vector< _ImportNode > nodes;
			nodes.reserve( numNodes );
			penalties.reserve( penaltyClasses.size() );
			penalties.resize( penaltyClasses.size() );

			typename std::vector< int >::const_iterator penaltyIter = quantizedPenalties.begin();
			for ( unsigned tableClass = 0; tableClass < penaltyClasses.size(); tableClass++ ) {
				penalties[tableClass].inDegree = penaltyClasses[tableClass].in;
				penalties[tableClass].outDegree = penaltyClasses[tableClass].out;
				unsigned penaltySize = ( unsigned ) penaltyClasses[tableClass].in * ( unsigned ) penaltyClasses[tableClass].out;
				penalties[tableClass].data.reserve( penaltySize );
				for ( unsigned i = 0; i < penaltySize; ++i, ++penaltyIter ) {
					assert( penaltyIter != quantizedPenalties.end() );
					penalties[tableClass].data.push_back( *penaltyIter );
				}
			}
			assert( penaltyIter == quantizedPenalties.end() );

			for ( NodeID u = 0; u < numNodes; ++u ) {
				_ImportNode node;
				node.firstPenalty = classMapping[u];

//				if (node.inDegree + node.outDegree > 15) qDebug() << u << node.inDegree << node.outDegree;
				nodes.push_back( node );
			}

			#ifndef NDEBUG
			if ( SPECIAL_NODE < numNodes ) {
				unsigned n = SPECIAL_NODE;
				unsigned begin = 0;
				for ( unsigned u = 0; u < n; ++u ) {
					begin += inDegree[u] * outDegree[u];
				}
				std::stringstream ss;
				for ( int out = 0; out < outDegree[n]; ++out ) {
					ss << "\t" << out;
				}
				ss << "\n";
				for ( int in = 0; in < inDegree[n]; ++in ) {
					ss << in;
					for ( int out = 0; out < outDegree[n]; ++out ) {
						ss << "\t" << inputPenalties[ begin + in * outDegree[n] + out ];
					}
					ss << "\n";
				}
				qDebug() << ss.str().c_str();
			}
			#endif

			assert( penalties.size() == penaltyClasses.size() );

			_graph = new _DynamicGraph( nodes, edges, penalties );

			#ifndef NDEBUG
			if ( SPECIAL_NODE < numNodes ) {
				qDebug() << _graph->DebugStringPenaltyTable( SPECIAL_NODE ).c_str();
			}
			#endif

			std::vector< _ImportNode >().swap( nodes );
			std::vector< _ImportEdge >().swap( edges );
			std::vector< _ImportPenalty >().swap( penalties );

			_ComputeGammaTable();

		}

		~TurnContractor() {
			delete _graph;
		}

		void Run() {
			const NodeID numberOfNodes = _graph->GetNumberOfNodes();
			_LogData log;

//			omp_set_num_threads(1);
			int maxThreads = omp_get_max_threads();
			std::vector < _ThreadData* > threadData;
			for ( int threadNum = 0; threadNum < maxThreads; ++threadNum ) {
				threadData.push_back( new _ThreadData( _graph->GetNumberOfOriginalEdges() ) );
			}
			qDebug( "%d nodes, %d edges", numberOfNodes, _graph->GetNumberOfEdges() );
			qDebug( "using %d threads", maxThreads );

			NodeID levelID = 0;
			NodeID iteration = 0;
			std::vector< std::pair< NodeID, bool > > remainingNodes( numberOfNodes );
			std::vector< double > nodePriority( numberOfNodes );
			std::vector< _PriorityData > nodeData( numberOfNodes );

			//initialize the variables
			#pragma omp parallel for schedule ( guided )
			for ( int x = 0; x < ( int ) numberOfNodes; ++x )
				remainingNodes[x].first = x;
			{
				srand48(698176);
				RandomShuffleGenerator gen;
				std::random_shuffle( remainingNodes.begin(), remainingNodes.end(), gen );
			}

			for ( int x = 0; x < ( int ) numberOfNodes; ++x )
				nodeData[remainingNodes[x].first].bias = x;

			qDebug( "Initialise Node Priorities ... " );
			_LogItem statistics0;
			statistics0.updating = _Timestamp();
			statistics0.iteration = 0;

			int numRemainingEdges = _graph->GetNumberOfEdges();
//			_CheckRemainingEdges(remainingNodes, numberOfNodes, numRemainingEdges);
			#pragma omp parallel
			{
				_ThreadData* data = threadData[omp_get_thread_num()];
				#pragma omp for schedule ( guided )
				for ( int x = 0; x < ( int ) numberOfNodes; ++x ) {
//					qDebug() << "node" << x;
					nodePriority[x] = _Evaluate( data, &nodeData[x], x );
				}
			}
			qDebug( "done" );

			statistics0.updating = _Timestamp() - statistics0.updating;
			statistics0.remainingNodes = numberOfNodes;
			statistics0.remainingEdges = numRemainingEdges;
			log.Insert( statistics0 );

			log.PrintHeader();
			statistics0.PrintStatistics();

				#ifndef NDEBUG
			typedef TurnQuery<_DynamicGraph, false /*stall on demand*/> TestQuery;
			TestQuery testQuery( *_graph );
				#endif

			while ( levelID < numberOfNodes ) {

					 #ifndef NDEBUG
				if ( false ) {
					 //TurnQueryEdge testSource( 44912, 8058,  92427 );
					 //TurnQueryEdge testTarget( 12578, 41446, 7988 );
					TurnQueryEdge testSource( 44912, 8058,  92427 );
					TurnQueryEdge testTarget( 49482, 10020, 65423 );
					int dist = testQuery.BidirSearch( testSource, testTarget );
					TestQuery::Path path;

					qDebug() << "up";
					{
						unsigned origUp = testQuery.m_middle.in;
						do {
							std::stringstream ss;
							assert( testQuery.m_heapForward.WasInserted( origUp ) );
							const TestQuery::HeapData& heapData = testQuery.m_heapForward.GetData( origUp );
							_DynamicGraph::EdgeIterator edge = heapData.parentEdge;

							ss << "\t" << testQuery.m_heapForward.GetKey( origUp ) << "\t" << _graph->DebugStringEdge(edge);
							qDebug() << ss.str().c_str();

							origUp = heapData.parentOrig;
						} while ( origUp != (unsigned)-1 );
					}
					qDebug();

					qDebug() << "down";
					{
						unsigned origDown = testQuery.m_middle.out;
						do {
							std::stringstream ss;
							assert( testQuery.m_heapBackward.WasInserted( origDown ) );
							const TestQuery::HeapData& heapData = testQuery.m_heapBackward.GetData( origDown );
							_DynamicGraph::EdgeIterator edge = heapData.parentEdge;

							ss << "\t" << testQuery.m_heapBackward.GetKey( origDown ) << "\t" << _graph->DebugStringEdge(edge);
							qDebug() << ss.str().c_str();

							origDown = heapData.parentOrig;
						} while ( origDown != (unsigned)-1 );
					}
					qDebug();



					qDebug() << "dist" << dist;
					if (dist != 5543) {
						qDebug() << "dist wrong" << dist;
						exit(1);
					}
					testQuery.Clear();
				}
					 #endif

				_LogItem statistics;
				statistics.iteration = iteration++;
				const int last = ( int ) remainingNodes.size();

				//determine independent node set
				double timeLast = _Timestamp();
				#pragma omp parallel
				{
					_ThreadData* const data = threadData[omp_get_thread_num()];
					#pragma omp for schedule ( guided )
					for ( int i = 0; i < last; ++i ) {
						const NodeID node = remainingNodes[i].first;
						remainingNodes[i].second = _IsIndependent( nodePriority, nodeData, data, node );
					}
				}
				_NodePartitionor functor;
				const std::vector < std::pair < NodeID, bool > >::const_iterator first = stable_partition( remainingNodes.begin(), remainingNodes.end(), functor );
				const int firstIndependent = first - remainingNodes.begin();
				statistics.nodes = last - firstIndependent;
				statistics.independent += _Timestamp() - timeLast;
				timeLast = _Timestamp();

				//contract independent nodes
				qDebug() << "contract";
				#pragma omp parallel
				{
					_ThreadData* const data = threadData[omp_get_thread_num()];
					#pragma omp for schedule ( guided ) nowait
					for ( int position = firstIndependent ; position < last; ++position ) {
						NodeID x = remainingNodes[position].first;
//						if ( _graph->GetOutDegree(x) > 20 ) {
//							qDebug() << "contract" << x;
//							qDebug() << _graph->DebugStringEdgesOf(x).c_str();
//							exit(1);
//						}
						_Contract< false > ( data, x );
						#ifndef NDEBUG
						if (x == SPECIAL_NODE)
							qDebug() << "contracted " << x;

						#endif
						nodePriority[x] = -1;
					}
					std::sort( data->insertedEdges.begin(), data->insertedEdges.end() );
				}
				statistics.contraction += _Timestamp() - timeLast;
				timeLast = _Timestamp();

				#pragma omp parallel
				{
					_ThreadData* const data = threadData[omp_get_thread_num()];
					data->deletedEdges = 0;
					#pragma omp for schedule ( guided ) nowait
					for ( int position = firstIndependent ; position < last; ++position ) {
						NodeID x = remainingNodes[position].first;
						data->deletedEdges += _graph->GetOutDegree( x );
						data->deletedEdges += _DeleteIncomingEdges( data, x );
					}
				}
				statistics.removing += _Timestamp() - timeLast;
				timeLast = _Timestamp();

				//insert new edges
				qDebug() << "insert new edges";
				for ( int threadNum = 0; threadNum < maxThreads; ++threadNum ) {
					_ThreadData& data = *threadData[threadNum];
					numRemainingEdges -= data.deletedEdges;
					assert( numRemainingEdges >= 0 );
					numRemainingEdges += data.insertedEdges.size();
					for ( int i = 0; i < ( int ) data.insertedEdges.size(); ++i ) {
						const _ImportEdge& edge = data.insertedEdges[i];
						// Do not insert edges to already contracted nodes.
						assert( nodePriority[edge.source] >= 0 );
						assert( nodePriority[edge.target] >= 0 );
						_graph->InsertEdge( edge );
					}
					std::vector< _ImportEdge >().swap( data.insertedEdges );
				}
				statistics.inserting += _Timestamp() - timeLast;
				timeLast = _Timestamp();

//				_CheckRemainingEdges(remainingNodes, firstIndependent, numRemainingEdges);

				//update priorities
				qDebug() << "update priorities";
				#pragma omp parallel
				{
					_ThreadData* const data = threadData[omp_get_thread_num()];
					#pragma omp for schedule ( guided ) nowait
					for ( int position = firstIndependent ; position < last; ++position ) {
						NodeID x = remainingNodes[position].first;
						_UpdateNeighbours( &nodePriority, &nodeData, data, x );
					}
				}
				statistics.updating += _Timestamp() - timeLast;
				timeLast = _Timestamp();

				statistics.remainingNodes = firstIndependent;
				statistics.remainingEdges = numRemainingEdges;

				//output some statistics
				statistics.PrintStatistics();
				//qDebug( wxT( "Printed" ) );

				//remove contracted nodes from the pool
				levelID += last - firstIndependent;
				remainingNodes.resize( firstIndependent );
				std::vector< std::pair< NodeID, bool > >( remainingNodes ).swap( remainingNodes );
				log.Insert( statistics );
			}

			for ( int threadNum = 0; threadNum < maxThreads; threadNum++ ) {
				_witnessList.insert( _witnessList.end(), threadData[threadNum]->witnessList.begin(), threadData[threadNum]->witnessList.end() );
				delete threadData[threadNum];
			}

			log.PrintSummary();
			qDebug( "Total Time: %lf s", log.GetSum().GetTotalTime() );
			qDebug() << "saved shortcuts:" << m_savedShortcuts;

		}

		template< class Edge >
		void GetEdges( std::vector< Edge >* edges ) {
			NodeID numberOfNodes = _graph->GetNumberOfNodes();
			for ( NodeID node = 0; node < numberOfNodes; ++node ) {
				for ( _DynamicGraph::EdgeIterator edge = _graph->BeginEdges( node ), endEdges = _graph->EndEdges( node ); edge != endEdges; ++edge ) {
					const NodeID target = _graph->GetTarget( edge );
					const _EdgeData& data = _graph->GetEdgeData( edge );
					Edge newEdge;
					newEdge.source = node;
					newEdge.target = target;
					newEdge.data.distance = data.distance;
					newEdge.data.shortcut = data.shortcut;
					if ( data.shortcut )
						newEdge.data.middle = data.middle;
					else
						newEdge.data.id = data.id;
					newEdge.data.forward = data.forward;
					newEdge.data.backward = data.backward;
					edges->push_back( newEdge );
				}
			}
		}

		template< class Edge >
		void GetLoops( std::vector< Edge >* edges ) {
			for ( unsigned i = 0; i < _loops.size(); i++ ) {
				Edge newEdge;
				newEdge.source = _loops[i].source;
				newEdge.target = _loops[i].target;
				newEdge.data.distance = _loops[i].data.distance;
				newEdge.data.shortcut = _loops[i].data.shortcut;
				newEdge.data.id = _loops[i].data.id;
				newEdge.data.forward = _loops[i].data.forward;
				newEdge.data.backward = _loops[i].data.backward;
				edges->push_back( newEdge );
			}
		}

		void GetWitnessList( std::vector< Witness >& list ) {
			list = _witnessList;
		}

		const DynamicGraph& GetGraph() const {
			return *_graph;
		}

	private:

		struct RandomShuffleGenerator
		{
			size_t operator()(size_t n) const
			{
				return lrand48() % n;
			}
		};

		void _ComputeGammaTable()
		{
			_gammaIndex.clear();
			_gamma.clear();
			const NodeID numNodes = _graph->GetNumberOfNodes();
				_gammaIndex.reserve( numNodes );

				QHash< unsigned, GammaIndexItem > penaltyMap;
				unsigned squareSum = 0;
				std::vector< NodeID > idOrder;
				for ( NodeID u = 0; u < numNodes; ++u ) {
					unsigned id = _graph->GetPenaltyTableID(u);
					QHash< unsigned, GammaIndexItem >::iterator it = penaltyMap.find(id);
					if ( it != penaltyMap.end() ) {
						_gammaIndex.push_back( it.value() );
					} else {
						unsigned out = _graph->GetOriginalOutDegree(u);
						unsigned in = _graph->GetOriginalInDegree(u);
						GammaIndexItem item( squareSum, squareSum + out * out );
						_gammaIndex.push_back( item );
						penaltyMap.insert( id, item );
						idOrder.push_back( u );
						squareSum += out * out;
						squareSum += in * in;
					}
				}
				assert( _gammaIndex.size() == numNodes );

				_gamma.reserve( squareSum );
				{
					for ( unsigned i = 0; i < idOrder.size(); ++i ) {
						NodeID u = idOrder[i];
						  _DynamicGraph::PenaltyTable penaltyTable = _graph->GetPenaltyTable( u );
						  unsigned inDegree = penaltyTable.GetInDegree();
						  unsigned outDegree = penaltyTable.GetOutDegree();

						  assert( _gamma.size() == _gammaIndex[u].forward );

						  for ( unsigned out = 0; out < outDegree; ++out ) {
								for ( unsigned out2 = 0; out2 < outDegree; ++out2 ) {
									 GammaValue g = std::numeric_limits< GammaValue >::min();
									 bool restricted = false;
									 for ( unsigned in = 0; in < inDegree; ++in ) {
										GammaValue penalty = penaltyTable.GetData( in, out );
										GammaValue penalty2 = penaltyTable.GetData( in, out2);
										  if (penalty != RESTRICTED_TURN && penalty2 == RESTRICTED_TURN) {
												// 'u -> out2' not in 'alpha(u -> out)'
												g = RESTRICTED_NEIGHBOUR;
												restricted = true;
												break;
										  }
										  if (penalty != RESTRICTED_TURN && penalty2 != RESTRICTED_TURN) {
												g = std::max<GammaValue>( g, penalty2 - penalty );
										  }
									 }
									 if ( !restricted && g == std::numeric_limits< GammaValue >::min() ) g = 0;
									 _gamma.push_back(g);
								}
						  }
						  assert( _gamma.size() == _gammaIndex[u].backward );

						  for ( unsigned in = 0; in < inDegree; ++in ) {
								for ( unsigned in2 = 0; in2 < inDegree; ++in2 ) {
									 GammaValue g = std::numeric_limits< GammaValue >::min();
									 bool restricted = false;
									 for ( unsigned out = 0; out < outDegree; ++out ) {
										  GammaValue penalty = penaltyTable.GetData( in, out);
										  GammaValue penalty2 = penaltyTable.GetData( in2, out);
										  if (penalty != RESTRICTED_TURN && penalty2 == RESTRICTED_TURN) {
												// 'in2 -> u' not in 'alpha(in -> u)'
												g = RESTRICTED_NEIGHBOUR;
												restricted = true;
												break;
										  }
										  if (penalty != RESTRICTED_TURN && penalty2 != RESTRICTED_TURN) {
												g = std::max<GammaValue>( g, penalty2 - penalty );
										  }
									 }
									 if ( !restricted && g == std::numeric_limits< GammaValue >::min() ) g = 0;
									 _gamma.push_back(g);
								}
						  }

					 }
				}
			#ifndef NDEBUG
				if ( SPECIAL_NODE < numNodes ) {
				qDebug() << _DebugStringGammaForward( SPECIAL_NODE ).c_str();
				qDebug() << _DebugStringGammaBackward( SPECIAL_NODE ).c_str();
				}
			#endif
		}

		double _Timestamp() {
			static Timer timer;
			return ( double ) timer.elapsed() / 1000;
		}

		bool _ConstructCH( _DynamicGraph* _graph );

		void _Dijkstra( const NodeID contracted, int numOnlyViaContracted, const int maxNodes, _ThreadData* const thread_data ){

			_Heap& heap = thread_data->heap;

			//qDebug() << "numOnlyViaContracted" << numOnlyViaContracted;

			int nodes = 0;
			while ( heap.Size() > 0 ) {
				const NodeID originalEdge = heap.DeleteMin();
				const int distance = heap.GetKey( originalEdge );
				const _HeapData data = heap.GetData( originalEdge );  // no reference, as heap size may change below
//				#ifndef NDEBUG
//				if (contracted == SPECIAL_NODE) qDebug() << "settle" << originalEdge << "---" << (int)data.originalEdge << "->" << data.node << ":" << distance << data.onlyViaContracted;
//				#endif
				if ( data.onlyViaContracted )
					--numOnlyViaContracted;

					 #ifndef NDEBUG
				if (numOnlyViaContracted < 0 ) qDebug() << contracted;
					 #endif
				assert( numOnlyViaContracted >= 0 );
					 nodes++;
					 const bool toOnlyViaContracted = data.onlyViaContracted && data.node == contracted;
					 if ( !toOnlyViaContracted ) {
						  // Stop search as soon as no prefix of a potential shortcut remains in the queue.
						  if ( numOnlyViaContracted == 0 )
								return;
						  // After settling 'maxNodes', we only look for potential shortcuts.
						  else if ( nodes > maxNodes )
								continue;
					 }

				_DynamicGraph::PenaltyTable penaltyTable = _graph->GetPenaltyTable( data.node );

				//qDebug() << _graph->DebugStringEdgesOf(data.node).c_str();
				//qDebug() << _graph->DebugStringPenaltyTable(data.node).c_str();

				//iterate over all edges of data.node
				for ( _DynamicGraph::EdgeIterator edge = _graph->BeginEdges( data.node ), endEdges = _graph->EndEdges( data.node ); edge != endEdges; ++edge ) {
					const _EdgeData& edgeData = _graph->GetEdgeData( edge );
					if ( !edgeData.forward )
						continue;
					const NodeID to = _graph->GetTarget( edge );
					assert( data.originalEdge < penaltyTable.GetInDegree() );
					#ifndef NDEBUG
					if ( _graph->GetOriginalEdgeSource( edge ) >= _graph->GetOriginalOutDegree( data.node ) ) {
						qDebug() << "Ds" << data.node << edge << _graph->GetOriginalEdgeSource( edge ) << penaltyTable.GetOutDegree() << _graph->GetOriginalOutDegree( data.node );
						qDebug() << "Ds" << _graph->DebugStringEdgesOf( data.node ).c_str();
					}
					#endif
					assert( _graph->GetOriginalEdgeSource( edge ) < penaltyTable.GetOutDegree() );
					const _PenaltyData penalty = penaltyTable.GetData( data.originalEdge, _graph->GetOriginalEdgeSource( edge ) );
					if (penalty == RESTRICTED_TURN)
						continue;
					const int toDistance = distance + penalty + edgeData.distance;
						  const unsigned toOriginalEdge = _graph->GetFirstOriginalEdge(to) + _graph->GetOriginalEdgeTarget( edge );
						  _HeapData toData( to, data.numOriginalEdges + edgeData.originalEdges, _graph->GetOriginalEdgeTarget( edge ), toOnlyViaContracted );
						  //qDebug() << " relax" << "---" << _graph->GetOriginalEdgeTarget( edge ) << "->" << to << ":" << distance << "+" << penalty << "+" << edgeData.distance;

					//New Node discovered -> Add to Heap + Node Info Storage
					if ( !heap.WasInserted( toOriginalEdge ) ) {
						heap.Insert( toOriginalEdge, toDistance, toData );
						numOnlyViaContracted += toOnlyViaContracted;

					//Found a shorter Path -> Update distance
					} else if ( toDistance < heap.GetKey( toOriginalEdge ) ) {
						heap.DecreaseKey( toOriginalEdge, toDistance );
						_HeapData& existingData = heap.GetData( toOriginalEdge );
						numOnlyViaContracted -= existingData.onlyViaContracted;
						numOnlyViaContracted += toOnlyViaContracted;
						existingData = toData;
					}
				}
			}
		}

		double _Evaluate( _ThreadData* const data, _PriorityData* const nodeData, NodeID node ){
			_ContractionInformation stats;

			//perform simulated contraction
			_Contract< true > ( data, node, &stats );

			// Result will contain the priority
			double result;
			if ( stats.edgesDeleted == 0 || stats.originalEdgesDeleted == 0 )
				result = 1 * nodeData->depth;
			else
				result =  2 * ((( double ) stats.edgesAdded ) / stats.edgesDeleted ) + 1 * ((( double ) stats.originalEdgesAdded ) / stats.originalEdgesDeleted ) + 1 * nodeData->depth;
			assert( result >= 0 );
			return result;
		}

		void _Dijkstra2( unsigned numIn, unsigned maxNodes, _ThreadData* const thread_data ){

			_Heap& heap = thread_data->aggressiveHeap;

			unsigned nodes = 0;
			unsigned in = 0;
			while ( heap.Size() > 0 ) {
				const NodeID originalEdge = heap.DeleteMin();
				const int distance = heap.GetKey( originalEdge );

				if ( distance == std::numeric_limits< int >::max() )
					continue;

				const _HeapData data = heap.GetData( originalEdge );  // no reference, as heap size may change below

				nodes++;
				if ( nodes == maxNodes )
					return;

				if ( data.onlyViaContracted ) {
					in++;
					if ( in == numIn )
						return;
				}

				_DynamicGraph::PenaltyTable penaltyTable = _graph->GetPenaltyTable( data.node );

				//iterate over all edges of data.node
				for ( _DynamicGraph::EdgeIterator edge = _graph->BeginEdges( data.node ), endEdges = _graph->EndEdges( data.node ); edge != endEdges; ++edge ) {
					const _EdgeData& edgeData = _graph->GetEdgeData( edge );
					if ( !edgeData.forward )
						continue;
					const NodeID to = _graph->GetTarget( edge );
					assert( data.originalEdge < penaltyTable.GetInDegree() );

					assert( _graph->GetOriginalEdgeSource( edge ) < penaltyTable.GetOutDegree() );
					const _PenaltyData penalty = penaltyTable.GetData( data.originalEdge, _graph->GetOriginalEdgeSource( edge ) );
					if (penalty == RESTRICTED_TURN)
						continue;
					const int toDistance = distance + penalty + edgeData.distance;
					const unsigned toOriginalEdge = _graph->GetFirstOriginalEdge(to) + _graph->GetOriginalEdgeTarget( edge );
					_HeapData toData( to, data.numOriginalEdges + edgeData.originalEdges, _graph->GetOriginalEdgeTarget( edge ), 0 );

					//New Node discovered -> Add to Heap + Node Info Storage
					if ( !heap.WasInserted( toOriginalEdge ) ) {
						heap.Insert( toOriginalEdge, toDistance, toData );

					//Found a shorter Path -> Update distance
					} else if ( toDistance < heap.GetKey( toOriginalEdge ) ) {
						heap.DecreaseKey( toOriginalEdge, toDistance );
						_HeapData& existingData = heap.GetData( toOriginalEdge );
						toData.onlyViaContracted = existingData.onlyViaContracted;
						existingData = toData;
					}
				}
			}
		}

		bool shortcutNecessary( _DynamicGraph::EdgeIterator inEdge, _DynamicGraph::EdgeIterator outEdge, int shortcutDistance, _ThreadData* const thread_data )
		{
			_Heap& heap = thread_data->aggressiveHeap;

			NodeID source = _graph->GetTarget( inEdge );
			NodeID target = _graph->GetTarget( outEdge );
			const _DynamicGraph::PenaltyTable& sourceTable = _graph->GetPenaltyTable( source );
			const _DynamicGraph::PenaltyTable& targetTable = _graph->GetPenaltyTable( target );

			for ( unsigned preID = 0; preID < sourceTable.GetInDegree(); preID ++ ) {

				_PenaltyData prePenalty = sourceTable.GetData( preID, _graph->GetOriginalEdgeTarget( inEdge ) );
				if ( prePenalty == RESTRICTED_TURN )
					continue;

				int pathDistance = prePenalty + shortcutDistance;

				{
					heap.Clear();
					unsigned edgeID = _graph->GetFirstOriginalEdge( source ) + preID;
					_HeapData heapData( source, 0, preID, false );
					heap.Insert( edgeID, 0, heapData );
				}

				unsigned numIn = 0;
				for ( _DynamicGraph::EdgeIterator targetEdge = _graph->BeginEdges( target ); targetEdge < _graph->EndEdges( target ); targetEdge++ ) {
					if ( !_graph->GetEdgeData( targetEdge ).backward )
						continue;
					unsigned edgeID = _graph->GetFirstOriginalEdge( target ) + _graph->GetOriginalEdgeSource( targetEdge );
					_HeapData heapData( target, 0, _graph->GetOriginalEdgeSource( targetEdge ), true );
					if ( !heap.WasInserted( edgeID ) ) {
						numIn++;
						heap.Insert( edgeID, std::numeric_limits< int >::max(), heapData );
					}
				}

				_Dijkstra2( numIn, 2000, thread_data );

				unsigned targetID = _graph->GetOriginalEdgeTarget( outEdge );
				for ( unsigned postID = 0; postID < targetTable.GetOutDegree(); postID++ ) {
					_PenaltyData referencePenalty = targetTable.GetData( targetID, postID );
					if ( referencePenalty == RESTRICTED_TURN )
						continue;

					int referenceDistance = pathDistance + referencePenalty;
					int bestDistance = std::numeric_limits< int >::max();
					for ( unsigned postInID = 0; postInID < targetTable.GetInDegree(); postInID++ ) {
						unsigned edgeID = _graph->GetFirstOriginalEdge( target ) + postInID;
						if ( !heap.WasInserted( edgeID ) )
							continue;

						_PenaltyData newPenalty = targetTable.GetData( postInID, postID );
						if ( newPenalty == RESTRICTED_TURN )
							continue;

						int edgeKey = heap.GetKey( edgeID );
						if ( edgeKey == std::numeric_limits< int >::max() )
							continue;

						int newDistance = edgeKey + newPenalty;
						if ( newDistance < bestDistance )
							bestDistance = newDistance;
					}

					if ( bestDistance >= referenceDistance )
						return true;
				}
			}
			//if ( sourceTable.GetInDegree() == 0 || targetTable.GetOutDegree() == 0 )
			//	return true;
			return false;
		}


		template< bool Simulate > bool _Contract( _ThreadData* const data, const NodeID node, _ContractionInformation* const stats = NULL ) {
			_Heap& heap = data->heap;
			//std::vector< Witness >& witnessList = data->witnessList;
			int insertedEdgesSize = data->insertedEdges.size();
			std::vector< _ImportEdge >& insertedEdges = data->insertedEdges;

			#ifndef NDEBUG
			if (node == SPECIAL_NODE) qDebug() << _graph->DebugStringEdgesOf( node ).c_str();
			#endif


			for ( _DynamicGraph::EdgeIterator inEdge = _graph->BeginEdges( node ), endInEdges = _graph->EndEdges( node ); inEdge != endInEdges; ++inEdge ) {
				const _EdgeData& inData = _graph->GetEdgeData( inEdge );
				const NodeID source = _graph->GetTarget( inEdge );
				if ( Simulate ) {
					assert( stats != NULL );
					stats->edgesDeleted++;
					stats->originalEdgesDeleted += inData.originalEdges;
				}
				if ( !inData.backward || source == node )
					continue;

				#ifndef NDEBUG
				if (node == SPECIAL_NODE) {
					qDebug() << "in " << _graph->DebugStringEdge(inEdge).c_str();
					qDebug() << _graph->DebugStringEdgesOf( source ).c_str();
					qDebug() << _graph->DebugStringPenaltyTable( source ).c_str();
					qDebug() << _DebugStringGammaForward( source ).c_str();
				}
				#endif

				// Prepare witness search.
					 // -----------------------
					 heap.Clear();
					 int numOnlyViaContracted = 0;
					 const unsigned inOut = _graph->GetOriginalEdgeTarget( inEdge );
					 const unsigned sourceOutDegree = _graph->GetOriginalOutDegree( source );
					 assert( inOut < sourceOutDegree );
					 {
					const unsigned gammaIndexDelta = _gammaIndex[source].forward + sourceOutDegree * inOut;
					for ( _DynamicGraph::EdgeIterator sourceEdge = _graph->BeginEdges( source ), endSourceEdges = _graph->EndEdges( source ); sourceEdge != endSourceEdges; ++sourceEdge ) {
						const _EdgeData& sourceData = _graph->GetEdgeData( sourceEdge );
						if ( !sourceData.forward )
							continue;

						const unsigned sourceOut = _graph->GetOriginalEdgeSource( sourceEdge );
						GammaValue g = _gamma[ gammaIndexDelta + sourceOut ];
						if (g == RESTRICTED_NEIGHBOUR)
							continue;

						const NodeID sourceTarget = _graph->GetTarget( sourceEdge );
						const bool isViaNode = (sourceTarget == node && sourceOut == inOut);

						assert( _graph->GetOriginalEdgeTarget( sourceEdge ) < _graph->GetOriginalInDegree( sourceTarget ) );
						const unsigned sourceOriginalTarget = _graph->GetFirstOriginalEdge( sourceTarget ) + _graph->GetOriginalEdgeTarget( sourceEdge );
						const int pathDistance = (int)sourceData.distance + g;
						#ifndef NDEBUG
						if (node == SPECIAL_NODE) {
							qDebug() << sourceOriginalTarget << "-" << _graph->GetOriginalEdgeSource( sourceEdge ) << "--->" << sourceTarget << ":" << sourceData.distance << "+" << g << "vc" << isViaNode;
							qDebug() << _graph->GetFirstOriginalEdge(sourceTarget) << _graph->GetOriginalInDegree(sourceTarget);
						}
						#endif
						_HeapData heapData( sourceTarget, sourceData.originalEdges, _graph->GetOriginalEdgeTarget( sourceEdge ), isViaNode);
						if ( !heap.WasInserted( sourceOriginalTarget ) ) {
							heap.Insert( sourceOriginalTarget, pathDistance, heapData );
							numOnlyViaContracted += isViaNode;
						}
						else if ( pathDistance < heap.GetKey( sourceOriginalTarget ) ) {
							heap.DecreaseKey( sourceOriginalTarget, pathDistance );
							_HeapData& existingData = heap.GetData( sourceOriginalTarget );
							numOnlyViaContracted -= existingData.onlyViaContracted;
							numOnlyViaContracted += isViaNode;
							existingData = heapData;
						}
					}
					 }

					 // Run witness search.
					 // -------------------
				if ( Simulate )
					_Dijkstra( node, numOnlyViaContracted, 1000, data );
				else
					_Dijkstra( node, numOnlyViaContracted, 2000, data );

				// Evaluate witness search.
					 // ------------------------
				for ( _DynamicGraph::EdgeIterator outEdge = _graph->BeginEdges( node ),
						endOutEdges = _graph->EndEdges( node ); outEdge != endOutEdges; ++outEdge ) {
					const _EdgeData& outData = _graph->GetEdgeData( outEdge );
					const NodeID target = _graph->GetTarget( outEdge );
					if ( !outData.forward || target == node )
						continue;

					#ifndef NDEBUG
					if (node == SPECIAL_NODE) {
						qDebug() << "out " << _graph->DebugStringEdge(outEdge).c_str();
						qDebug() << _graph->DebugStringEdgesOf( target ).c_str();
						qDebug() << _graph->DebugStringPenaltyTable( target ).c_str();
						qDebug() << _DebugStringGammaBackward( target ).c_str();
					}
					#endif

						  const unsigned outIn = _graph->GetOriginalEdgeTarget( outEdge );
					const unsigned targetInDegree = _graph->GetOriginalInDegree( target );
					const unsigned targetFirstOriginal = _graph->GetFirstOriginalEdge( target );
					const unsigned gammaIndexDelta = _gammaIndex[target].backward + targetInDegree * outIn;
					assert( outIn < targetInDegree );
					const unsigned originalEdgeTarget = targetFirstOriginal + outIn;
					// A shortcut is only potentially necessary if the path found to 'originalEdgeTarget'
					// is only via the contracted node.
						  #ifndef NDEBUG
					if (node == SPECIAL_NODE) {
							  if (!heap.WasInserted( originalEdgeTarget )) qDebug() << "out not in heap";
							  else if (!heap.GetData( originalEdgeTarget ).onlyViaContracted) {
									qDebug() << "not via contracted" << heap.GetKey( originalEdgeTarget ) << heap.GetData( originalEdgeTarget ).DebugString().c_str();
							  }
					}
						  #endif
					if ( !heap.WasInserted( originalEdgeTarget ) || !heap.GetData( originalEdgeTarget ).onlyViaContracted )
						continue;
					int shortcutDistance = heap.GetKey( originalEdgeTarget );
					assert( shortcutDistance >= 0 );
					bool needShortcut = true;

					for ( unsigned i = 0; i < targetInDegree; ++i ) {
						GammaValue g = _gamma[ gammaIndexDelta + i ];
						if ( g == RESTRICTED_NEIGHBOUR || i == outIn )
							continue;
						unsigned originalEdgeTarget2 = targetFirstOriginal + i;
						if ( heap.WasInserted( originalEdgeTarget2 )
								&& (heap.GetKey( originalEdgeTarget2 ) + g) < shortcutDistance ) {
							#ifndef NDEBUG
							if (node == SPECIAL_NODE) qDebug() << "dont need shortcut other in" << i << "gamma" << g;
							#endif
							needShortcut = false;
							break;
						}
					}

					// Loop avoidance.
					if ( needShortcut && source == target ) {
						needShortcut = false;
						// Decide locally whether the loop makes sense.
						_DynamicGraph::PenaltyTable penaltyTable = _graph->GetPenaltyTable( source );
						assert( sourceOutDegree == penaltyTable.GetOutDegree() );
						assert( targetInDegree == penaltyTable.GetInDegree() );
						for ( unsigned in = 0; in < targetInDegree && !needShortcut; ++in ) {
							for ( unsigned out = 0; out < sourceOutDegree && !needShortcut; ++out ) {
								_PenaltyData turn = penaltyTable.GetData( in, out );
								_PenaltyData turn1 = penaltyTable.GetData( in, inOut );
								_PenaltyData turn2 = penaltyTable.GetData( outIn, out );
								// Check whether 'turn' is worse than 'turn1' followed by 'turn2'.
								if ( ( turn == RESTRICTED_TURN && turn1 != RESTRICTED_TURN && turn2 != RESTRICTED_TURN ) ||
										turn1 + shortcutDistance + turn2 < turn ) {
									needShortcut = true;
								}
							}
						}
					}


					if ( needShortcut ) {

						#ifndef NDEBUG
						if (node == SPECIAL_NODE) qDebug() << "shortcut necessary" << source << "<-" << inOut << "---" << shortcutDistance << "---" << outIn << "->" << target;
						#endif
						const unsigned shortcutOriginalEdges = heap.GetData( originalEdgeTarget ).numOriginalEdges;
						if ( !Simulate &&  m_aggressive ) {
							needShortcut = shortcutNecessary( inEdge, outEdge, shortcutDistance, data );
							if ( !needShortcut )
								m_savedShortcuts++;
						}
						if ( needShortcut ) {
							if ( Simulate ) {
								assert( stats != NULL );
								stats->edgesAdded += 2;
								stats->originalEdgesAdded += 2 * ( shortcutOriginalEdges );
							} else {
								_ImportEdge newEdge;
								newEdge.source = source;
								newEdge.target = target;
								newEdge.originalEdgeSource = inOut;
								newEdge.originalEdgeTarget = outIn;
								newEdge.data.distance = shortcutDistance;
								newEdge.data.forward = true;
								newEdge.data.backward = false;
								newEdge.data.middle = node;
								newEdge.data.shortcut = true;
								newEdge.data.originalEdges = shortcutOriginalEdges;
								#ifndef NDEBUG
								if (source == SPECIAL_NODE || target == SPECIAL_NODE) qDebug() << "add shortcut" << newEdge.DebugString().c_str();
								#endif
								insertedEdges.push_back( newEdge );
								std::swap( newEdge.source, newEdge.target );
								std::swap( newEdge.originalEdgeSource, newEdge.originalEdgeTarget );
								newEdge.data.forward = false;
								newEdge.data.backward = true;
								insertedEdges.push_back( newEdge );
							}
						}
					}  // if needShortcut
				}  // foreach outEdge
					 //exit(1);

			}  // foreach inEdge

			// Merge bidirectional edges.
			if ( !Simulate ) {
				for ( int i = insertedEdgesSize, iend = insertedEdges.size(); i < iend; i++ ) {
					bool found = false;
					for ( int other = i + 1 ; other < iend ; ++other ) {
						if ( insertedEdges[other].source != insertedEdges[i].source )
							continue;
						if ( insertedEdges[other].target != insertedEdges[i].target )
							continue;
						if ( insertedEdges[other].originalEdgeSource != insertedEdges[i].originalEdgeSource )
							continue;
						if ( insertedEdges[other].originalEdgeTarget != insertedEdges[i].originalEdgeTarget )
							continue;
						if ( insertedEdges[other].data.distance != insertedEdges[i].data.distance )
							continue;
						assert( insertedEdges[other].data.shortcut == insertedEdges[i].data.shortcut );
						insertedEdges[other].data.forward |= insertedEdges[i].data.forward;
						insertedEdges[other].data.backward |= insertedEdges[i].data.backward;
						found = true;
						break;
					}
					if ( !found ) {
						assert( !insertedEdges[i].data.forward || insertedEdges[i].originalEdgeSource < _graph->GetOriginalOutDegree( insertedEdges[i].source ) );
						assert( !insertedEdges[i].data.forward || insertedEdges[i].originalEdgeTarget < _graph->GetOriginalInDegree( insertedEdges[i].target ) );
						assert( !insertedEdges[i].data.backward || insertedEdges[i].originalEdgeSource < _graph->GetOriginalInDegree( insertedEdges[i].source ) );
						assert( !insertedEdges[i].data.backward || insertedEdges[i].originalEdgeTarget < _graph->GetOriginalOutDegree( insertedEdges[i].target ) );
						insertedEdges[insertedEdgesSize++] = insertedEdges[i];
					}
				}
				insertedEdges.resize( insertedEdgesSize );
			}

			return true;
		}

		int _DeleteIncomingEdges( _ThreadData* const data, NodeID node ) {
			std::vector< NodeID >& neighbours = data->neighbours;
			neighbours.clear();

			//find all neighbours
			for ( _DynamicGraph::EdgeIterator e = _graph->BeginEdges( node ) ; e < _graph->EndEdges( node ) ; ++e ) {
				const NodeID u = _graph->GetTarget( e );
				if ( u == node )
					continue;
				neighbours.push_back( u );
			}
			//eliminate duplicate entries ( forward + backward edges )
			std::sort( neighbours.begin(), neighbours.end() );
			neighbours.resize( std::unique( neighbours.begin(), neighbours.end() ) - neighbours.begin() );

			int deleted = 0;
			for ( int i = 0, e = ( int ) neighbours.size(); i < e; ++i ) {
				const NodeID u = neighbours[i];
				deleted += _graph->DeleteEdgesTo( u, node );
			}

			return deleted;
		}

		bool _UpdateNeighbours( std::vector< double >* priorities, std::vector< _PriorityData >* const nodeData, _ThreadData* const data, NodeID node ) {
			std::vector< NodeID >& neighbours = data->neighbours;
			neighbours.clear();

			//find all neighbours
			for ( _DynamicGraph::EdgeIterator e = _graph->BeginEdges( node ) ; e < _graph->EndEdges( node ) ; ++e ) {
				const NodeID u = _graph->GetTarget( e );
				if ( u == node )
					continue;
				neighbours.push_back( u );
				( *nodeData )[u].depth = std::max(( *nodeData )[node].depth + 1, ( *nodeData )[u].depth );
			}
			//eliminate duplicate entries ( forward + backward edges )
			std::sort( neighbours.begin(), neighbours.end() );
			neighbours.resize( std::unique( neighbours.begin(), neighbours.end() ) - neighbours.begin() );

			for ( int i = 0, e = ( int ) neighbours.size(); i < e; ++i ) {
				const NodeID u = neighbours[i];
				( *priorities )[u] = _Evaluate( data, &( *nodeData )[u], u );
			}

			return true;
		}

		bool _IsIndependent( const std::vector< double >& priorities, const std::vector< _PriorityData >& nodeData, _ThreadData* const data, NodeID node ) {
			const double priority = priorities[node];

			std::vector< NodeID >& neighbours = data->neighbours;
			neighbours.clear();

			for ( _DynamicGraph::EdgeIterator e = _graph->BeginEdges( node ) ; e < _graph->EndEdges( node ) ; ++e ) {
				const NodeID target = _graph->GetTarget( e );
				const double targetPriority = priorities[target];
				assert( targetPriority >= 0 );
				//found a neighbour with lower priority?
				if ( priority > targetPriority )
					return false;
				//tie breaking
				if ( priority == targetPriority && nodeData[node].bias < nodeData[target].bias )
					return false;
				neighbours.push_back( target );
			}

			std::sort( neighbours.begin(), neighbours.end() );
			neighbours.resize( std::unique( neighbours.begin(), neighbours.end() ) - neighbours.begin() );

			//examine all neighbours that are at most 2 hops away
			for ( std::vector< NodeID >::const_iterator i = neighbours.begin(), lastNode = neighbours.end(); i != lastNode; ++i ) {
				const NodeID u = *i;

				for ( _DynamicGraph::EdgeIterator e = _graph->BeginEdges( u ) ; e < _graph->EndEdges( u ) ; ++e ) {
					const NodeID target = _graph->GetTarget( e );

					assert( target < priorities.size() );
					const double targetPriority = priorities[target];
					assert( targetPriority >= 0 );
					//found a neighbour with lower priority?
					if ( priority > targetPriority )
						return false;
					//tie breaking
					if ( priority == targetPriority && nodeData[node].bias < nodeData[target].bias )
						return false;
				}
			}

			return true;
		}

		void _CheckRemainingEdges( const std::vector< std::pair< NodeID, bool > >& remainingNodes, unsigned firstIndependent, int numRemainingEdges ) const
		{
			int sum = 0;
			for (unsigned i = 0; i < firstIndependent; ++i)
			{
				NodeID x = remainingNodes[i].first;
				sum += _graph->GetOutDegree(x);
			}
			if (sum != numRemainingEdges)
			{
				qDebug() << sum << "!=" << numRemainingEdges;
				exit(1);
			}
		}

		std::string _DebugStringGammaForward( NodeID n ) const {
			std::stringstream ss;
			GammaIndexItem ind = _gammaIndex[ n ];
			ss << "forward/out\n";
				unsigned outDegree = _graph->GetOriginalOutDegree( n );

				for ( unsigned out2 = 0; out2 < outDegree; ++out2 ) {
					ss << "\t" << out2;
				}
				ss << "\n";
				for ( unsigned out = 0; out < outDegree; ++out ) {
					ss << out;
					 for ( unsigned out2 = 0; out2 < outDegree; ++out2 ) {
						  ss << "\t" << _gamma[ ind.forward + outDegree * out + out2 ];
					 }
					 ss << "\n";
				}
				return ss.str();
		}
		std::string _DebugStringGammaBackward( NodeID n ) const {
			std::stringstream ss;
			GammaIndexItem ind = _gammaIndex[ n ];
				unsigned inDegree = _graph->GetOriginalInDegree( n );
				ss << "\nbackward/in\n";
				for ( unsigned in2 = 0; in2 < inDegree; ++in2 ) {
					ss << "\t" << in2;
				}
				ss << "\n";
				for ( unsigned in = 0; in < inDegree; ++in ) {
					ss << in;
					for ( unsigned in2 = 0; in2 < inDegree; ++in2 ) {
						  ss << "\t" << _gamma[ ind.backward + inDegree * in + in2 ];
					 }
					 ss << "\n";
				}
				return ss.str();
		}



		_DynamicGraph* _graph;
		std::vector< Witness > _witnessList;
		std::vector< _ImportEdge > _loops;

		std::vector< GammaIndexItem > _gammaIndex;
		std::vector< GammaValue > _gamma;

};

#ifndef NDEBUG
const unsigned TurnContractor::SPECIAL_NODE;
#endif
const TurnContractor::_PenaltyData TurnContractor::RESTRICTED_TURN;
const TurnContractor::GammaValue TurnContractor::RESTRICTED_NEIGHBOUR;

#endif // TURNCONTRACTOR_H_INCLUDED
