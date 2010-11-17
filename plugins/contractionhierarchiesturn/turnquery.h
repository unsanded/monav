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

#ifndef TURNQUERY_H_INCLUDED
#define TURNQUERY_H_INCLUDED
#include <vector>
#include <omp.h>
#include <limits.h>
#include <limits>
#include <sstream>
#include "utils/qthelpers.h"
#include "dynamicturngraph.h"
#include "../contractionhierarchies/binaryheap.h"
#include "utils/config.h"

template <typename Graph>
class TurnQuery {
public:
	typename Graph::NodeIterator NodeIterator;
	typename Graph::EdgeIterator EdgeIterator;
	typename Graph::EdgeData EdgeData;
	typename Graph::PenaltyData PenaltyData;

	TurnQuery(const Graph& graph) : m_graph( graph ), m_heapForward( graph.GetNumberOfOriginalEdges() ),
			m_heapBackward( graph.GetNumberOfOriginalEdges() ) {}

protected:
	const Graph& m_graph;

	struct HeapData {
		NodeIterator parent;
		bool stalled: 1;
		HeapData( NodeIterator p ) {
			parent = p;
			stalled = false;
		}
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

	typedef BinaryHeap< NodeIterator, int, int, HeapData > Heap;

	Heap m_heapForward;
	Heap m_heapBackward;

	template <class EdgeAllowed>
	void initHeap( Heap* heap, const NodeIterator node, const unsigned originalEdge, const EdgeAllowed& edgeAllowed ) {
		for ( EdgeIterator edge = m_graph.BeginEdges( node ), edgeEnd = m_graph.EndEdges( node ); edge < edgeEnd; ++edge ) {
			const EdgeData& edgeData = m_graph.GetEdgeData( edge );
			const NodeIterator edgeTarget = m_graph.GetTarget( edge );
			if ( edgeAllowed(edgeData.forward, edgeData.backward) != forward || m_graph.GetOriginalEdgeSource( edge ) == originalEdge )
				continue;

			const unsigned sourceOriginalTarget = m_graph->GetFirstOriginalEdge( edgeTarget ) + _graph->GetOriginalEdgeTarget( edge );
			if ( !heap->WasInserted( sourceOriginalTarget ) ) {
				heap->Insert( sourceOriginalTarget, edgeData.distance, -1 );
			}
			else if ( edgeData.distance < heap->GetKey( sourceOriginalTarget ) ) {
				heap->DecreaseKey( sourceOriginalTarget, edgeData.distance );
			}
		}
	}

public:
	int computeRoute( const NodeIterator source, const NodeIterator target, const unsigned originalEdgeSource, const unsigned originalEdgeTarget ) {
		assert( source < m_graph.GetNumberOfNodes() );
		assert( target < m_graph.GetNumberOfNodes() );
		assert( originalEdgeSource < m_graph.GetOriginalOutDegree( source ) );
		assert( originalEdgeTarget < m_graph.GetOriginalInDegree( target ) );

		AllowForwardEdge forward;
		AllowBackwardEdge backward;
		initHeap( &m_heapForward, source, originalEdgeSource, forward );
		initHeap( &m_heapBackward, target, originalEdgeTarget, backward );

		int targetDistance = std::numeric_limits< int >::max();
		NodeIterator middle = ( NodeIterator ) 0;
		AllowForwardEdge forward;
		AllowBackwardEdge backward;

		while ( m_heapForward->Size() + m_heapBackward->Size() > 0 ) {

			if ( m_heapForward->Size() > 0 )
				computeStep( m_heapForward, m_heapBackward, forward, backward, &middle, &targetDistance );

			if ( m_heapBackward->Size() > 0 )
				computeStep( m_heapBackward, m_heapForward, backward, forward, &middle, &targetDistance );

		}

		if ( targetDistance == std::numeric_limits< int >::max() )
			return std::numeric_limits< int >::max();

		std::stack< NodeIterator > stack;
		NodeIterator pathNode = middle;
		while ( true ) {
			NodeIterator parent = m_heapForward->GetData( pathNode ).parent;
			stack.push( pathNode );
			if ( parent == pathNode )
				break;
			pathNode = parent;
		}

		pathNodes->push_back( source.nearestPoint );
		bool reverseSourceDescription = pathNode != source.target;
		if ( source.source == source.target && sourceEdge.backward() && sourceEdge.forward() && source.percentage < 0.5 )
			reverseSourceDescription = !reverseSourceDescription;
		if ( sourceEdge.unpacked() ) {
			bool unpackSourceForward = source.target != sourceEdge.target() ? reverseSourceDescription : !reverseSourceDescription;
			m_graph.path( sourceEdge, pathNodes, pathEdges, unpackSourceForward );
			if ( reverseSourceDescription ) {
				pathNodes->remove( 1, pathNodes->size() - 1 - source.previousWayCoordinates );
			} else {
				pathNodes->remove( 1, source.previousWayCoordinates - 1 );
			}
		} else {
			pathNodes->push_back( m_graph.node( pathNode ) );
			pathEdges->push_back( sourceEdge.description() );
		}
		pathEdges->front().length = pathNodes->size() - 1;
		pathEdges->front().seconds *= reverseSourceDescription ? source.percentage : 1 - source.percentage;

		while ( stack.size() > 1 ) {
			const NodeIterator node = stack.top();
			stack.pop();
			unpackEdge( node, stack.top(), true, pathNodes, pathEdges );
		}

		pathNode = middle;
		while ( true ) {
			NodeIterator parent = m_heapBackward->GetData( pathNode ).parent;
			if ( parent == pathNode )
				break;
			unpackEdge( parent, pathNode, false, pathNodes, pathEdges );
			pathNode = parent;
		}

		int begin = pathNodes->size();
		bool reverseTargetDescription = pathNode != target.source;
		if ( target.source == target.target && targetEdge.backward() && targetEdge.forward() && target.percentage > 0.5 )
			reverseTargetDescription = !reverseTargetDescription;
		if ( targetEdge.unpacked() ) {
			bool unpackTargetForward = target.target != targetEdge.target() ? reverseTargetDescription : !reverseTargetDescription;
			m_graph.path( targetEdge, pathNodes, pathEdges, unpackTargetForward );
			if ( reverseTargetDescription ) {
				pathNodes->resize( pathNodes->size() - target.previousWayCoordinates );
			} else {
				pathNodes->resize( begin + target.previousWayCoordinates - 1 );
			}
		} else {
			pathEdges->push_back( targetEdge.description() );
		}
		pathNodes->push_back( target.nearestPoint );
		pathEdges->back().length = pathNodes->size() - begin;
		pathEdges->back().seconds *= reverseTargetDescription ? 1 - target.percentage : target.percentage;

		return targetDistance;
	}

};

#endif // TURNQUERY_H_INCLUDED
