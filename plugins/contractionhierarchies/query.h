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

#ifndef QUERY_H_INCLUDED
#define QUERY_H_INCLUDED
#include <vector>
#include <omp.h>
#include <limits.h>
#include <limits>
#include <sstream>
#include <queue>
#include "utils/qthelpers.h"
#include "dynamicgraph.h"
#include "binaryheap.h"
#include "utils/config.h"

template <typename Graph, bool StallOnDemand = true >
class Query {
public:
	typedef typename Graph::NodeIterator NodeIterator;
	typedef typename Graph::EdgeIterator EdgeIterator;
	typedef typename Graph::EdgeData EdgeData;

	Query(const Graph& graph) : m_graph( graph ), m_heapForward( graph.GetNumberOfNodes() ) ,
			m_heapBackward( graph.GetNumberOfNodes() ) {}

public:
	const Graph& m_graph;


	struct HeapData {
		NodeIterator parent;
		bool stalled: 1;
		HeapData( NodeIterator p ) : parent(p), stalled(false) {}
	};

	class AllowForwardEdge {
		public:
			bool operator()( bool forward, bool /*backward*/ ) const {
				return forward;
			}
	};

	class AllowBackwardEdge {
		public:
			bool operator()( bool /*forward*/, bool backward ) const {
				return backward;
			}
	};


	typedef BinaryHeap< NodeIterator, unsigned, int, HeapData > Heap;

	Heap m_heapForward;
	Heap m_heapBackward;

	std::queue< NodeIterator > m_stallQueue;

	#ifdef QUERY_COUNT
	struct Counter {
		unsigned long long queries;
		unsigned long long settledNodes;
		unsigned long long stalledNodes;
		unsigned long long scannedEdges;
		unsigned long long relaxedEdges;
		unsigned long long insertHeap;
		unsigned long long updateHeap;
		unsigned long long stallOps;
		unsigned long long stallSteps;
		Counter()  {
			Clear();
		}
		void Clear() {
			queries = 0;
			settledNodes = 0;
			stalledNodes = 0;
			scannedEdges = 0;
			relaxedEdges = 0;
			insertHeap = 0;
			updateHeap = 0;
			stallOps = 0;
			stallSteps = 0;
		}
		double avg(unsigned long long sum) const {
			if (queries == 0) return 0;
			return (double)sum/queries;
		}
		std::string DebugString() const {
			std::stringstream ss;
			ss << "#settled/stalled nodes = " << avg(settledNodes) << " / " << avg(stalledNodes) << "\n";
			ss << "#scanned/relaxed/insert/update edges = " << avg(scannedEdges) << " / " << avg(relaxedEdges) << " / " << avg(insertHeap) << " / " << avg(updateHeap) << "\n";
			ss << "#ops/steps stall = " << avg(stallOps) << " / " << avg(stallSteps);
			return ss.str();
		}
	} m_counter;
	#endif

	template< class EdgeAllowed, class StallEdgeAllowed >
	void computeStep( Heap* heapForward, Heap* heapBackward, const EdgeAllowed& edgeAllowed, const StallEdgeAllowed& stallEdgeAllowed, NodeIterator* middle, int* targetDistance ) {

		#ifdef QUERY_COUNT
		++m_counter.settledNodes;
		#endif

		const NodeIterator node = heapForward->DeleteMin();
		const int distance = heapForward->GetKey( node );

		if ( StallOnDemand && heapForward->GetData( node ).stalled ) {
			#ifdef QUERY_COUNT
			++m_counter.stalledNodes;
			#endif
			return;
		}

		if ( heapBackward->WasInserted( node ) && !heapBackward->GetData( node ).stalled ) {
			const int newDistance = heapBackward->GetKey( node ) + distance;
			if ( newDistance < *targetDistance ) {
				*middle = node;
				*targetDistance = newDistance;
			}
		}

		if ( distance > *targetDistance ) {
			heapForward->DeleteAll();
			return;
		}
		for ( EdgeIterator edge = m_graph.BeginEdges( node ), edgeEnd = m_graph.EndEdges( node ); edge < edgeEnd; ++edge ) {
			#ifdef QUERY_COUNT
			++m_counter.scannedEdges;
			#endif

			const NodeIterator to = m_graph.GetTarget( edge );
			const EdgeData& edgeData = m_graph.GetEdgeData( edge );
			const int edgeWeight = edgeData.distance;
			assert( edgeWeight > 0 );
			const int toDistance = distance + edgeWeight;

			if ( StallOnDemand && stallEdgeAllowed( edgeData.forward, edgeData.backward ) && heapForward->WasInserted( to ) ) {
				const int shorterDistance = heapForward->GetKey( to ) + edgeWeight;
				if ( shorterDistance < distance ) {
					//perform a bfs starting at node
					//only insert nodes when a sub-optimal path can be proven
					//insert node into the stall queue
					heapForward->GetKey( node ) = shorterDistance;
					heapForward->GetData( node ).stalled = true;
					m_stallQueue.push( node );

					#ifdef QUERY_COUNT
					++m_counter.stallOps;
					#endif

					while ( !m_stallQueue.empty() ) {
						#ifdef QUERY_COUNT
						++m_counter.stallSteps;
						#endif


						//get node from the queue
						const NodeIterator stallNode = m_stallQueue.front();
						m_stallQueue.pop();
						const int stallDistance = heapForward->GetKey( stallNode );

						//iterate over outgoing edges
						for ( EdgeIterator stallEdge = m_graph.BeginEdges( stallNode ), stallEdgeEnd = m_graph.EndEdges( stallNode ); stallEdge < stallEdgeEnd; ++stallEdge ) {
							const EdgeData& stallEdgeData = m_graph.GetEdgeData( stallEdge );
							//is edge outgoing/reached/stalled?
							if ( !edgeAllowed( stallEdgeData.forward, stallEdgeData.backward ) )
								continue;
							const NodeIterator stallTo = m_graph.GetTarget( stallEdge );
							if ( !heapForward->WasInserted( stallTo ) )
								continue;
							if ( heapForward->GetData( stallTo ).stalled == true )
								continue;

							const int stallToDistance = stallDistance + stallEdgeData.distance;
							//sub-optimal path found -> insert stallTo
							if ( stallToDistance < heapForward->GetKey( stallTo ) ) {
								if ( heapForward->WasRemoved( stallTo ) )
									heapForward->GetKey( stallTo ) = stallToDistance;
								else
									heapForward->DecreaseKey( stallTo, stallToDistance );

								m_stallQueue.push( stallTo );
								heapForward->GetData( stallTo ).stalled = true;
							}
						}
					}
					break;
				}
			}

			if ( edgeAllowed( edgeData.forward, edgeData.backward ) ) {

				#ifdef QUERY_COUNT
				++m_counter.relaxedEdges;
				#endif

				//New Node discovered -> Add to Heap + Node Info Storage
				if ( !heapForward->WasInserted( to ) ) {
					#ifdef QUERY_COUNT
					++m_counter.insertHeap;
					#endif
					heapForward->Insert( to, toDistance, node );

				//Found a shorter Path -> Update distance
				} else if ( toDistance <= heapForward->GetKey( to ) ) {
					#ifdef QUERY_COUNT
					++m_counter.updateHeap;
					#endif
					heapForward->DecreaseKey( to, toDistance );
					//new parent + unstall
					heapForward->GetData( to ).parent = node;
					heapForward->GetData( to ).stalled = false;
				}
			}
		}
	}

public:
	int BidirSearch( const NodeID source, const NodeID target ) {
		assert( source < m_graph.GetNumberOfNodes() );
		assert( target < m_graph.GetNumberOfNodes() );
		assert( m_heapForward.Size() == 0 );
		assert( m_heapBackward.Size() == 0 );

		#ifndef NDEBUG
//		qDebug() << source.DebugString().c_str() << "..." << target.DebugString().c_str();
		#endif

		#ifdef QUERY_COUNT
		++m_counter.queries;
		#endif

		//insert source into heap
		m_heapForward.Insert( source, 0, -1 );

		//insert target into heap
		m_heapBackward.Insert( target, 0, -1 );


		int targetDistance = std::numeric_limits< int >::max();
		NodeIterator middle = ( NodeIterator ) 0;
		AllowForwardEdge forward;
		AllowBackwardEdge backward;
		while ( m_heapForward.Size() + m_heapBackward.Size() > 0 ) {

			if ( m_heapForward.Size() > 0 )
				computeStep( &m_heapForward, &m_heapBackward, forward, backward, &middle, &targetDistance );

			if ( m_heapBackward.Size() > 0 )
				computeStep( &m_heapBackward, &m_heapForward, backward, forward, &middle, &targetDistance );

		}

		return targetDistance;
	}

	int UnidirSearch( const NodeID source, const NodeID target ) {
		assert( source < m_graph.GetNumberOfNodes() );
		assert( target < m_graph.GetNumberOfNodes() );
		assert( m_heapForward.Size() == 0 );
		assert( m_heapBackward.Size() == 0 );

		#ifndef NDEBUG
//		qDebug() << source.DebugString().c_str() << "..." << target.DebugString().c_str();
		#endif

		#ifdef QUERY_COUNT
		++m_counter.queries;
		#endif

		//insert source into heap
		m_heapForward.Insert( source, 0, -1 );

		//insert target into heap
		m_heapBackward.Insert( target, 0, -1 );


		int targetDistance = std::numeric_limits< int >::max();
		NodeIterator middle = ( NodeIterator ) 0;
		AllowForwardEdge forward;
		AllowBackwardEdge backward;
		while ( m_heapForward.Size() > 0 ) {

			if ( m_heapForward.Size() > 0 )
				computeStep( &m_heapForward, &m_heapBackward, forward, backward, &middle, &targetDistance );
		}

		return targetDistance;
	}

	void Clear() {
		m_heapForward.Clear();
		m_heapBackward.Clear();
	}

	#ifdef QUERY_COUNT
	void ClearCounter() {
		m_counter.Clear();
	}
	const Counter& GetCounter() const {
		return m_counter;
	}
	#endif

};

#endif // QUERY_H_INCLUDED
