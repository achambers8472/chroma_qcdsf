// $Id: prec_fermact_qprop_array.cc,v 1.1 2003-11-23 06:17:28 edwards Exp $
/*! \file
 *  \brief Propagator solver for a generic even-odd preconditioned fermion operator
 *
 *  Solve for the propagator of a even-odd non-preconditioned fermion operator
 */

#include "chromabase.h"
#include "fermact.h"
#include "actions/ferm/invert/invcg2_array.h"

using namespace QDP;

//! Propagator of a generic even-odd preconditioned fermion linear operator
/*! \ingroup qprop
 *
 * This routine is actually generic to all even-odd preconditioned fermions
 *
 * \param psi      initial solution ( Modify )
 * \param u        gauge field ( Read )
 * \param chi      source ( Read )
 * \param invType  inverter type ( Read (
 * \param RsdCG    CG (or MR) residual used here ( Read )
 * \param MaxCG    maximum number of CG iterations ( Read )
 * \param ncg_had  number of CG iterations ( Write )
 */

template<typename T>
static 
void qprop_t(const EvenOddPrecWilsonTypeFermAct< multi1d<T> >& me,
	     multi1d<T>& psi, 
	     const multi1d<LatticeColorMatrix>& u, 
	     const multi1d<T>& chi, 
	     enum InvType invType,
	     const Real& RsdCG, 
	     int MaxCG, int& ncg_had)
{
  START_CODE("EvenOddPrecWilsonTypeFermActArray::qprop");

  int n_count;
  
  /* Construct the linear operator */
  /* This allocates field for the appropriate action */
  const EvenOddPrecLinearOperator< multi1d<T> >* A = me.linOp(u);

  /* Step (i) */
  /* chi_tmp =  chi_o - D_oe * A_ee^-1 * chi_o */
  multi1d<T> chi_tmp(me.size());
  {
    multi1d<T> tmp1(me.size());
    multi1d<T> tmp2(me.size());

    A->evenEvenInvLinOp(tmp1, chi, PLUS);
    A->oddEvenLinOp(tmp2, tmp1, PLUS);
    for(int n=0; n < me.size(); ++n)
      chi_tmp[n][rb[1]] = chi[n] - tmp2[n];
  }

  switch(invType)
  {
  case CG_INVERTER: 
  {
    /* chi_1 = M_dag(u) * chi_1 */
    multi1d<T> tmp(me.size());
    (*A)(tmp, chi, MINUS);
    
    /* psi = (M^dag * M)^(-1) chi */
    InvCG2 (*A, tmp, psi, RsdCG, MaxCG, n_count);
  }
  break;
  
#if 0
  case MR_INVERTER:
    /* psi = M^(-1) chi */
    InvMR (*A, chi, psi, MRover, RsdCG, MaxCG, n_count);
    break;

  case BICG_INVERTER:
    /* psi = M^(-1) chi */
    InvBiCG (*A, chi, psi, RsdCG, MaxCG, n_count);
    break;
#endif
  
  default:
    QDP_error_exit("Unknown inverter type", invType);
  }
  
  if ( n_count == MaxCG )
    QDP_error_exit("no convergence in the inverter", n_count);
  
  ncg_had = n_count;
  
  /* Step (ii) */
  /* psi_e = A_ee^-1 * [chi_e  -  D_eo * psi_o] */
  {
    multi1d<T> tmp1(me.size());
    multi1d<T> tmp2(me.size());

    A->evenOddLinOp(tmp1, psi, PLUS);
    for(int n=0; n < me.size(); ++n)
      tmp2[n][rb[0]] = chi[n] - tmp1[n];
    A->evenEvenInvLinOp(psi, tmp2, PLUS);
  }
  
  // Call the virtual destructor of A
  delete A;

  END_CODE("EvenOddPrecWilsonTypeFermActArray::qprop");
}


template<>
void 
EvenOddPrecWilsonTypeFermAct< multi1d<LatticeFermion> >::qpropT(multi1d<LatticeFermion>& psi, 
								const multi1d<LatticeColorMatrix>& u, 
								const multi1d<LatticeFermion>& chi, 
								enum InvType invType,
								const Real& RsdCG, 
								int MaxCG, int& ncg_had) const
{
  qprop_t<LatticeFermion>(*this, psi, u, chi, invType, RsdCG, MaxCG, ncg_had);
}





