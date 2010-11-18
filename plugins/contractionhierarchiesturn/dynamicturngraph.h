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

#ifndef DYNAMICTURNGRAPH_H_INCLUDED
#define DYNAMICTURNGRAPH_H_INCLUDED

#include <vector>
#include <algorithm>
#include <limits>
#include <sstream>
#include "utils/bithelpers.h"

template< typename _EdgeData, typename _PenaltyData >
class DynamicTurnGraph {
	public:
		typedef unsigned NodeIterator;
		typedef unsigned EdgeIterator;
		typedef _EdgeData EdgeData;
		typedef _PenaltyData PenaltyData;

		class InputNode {
			public:
				unsigned short inDegree;
				unsigned short outDegree;
				unsigned firstPenalty;
				std::string DebugString() const {
					std::stringstream ss;
					ss << "inDegree: " << inDegree << ", outDegree: " << outDegree << ", firstPenalty: " << firstPenalty;
					return ss.str();
				}
		};

		class InputEdge {
			public:
				NodeIterator source;
				NodeIterator target;
	            unsigned short originalEdgeSource; // uniquely identifies this edge among all outgoing edges adjacent to the source, at most degree( source ) - 1
	            unsigned short originalEdgeTarget; // uniquely identifies this edge among all incoming edges adjacent to the target, at most degree( target ) - 1
				EdgeData data;
				bool operator<( const InputEdge& right ) const {
					if ( source != right.source )
						return source < right.source;
					if ( originalEdgeSource != right.originalEdgeSource )
						return originalEdgeSource < right.originalEdgeSource;
					if ( target != right.target )
						return target < right.target;
					return originalEdgeTarget < right.originalEdgeTarget;
				}
				std::string DebugString() const {
					std::stringstream ss;
					ss << source << " <- " << originalEdgeSource << " --- " << originalEdgeTarget << " -> " << target << ": " << data.DebugString();
					return ss.str();
				}
		};

		DynamicTurnGraph( const std::vector< InputNode > &nodes, const std::vector< InputEdge > &graph,
				const std::vector< PenaltyData > &penalties ) {
			m_numNodes = nodes.size();
			m_numEdges = ( EdgeIterator ) graph.size();
			m_nodes.reserve( m_numNodes );
			m_nodes.resize( m_numNodes );
			EdgeIterator edge = 0;
			EdgeIterator position = 0;
			EdgeIterator sumInDegree = 0;
			EdgeIterator sumOutDegree = 0;
			EdgeIterator originalPosition = 0;
			for ( NodeIterator node = 0; node < m_numNodes; ++node ) {
				EdgeIterator lastEdge = edge;
				while ( edge < m_numEdges && graph[edge].source == node ) {
					++edge;
				}
				m_nodes[node].firstEdge = position;
				m_nodes[node].edges = edge - lastEdge;
                position += m_nodes[node].edges;

				m_nodes[node].inDegree = nodes[node].inDegree;
				m_nodes[node].outDegree = nodes[node].outDegree;
				m_nodes[node].firstPenalty = nodes[node].firstPenalty;
                m_nodes[node].firstOriginalEdge = originalPosition;
				sumInDegree += nodes[node].inDegree;
				sumOutDegree += nodes[node].outDegree;
				// Summing up the indegree(outdegre) allows to compute a globally unique
				// original-edge-id for incoming(outgoing) edges, max(in,out) allows to do both.
				originalPosition += std::max( nodes[node].inDegree, nodes[node].outDegree );
			}
			assert( m_numEdges == position );
			m_numOriginalEdges = originalPosition;
			m_edges.reserve( position * 1.2 );
			m_edges.resize( position );
			qDebug() << "Contraction Hiearchies Turn: dynamic graph usage:" << m_numEdges << m_edges.size() << m_edges.capacity();
			qDebug("in %u, out, %u, inout %u", sumInDegree, sumOutDegree, originalPosition);
			edge = 0;
			for ( NodeIterator node = 0; node < m_numNodes; ++node ) {
				for ( EdgeIterator i = m_nodes[node].firstEdge, e = m_nodes[node].firstEdge + m_nodes[node].edges; i != e; ++i ) {
					m_edges[i].target = graph[edge].target;
					m_edges[i].originalEdgeSource = graph[edge].originalEdgeSource;
					m_edges[i].originalEdgeTarget = graph[edge].originalEdgeTarget;
					m_edges[i].data = graph[edge].data;
					edge++;
				}
			}
			m_penalties.reserve( penalties.size() );
			for ( size_t i = 0; i < penalties.size(); ++i )
			{
				m_penalties.push_back( Penalty( penalties[i] ) );
			}
		}

		~DynamicTurnGraph()
		{
			qDebug() << "Contraction Hiearchies Turn: dynamic graph usage:" << m_numEdges << m_edges.size() << m_edges.capacity();
		}

		unsigned GetNumberOfNodes() const
		{
			return m_numNodes;
		}

		unsigned GetNumberOfEdges() const
		{
			return m_numEdges;
		}

		unsigned GetNumberOfOriginalEdges() const
		{
			return m_numOriginalEdges;
		}

		unsigned GetOutDegree( const NodeIterator &n ) const
		{
			assert( n < m_nodes.size() );
			return m_nodes[n].edges;
		}

		NodeIterator GetTarget( const EdgeIterator &e ) const
		{
			assert( e < m_edges.size() );
			return NodeIterator( m_edges[e].target );
		}

		unsigned GetOriginalEdgeSource( const EdgeIterator &e ) const
        {
			assert( e < m_edges.size() );
            return m_edges[e].originalEdgeSource;
        }
        unsigned GetOriginalEdgeTarget( const EdgeIterator &e ) const
        {
        	assert( e < m_edges.size() );
            return m_edges[e].originalEdgeTarget;
        }

		unsigned GetFirstOriginalEdge( const NodeIterator &n ) const
		{
			assert( n < m_nodes.size() );
			return m_nodes[n].firstOriginalEdge;
		}

		EdgeData &GetEdgeData( const EdgeIterator &e )
		{
			assert( e < m_edges.size() );
			return m_edges[e].data;
		}

		const EdgeData &GetEdgeData( const EdgeIterator &e ) const
		{
			assert( e < m_edges.size() );
			return m_edges[e].data;
		}

        unsigned GetOriginalInDegree( const NodeIterator &n ) const
        {
        	assert( n < m_nodes.size() );
        	return m_nodes[n].inDegree;
        }

        unsigned GetOriginalOutDegree( const NodeIterator &n ) const
        {
        	assert( n < m_nodes.size() );
        	return m_nodes[n].outDegree;
        }


        const PenaltyData &GetPenaltyData( const NodeIterator& n, unsigned short originalEdgeIn, unsigned short originalEdgeOut ) const
        {
        	assert( n < m_nodes.size() );
            unsigned first = m_nodes[n].firstPenalty;
            unsigned originalOutDegree = m_nodes[n].outDegree;
            assert( originalEdgeIn < m_nodes[n].inDegree );
            if ( originalEdgeOut >= originalOutDegree ) {
            	qDebug() << n << originalEdgeOut << originalOutDegree;
            }
            assert( originalEdgeOut < originalOutDegree );
            assert( first + (originalEdgeIn * originalOutDegree) + originalEdgeOut < m_penalties.size() );
            return m_penalties[ first + (originalEdgeIn * originalOutDegree) + originalEdgeOut ].data;
        }

        unsigned GetFirstPenalty( const NodeIterator& n ) const {
        	assert( n < m_nodes.size() );
            return m_nodes[n].firstPenalty;
        }

        unsigned GetNumberOfPenalties() const {
        	return m_penalties.size();
        }

        const PenaltyData &GetPenaltyDataWithIndex( unsigned i ) const {
        	assert ( i < m_penalties.size() );
        	return m_penalties[i].data;
        }

		EdgeIterator BeginEdges( const NodeIterator &n ) const
		{
			//assert( EndEdges( n ) - EdgeIterator( _nodes[n].firstEdge ) <= 100 );
			return EdgeIterator( m_nodes[n].firstEdge );
		}

		EdgeIterator EndEdges( const NodeIterator &n ) const
		{
			return EdgeIterator( m_nodes[n].firstEdge + m_nodes[n].edges );
		}

		//adds an edge. Invalidates edge iterators for the source node
		EdgeIterator InsertEdge( const InputEdge& newEdge )
		{
			assert( newEdge.source < m_nodes.size() );
			assert( newEdge.target < m_nodes.size() );
			Node &node = m_nodes[newEdge.source];
			EdgeIterator newFirstEdge = node.edges + node.firstEdge;
			if ( newFirstEdge >= m_edges.size() || !isDummy( newFirstEdge ) ) {
				if ( false && node.firstEdge != 0 && isDummy( node.firstEdge - 1 ) ) {
					node.firstEdge--;
					m_edges[node.firstEdge] = m_edges[node.firstEdge + node.edges];
				} else {
					EdgeIterator newFirstEdge = ( EdgeIterator ) m_edges.size();
					unsigned newSize = node.edges * 1.2 + 2;
					EdgeIterator requiredCapacity = newSize + m_edges.size();
					EdgeIterator oldCapacity = m_edges.capacity();
					if ( requiredCapacity >= oldCapacity ) {
						m_edges.reserve( requiredCapacity * 1.1 );
						qDebug() << "Contraction Hiearchies: increased graph size:" << m_edges.capacity();
					}
					m_edges.resize( m_edges.size() + newSize );
					for ( EdgeIterator i = 0; i < node.edges; ++i ) {
						m_edges[newFirstEdge + i ] = m_edges[node.firstEdge + i];
						makeDummy( node.firstEdge + i );
					}
					for ( EdgeIterator i = node.edges + 1; i < newSize; i++ )
						makeDummy( newFirstEdge + i );
					node.firstEdge = newFirstEdge;
				}
			}
			Edge &edge = m_edges[node.firstEdge + node.edges];
			edge.target = newEdge.target;
			edge.originalEdgeSource = newEdge.originalEdgeSource;
			edge.originalEdgeTarget = newEdge.originalEdgeTarget;
			edge.data = newEdge.data;
			m_numEdges++;
			node.edges++;
			return EdgeIterator( node.firstEdge + node.edges );
		}

		//removes an edge. Invalidates edge iterators for the source node
		void DeleteEdge( const NodeIterator source, const EdgeIterator &e ) {
			Node &node = m_nodes[source];
			--m_numEdges;
			--node.edges;
			const unsigned last = node.firstEdge + node.edges;
			//swap with last edge
			m_edges[e] = m_edges[last];
			makeDummy( last );
		}

		//removes all edges (source,target)
		int DeleteEdgesTo( const NodeIterator source, const NodeIterator target )
		{
			int deleted = 0;
			for ( EdgeIterator i = BeginEdges( source ), iend = EndEdges( source ); i < iend - deleted; ++i ) {
				if ( m_edges[i].target == target ) {
					do {
						deleted++;
						m_edges[i] = m_edges[iend - deleted];
						makeDummy( iend - deleted );
					} while ( i < iend - deleted && m_edges[i].target == target );
				}
			}

			#pragma omp atomic
			m_numEdges -= deleted;
			m_nodes[source].edges -= deleted;

			return deleted;
		}

		//searches for a specific edge
		EdgeIterator FindEdge( const NodeIterator &from, const NodeIterator &to, const unsigned originalEdgeSource, const unsigned originalEdgeTarget ) const
		{
			for ( EdgeIterator i = BeginEdges( from ), iend = EndEdges( from ); i != iend; ++i ) {
				const Edge& edge = m_edges[i];
				if ( edge.target == to
						&& edge.originalEdgeSource == originalEdgeSource
						&& edge.originalEdgeTarget == originalEdgeTarget ) {
					return i;
				}
			}
			return EndEdges( from );
		}

		//searches for a specific edge
		EdgeIterator FindOriginalEdge( const NodeIterator &from, const unsigned originalEdgeSource, bool forward ) const
		{

			for ( EdgeIterator i = BeginEdges( from ), iend = EndEdges( from ); i != iend; ++i ) {
				const Edge& edge = m_edges[i];
				if ( edge.originalEdgeSource == originalEdgeSource ) {
					if ( forward && edge.data.forward )
						return i;
					if ( !forward && edge.data.backward )
						return i;
				}
			}
			return EndEdges( from );
		}


        std::string DebugStringPenaltyData( const NodeIterator& n ) const {
        	std::stringstream ss;
        	unsigned inDegree = GetOriginalInDegree( n );
        	unsigned outDegree = GetOriginalOutDegree( n );
        	for ( unsigned out = 0; out < outDegree; ++out ) {
        		ss << "\t" << out;
        	}
        	ss << "\n";
            unsigned delta = m_nodes[n].firstPenalty;
        	for ( unsigned in = 0; in < inDegree; ++in )
        	{
        		ss << in;
        		for ( unsigned out = 0; out < outDegree; ++out ) {
        			int penalty = m_penalties[ delta + (in * outDegree) + out ].data;
        			assert( penalty == (int)GetPenaltyData( n, in, out ) );
        			ss << "\t" << penalty;
        		}
        		ss << "\n";
        	}
        	return ss.str();
        }

        std::string DebugStringEdge( const EdgeIterator& e ) const {
        	return m_edges[ e ].DebugString();
        }
        std::string DebugStringEdgesOf( const NodeIterator& n ) const {
        	std::stringstream ss;
        	ss << "edges of node " << n << ":\n";
        	for ( EdgeIterator edge =  BeginEdges( n ); edge != EndEdges( n ); ++edge ) {
        		ss << DebugStringEdge(edge) << "\n";
        	}
        	return ss.str();
        }

	protected:

		bool isDummy( EdgeIterator edge ) const
		{
			return m_edges[edge].target == std::numeric_limits< NodeIterator >::max();
		}

		void makeDummy( EdgeIterator edge )
		{
			m_edges[edge].target = std::numeric_limits< NodeIterator >::max();
		}

		struct Node {
			//index of the first edge
			EdgeIterator firstEdge;
			//amount of edges
			unsigned short edges;
			unsigned char inDegree;
			unsigned char outDegree;
			unsigned firstPenalty;
			EdgeIterator firstOriginalEdge;
		};

		struct Edge {
			NodeIterator target;
	        unsigned short originalEdgeSource; // uniquely identifies this edge among all outgoing edges adjacent to the source, at most degree( source ) - 1
	        unsigned short originalEdgeTarget; // uniquely identifies this edge among all incoming edges adjacent to the target, at most degree( target ) - 1
			EdgeData data;
			std::string DebugString() const {
				std::stringstream ss;
				ss << "- " << originalEdgeSource << " --- " << originalEdgeTarget << " -> " << target << ": " << data.DebugString();
				return ss.str();
			}
		};

		struct Penalty
		{
			explicit Penalty( const PenaltyData &d ) : data(d) {}
			PenaltyData data;
		};

		NodeIterator m_numNodes;
		EdgeIterator m_numEdges;
		EdgeIterator m_numOriginalEdges;

		std::vector< Node > m_nodes;
		std::vector< Edge > m_edges;
		std::vector< Penalty > m_penalties;
};

#endif // DYNAMICTURNGRAPH_H_INCLUDED
