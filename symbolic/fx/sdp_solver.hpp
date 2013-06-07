/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef SDP_SOLVER_HPP
#define SDP_SOLVER_HPP

#include "fx.hpp"


/** \defgroup SDPSolver_doc

  Solves an SDP problem in standard form.
  See http://sdpa.indsys.chuo-u.ac.jp/sdpa/files/sdpa-c.6.2.0.manual.pdf
  
  Primal:

  \verbatim
  min          c' x 
   x 
  subject to
                P = Sum_i^m F_i x_i - G 
                P negative semidefinite                   
              
              LBA <= A x <= UBA
              LBX <= x   <= UBX
              
      with x ( n x 1)
           c ( n x 1 )
           G, F_i  sparse symmetric (m x m)
           X dense symmetric ( m x m )
           A sparse matrix ( nc x n)
           LBA, UBA dense vector (nc x 1)
           LBX, UBX dense vector (n x 1)

  \endverbatim
  
  This formulation is chosen as primal, because it does not call for a large decision variable space.
  
  Dual:
  
  \verbatim
  max          trace(G Y)
   Y 
  
  subject to
              trace(F_i Y) = c_i
              Y positive semidefinite
              
      with Y dense symmetric ( m x m)

  \endverbatim
  
  On generality: you might have formulation with block partitioning:
  
  Primal:
  
  \verbatim
  min          c' x 
   x 
  subject to
                Pj = Sum_i^m F_ij x_i - gj   for all j
                Pj negative semidefinite   for all j
              
      with x ( n x 1)
           c ( n x 1 )
           G, F_i  sparse symmetric (m x m)
           X dense symmetric ( m x m )
      
  \endverbatim
  
  Dual:
  \verbatim
  max          Sum_j trace(Gj Yj)
   Yj 
  
  subject to
              Sum_j trace(F_ij Yj) = c_i   for all j
              Yj positive semidefinite     for all j
              
      with Y dense symmetric ( m x m)

  \endverbatim
  
  You can cast this into the standard form with:
    G  = blkdiag(Gj for all j)
    Fi = blkdiag(F_ij for all j)
    
  Implementations of SDPSolver are encouraged to exploit this block structure.
  
*/
      
namespace CasADi{
  
/// Input arguments of a SDP problem [sdpIn]
enum SDPInput{
  /// The vertical stack of all matrices F_i: ( nm x m) [f]
  SDP_F,
  /// The vector c: ( n x 1) [c]
  SDP_C,
  /// The matrix G: ( m x m) [g]
  SDP_G,
  /// The matrix A: ( nc x n) [a]
  SDP_A,
  /// Lower bounds on Ax ( nc x 1) [lba]
  SDP_LBA,
  /// Upper bounds on Ax  ( nc x 1) [uba]
  SDP_UBA,
  /// Lower bounds on x ( n x 1 ) [lbx]
  SDP_LBX,
  /// Upper bounds on x ( n x 1 ) [ubx]
  SDP_UBX,
  SDP_NUM_IN};

/// Output arguments of an SDP Solver [sdpOut]
enum SDPOutput{
  /// The primal solution (n x 1) - may be used as initial guess [x]
  SDP_X,
  /// The solution P (m x m) - may be used as initial guess [p]
  SDP_P,
  /// The dual solution (m x m) - may be used as initial guess [dual]
  SDP_DUAL,
  /// The primal optimal cost (1 x 1) [cost]
  SDP_COST,
  /// The dual optimal cost (1 x 1) [dual_cost]
  SDP_DUAL_COST,
  /// The dual solution corresponding to the linear constraints  (nc x 1) [lam_a]
  SDP_LAMBDA_A,
  /// The dual solution corresponding to simple bounds  (n x 1) [lam_x]
  SDP_LAMBDA_X,
  SDP_NUM_OUT};
  
/// Structure specification of an SDP [sdpStruct]
enum SDPStruct{
  /// The vertical stack of all matrices F_i: ( nm x m) [f]
  SDP_STRUCT_F,
  /// The matrix G: ( m x m) [g]
  SDP_STRUCT_G,
  /// The matrix A: ( nc x n) [a]
  SDP_STRUCT_A,
  SDP_STRUCT_NUM};
  
// Forward declaration of internal class
class SDPSolverInternal;

/** \brief SDPSolver


@copydoc SDPSolver_doc

  \author Joel Andersson 
  \date 2010
*/
class SDPSolver : public FX{
  public:

  /// Default constructor
  SDPSolver();
  
  /// Access functions of the node
  SDPSolverInternal* operator->();
  const SDPSolverInternal* operator->() const;

  /// Check if the node is pointing to the right type of object
  virtual bool checkNode() const;
};

} // namespace CasADi

#endif // SDP_SOLVER_HPP

