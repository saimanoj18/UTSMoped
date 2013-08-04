/*
 * CLUSTER_MEAN_SHIFT_CPU.Hpp
 *
 *  Created on: Jun 23, 2013
 *      Author: wu
 */

#pragma once

namespace MopedNS {
	bool g_start = false;
	bool * clusterObvFlag = new bool[10];

	class CLUSTER_MEAN_SHIFT_CPU_WU :public MopedAlg {

		float Radius;
		float Merge;
		int MinPts;
		int MaxIterations;

		template< typename T, int N >
		struct Canopy {
			Pt<N> center;
			Pt<N> touchPtsAggregate;
			list<T> boundPoints;
			int boundPointsSize;
			int canopyId;
			Canopy<T,N> *merges;
			Canopy<T,N>( Pt<N> &p, T &t, int id ) {
				center = p;
				boundPoints.push_back( t );
				boundPointsSize = 1;
				canopyId = id;
			}
		};

		template< typename T, int N >
		void MeanShift( vector< list<T> > &clusters, vector< pair<Pt<N>,T> > &points, float Radius, float Merge, int MinPts, int MaxIterations ) {
			float SqRadius = Radius * Radius;
			float SqMerge  = Merge  * Merge;
			vector< Canopy<T,N> > can;
			can.reserve( points.size() );
			list< int > canopiesRemaining;

			for(int i=0; i<(int)points.size(); i++) {
				// points[i].first is the pt<2>, points[i].second and i are same indexs
				can.push_back( Canopy<T,N>( points[i].first, points[i].second, i ) );
				can.back().merges = &can.back();
				canopiesRemaining.push_back( i );
			}

			bool done = false;
			for( int nIter = 0; !done && nIter < MaxIterations; nIter ++ ) { // shift canopies to their centroids
				done = true;
				foreach ( cId, canopiesRemaining ) {
					can[cId].touchPtsAggregate = can[cId].center * can[cId].boundPointsSize;
					int touchPtsN = can[cId].boundPointsSize;
					foreach ( othercId, canopiesRemaining ) {
						if( cId == othercId )
							continue;
						// compute the Euclidean distance from other features to current feature
						float dist = can[cId].center.sqEuclDist( can[othercId].center );
						if (dist < SqRadius) {
							touchPtsN += can[othercId].boundPointsSize;
							can[cId].touchPtsAggregate += can[othercId].center * can[othercId].boundPointsSize;
						}
					}
					can[cId].touchPtsAggregate /= touchPtsN;
				}

				// merge seems to set the dimension of the cluster
				foreach ( cId, canopiesRemaining ) {
					foreach ( othercId, canopiesRemaining ) {
						if( cId==othercId ) {
							break;
							cout << "Break!" << endl;
						}
						float dist = can[cId].touchPtsAggregate.sqEuclDist( can[othercId].touchPtsAggregate );
						if (dist < SqMerge) {
							can[othercId].merges->merges = &can[cId];
							can[othercId].merges = &can[cId];
						}
					}
				}

				eforeach ( cId, cId_it, canopiesRemaining ) {
					if( can[cId].merges != &can[cId] ) {
						can[cId].merges->center = can[cId].merges->center * can[cId].merges->boundPoints.size() + can[cId].center * can[cId].boundPointsSize;
						can[cId].merges->boundPoints.splice( can[cId].merges->boundPoints.end(), can[cId].boundPoints );
						can[cId].merges->boundPointsSize += can[cId].boundPointsSize;
						can[cId].merges->center /= can[cId].merges->boundPointsSize;
						cId_it = canopiesRemaining.erase( cId_it );
						done = false;
					}
				}
			}

			foreach ( cId, canopiesRemaining ) {

				if( can[cId].boundPointsSize < MinPts )	continue;

				clusters.resize( clusters.size() + 1 );
				clusters.back().splice( clusters.back().end(), can[cId].boundPoints );
			}
		}

	public:

		CLUSTER_MEAN_SHIFT_CPU_WU(	float Radius, float Merge, unsigned int MinPts, unsigned int MaxIterations )
		: Radius(Radius), Merge(Merge), MinPts(MinPts), MaxIterations(MaxIterations)  {
		}

		void getConfig( map<string,string> &config ) const {

			GET_CONFIG( Radius );
			GET_CONFIG( Merge );
			GET_CONFIG( MinPts );
			GET_CONFIG( MaxIterations );
		}

		void setConfig( map<string,string> &config ) {

			SET_CONFIG( Radius );
			SET_CONFIG( Merge );
			SET_CONFIG( MinPts );
			SET_CONFIG( MaxIterations );
		}

		void process( FrameData &frameData ) {
			cout << "CLUSTER_MEAN_SHIFT_CPU\n";
			frameData.clusters.resize( models->size() );
			pcl::PointCloud<pcl::PointXYZ>::Ptr cloud = frameData.cloudPclPtr;
			#pragma omp parallel for
			for( int model = 0; model < (int)frameData.matches.size(); model ++) {
				vector< vector< pair<Pt<3>, int> > > pointsPerImage( frameData.images.size() ) ;
				for( int match = 0; match < (int)frameData.matches[model].size(); match ++) {
					Pt<2> tmpPt = matches[model][match].coord2D;
					int i = tmpPt[1], j = tmpPt[0];
					if(	cloud->points[i*cloud->width+j].x != NULL &&
						cloud->points[i*cloud->width+j].y != NULL &&
						cloud->points[i*cloud->width+j].z != NULL ) {
						pointsPerImage[frameData.matches[model][match].imageIdx].push_back( make_pair( frameData.matches[model][match].coord2D, match ) );					
					}
				}
				for( int i=0; i<(int)frameData.images.size(); i++ ) {
					MeanShift( frameData.clusters[model], pointsPerImage[i], Radius, Merge, MinPts, MaxIterations );
				}
			}
			if( _stepName == "CLUSTER" ) 
				frameData.oldClusters = frameData.clusters;



/*			int clustersNum = (int)frameData.clusters.size();
			if ( g_start == false ) {
				for ( int obvIdx = 0; obvIdx < clustersNum; obvIdx ++ ) {
					clusterObvFlag[obvIdx] = false;
				}
				g_start = true;
			}
			string DescriptorType = "SIFT";
			for ( int clusterIdx = 0; clusterIdx < (int)frameData.clusters.size(); clusterIdx ++  ) {
				if ( (int)frameData.clusters[clusterIdx].size() != 0  )
					clusterObvFlag[clusterIdx] = true;

			}*/

		}
	};
};