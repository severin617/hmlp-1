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




/** STRASSEN templates */
#include <primitives/strassen.hpp>

using namespace hmlp::strassen;

void strassen
(
  hmlpOperation_t transA, hmlpOperation_t transB,
	int m, int n, int k,
	float *A, int lda,
	float *B, int ldb,
	float *C, int ldc
)
{
  printf( "no implementation\n" );
  exit( 1 );
};

void strassen
(
  hmlpOperation_t transA, hmlpOperation_t transB,
	int m, int n, int k,
	double *A, int lda,
	double *B, int ldb,
	double *C, int ldc
)
{
  printf( "no implementation\n" );
  exit( 1 );
}; 


void sstrassen
(
  hmlpOperation_t transA, hmlpOperation_t transB,
	int m, int n, int k,
	float *A, int lda,
	float *B, int ldb,
	float *C, int ldc
)
{
  strassen( transA, transB, m, n, k, A, lda, B, ldb, C, ldc );
};


void dstrassen
(
  hmlpOperation_t transA, hmlpOperation_t transB,
	int m, int n, int k,
	double *A, int lda,
	double *B, int ldb,
	double *C, int ldc
)
{
  strassen( transA, transB, m, n, k, A, lda, B, ldb, C, ldc );
};
