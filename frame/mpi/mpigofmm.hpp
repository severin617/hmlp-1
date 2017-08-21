/**
 *  HMLP (High-Performance Machine Learning Primitives)
 *  
 *  Copyright (C) 2014-2017, The University of Texas at Austin
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see the LICENSE file.
 *
 **/  


#ifndef GOFMM_MPI_HPP
#define GOFMM_MPI_HPP

#include <mpi/tree_mpi.hpp>
#include <gofmm/gofmm.hpp>
#include <primitives/combinatorics.hpp>


using namespace hmlp::gofmm;

namespace hmlp
{
namespace mpigofmm
{



/**
 *  @brief This the main splitter used to build the Spd-Askit tree.
 *         First compute the approximate center using subsamples.
 *         Then find the two most far away points to do the 
 *         projection.
 *
 *  @TODO  This splitter often fails to produce an even split when
 *         the matrix is sparse.
 *
 */ 
template<typename SPDMATRIX, int N_SPLIT, typename T> 
struct centersplit
{
  /** closure */
  SPDMATRIX *Kptr = NULL;

	/** (default) using angle distance from the Gram vector space */
  DistanceMetric metric = ANGLE_DISTANCE;

  /** number samples to approximate centroid */
  size_t n_centroid_samples = 1;



  /** compute all2c (distance to the approximate centroid) */
  std::vector<T> AllToCentroid( std::vector<size_t> &gids ) const
  {
    /** declaration */
    SPDMATRIX &K = *Kptr;
    std::vector<T> temp( gids.size(), 0.0 );

    /** loop over each point in gids */
    #pragma omp parallel for
    for ( size_t i = 0; i < gids.size(); i ++ )
    {
      switch ( metric )
      {
        case KERNEL_DISTANCE:
        {
          temp[ i ] = K( gids[ i ], gids[ i ] );
          for ( size_t j = 0; j < n_centroid_samples; j ++ )
          {
            /** important sample ( Kij, j ) if provided */
            std::pair<T, size_t> sample = K.ImportantSample( gids[ i ] );
            temp[ i ] -= ( 2.0 / n_centroid_samples ) * sample.first;
          }
          break;
        }
        case ANGLE_DISTANCE:
        {
          temp[ i ] = 0.0;
          for ( size_t j = 0; j < n_centroid_samples; j ++ )
          {
            /** important sample ( Kij, j ) if provided */
            std::pair<T, size_t> sample = K.ImportantSample( gids[ i ] );
            T kij = sample.first;
            T kii = K( gids[ i ], gids[ i ] );
            T kjj = K( sample.second, sample.second );
            temp[ i ] += ( 1.0 - ( kij * kij ) / ( kii * kjj ) );
          }
          temp[ i ] /= n_centroid_samples;
          break;
        }
        default:
        {
          printf( "centersplit() invalid splitting scheme\n" ); fflush( stdout );
          exit( 1 );
        }
      } /** end switch ( metric ) */
    }

    return temp;

  }; /** end AllToCentroid() */



  /** compute all2f (distance to the farthest point) */
  std::vector<T> AllToFarthest( std::vector<size_t> &gids, size_t gidf2c ) const
  {
    /** declaration */
    SPDMATRIX &K = *Kptr;
    std::vector<T> temp( gids.size(), 0.0 );

    /** loop over each point in gids */
    #pragma omp parallel for
    for ( size_t i = 0; i < gids.size(); i ++ )
    {
      switch ( metric )
      {
        case KERNEL_DISTANCE:
        {
          T kij = K( gids[ i ],    gidf2c );
          T kii = K( gids[ i ], gids[ i ] );
          temp[ i ] = kii - 2.0 * kij;
          break;
        }
        case ANGLE_DISTANCE:
        {
          T kij = K( gids[ i ],    gidf2c );
          T kii = K( gids[ i ], gids[ i ] );
          T kjj = K(    gidf2c,    gidf2c );
          temp[ i ] = ( 1.0 - ( kij * kij ) / ( kii * kjj ) );
          break;
        }
        default:
        {
          printf( "centersplit() invalid splitting scheme\n" ); fflush( stdout );
          exit( 1 );
        }
      } /** end switch ( metric ) */
    }

    return temp;

  }; /** end AllToFarthest() */



  /** compute all projection i.e. dip - diq for all i */
  std::vector<T> AllToLeftRight( std::vector<size_t> &gids,
      size_t gidf2c, size_t gidf2f ) const
  {
    /** declaration */
    SPDMATRIX &K = *Kptr;
    std::vector<T> temp( gids.size(), 0.0 );

    /** loop over each point in gids */
    #pragma omp parallel for
    for ( size_t i = 0; i < gids.size(); i ++ )
    {
      switch ( metric )
      {
        case KERNEL_DISTANCE:
        {
          T kip = K( gids[ i ], gidf2f );
          T kiq = K( gids[ i ], gidf2c );
          temp[ i ] = kip - kiq;
          break;
        }
        case ANGLE_DISTANCE:
        {
          T kip = K( gids[ i ],    gidf2f );
          T kiq = K( gids[ i ],    gidf2c );
          T kii = K( gids[ i ], gids[ i ] );
          T kpp = K(    gidf2f,    gidf2f );
          T kqq = K(    gidf2c,    gidf2c );
          /** ingore 1 from both terms */
          temp[ i ] = ( kip * kip ) / ( kii * kpp ) - ( kiq * kiq ) / ( kii * kqq );
          break;
        }
        default:
        {
          printf( "centersplit() invalid splitting scheme\n" ); fflush( stdout );
          exit( 1 );
        }
      }
    }

    return temp;

  }; /** end AllToLeftRight() */






	/** overload the operator */
  inline std::vector<std::vector<std::size_t> > operator()
  ( 
    std::vector<std::size_t>& gids,
    std::vector<std::size_t>& lids
  ) const 
  {
    /** all assertions */
    assert( N_SPLIT == 2 );
    assert( Kptr );

    /** declaration */
    SPDMATRIX &K = *Kptr;
    std::vector<std::vector<std::size_t>> split( N_SPLIT );
    size_t n = gids.size();
    std::vector<T> temp( n, 0.0 );

    /** all timers */
    double beg, d2c_time, d2f_time, projection_time, max_time;

    /** compute all2c (distance to the approximate centroid) */
    beg = omp_get_wtime();
    temp = AllToCentroid( gids );
    d2c_time = omp_get_wtime() - beg;

    /** find f2c (farthest from center) */
    auto itf2c = std::max_element( temp.begin(), temp.end() );
    auto idf2c = std::distance( temp.begin(), itf2c );
    auto gidf2c = gids[ idf2c ];

    /** compute the all2f (distance to farthest point) */
    beg = omp_get_wtime();
    temp = AllToFarthest( gids, gidf2c );
    d2f_time = omp_get_wtime() - beg;

    /** find f2f (far most to far most) */
    beg = omp_get_wtime();
    auto itf2f = std::max_element( temp.begin(), temp.end() );
    auto idf2f = std::distance( temp.begin(), itf2f );
    auto gidf2f = gids[ idf2f ];
    max_time = omp_get_wtime() - beg;

    /** compute all2leftright (projection i.e. dip - diq) */
    beg = omp_get_wtime();
    temp = AllToLeftRight( gids, gidf2c, gidf2f );
    projection_time = omp_get_wtime() - beg;






    /** parallel median search */
    T median;

    if ( 1 )
    {
      median = hmlp::combinatorics::Select( n, n / 2, temp );
    }
    else
    {
      auto temp_copy = temp;
      std::sort( temp_copy.begin(), temp_copy.end() );
      median = temp_copy[ n / 2 ];
    }

    split[ 0 ].reserve( n / 2 + 1 );
    split[ 1 ].reserve( n / 2 + 1 );

    /** TODO: Can be parallelized */
    std::vector<std::size_t> middle;
    for ( size_t i = 0; i < n; i ++ )
    {
      if      ( temp[ i ] < median ) split[ 0 ].push_back( i );
      else if ( temp[ i ] > median ) split[ 1 ].push_back( i );
      else                               middle.push_back( i );
    }

    for ( size_t i = 0; i < middle.size(); i ++ )
    {
      if ( split[ 0 ].size() <= split[ 1 ].size() ) 
        split[ 0 ].push_back( middle[ i ] );
      else                                          
        split[ 1 ].push_back( middle[ i ] );
    }

    return split;
  };






	/** distributed operator */
  inline std::vector<std::vector<size_t> > operator()
  ( 
    std::vector<size_t>& gids,
    hmlp::mpi::Comm comm
  ) const 
  {
    /** all assertions */
    assert( N_SPLIT == 2 );
    assert( Kptr );

    /** declaration */
    int size, rank;
    hmlp::mpi::Comm_size( comm, &size );
    hmlp::mpi::Comm_rank( comm, &rank );
    SPDMATRIX &K = *Kptr;
    std::vector<std::vector<size_t>> split( N_SPLIT );

    /** reduce to get the total size of gids */
    int n = 0;
    int num_points_owned = gids.size();
    std::vector<T> temp( gids.size(), 0.0 );

    /** n = sum( num_points_owned ) over all MPI processes in comm */
    hmlp::mpi::Allreduce( &num_points_owned, &n, 1, 
        MPI_INT, MPI_SUM, comm );

    /** early return */
    if ( n == 0 ) return split;

    /** compute d2c (distance to the approx centroid) for each owned point */
    temp = AllToCentroid( gids );

    /** find the f2c (far most to center) from points owned */
    auto itf2c = std::max_element( temp.begin(), temp.end() );
    auto idf2c = std::distance( temp.begin(), itf2c );

    /** create a pair for MPI Allreduce */
    hmlp::mpi::NumberIntPair<T> local_max_pair, max_pair; 
    local_max_pair.val = temp[ idf2c ];
    local_max_pair.key = rank;

    /** max_pair = max( local_max_pairs ) over all MPI processes in comm */
    hmlp::mpi::Allreduce( &local_max_pair, &max_pair, 1, MPI_MAXLOC, comm );

    /** boardcast gidf2c from the MPI process which has the max_pair */
    int gidf2c = gids[ idf2c ];
    hmlp::mpi::Bcast( &gidf2c, 1, MPI_INT, max_pair.key, comm );

    //printf( "rank %d val %E key %d; global val %E key %d\n", 
    //    rank, local_max_pair.val, local_max_pair.key,
    //    max_pair.val, max_pair.key ); fflush( stdout );
    //printf( "rank %d gidf2c %d\n", rank, gidf2c  ); fflush( stdout );



    /** compute all2f (distance to farthest point) */
    temp = AllToFarthest( gids, gidf2c );




    /** find f2f (far most to far most) from owned points */
    auto itf2f = std::max_element( temp.begin(), temp.end() );
    auto idf2f = std::distance( temp.begin(), itf2f );

    /** create a pair for MPI Allreduce */
    local_max_pair.val = temp[ idf2f ];
    local_max_pair.key = rank;

    /** max_pair = max( local_max_pairs ) over all MPI processes in comm */
    hmlp::mpi::Allreduce( &local_max_pair, &max_pair, 1, MPI_MAXLOC, comm );

    /** boardcast gidf2f from the MPI process which has the max_pair */
    int gidf2f = gids[ idf2f ];
    hmlp::mpi::Bcast( &gidf2f, 1, MPI_INT, max_pair.key, comm );

    //printf( "rank %d val %E key %d; global val %E key %d\n", 
    //    rank, local_max_pair.val, local_max_pair.key,
    //    max_pair.val, max_pair.key ); fflush( stdout );
    //printf( "rank %d gidf2f %d\n", rank, gidf2f  ); fflush( stdout );


    /** compute all2leftright (projection i.e. dip - diq) */
    temp = AllToLeftRight( gids, gidf2c, gidf2f );


    /** parallel median select */
    T  median = hmlp::combinatorics::Select( n / 2, temp, comm );

    //printf( "rank %d median %E\n", rank, median ); fflush( stdout );

    std::vector<size_t> middle;
    middle.reserve( gids.size() );
    split[ 0 ].reserve( gids.size() );
    split[ 1 ].reserve( gids.size() );

    /** TODO: Can be parallelized */
    for ( size_t i = 0; i < gids.size(); i ++ )
    {
      auto val = temp[ i ];

      if ( std::fabs( val - median ) < 1E-6 && !std::isinf( val ) && !std::isnan( val) )
      {
        middle.push_back( i );
      }
      else if ( val < median ) 
      {
        split[ 0 ].push_back( i );
      }
      else
      {
        split[ 1 ].push_back( i );
      }
    }

    int nmid = 0;
    int nlhs = 0;
    int nrhs = 0;
    int num_mid_owned = middle.size();
    int num_lhs_owned = split[ 0 ].size();
    int num_rhs_owned = split[ 1 ].size();

    /** nmid = sum( num_mid_owned ) over all MPI processes in comm */
    hmlp::mpi::Allreduce( &num_mid_owned, &nmid, 1, MPI_SUM, comm );
    hmlp::mpi::Allreduce( &num_lhs_owned, &nlhs, 1, MPI_SUM, comm );
    hmlp::mpi::Allreduce( &num_rhs_owned, &nrhs, 1, MPI_SUM, comm );

    printf( "rank %d [ %d %d %d ] global [ %d %d %d ]\n",
        rank, num_lhs_owned, num_mid_owned, num_rhs_owned,
        nlhs, nmid, nrhs ); fflush( stdout );

    /** assign points in the middle to left or right */
    if ( nmid )
    {
      int nlhs_required = ( n - 1 ) / 2 + 1 - nlhs;
      int nrhs_required = nmid - nlhs_required;
      assert( nlhs_required >= 0 );
      assert( nrhs_required >= 0 );

      /** now decide the portion */
      int nlhs_required_owned = num_mid_owned * nlhs_required / nmid;
      int nrhs_required_owned = num_mid_owned - nlhs_required_owned;
      assert( nlhs_required_owned >= 0 );
      assert( nrhs_required_owned >= 0 );

      for ( size_t i = 0; i < middle.size(); i ++ )
      {
        if ( i < nlhs_required_owned ) split[ 0 ].push_back( middle[ i ] );
        else                           split[ 1 ].push_back( middle[ i ] );
      }
    };

    return split;
  };


}; /** end struct centersplit */




























/**
 *  @brief ComputeAll
 */ 
template<
bool USE_RUNTIME, 
bool USE_OMP_TASK, 
bool SYMMETRIC_PRUNE, 
bool NNPRUNE, 
bool CACHE, 
typename NODE, 
typename TREE, 
typename T>
hmlp::Data<T> Evaluate
( 
  TREE &tree,
  hmlp::Data<T> &weights
)
{
  const bool AUTO_DEPENDENCY = true;

  /** all timers */
  double beg, time_ratio, evaluation_time = 0.0;
  double allocate_time, computeall_time;
  double forward_permute_time, backward_permute_time;

  /** nrhs-by-n initialize potentials */
  beg = omp_get_wtime();
  hmlp::Data<T> potentials( weights.row(), weights.col(), 0.0 );
  tree.setup.w = &weights;
  tree.setup.u = &potentials;
  allocate_time = omp_get_wtime() - beg;


  /** not yet implemented */
  printf( "\nnot yet implemented\n" ); fflush( stdout );


  return potentials;

}; /** end Evaluate() */









/**
 *  @brielf template of the compress routine
 */ 
template<
  bool        ADAPTIVE, 
  bool        LEVELRESTRICTION, 
  typename    SPLITTER, 
  typename    RKDTSPLITTER, 
  typename    T, 
  typename    SPDMATRIX>
hmlp::mpitree::Tree<
  hmlp::gofmm::Setup<SPDMATRIX, SPLITTER, T>, 
  hmlp::gofmm::Data<T>,
  N_CHILDREN,
  T> 
*Compress
( 
  hmlp::Data<T> *X,
  SPDMATRIX &K, 
  hmlp::Data<std::pair<T, std::size_t>> &NN,
  SPLITTER splitter, 
  RKDTSPLITTER rkdtsplitter,
	Configuration<T> &config
)
{
  /** get all user-defined parameters */
  DistanceMetric metric = config.MetricType();
  size_t n = config.ProblemSize();
	size_t m = config.LeafNodeSize();
	size_t k = config.NeighborSize(); 
	size_t s = config.MaximumRank(); 
  T stol = config.Tolerance();
	T budget = config.Budget(); 

  /** options */
  const bool SYMMETRIC = true;
  const bool NNPRUNE   = true;
  const bool CACHE     = true;

  /** instantiation for the Spd-Askit tree */
  using SETUP              = hmlp::gofmm::Setup<SPDMATRIX, SPLITTER, T>;
  using DATA               = hmlp::gofmm::Data<T>;
  using NODE               = hmlp::mpitree::Node<SETUP, N_CHILDREN, DATA, T>;
  using TREE               = hmlp::mpitree::Tree<SETUP, DATA, N_CHILDREN, T>;
  using SKELTASK           = hmlp::gofmm::SkeletonizeTask<ADAPTIVE, LEVELRESTRICTION, NODE, T>;
  using PROJTASK           = hmlp::gofmm::InterpolateTask<NODE, T>;
  using NEARNODESTASK      = hmlp::gofmm::NearNodesTask<SYMMETRIC, TREE>;
  using CACHENEARNODESTASK = hmlp::gofmm::CacheNearNodesTask<NNPRUNE, NODE>;

  /** instantiation for the randomisze Spd-Askit tree */
  using RKDTSETUP          = hmlp::gofmm::Setup<SPDMATRIX, RKDTSPLITTER, T>;
  using RKDTNODE           = hmlp::tree::Node<RKDTSETUP, N_CHILDREN, DATA, T>;
  using KNNTASK            = hmlp::gofmm::KNNTask<3, RKDTNODE, T>;

  /** all timers */
  double beg, omptask45_time, omptask_time, ref_time;
  double time_ratio, compress_time = 0.0, other_time = 0.0;
  double ann_time, tree_time, skel_time, mergefarnodes_time, cachefarnodes_time;
  double nneval_time, nonneval_time, fmm_evaluation_time, symbolic_evaluation_time;

  /** dummy instances for each task */
  SKELTASK           skeltask;
  PROJTASK           projtask;
  KNNTASK            knntask;
  CACHENEARNODESTASK cachenearnodestask;


  /** original order of the matrix */
  beg = omp_get_wtime();
  std::vector<std::size_t> gids( n ), lids( n );
  #pragma omp parallel for
  for ( auto i = 0; i < n; i ++ ) 
  {
    gids[ i ] = i;
    lids[ i ] = i;
  }
  other_time += omp_get_wtime() - beg;


  /** iterative all nearnest-neighbor (ANN) */
  const size_t n_iter = 10;
  const bool SORTED = false;
  /** do not change anything below this line */
  hmlp::mpitree::Tree<RKDTSETUP, DATA, N_CHILDREN, T> rkdt;
  rkdt.setup.X = X;
  rkdt.setup.K = &K;
	rkdt.setup.metric = metric; 
  rkdt.setup.splitter = rkdtsplitter;
  std::pair<T, std::size_t> initNN( std::numeric_limits<T>::max(), n );
  printf( "NeighborSearch ...\n" ); fflush( stdout );
  beg = omp_get_wtime();
  if ( NN.size() != n * k )
  {
    NN = rkdt.template AllNearestNeighbor<SORTED>
         ( n_iter, k, 10, gids, lids, initNN, knntask );
  }
  else
  {
    printf( "not performed (precomputed or k=0) ...\n" ); fflush( stdout );
  }
  ann_time = omp_get_wtime() - beg;

  /** check illegle values in NN */
  for ( size_t j = 0; j < NN.col(); j ++ )
  {
    for ( size_t i = 0; i < NN.row(); i ++ )
    {
      size_t neighbor_gid = NN( i, j ).second;
      if ( neighbor_gid < 0 || neighbor_gid >= n )
      {
        printf( "NN( %lu, %lu ) has illegle values %lu\n", i, j, neighbor_gid );
        break;
      }
    }
  }






  /** initialize metric ball tree using approximate center split */
  auto *tree_ptr = new hmlp::mpitree::Tree<SETUP, DATA, N_CHILDREN, T>();
	auto &tree = *tree_ptr;
  tree.setup.X = X;
  tree.setup.K = &K;
	tree.setup.metric = metric; 
  tree.setup.splitter = splitter;
  tree.setup.NN = &NN;
  tree.setup.m = m;
  tree.setup.k = k;
  tree.setup.s = s;
  tree.setup.stol = stol;
  printf( "TreePartitioning ...\n" ); fflush( stdout );
  beg = omp_get_wtime();
  tree.TreePartition( n );
  tree_time = omp_get_wtime() - beg;
  printf( "end TreePartitioning ...\n" ); fflush( stdout );
  

  return tree_ptr;

}; /** end Compress() */




















}; /** end namespace gofmm */
}; /** end namespace hmlp */

#endif /** define GOFMM_MPI_HPP */
