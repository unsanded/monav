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

	struct Demand {
		unsigned source;
		unsigned source2;
		unsigned target;
		unsigned target2;
		int distance;
	};

	TurnExperiments() {}

protected:
	double _Timestamp() {
		static Timer timer;
		return ( double ) timer.elapsed() / 1000;
	}
public:

	void plainDijkstra(QString dir, const TurnContractor& contractor) {
		const Graph& graph = contractor.graph();
		unsigned numNodes = graph.GetNumberOfNodes();

		unsigned numQueries = 1000;
		srand48(176);
		std::vector< Demand > demands;
		demands.reserve( numQueries );
		for ( unsigned i = 0; i < numQueries; ++i ) {
			Demand demand;
			demand.source = lrand48() % numNodes;
			vector< unsigned > temp;
			for ( unsigned e = graph.BeginEdges( demand.source ); e < graph.EndEdges( demand.source ); ++ e) {
				if ( graph.GetEdgeData( e ).forward ) {
					temp.push_back( graph.GetTarget( e ) );
				}
			}
			if ( !temp.empty() ) {
				unsigned i = lrand48() % temp.size();
				demand.source2 = temp[i];
				temp.clear();
			}
			else
				demand.source2 = lrand48() % numNodes;

			demand.target = lrand48() % numNodes;
			for ( unsigned e = graph.BeginEdges( demand.target ); e < graph.EndEdges( demand.target ); ++ e) {
				if ( graph.GetEdgeData( e ).backward ) {
					temp.push_back( graph.GetTarget( e ) );
				}
			}
			if ( !temp.empty() ) {
				unsigned i = lrand48() % temp.size();
				demand.target2 = temp[i];
				temp.clear();
			}
			else
				demand.target2 = lrand48() % numNodes;

			demand.distance = query.unidirSearch( source, source2, target, target2 );
			demands.push_back( demand );
		}

		#pragma omp parallel
		{
			TurnQuery<Graph, false /*stall on demand*/> query(graph);
			#pragma omp for schedule ( guided )
			for ( int i = 0; i < (int)demands.size(); ++i )
			{
				Demand& demand = demands[i];
				demand.distance = query.unidirSearch( demand.source, demand.source2, demand.target, demand.target2 );
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
			data << demand.source << demand.source2 << demand.target << demand.target2 << demand.distance;
		}
	}

	void chtQuery(QString dir) {
		const Graph* graph = TurnContractor::ReadGraphFromFile( fileInDirectory( dir, "CHT Dynamic Graph") );

		unsigned numQueries;
		std::vector< Demands > demands;
		{
			QString filename = fileInDirectory( dir, "CHT Demands");
			FileStream data( filename );

			if ( !data.open( QIODevice::ReadOnly ) )
				return false;

			data >> numQueries;
			demands.reserver( numQueries );

			for ( unsigned i = 0; i < demands.size(); ++i )
			{
				Demand demand;
				data >> demand.source >> demand.source2 >> demand.target >> demand.target2 >> demand.distance;
				demands.push_back( demand );
			}
		}

		TurnQuery<Graph, true /*stall on demand*/> query(*graph);
		double duration = _Timestamp();
		for ( int i = 0; i < (int)demands.size(); ++i )
		{
			const Demand& demand = demands[i];
			int distance = query.bidirSearch( demand.source, demand.source2, demand.target, demand.target2 );
			if (distance != demand.distance) {
				qDebug() << demand.source << "->" << demand.source2 << demand.target << "->" << demand.target2
						<< ": CHT" << distance << "plain" << demand.distance;
			}
		}
		duration = _Timestamp() - duration;
		qDebug() << "CHT queries done in" << duration << "seconds.";

		delete graph;
	}

};

#endif // TURNEXPERIMENTS_H_INCLUDED
