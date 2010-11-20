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
		typedef DynamicTurnGraph< EdgeData, PenaltyData > Self;

		class InputNode {
			public:
				unsigned firstPenalty;
				std::string DebugString() const {
					std::stringstream ss;
					ss << "firstPenalty: " << firstPenalty;
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

		class InputPenalty {
			public:
				char inDegree;
				char outDegree;
				std::vector< PenaltyData > data;
		};

		DynamicTurnGraph( const std::vector< InputNode > &nodes, const std::vector< InputEdge > &graph,
				const std::vector< InputPenalty > &penalties ) {
			m_numNodes = nodes.size();
			m_numEdges = ( EdgeIterator ) graph.size();
			m_numPenaltyTables = penalties.size();

			std::vector< unsigned > penaltyMap;
			penaltyMap.reserve( penalties.size() );
			unsigned penaltySize = 0;
			for ( unsigned i = 0; i < penalties.size(); ++i )
			{
				penaltyMap.push_back(penaltySize);
				penaltySize += 2 + (unsigned)penalties[i].inDegree * (unsigned)penalties[i].outDegree;
			}
			m_penalties.reserve( penaltySize );
			for ( unsigned i = 0; i < penalties.size(); ++i )
			{
				Penalty penalty;
				const InputPenalty& input = penalties[i];
				penalty.degree = input.inDegree;
				m_penalties.push_back( penalty );
				penalty.degree = input.outDegree;
				m_penalties.push_back( penalty );
				assert( input.data.size() == (unsigned)input.inDegree * (unsigned)input.outDegree );
				for ( unsigned j = 0; j < input.data.size(); ++j ) {
					penalty.data = input.data[j];
					m_penalties.push_back( penalty );
				}
			}


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

                assert( nodes[node].firstPenalty < penaltyMap.size() );
				m_nodes[node].firstPenalty = penaltyMap[nodes[node].firstPenalty];
                m_nodes[node].firstOriginalEdge = originalPosition;
                unsigned inDegree = penalties[ nodes[node].firstPenalty ].inDegree;
                unsigned outDegree = penalties[ nodes[node].firstPenalty ].outDegree;
				sumInDegree += inDegree;
				sumOutDegree += outDegree;
				// Summing up the indegree(outdegre) allows to compute a globally unique
				// original-edge-id for incoming(outgoing) edges, max(in,out) allows to do both.
				originalPosition += std::max( inDegree, outDegree );
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
        	unsigned i = m_nodes[n].firstPenalty;
        	assert ( i < m_penalties.size() );
        	return m_penalties[i].degree;
        }

        unsigned GetOriginalOutDegree( const NodeIterator &n ) const
        {
        	assert( n < m_nodes.size() );
        	unsigned i = m_nodes[n].firstPenalty + 1;
        	assert ( i < m_penalties.size() );
        	return m_penalties[i].degree;
        }

protected:
		struct Penalty
		{
			union {
				PenaltyData data;  // penalty data
				char degree; // degree
			};
		};
public:

        class PenaltyTable {
        	friend class DynamicTurnGraph;
        private:
        	unsigned outDegree;
        	typename std::vector< Penalty >::const_iterator table;
        public:
        	PenaltyTable( const typename std::vector< Penalty >::const_iterator& it ) {
        		table = it + 1;
        		outDegree = table->degree;
        		++table;
        	}
        	unsigned GetInDegree() const {
        		return (table - 2)->degree;
        	}
        	unsigned GetOutDegree() const {
        		return outDegree;
        	}
        	const PenaltyData &GetData(unsigned in, unsigned out) const {
        		assert( in < GetInDegree() );
        		assert( out < GetOutDegree() );
        		return ( table + ( in * GetOutDegree() ) + out )->data;
        	}

            std::string DebugString() const {
            	std::stringstream ss;
            	for ( unsigned out = 0; out < GetOutDegree(); ++out ) {
            		ss << "\t" << out;
            	}
            	ss << "\n";
            	for ( unsigned in = 0; in < GetInDegree(); ++in )
            	{
            		ss << in;
            		for ( unsigned out = 0; out < GetOutDegree(); ++out ) {
            			ss << "\t" << (int)GetData( in, out );
            		}
            		ss << "\n";
            	}
            	return ss.str();
            }



        };

//        const PenaltyData &GetPenaltyData( const NodeIterator& n, unsigned short originalEdgeIn, unsigned short originalEdgeOut ) const
//        {
//        	assert( n < m_nodes.size() );
//            unsigned first = m_nodes[n].firstPenalty + 1;
//            unsigned originalOutDegree = m_penalties[ first++ ].degree;
//            if ( originalEdgeIn >= m_penalties[ m_nodes[n].firstPenalty ].degree ) {
//            	qDebug() << n << originalEdgeIn << m_nodes[n].inDegree;
//            }
//            assert( originalEdgeIn < m_penalties[ m_nodes[n].firstPenalty ].degree );
//            if ( originalEdgeOut >= originalOutDegree ) {
//            	qDebug() << n << originalEdgeOut << originalOutDegree;
//            }
//            assert( originalEdgeOut < originalOutDegree );
//            assert( first + (originalEdgeIn * originalOutDegree) + originalEdgeOut < m_penalties.size() );
//            return m_penalties[ first + (originalEdgeIn * originalOutDegree) + originalEdgeOut ].data;
//        }

        PenaltyTable GetPenaltyTable( const NodeIterator& n ) const {
        	assert( n < m_nodes.size() );
        	assert( m_nodes[n].firstPenalty + 2 + ( GetOriginalInDegree( n ) * GetOriginalOutDegree( n ) ) <= m_penalties.size() );
            #ifndef NDEBUG
        	PenaltyTable pt( m_penalties.begin() + m_nodes[n].firstPenalty );
        	assert( pt.GetInDegree() == GetOriginalInDegree( n ) );
        	assert( pt.GetOutDegree() == GetOriginalOutDegree( n ) );
            #endif
        	return PenaltyTable( m_penalties.begin() + m_nodes[n].firstPenalty );
        }

		EdgeIterator BeginEdges( const NodeIterator &n ) const
		{
			//assert( EndEdges( n ) - EdgeIterator( _nodes[n].firstEdge ) <= 100 );
			assert( n < m_nodes.size() );
			return EdgeIterator( m_nodes[n].firstEdge );
		}

		EdgeIterator EndEdges( const NodeIterator &n ) const
		{
			assert( n < m_nodes.size() );
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

		bool WriteToFile( QString filename ) const {

			FileStream data( filename );

			if ( !data.open( QIODevice::WriteOnly ) )
				return false;

			unsigned numNodes = m_numNodes;
			unsigned numEdges = m_numEdges;
			unsigned numPenaltyTables = m_numPenaltyTables;
			data << numNodes << numEdges << numPenaltyTables;

			QHash< unsigned, unsigned > penaltyMap;
			unsigned tableCounter = 0;
			for ( typename std::vector< Penalty >::const_iterator it = m_penalties.begin(); it != m_penalties.end(); ) {
				penaltyMap.insert( it - m_penalties.begin(), tableCounter++ );
				unsigned inDegree = it->degree;
				++it;
				unsigned outDegree = it->degree;
				++it;
				unsigned size = inDegree * outDegree;
				data << qint8(inDegree) << qint8(outDegree);
				for ( unsigned i = 0; i < size; ++i, ++it ) {
					data << qint16( it->data );
				}
			}


			for ( NodeID u = 0; u < GetNumberOfNodes(); ++u )
			{
				const Node& node = m_nodes[u];
				QHash< unsigned, unsigned >::iterator it = penaltyMap.find( node.firstPenalty );
				if ( it == penaltyMap.end() ) {
					qFatal( "Data corruption." );
					exit(-1);
				}
				unsigned firstPenalty = it.value();
				data << firstPenalty;
			}

			for ( unsigned source = 0; source < GetNumberOfNodes(); ++source )
			{
				std::vector< InputEdge > edges;
				for ( EdgeIterator e = BeginEdges( source ), ee = EndEdges( source ); e != ee; ++e ) {
					InputEdge edge;
					edge.target = GetTarget( e );
					edge.originalEdgeSource = GetOriginalEdgeSource( e );
					edge.originalEdgeTarget = GetOriginalEdgeTarget( e );
					edge.data = GetEdgeData( e );
					edges.push_back( edge );
				}
				std::sort( edges.begin(), edges.end() );
				for ( unsigned i = 0; i < edges.size(); ++i )
				{
					const InputEdge& edge = edges[i];
					unsigned target = edge.target;
					qint8 originalEdgeSource = edge.originalEdgeSource;
					qint8 originalEdgeTarget = edge.originalEdgeTarget;

					data << source << target << originalEdgeSource << originalEdgeTarget;
					edge.data.Serialize( &data );
				}
			}
			return true;
		}

		static Self* ReadFromFile( QString filename ) {
			FileStream data( filename );

			if ( !data.open( QIODevice::ReadOnly ) )
				return NULL;

			unsigned numNodes;
			unsigned numEdges;
			unsigned numPenaltyTables;
			data >> numNodes >> numEdges >> numPenaltyTables;

			if ( data.status() == QDataStream::ReadPastEnd ) {
				qCritical() << "TurnContractor::ReadGraphFromFile(): Corrupted Graph Data";
				exit(1);
			}

			std::vector< InputNode > nodes;
			std::vector< InputEdge > edges;
			std::vector< InputPenalty > penalties;

			nodes.reserve( numNodes );
			edges.reserve( numEdges );
			penalties.reserve( numPenaltyTables );
			penalties.resize( numPenaltyTables );
			for ( unsigned i = 0; i < numPenaltyTables; ++i )
			{
				qint8 inDegree, outDegree;
				data >> inDegree >> outDegree;
				penalties[i].inDegree = inDegree;
				penalties[i].outDegree = outDegree;
				unsigned size = inDegree * outDegree;
				penalties[i].data.reserve( size );
				for ( unsigned j = 0; j < size; ++j ) {
					qint16 penalty;
					data >> penalty;
					penalties[i].data.push_back( penalty );
				}
			}

			for ( NodeID u = 0; u < numNodes; ++u )
			{
				unsigned firstPenalty;
				data >> firstPenalty;
				InputNode node;
				node.firstPenalty = firstPenalty;
				nodes.push_back( node );
			}


			for ( unsigned i = 0; i < numEdges; ++i ) {
				unsigned source, target;
				qint8 originalEdgeSource, originalEdgeTarget;
				data >> source >> target >> originalEdgeSource >> originalEdgeTarget;

				InputEdge edge;
				edge.source = source;
				edge.target = target;
				edge.originalEdgeSource = originalEdgeSource;
				edge.originalEdgeTarget = originalEdgeTarget;
				edge.data.Deserialize( &data );

				if ( data.status() == QDataStream::ReadPastEnd ) {
					qCritical() << "TurnContractor::ReadGraphFromFile(): Corrupted Graph Data";
					exit(1);
				}
				edges.push_back( edge );
			}
			Self* graph = new Self( nodes, edges, penalties );
			return graph;
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


        std::string DebugStringPenaltyTable( const NodeIterator& n ) const {
        	PenaltyTable table = GetPenaltyTable( n );
        	return table.DebugString();
        }

        std::string DebugStringEdge( const EdgeIterator& e ) const {
        	return m_edges[ e ].DebugString();
        }
        std::string DebugStringEdgesOf( const NodeIterator& n ) const {
        	std::stringstream ss;
        	ss << "edges of node " << n << ":\n";
        	for ( EdgeIterator edge =  BeginEdges( n ); edge != EndEdges( n ); ++edge ) {
        		ss << "[" << edge << "] " << DebugStringEdge(edge) << "\n";
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
			unsigned edges;
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

		NodeIterator m_numNodes;
		EdgeIterator m_numEdges;
		EdgeIterator m_numOriginalEdges;
		unsigned m_numPenaltyTables;

		std::vector< Node > m_nodes;
		std::vector< Edge > m_edges;
		std::vector< Penalty > m_penalties;
};

#endif // DYNAMICTURNGRAPH_H_INCLUDED
