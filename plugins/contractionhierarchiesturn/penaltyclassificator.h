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

#ifndef PENALTYCLASSIFICATOR_H
#define PENALTYCLASSIFICATOR_H

#include <vector>
#include <algorithm>
#include "interfaces/iimporter.h"
#include "utils/qthelpers.h"

class PenaltyClassificator {

public:

	struct PenaltyClass {
		char in;
		char out;
		unsigned index;
	};

	PenaltyClassificator( const std::vector< char >& inDegree, const std::vector< char >& outDegree, std::vector< int >* penalties, const std::vector< IImporter::RoutingEdge >& edges )
	{
		Timer time;
		assert( inDegree.size() == outDegree.size() );
		m_penalties.swap( *penalties );
		m_permutations = 0;
		m_skipped = 0;
		m_tables.reserve( inDegree.size() );
		m_tables.resize( inDegree.size() );

		unsigned index = 0;
		for ( unsigned node = 0; node < inDegree.size(); node++ ) {
			Table& table = m_tables[node];
			table.in = inDegree[node];
			table.out = outDegree[node];
			table.bidirectional = 0;
			table.index = index;
			table.id = node;
			table.hash = 0;
			table.mapping = 0;

			index += inDegree[node] * outDegree[node];
		}
		assert( index == m_penalties.size() );

		for ( unsigned edge = 0; edge < edges.size(); edge++ ) {
			const IImporter::RoutingEdge& edgeData = edges[edge];
			assert( edgeData.edgeIDAtSource < m_tables[edgeData.source].out );
			assert( edgeData.edgeIDAtTarget < m_tables[edgeData.target].in );
			if ( !edgeData.bidirectional )
				continue;
			assert( edgeData.edgeIDAtSource < m_tables[edgeData.source].in );
			assert( edgeData.edgeIDAtTarget < m_tables[edgeData.target].out );
			m_tables[edgeData.source].bidirectional = std::max( m_tables[edgeData.source].bidirectional, ( char ) edgeData.edgeIDAtSource );
			m_tables[edgeData.target].bidirectional = std::max( m_tables[edgeData.target].bidirectional, ( char ) edgeData.edgeIDAtTarget );
		}

		for ( unsigned table = 0; table < m_tables.size(); table++ )
			m_tables[table].hash = hash( m_tables[table] );
		qDebug() << "Contraction Hierarchies: initialized penalty classificator:" << time.elapsed() << "ms";
	}

	void run()
	{
		if ( m_tables.empty() )
			return;

		Timer time;
		std::sort( m_tables.begin(), m_tables.end(), Table::compareByHash );

		m_classes.push_back( m_tables.front() );

		unsigned tested = 0;
		unsigned remainingSize = 0;
		Timer outputTimer;
		for ( unsigned table = 1; table < m_tables.size(); table++ ) {
			bool unique = true;
			for ( int tableClass = m_classes.size() - 1; tableClass >= 0; tableClass-- ) {
				if ( Table::compareByHash( m_classes[tableClass], m_tables[table] ) )
					break;
				tested++;
				if ( mapTable( m_tables[table], m_classes[tableClass] ) ) {
					m_tables[table].mapping = tableClass;
					unique = false;
					break;
				}
			}
			if ( unique ) {
				m_tables[table].mapping = m_classes.size();
				m_classes.push_back( m_tables[table] );
				remainingSize += ( unsigned ) m_tables[table].in * m_tables[table].out;
				if ( outputTimer.elapsed() > 1000 ) {
					qDebug() << "Contraction Hierarchies: categorization progress:" << m_classes.size() << "found," << table * 100.0 / m_tables.size() << "%";
					outputTimer.restart();
				}
			}
		}

		std::sort( m_tables.begin(), m_tables.end(), Table::compareByID );
		qDebug() << "Contraction Hierarchies: categorized penalty tables:" << time.restart() << "ms";
		qDebug() << "Contraction Hierarchies: tested penalty table classes:" << tested;
		qDebug() << "Contraction Hierarchies: tested penalty permutations:" << m_permutations - tested;
		qDebug() << "Contraction Hierarchies: skipped permutating tables:" << m_skipped;
		qDebug() << "Contraction Hierarchies: penalty tables left:" << m_classes.size() << "," << m_classes.size() * 100.0 / m_tables.size() << "%";
		qDebug() << "Contraction Hierarchies: penalty table entries left:" << remainingSize << "/" << m_penalties.size() << remainingSize * 100.0 / m_penalties.size() << "%";
	}

	// The penalties table mapping:
	// classes +  penalties represent the penalty class plus their tables
	// nodeMapping contains the mapping from the node to their respective penalty classes
	// edgeMapping contains the serialized mapping from original edgeAtSource/Target IDs to the class specific IDs
	//     -> for each node the mapping of each incoming ID is stored and then the mapping of each outgoing ID
	void getClasses( std::vector< PenaltyClass >* classes, std::vector< int >* penalties, std::vector< unsigned >* nodeMapping, std::vector< char >* edgeMapping )
	{
		assert( classes != NULL );
		assert( nodeMapping != NULL );
		assert( edgeMapping != NULL );

		for ( unsigned int i = 0; i < m_classes.size(); i++ ) {
			PenaltyClass newClass;
			newClass.index = penalties->size();
			newClass.in = m_classes[i].in;
			newClass.out = m_classes[i].out;
			classes->push_back( newClass );
			for ( unsigned entry = 0; entry < ( unsigned ) newClass.in * ( unsigned ) newClass.out; entry++ )
				penalties->push_back( m_penalties[m_classes[i].index + entry] );
		}

		for ( unsigned int i = 0; i < m_tables.size(); i++ ) {
			nodeMapping->push_back( m_tables[i].mapping );
			bool mappable = mapTable( m_tables[i], m_classes[m_tables[i].mapping] );
			assert( mappable );
			assert( ( unsigned ) m_tables[i].in == m_in.size() );
			assert( ( unsigned ) m_tables[i].out == m_out.size() );
			for ( unsigned in = 0; in < ( unsigned ) m_tables[i].in; in++ ) {
				assert ( m_in[in] < ( unsigned ) m_tables[i].in );
				edgeMapping->push_back( m_in[in] );
			}
			for ( unsigned out = 0; out < ( unsigned ) m_tables[i].out; out++ ) {
				assert( m_out[out] < ( unsigned ) m_tables[i].out );
				edgeMapping->push_back( m_out[out] );
			}
		}
	}

protected:

	struct Table {
		char in;
		char out;
		char bidirectional;
		unsigned mapping;
		unsigned index;
		unsigned hash;
		unsigned id;

		static bool compareByID( const Table& left, const Table& right )
		{
			return left.id < right.id;
		}
		static bool compareByHash( const Table& left, const Table& right )
		{
			if ( left.hash != right.hash )
				return left.hash < right.hash;
			if ( left.in != right.in )
				return left.in < right.in;
			//if ( left.out != right.out )
				return left.out < right.out;
		}
	};

	std::vector< unsigned > m_nodes;
	std::vector< unsigned > m_hashes;

	unsigned hash( const Table& table )
	{
		m_hashes.clear();
		for ( unsigned from = 0; from < ( unsigned ) table.in; from++ ) {
			m_nodes.clear();
			unsigned value = 0;
			for ( unsigned to = 0; to < ( unsigned ) table.out; to++ ) {
				unsigned node = m_penalties[table.index + from * table.out + to];
				m_nodes.push_back( node );
			}
			std::sort( m_nodes.begin(), m_nodes.end() );
			for ( unsigned i = 0; i < m_nodes.size(); i++ )
				value = ( value * 13 ) ^ m_nodes[i];
			m_hashes.push_back( value );
		}

		unsigned value = 0;
		std::sort( m_hashes.begin(), m_hashes.end() );
		for ( unsigned i = 0; i < m_hashes.size(); i++ )
			value = ( value * 1009 ) ^ m_hashes[i];

		return value;
	}

	std::vector< unsigned > m_in;
	std::vector< unsigned > m_out;

	bool mapTable( const Table& source, const Table& target )
	{
		if ( source.in != target.in )
			return false;
		if ( source.out != target.out )
			return false;

		m_in.clear();
		m_out.clear();
		for ( unsigned i = 0; i < ( unsigned ) source.in; i++ )
			m_in.push_back( i );
		for ( unsigned i = 0; i < ( unsigned ) source.out; i++ )
			m_out.push_back( i );

		if ( source.in == 0 || source.out == 0 )
			return true;

		// try to map left to right
		unsigned runs = 0;
		do {
			do {
				do {
					runs++;
					m_permutations++;
					// test for equality
					bool equal = true;
					for ( unsigned from = 0; from < ( unsigned ) source.in; from++ ) {
						for ( unsigned to = 0; to < ( unsigned ) source.out; to++ ) {
							if ( m_penalties[source.index + m_in[from] * source.out + m_out[to]] != m_penalties[target.index + from * source.out + to] ) {
								equal = false;
								break;
							}
						}
						if ( equal == false )
							break;
					}
					if ( equal )
						return true;
					if ( runs > 1024 ) { // prune search for huge tables
						m_skipped++;
						return false;
					}
				} while( std::next_permutation( m_in.begin() + source.bidirectional + 1, m_in.end() ) );
			} while( std::next_permutation( m_out.begin() + source.bidirectional + 1, m_out.end() ) );
		} while ( std::next_permutation( m_in.begin(), m_in.begin() + source.bidirectional + 1 ) && std::next_permutation( m_out.begin(), m_out.begin() + source.bidirectional + 1 ) );

		return false;
	}

	unsigned m_skipped;
	unsigned m_permutations;
	std::vector< int > m_penalties;
	std::vector< Table > m_tables;

	std::vector< Table > m_classes;

};

#endif // PENALTYCLASSIFICATOR_H
