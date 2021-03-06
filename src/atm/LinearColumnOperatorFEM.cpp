///////////////////////////////////////////////////////////////////////////////
///
///	\file    LinearColumnOperatorFEM.cpp
///	\author  Paul Ullrich
///	\version July 29, 2014
///
///	<remarks>
///		Copyright 2000-2010 Paul Ullrich
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#include "LinearColumnOperatorFEM.h"

#include "FluxCorrectionFunction.h"

#include "PolynomialInterp.h"

#include "GaussLobattoQuadrature.h"

#include <cmath>

///////////////////////////////////////////////////////////////////////////////
/// LinearColumnInterpFEM
///////////////////////////////////////////////////////////////////////////////

void LinearColumnInterpFEM::Initialize(
	InterpSource eInterpSource,
	int nVerticalOrder,
	const DataVector<double> & dREtaNode,
	const DataVector<double> & dREtaREdge,
	const DataVector<double> & dREtaOut,
	bool fZeroBoundaries
) {
	const double ParamEpsilon = 1.0e-12;

	const int nRElementsIn  = dREtaNode.GetRows();
	const int nRElementsOut = dREtaOut.GetRows();

	const int nFiniteElementsIn = nRElementsIn / nVerticalOrder;

	// Verify input parameters
	if (nRElementsIn == 0) {
		_EXCEPTIONT("At least one row required for dREtaNode");
	}
	if (dREtaREdge.GetRows() != nRElementsIn + 1) {
		_EXCEPTIONT("REtaNode / REtaREdge mismatch");
	}
	if (nRElementsIn % nVerticalOrder != 0) {
		_EXCEPTIONT("Column RElements / VerticalOrder mismatch");
	}

	// Initialize LinearColumnOperator
	if (eInterpSource == InterpSource_Interfaces) {
		LinearColumnOperator::Initialize(nRElementsIn+1, nRElementsOut);
	} else {
		LinearColumnOperator::Initialize(nRElementsIn, nRElementsOut);
	}

	// If zero boundaries is set, don't affect nodes at REta = 0 or REta = 1
	int lBegin = 0;
	int lEnd = nRElementsOut;

	if ((fZeroBoundaries) && (fabs(dREtaOut[0]) < ParamEpsilon)) {
		lBegin = 1;
	}
	if ((fZeroBoundaries) &&
	    (fabs(dREtaOut[dREtaOut.GetRows()-1] - 1.0) < ParamEpsilon)
	) {
		lEnd = nRElementsOut-1;
	}

	// Loop through all output elements
	for (int l = lBegin; l < lEnd; l++) {

		// Determine input element index
		bool fOnREdge = false;
		int a;

		for (a = 0; a < nFiniteElementsIn - 1; a++) {
			double dNextREtaREdgeIn =
				dREtaREdge[(a+1) * nVerticalOrder] - ParamEpsilon;

			if (dREtaOut[l] < dNextREtaREdgeIn) {
				break;
			}
			if (dREtaOut[l] < dNextREtaREdgeIn + 2.0 * ParamEpsilon) {
				fOnREdge = true;
				break;
			}
		}

		// Interpolation coefficients for a continuous basis
		if (eInterpSource == InterpSource_Interfaces) {

			if (fOnREdge) {
				m_dCoeff[l][(a+1) * nVerticalOrder] = 1.0;

			} else {
				PolynomialInterp::LagrangianPolynomialCoeffs(
					nVerticalOrder + 1,
					&(dREtaREdge[a * nVerticalOrder]),
					&(m_dCoeff[l][a * nVerticalOrder]),
					dREtaOut[l]);

				m_iBegin[l] =  a    * nVerticalOrder;
				m_iEnd[l]   = (a+1) * nVerticalOrder + 1;
			}

		// Interpolation coefficients for a discontinuous basis
		} else {

			// For nVerticalOrder = 1, override default O(dx) interpolant
			if ((nVerticalOrder == 1) && (l == 0)) {
				PolynomialInterp::LagrangianPolynomialCoeffs(
					nVerticalOrder + 1,
					&(dREtaNode[a * nVerticalOrder]),
					&(m_dCoeff[l][a * nVerticalOrder]),
					dREtaOut[l]);

				m_iBegin[l] =  a    * nVerticalOrder;
				m_iEnd[l]   = (a+2) * nVerticalOrder;

			} else if ((nVerticalOrder == 1) && (l == nRElementsOut-1)) {
				PolynomialInterp::LagrangianPolynomialCoeffs(
					nVerticalOrder + 1,
					&(dREtaNode[(a-1) * nVerticalOrder]),
					&(m_dCoeff[l][(a-1) * nVerticalOrder]),
					dREtaOut[l]);

				m_iBegin[l] = (a-1) * nVerticalOrder;
				m_iEnd[l]   = (a+1) * nVerticalOrder;

			} else {
				PolynomialInterp::LagrangianPolynomialCoeffs(
					nVerticalOrder,
					&(dREtaNode[a * nVerticalOrder]),
					&(m_dCoeff[l][a * nVerticalOrder]),
					dREtaOut[l]);

				m_iBegin[l] =  a    * nVerticalOrder;
				m_iEnd[l]   = (a+1) * nVerticalOrder;
			}

			// Interpolating from nodes to edges, weight left and right
			// interpolant to minimize error.  Note that this approach does not
			// guarantee minimum error for sub-element stretching of REta.
			if (fOnREdge) {

				// Calculate one-sided errors and interpolant weights
				double dDeltaREtaL =
					  dREtaREdge[(a+1) * nVerticalOrder]
					- dREtaREdge[ a    * nVerticalOrder];

				double dDeltaREtaR =
					  dREtaREdge[(a+2) * nVerticalOrder]
					- dREtaREdge[(a+1) * nVerticalOrder];

				double dErrorL =
					pow(dDeltaREtaL, static_cast<double>(nVerticalOrder));

				double dErrorR =
					pow(dDeltaREtaR, static_cast<double>(nVerticalOrder));

				double dWeightL = dErrorR / (dErrorL + dErrorR);
				double dWeightR = dErrorL / (dErrorL + dErrorR);

				// Right interpolant coefficients
				PolynomialInterp::LagrangianPolynomialCoeffs(
					nVerticalOrder,
					&(dREtaNode[(a+1) * nVerticalOrder]),
					&(m_dCoeff[l][m_iEnd[l]]),
					dREtaOut[l]);

				// Weight interpolants by error
				for (int k = m_iBegin[l]; k < m_iEnd[l]; k++) {
					m_dCoeff[l][k] *= dWeightL;
				}

				int iNewEnd = m_iEnd[l] + nVerticalOrder;
				for (int k = m_iEnd[l]; k < iNewEnd; k++) {
					m_dCoeff[l][k] *= dWeightR;
				}

				m_iEnd[l] = iNewEnd;
			}
		}
	}
/*
	// DEBUGGING
	if ((m_dCoeff.GetRows() == dREtaREdge.GetRows()) &&
	    (m_dCoeff.GetColumns() == dREtaNode.GetRows())
	) {
		DebugOutput(&dREtaNode, &dREtaREdge);
	}
*/
}

///////////////////////////////////////////////////////////////////////////////
/// LinearColumnDiffFEM
///////////////////////////////////////////////////////////////////////////////

void LinearColumnDiffFEM::InitializeInterfaceMethod(
	InterpSource eInterpSource,
	int nVerticalOrder,
	const DataVector<double> & dREtaNode,
	const DataVector<double> & dREtaREdge,
	const DataVector<double> & dREtaOut,
	bool fZeroBoundaries
) {
	//const int ParamFluxCorrectionType = 2;

	const double ParamEpsilon = 1.0e-12;

	const int nRElementsIn  = dREtaNode.GetRows();
	const int nRElementsOut = dREtaOut.GetRows();

	const int nFiniteElements = nRElementsIn / nVerticalOrder;

	// Verify input parameters
	if (nRElementsIn == 0) {
		_EXCEPTIONT("At least one row required for dREtaNode");
	}
	if (dREtaREdge.GetRows() != nRElementsIn + 1) {
		_EXCEPTIONT("REtaNode / REtaREdge mismatch");
	}
	if (nRElementsIn % nVerticalOrder != 0) {
		_EXCEPTIONT("Column RElements / VerticalOrder mismatch");
	}

	// Initialize LinearColumnOperator for differentiation from interfaces
	LinearColumnOperator::Initialize(nRElementsIn+1, nRElementsOut);

	// Loop through all output elements
	for (int l = 0; l < nRElementsOut; l++) {

		// Determine input element index and whether we are on a finite
		// element edge (excluding top and bottom boundary)
		bool fOnREdge = false;
		int a;

		for (a = 0; a < nFiniteElements - 1; a++) {
			double dNextREtaREdgeIn =
				dREtaREdge[(a+1) * nVerticalOrder] - ParamEpsilon;

			if (dREtaOut[l] < dNextREtaREdgeIn) {
				break;
			}
			if (dREtaOut[l] < dNextREtaREdgeIn + 2.0 * ParamEpsilon) {
				fOnREdge = true;
				break;
			}
		}

		// Construct interpolator from interfaces to output location
		PolynomialInterp::DiffLagrangianPolynomialCoeffs(
			nVerticalOrder + 1,
			&(dREtaREdge[a * nVerticalOrder]),
			&(m_dCoeff[l][a * nVerticalOrder]),
			dREtaOut[l]);

		// Set bounds on coefficients
		if (!fOnREdge) {
			m_iBegin[l] =  a * nVerticalOrder;
			m_iEnd[l]   = (a+1) * nVerticalOrder + 1;
		}

		// Special treatment of derivatives at interfaces
		if (fOnREdge) {

			// Temporary coefficients
			DataVector<double> dTempCoeff;
			dTempCoeff.Initialize(nVerticalOrder + 1);
			
			// Calculate one-sided errors and derivative weights
			double dDeltaREtaL =
				  dREtaREdge[(a+1) * nVerticalOrder]
				- dREtaREdge[ a    * nVerticalOrder];

			double dDeltaREtaR =
				  dREtaREdge[(a+2) * nVerticalOrder]
				- dREtaREdge[(a+1) * nVerticalOrder];

			double dErrorL =
				pow(dDeltaREtaL, static_cast<double>(nVerticalOrder));

			double dErrorR =
				pow(dDeltaREtaR, static_cast<double>(nVerticalOrder));

			double dWeightL = dErrorR / (dErrorL + dErrorR);
			double dWeightR = dErrorL / (dErrorL + dErrorR);

			// Calculate right-side derivative coefficients
			PolynomialInterp::DiffLagrangianPolynomialCoeffs(
				nVerticalOrder + 1,
				&(dREtaREdge[(a+1) * nVerticalOrder]),
				&(dTempCoeff[0]),
				dREtaOut[l]);

			for (int k = 0; k <= nVerticalOrder; k++) {
				m_dCoeff[l][a * nVerticalOrder + k] *= dWeightL;
			}

			for (int k = 0; k <= nVerticalOrder; k++) {
				m_dCoeff[l][(a+1) * nVerticalOrder + k] +=
					dWeightR * dTempCoeff[k];
			}

			// Set bounds on coefficients
			m_iBegin[l] =  a * nVerticalOrder;
			m_iEnd[l]   = (a+2) * nVerticalOrder + 1;
		}
	}

	// If the source is nodes, compose the differentiation operator with
	// an interpolation operator.
	if (eInterpSource == InterpSource_Levels) {
		LinearColumnInterpFEM opInterp;

		opInterp.Initialize(
			LinearColumnInterpFEM::InterpSource_Levels,
			nVerticalOrder,
			dREtaNode,
			dREtaREdge,
			dREtaREdge,
			fZeroBoundaries);

		ComposeWith(opInterp);
	}
/*
	// DEBUGGING
	if ((m_dCoeff.GetRows() == dREtaREdge.GetRows()) &&
	    (m_dCoeff.GetColumns() == dREtaREdge.GetRows())
	) {
		DebugOutput(&dREtaNode, &dREtaREdge);
	}
*/
}

///////////////////////////////////////////////////////////////////////////////

void LinearColumnDiffFEM::InitializeFluxCorrectionMethod(
	InterpSource eInterpSource,
	int nVerticalOrder,
	const DataVector<double> & dREtaNode,
	const DataVector<double> & dREtaREdge,
	const DataVector<double> & dREtaOut,
	bool fZeroBoundaries
) {
	const int ParamFluxCorrectionType = 2;

	const double ParamEpsilon = 1.0e-12;

	const int nRElementsIn  = dREtaNode.GetRows();
	const int nRElementsOut = dREtaOut.GetRows();

	const int nFiniteElements = nRElementsIn / nVerticalOrder;

	// Verify input parameters
	if (nRElementsIn == 0) {
		_EXCEPTIONT("At least one row required for dREtaNode");
	}
	if (dREtaREdge.GetRows() != nRElementsIn + 1) {
		_EXCEPTIONT("REtaNode / REtaREdge mismatch");
	}
	if (nRElementsIn % nVerticalOrder != 0) {
		_EXCEPTIONT("Column RElements / VerticalOrder mismatch");
	}

	// Initialize LinearColumnOperator for differentiation from interfaces
	if (eInterpSource == InterpSource_Interfaces) {
		LinearColumnOperator::Initialize(nRElementsIn+1, nRElementsOut);
	} else {
		LinearColumnOperator::Initialize(nRElementsIn, nRElementsOut);
	}

	// Loop through all output elements
	for (int l = 0; l < nRElementsOut; l++) {

		// Determine input element index and whether we are on a finite
		// element edge (excluding top and bottom boundary)
		bool fOnREdge = false;
		int a;

		for (a = 0; a < nFiniteElements - 1; a++) {
			double dNextREtaREdgeIn =
				dREtaREdge[(a+1) * nVerticalOrder] - ParamEpsilon;

			if (dREtaOut[l] < dNextREtaREdgeIn) {
				break;
			}
			if (dREtaOut[l] < dNextREtaREdgeIn + 2.0 * ParamEpsilon) {
				fOnREdge = true;
				break;
			}
		}

		if ((dREtaOut[l] < dREtaREdge[0]) ||
			(dREtaOut[l] > dREtaREdge[nRElementsIn])
		) {
			_EXCEPTIONT("REta out of range");
		}

		// Element spacing
		double dDeltaREta = 
			  dREtaREdge[(a+1) * nVerticalOrder]
			- dREtaREdge[ a    * nVerticalOrder];

		// Contribution due to local derivative
		PolynomialInterp::DiffLagrangianPolynomialCoeffs(
			nVerticalOrder,
			&(dREtaNode[a * nVerticalOrder]),
			&(m_dCoeff[l][a * nVerticalOrder]),
			dREtaOut[l]);

		if (fOnREdge) {
			PolynomialInterp::DiffLagrangianPolynomialCoeffs(
				nVerticalOrder,
				&(dREtaNode[(a+1) * nVerticalOrder]),
				&(m_dCoeff[l][(a+1) * nVerticalOrder]),
				dREtaOut[l]);

			for (int k = 0; k < nRElementsIn; k++) {
				m_dCoeff[l][k] *= 0.5 * dDeltaREta;
			}

		} else {
			for (int k = 0; k < nRElementsIn; k++) {
				m_dCoeff[l][k] *= dDeltaREta;
			}
		}

		// Interpolation coefficients to finite element edges
		DataVector<double> dTempCoeffLL;
		dTempCoeffLL.Initialize(nVerticalOrder);

		DataVector<double> dTempCoeffLR;
		dTempCoeffLR.Initialize(nVerticalOrder);

		DataVector<double> dTempCoeffRL;
		dTempCoeffRL.Initialize(nVerticalOrder);

		DataVector<double> dTempCoeffRR;
		dTempCoeffRR.Initialize(nVerticalOrder);

		// Derivatives of the flux correction function at this point
		DataVector<double> dNodesR;
		dNodesR.Initialize(1);
		dNodesR[0] =
			(dREtaOut[l] - dREtaREdge[a * nVerticalOrder]) / dDeltaREta;

		DataVector<double> dNodesL;
		dNodesL.Initialize(1);
		dNodesL[0] = 1.0 - dNodesR[0];

		DataVector<double> dDerivR;
		dDerivR.Initialize(1);

		DataVector<double> dDerivL;
		dDerivL.Initialize(1);

		FluxCorrectionFunction::GetDerivatives(
			ParamFluxCorrectionType,
			nVerticalOrder + 1,
			dNodesR,
			dDerivR);

		FluxCorrectionFunction::GetDerivatives(
			ParamFluxCorrectionType,
			nVerticalOrder + 1,
			dNodesL,
			dDerivL);

		dDerivL[0] *= -1.0;

		// Interpolation coefficients to element interfaces
		PolynomialInterp::LagrangianPolynomialCoeffs(
			nVerticalOrder,
			&(dREtaNode[a * nVerticalOrder]),
			&(dTempCoeffLR[0]),
			dREtaREdge[a * nVerticalOrder]);

		PolynomialInterp::LagrangianPolynomialCoeffs(
			nVerticalOrder,
			&(dREtaNode[a * nVerticalOrder]),
			&(dTempCoeffRL[0]),
			dREtaREdge[(a+1) * nVerticalOrder]);

		if (a != 0) {
			PolynomialInterp::LagrangianPolynomialCoeffs(
				nVerticalOrder,
				&(dREtaNode[(a-1) * nVerticalOrder]),
				&(dTempCoeffLL[0]),
				dREtaREdge[a * nVerticalOrder]);
		}

		if (a != nFiniteElements - 1) {
			PolynomialInterp::LagrangianPolynomialCoeffs(
				nVerticalOrder,
				&(dREtaNode[(a+1) * nVerticalOrder]),
				&(dTempCoeffRR[0]),
				dREtaREdge[(a+1) * nVerticalOrder]);
		}

		// Apply edge interpolation coefficients
		if (a != 0) {
			if (!fOnREdge) {
				for (int k = 0; k < nVerticalOrder; k++) {
					m_dCoeff[l][(a-1) * nVerticalOrder + k] +=
						0.5 * dDerivL[0] * dTempCoeffLL[k];
				}
			}

			for (int k = 0; k < nVerticalOrder; k++) {
				m_dCoeff[l][a * nVerticalOrder + k] -=
					0.5 * dDerivL[0] * dTempCoeffLR[k];
			}

		} else {
			if ((!fZeroBoundaries) && (nFiniteElements != 1)) {
				for (int k = 0; k < nVerticalOrder; k++) {
					m_dCoeff[l][ a    * nVerticalOrder + k] +=
						0.5 * dDerivL[0] * dTempCoeffRL[k];
					m_dCoeff[l][(a+1) * nVerticalOrder + k] -=
						0.5 * dDerivL[0] * dTempCoeffRR[k];
				}
			}
		}

		if (a != nFiniteElements - 1) {
			for (int k = 0; k < nVerticalOrder; k++) {
				m_dCoeff[l][(a+1) * nVerticalOrder + k] +=
					0.5 * dDerivR[0] * dTempCoeffRR[k];
			}
			for (int k = 0; k < nVerticalOrder; k++) {
				m_dCoeff[l][a * nVerticalOrder + k] -=
					0.5 * dDerivR[0] * dTempCoeffRL[k];
			}

		} else {
			if ((!fZeroBoundaries) && (nFiniteElements != 1)) {
				for (int k = 0; k < nVerticalOrder; k++) {
					m_dCoeff[l][ a    * nVerticalOrder + k] +=
						0.5 * dDerivR[0] * dTempCoeffLR[k];
					m_dCoeff[l][(a-1) * nVerticalOrder + k] -=
						0.5 * dDerivR[0] * dTempCoeffLL[k];
				}
			}
		}

		for (int k = 0; k < nRElementsIn; k++) {
			m_dCoeff[l][k] /= dDeltaREta;
		}

		// Set boundaries
		if (a != 0) {
			m_iBegin[l] = (a-1) * nVerticalOrder;
		} else {
			m_iBegin[l] =  a    * nVerticalOrder;
		}

		if (a != nFiniteElements-1) {
			m_iEnd[l]   = (a+2) * nVerticalOrder;
		} else {
			m_iEnd[l]   = (a+1) * nVerticalOrder;
		}
	}
/*
	// DEBUGGING
	if ((m_dCoeff.GetRows() == dREtaREdge.GetRows()) &&
	    (m_dCoeff.GetColumns() == dREtaNode.GetRows())
	) {
		DebugOutput(&dREtaNode, &dREtaREdge);
	}
*/
}

///////////////////////////////////////////////////////////////////////////////

void LinearColumnDiffFEM::InitializeGLLNodes(
	int nVerticalOrder,
	const DataVector<double> & dREtaNode,
	const DataVector<double> & dREtaOut
) {

	const double ParamEpsilon = 1.0e-12;

	const int nRElementsIn  = dREtaNode.GetRows();
	const int nRElementsOut = dREtaOut.GetRows();

	const int nFiniteElements = (nRElementsIn - 1) / (nVerticalOrder - 1);

	// Verify input parameters
	if (nRElementsIn == 0) {
		_EXCEPTIONT("At least one row required for dREtaNode");
	}
	if ((nRElementsIn - 1) % (nVerticalOrder - 1) != 0) {
		_EXCEPTIONT("Column (RElements-1) / (VerticalOrder-1) mismatch");
	}

	// Initialize LinearColumnOperator for differentiation from interfaces
	LinearColumnOperator::Initialize(nRElementsIn, nRElementsOut);

	// Loop through all output elements
	for (int l = 0; l < nRElementsOut; l++) {

		// Determine input element index and whether we are on a finite
		// element edge (excluding top and bottom boundary)
		bool fOnREdge = false;
		int a;

		for (a = 0; a < nFiniteElements - 1; a++) {
			double dNextREtaREdgeIn =
				dREtaNode[(a+1) * (nVerticalOrder-1)] - ParamEpsilon;

			if (dREtaOut[l] < dNextREtaREdgeIn) {
				break;
			}
			if (dREtaOut[l] < dNextREtaREdgeIn + 2.0 * ParamEpsilon) {
				fOnREdge = true;
				break;
			}
		}

		// Construct interpolator from interfaces to output location
		PolynomialInterp::DiffLagrangianPolynomialCoeffs(
			nVerticalOrder,
			&(dREtaNode[a * (nVerticalOrder-1)]),
			&(m_dCoeff[l][a * (nVerticalOrder-1)]),
			dREtaOut[l]);

		// Set bounds on coefficients
		if (!fOnREdge) {
			m_iBegin[l] =  a * (nVerticalOrder-1);
			m_iEnd[l]   = (a+1) * (nVerticalOrder-1) + 1;
		}

		// Special treatment of derivatives at interfaces
		if (fOnREdge) {

			// Temporary coefficients
			DataVector<double> dTempCoeff;
			dTempCoeff.Initialize(nVerticalOrder);

			// Calculate one-sided errors and derivative weights
			double dDeltaREtaL =
				  dREtaNode[(a+1) * (nVerticalOrder-1)]
				- dREtaNode[ a    * (nVerticalOrder-1)];

			double dDeltaREtaR =
				  dREtaNode[(a+2) * (nVerticalOrder-1)]
				- dREtaNode[(a+1) * (nVerticalOrder-1)];

			double dErrorL =
				pow(dDeltaREtaL, static_cast<double>(nVerticalOrder-1));

			double dErrorR =
				pow(dDeltaREtaR, static_cast<double>(nVerticalOrder-1));

			double dWeightL = dErrorR / (dErrorL + dErrorR);
			double dWeightR = dErrorL / (dErrorL + dErrorR);

			// Calculate right-side derivative coefficients
			PolynomialInterp::DiffLagrangianPolynomialCoeffs(
				nVerticalOrder,
				&(dREtaNode[(a+1) * (nVerticalOrder-1)]),
				&(dTempCoeff[0]),
				dREtaOut[l]);

			for (int k = 0; k < nVerticalOrder; k++) {
				m_dCoeff[l][a * (nVerticalOrder-1) + k] *= dWeightL;
			}

			for (int k = 0; k < nVerticalOrder; k++) {
				m_dCoeff[l][(a+1) * (nVerticalOrder-1) + k] +=
					dWeightR * dTempCoeff[k];
			}

			// Set bounds on coefficients
			m_iBegin[l] =  a    * (nVerticalOrder-1);
			m_iEnd[l]   = (a+2) * (nVerticalOrder-1) + 1;
		}
	}
/*
	// DEBUGGING
	DebugOutput(&dREtaNode, &dREtaREdge);
*/
}

///////////////////////////////////////////////////////////////////////////////
/// LinearColumnDiffDiffFEM
///////////////////////////////////////////////////////////////////////////////

void LinearColumnDiffDiffFEM::Initialize(
	InterpSource eInterpSource,
	int nVerticalOrder,
	const DataVector<double> & dREtaNode,
	const DataVector<double> & dREtaREdge
) {
/*
	const double ParamEpsilon = 1.0e-12;

	const int nRElementsIn  = dREtaNode.GetRows();
	const int nRElementsOut = dREtaREdge.GetRows();

	const int nFiniteElements = nRElementsIn / nVerticalOrder;

	// Verify input parameters
	if (nRElementsIn == 0) {
		_EXCEPTIONT("At least one row required for dREtaNode");
	}
	if (nRElementsIn % nVerticalOrder != 0) {
		_EXCEPTIONT("Column RElements / VerticalOrder mismatch");
	}

	// Differentiation from levels to levels
	if (eInterpSource == InterpSource_Levels) {

		// Compute second differentiation coefficients from nodes to nodes
		m_dDiffDiffNodeToNode.Initialize(
			m_nVerticalOrder, m_nVerticalOrder);

		for (int n = 0; n < m_nVerticalOrder; n++) {
		for (int m = 0; m < m_nVerticalOrder; m++) {
			m_dDiffDiffNodeToNode[n][m] = 0.0;
			for (int s = 0; s < m_nVerticalOrder; s++) {
				m_dDiffDiffNodeToNode[n][m] -=
					  dDiffNodeToNode[s][n]
					* dDiffNodeToNode[s][m]
					* dW[s];
			}
			m_dDiffDiffNodeToNode[n][m] /= dW[n];
		}
		}

		// Scale by 1/dxi
		for (int n = 0; n < m_nVerticalOrder; n++) {
		for (int m = 0; m < m_nVerticalOrder; m++) {
			m_dDiffDiffNodeToNode[n][m] *= dElementDeltaXi;
		}
		}
	}
*/
}

///////////////////////////////////////////////////////////////////////////////

void LinearColumnDiffDiffFEM::InitializeGLLNodes(
	int nVerticalOrder,
	const DataVector<double> & dREtaNode
) {

	const double ParamEpsilon = 1.0e-12;

	const int nRElementsIn  = dREtaNode.GetRows();
	const int nRElementsOut = dREtaNode.GetRows();

	const int nFiniteElements = (nRElementsIn - 1) / (nVerticalOrder - 1);

	// Verify input parameters
	if (nRElementsIn == 0) {
		_EXCEPTIONT("At least one row required for dREtaNode");
	}
	if ((nRElementsIn - 1) % (nVerticalOrder - 1) != 0) {
		_EXCEPTIONT("Column (RElements-1) / (VerticalOrder-1) mismatch");
	}

	// Initialize LinearColumnOperator for differentiation from interfaces
	LinearColumnOperator::Initialize(nRElementsIn, nRElementsOut);

	// Local differentiation coefficients
	DataMatrix<double> dLocalDiffCoeff;
	dLocalDiffCoeff.Initialize(nVerticalOrder, nVerticalOrder);

	// Loop through all output elements
	for (int a = 0; a < nFiniteElements; a++) {

		// Gauss-Lobatto quadrature nodes
		DataVector<double> dG;
		DataVector<double> dW;
		GaussLobattoQuadrature::GetPoints(
			nVerticalOrder,
			dREtaNode[a * (nVerticalOrder-1)],
			dREtaNode[(a+1) * (nVerticalOrder-1)],
			dG, dW);

		// Get polynomial differentiation coefficients within this element
		for (int i = 0; i < nVerticalOrder; i++) {
			PolynomialInterp::DiffLagrangianPolynomialCoeffs(
				nVerticalOrder,
				&(dREtaNode[a * (nVerticalOrder-1)]),
				dLocalDiffCoeff[i],
				dG[i]);
			//printf("%i %1.15e %1.15e\n", a, dREtaNode[a * (nVerticalOrder-1) + i], dG[i]);
		}

		// Add contributions to each output node
		for (int j = 0; j < nVerticalOrder; j++) {
			int jx = j + a * (nVerticalOrder-1);

			double dWlocal = dW[j];

			if ((j == 0) && (a != 0)) {
				dWlocal *= 2.0;
			}
			if ((j == nVerticalOrder-1) && (a != nFiniteElements-1)) {
				dWlocal *= 2.0;
			}

			for (int i = 0; i < nVerticalOrder; i++) {

				int ix = i + a * (nVerticalOrder-1);
				for (int s = 0; s < nVerticalOrder; s++) {
					m_dCoeff[jx][ix] -=
						  dLocalDiffCoeff[s][j]
						* dLocalDiffCoeff[s][i]
						* dW[s] / dWlocal;
				}
			}
		}

		// Add contributions at boundaries
		if (a == 0) {
			for (int i = 0; i < nVerticalOrder; i++) {
				m_dCoeff[0][i] -=
					dLocalDiffCoeff[0][i] / (dW[0]);
			}
		}
		if (a == nFiniteElements-1) {
			for (int i = 0; i < nVerticalOrder; i++) {
				m_dCoeff[nRElementsOut-1][a * (nVerticalOrder-1) + i] +=
					dLocalDiffCoeff[nVerticalOrder-1][i] / (dW[nVerticalOrder-1]);
			}
		}
	}
/*
	// DEBUGGING
	DebugOutput(&dREtaNode, &dREtaREdge);
*/
}

///////////////////////////////////////////////////////////////////////////////

