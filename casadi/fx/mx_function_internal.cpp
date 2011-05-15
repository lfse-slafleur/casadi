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

#include "mx_function_internal.hpp"

#include "../stl_vector_tools.hpp"

#include <stack>
#include <typeinfo>

using namespace std;

namespace CasADi{

MXFunctionInternal::MXFunctionInternal(const std::vector<MX>& inputv_, const std::vector<MX>& outputv_) : inputv(inputv_), outputv(outputv_){
  setOption("name", "unnamed_mx_function");

  liftfun_ = 0;
  liftfun_ud_ = 0;

  for(int i=0; i<inputv_.size(); ++i) {
    if (inputv_[i].isNull()) {
      stringstream ss;
      ss << "MXFunctionInternal::MXFunctionInternal: MXfunction input arguments cannot be null." << endl;
      ss << "Argument #" << i << " is null." << endl;
      throw CasadiException(ss.str());
    } else if (!inputv_[i]->isSymbolic()) {
      stringstream ss;
      ss << "MXFunctionInternal::MXFunctionInternal: MXfunction input arguments must be purely symbolic." << endl;
      ss << "Argument #" << i << " is not symbolic." << endl;
      throw CasadiException(ss.str());
    }
  }
  
  // Allocate space for inputs
  setNumInputs(inputv.size());
  for(int i=0; i<input_.size(); ++i)
    input(i) = DMatrix(inputv[i].sparsity());

  // Allocate space for outputs
  setNumOutputs(outputv.size());
  for(int i=0; i<output_.size(); ++i)
    output(i) = DMatrix(outputv[i].sparsity());
}


MXFunctionInternal::~MXFunctionInternal(){
}

void MXFunctionInternal::init(){
  log("MXFunctionInternal::init begin");
  
  // Call the init function of the base class
  XFunctionInternal::init();

  // Stack for nodes to be added to the list of nodes
  stack<MXNode*> s;

  // Add the inputs to the stack
  for(vector<MX>::iterator it = inputv.begin(); it!=inputv.end(); ++it)
    if(it->get()) s.push(static_cast<MXNode*>(it->get()));

  // Add the outputs to the stack
  for(vector<MX>::iterator it = outputv.begin(); it!=outputv.end(); ++it)
    if(it->get()) s.push(static_cast<MXNode*>(it->get()));

  // All evaluation nodes in the order of evaluation
  vector<MXNode*> nodes;

  // Order the nodes in the order of dependencies using a depth-first topological sorting
  sort_depth_first(s,nodes);
  
  // TODO: Make sure that the output nodes are placed directly after the corresponding multiple output node

  // Resort the nodes in a more cache friendly order (Kahn 1962)
  //  resort_bredth_first(nodes);
  
  // Set the temporary variables to be the corresponding place in the sorted graph
  for(int i=0; i<nodes.size(); ++i)
    nodes[i]->temp = i;
  
  // Indices corresponding to the inputs
  inputv_ind.resize(inputv.size());
  for(int i=0; i<inputv_ind.size(); ++i)
    inputv_ind[i] = inputv[i]->temp;

  // Indices corresponding to the outputs
  outputv_ind.resize(outputv.size());
  for(int i=0; i<outputv_ind.size(); ++i)
    outputv_ind[i] = outputv[i]->temp;
  
  // Create runtime elements for each node
  alg.resize(nodes.size());
  for(int ii=0; ii<alg.size(); ++ii){
    MX &m = alg[ii].mx;
    AlgEl* it = &alg[ii];
    
    // Save the node
    it->mx.assignNode(nodes[ii]);
    it->val.data = Matrix<double>(m->sparsity());
    it->val.dataF.resize(nfdir_);
    it->val.dataA.resize(nadir_);
    it->val.init();

    // Save the indices of the children nodes
    it->ch.resize(m->ndep());
    for(int i=0; i<m->ndep(); ++i){
      if(m->dep(i).isNull())
        it->ch[i] = -1;
      else
        it->ch[i] = m->dep(i)->temp;
    }
    
    it->input.resize(m->ndep(),0);
    it->fwdSeed.resize(m->ndep());
    it->adjSens.resize(m->ndep());
    for(int i=0; i<m->ndep(); ++i){
      it->fwdSeed[i].resize(nfdir_,0);
      it->adjSens[i].resize(nadir_,0);
      if(it->ch[i]>=0){
        it->input[i] = &alg[it->ch[i]].val.data[0];
        for(int d=0; d<nfdir_; ++d)
          it->fwdSeed[i][d] = &alg[it->ch[i]].val.dataF[d][0];
        for(int d=0; d<nadir_; ++d)
          it->adjSens[i][d] = &alg[it->ch[i]].val.dataA[d][0];
      }
    }

    it->output = &it->val.data[0];
    it->fwdSens.resize(nfdir_);
    for(int d=0; d<nfdir_; ++d)
      it->fwdSens[d] = &it->val.dataF[d][0];
    it->adjSeed.resize(nadir_);
    for(int d=0; d<nadir_; ++d)
      it->adjSeed[d] = &it->val.dataA[d][0];
    
  }
  
  // Reset the temporary variables
  for(int i=0; i<nodes.size(); ++i){
    nodes[i]->temp = 0;
  }
  
  
  log("MXFunctionInternal::init end");
}

void MXFunctionInternal::setLiftingFunction(LiftingFunction liftfun, void* user_data){
  liftfun_ = liftfun;
  liftfun_ud_ = user_data;
}

void MXFunctionInternal::evaluate(int nfdir, int nadir){
  log("MXFunctionInternal::evaluate begin");
  
  // Pass the inputs
  for(int ind=0; ind<input_.size(); ++ind)
    alg[inputv_ind[ind]].val.data.set(input(ind));

  // Pass the forward seeds
  for(int dir=0; dir<nfdir; ++dir)
    for(int ind=0; ind<input_.size(); ++ind)
      alg[inputv_ind[ind]].val.dataF.at(dir).set(fwdSeed(ind,dir));
  
  // Evaluate all of the nodes of the algorithm: should only evaluate nodes that have not yet been calculated!
  for(vector<AlgEl>::iterator it=alg.begin(); it!=alg.end(); it++){
    it->mx->evaluate(it->input, it->output, it->fwdSeed, it->fwdSens, it->adjSeed, it->adjSens, nfdir, 0);
    // Lifting
    if(liftfun_ && it->mx->isNonLinear()){
      liftfun_(it->output,it->mx.size(),liftfun_ud_);
    }
  }
  
  log("MXFunctionInternal::evaluate evaluated forward");

  // Get the outputs
  for(int ind=0; ind<outputv.size(); ++ind)
    alg[outputv_ind[ind]].val.data.get(output(ind));

  // Get the forward sensitivities
  for(int dir=0; dir<nfdir; ++dir)
    for(int ind=0; ind<outputv.size(); ++ind)
      alg[outputv_ind[ind]].val.dataF.at(dir).get(fwdSens(ind,dir));
          
  if(nadir>0){

    // Clear the adjoint seeds
    for(vector<AlgEl>::iterator it=alg.begin(); it!=alg.end(); it++)
      for(int dir=0; dir<nadir; ++dir)
        fill(it->val.dataA.at(dir).begin(),it->val.dataA.at(dir).end(),0.0);

    // Pass the adjoint seeds
    for(int ind=0; ind<outputv.size(); ++ind)
      for(int dir=0; dir<nadir; ++dir)
        alg[outputv_ind[ind]].val.dataA.at(dir).set(adjSeed(ind,dir));

    // Evaluate all of the nodes of the algorithm: should only evaluate nodes that have not yet been calculated!
    for(vector<AlgEl>::reverse_iterator it=alg.rbegin(); it!=alg.rend(); it++){
      it->mx->evaluate(it->input, it->output, it->fwdSeed, it->fwdSens, it->adjSeed, it->adjSens, 0, nadir);
    }

    // Get the adjoint sensitivities
    for(int ind=0; ind<input_.size(); ++ind)
      for(int dir=0; dir<nadir; ++dir)
        alg[inputv_ind[ind]].val.dataA.at(dir).get(adjSens(ind,dir));
    
    log("MXFunctionInternal::evaluate evaluated adjoint");
  }
  log("MXFunctionInternal::evaluate end");
}

void MXFunctionInternal::print(ostream &stream) const{
  for(int i=0; i<alg.size(); ++i){
    stream << "i_" << i<< " =  ";
    vector<string> chname(alg[i].ch.size());
    for(int j=0; j<chname.size(); ++j){
      if(alg[i].ch[j]>=0){
        stringstream ss;
        ss << "i_" << alg[i].ch[j];
        chname[j] = ss.str();
      } else {
        chname[j] = "[]";
      }
    }
    alg[i].mx->print(stream, chname);
    stream << endl;
  }
}

MXFunctionInternal* MXFunctionInternal::clone() const{
  MXFunctionInternal* node = new MXFunctionInternal(inputv,outputv);
  node->setOption(dictionary());
  if(isInit()) node->init();
  return node;
}

  
std::vector<MX> MXFunctionInternal::jac(int iind){
  casadi_assert(isInit());
  
  // Number of columns in the Jacobian
  int ncol = alg[inputv_ind[iind]].mx.size1();
  
  // Get theseed matrices
  vector<MX> fseed(input_.size());
  for(int ind=0; ind<input_.size(); ++ind){
    // Number of rows in the seed matrix
    int nrow = alg[inputv_ind[ind]].mx.size1();
    
    // Create seed matrix
    fseed[ind] = ind==iind ? MX::eye(ncol) : MX::zeros(nrow,ncol);
   }
  
  // Forward mode automatic differentiation, symbolically
  return adFwd(fseed);
}

std::vector<MX> MXFunctionInternal::adFwd(const std::vector<MX>& fseed){
  casadi_assert(isInit());
  casadi_warning("MXFunctionInternal::jac: the feature is still experimental");
  
  // Get the number of columns
  const int ncol = fseed.front().size2();
  
  // Make sure that the number of columns is consistent
  for(vector<MX>::const_iterator it=fseed.begin(); it!=fseed.end(); ++it){
    casadi_assert_message(ncol==it->size2(),"Number of columns in seed matrices not consistent.");
  }
  
  // Directional derivative for each node
  std::vector<MX> derwork(alg.size());
  
  // Pass the seed matrices for the symbolic variables
  for(int ind=0; ind<input_.size(); ++ind){
    derwork[inputv_ind[ind]] = fseed[ind];
   }
   
  // Evaluate all the seed matrices of the algorithm sequentially
  for(int el=0; el<derwork.size(); ++el){
    
    // Skip the node if it has already been calculated (i.e. is symbolic or constant)
    if(!derwork[el].isNull()) continue;
    
    // Zero seed matrix if constant
    if(alg[el].mx->isConstant()){
      
      // Number of rows
      int nrow = alg[el].mx.numel();
      
      // Save zero matrix
      derwork[el] = MX::zeros(nrow,ncol);
      continue;
    }
    
    // Collect the seed matrices
    vector<MX> seed(alg[el].ch.size());
    casadi_assert(!seed.empty());
    for(int i=0; i<seed.size(); ++i){
      int ind = alg[el].ch[i];
      if(ind>=0)
        seed[i] = derwork[ind];
    }

    // Get the sensitivity matrix
    MX sens = alg[el].mx->adFwd(seed);
    
    // Save to the memory vector
    derwork[el] = sens;
  }

  // Collect the symbolic forward sensitivities
  vector<MX> ret(outputv.size());
  for(int ind=0; ind<outputv.size(); ++ind){
    ret[ind] = derwork[outputv_ind[ind]];
  }
  return ret;
}



} // namespace CasADi

