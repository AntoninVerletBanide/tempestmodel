///////////////////////////////////////////////////////////////////////////////
///
///	\file    GridPatchCartesianGLL.cpp
///	\author  Paul Ullrich, Jorge Guerra
///	\version September 26, 2013
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

#include "GridPatchCartesianGLL.h"
#include "GridCartesianGLL.h"
#include "Model.h"
#include "TestCase.h"
#include "GridSpacing.h"
#include "VerticalStretch.h"
#include "Defines.h"

#include "Direction.h"
#include "CubedSphereTrans.h"
#include "PolynomialInterp.h"
#include "GaussLobattoQuadrature.h"

#include "Announce.h"
#include "MathHelper.h"

#include <cmath>
#include <algorithm>

///////////////////////////////////////////////////////////////////////////////

GridPatchCartesianGLL::GridPatchCartesianGLL(
	GridCartesianGLL & grid,
	int ixPatch,
	const PatchBox & box,
	int nHorizontalOrder,
	int nVerticalOrder,
	double dGDim[],
	double dRefLat,
	double dTopoHeight
) :
	GridPatchGLL(
		grid,
		ixPatch,
		box,
		nHorizontalOrder,
		nVerticalOrder)
{
	// Bring in the reference latitude (if any) for large regions where the
	// beta plane approximation is necessary in the equations
	m_dRefLat = dRefLat;

	// Bring in the grid dimensions as a member variable
	// Bring through the grid dimensions
	m_dGDim[0] = dGDim[0]; m_dGDim[1] = dGDim[1];
	m_dGDim[2] = dGDim[2]; m_dGDim[3] = dGDim[3];
	m_dGDim[4] = dGDim[4]; m_dGDim[5] = dGDim[5];

	// Set the max topography height from the test case definition
	m_dTopoHeight = dTopoHeight;
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::InitializeDataLocal() {

	// Allocate data
	GridPatch::InitializeDataLocal();

	// Physical constants
	const PhysicalConstants & phys = m_grid.GetModel().GetPhysicalConstants();
	
	// Initialize the longitude and latitude at each node
	for (int i = 0; i < m_box.GetATotalWidth(); i++) {
	for (int j = 0; j < m_box.GetBTotalWidth(); j++) {
		// Longitude and latitude directly from box
		m_dataLon[i][j] = m_box.GetANode(i);
		m_dataLat[i][j] = m_box.GetBNode(j);

		//m_dataCoriolisF[i][j] = 0.0;
	}
	}

	// Set the scale height for the decay of topography features
	m_dSL = 10.0 * m_dTopoHeight;

	if (m_dSL >= m_grid.GetZtop()) {
 		_EXCEPTIONT("Coordinate scale height exceeds model top.");
	}

	//std::cout << m_box.GetATotalWidth() << " " << m_box.GetBTotalWidth() << "\n";
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::EvaluateTopography(
	const TestCase & test
) {
	const PhysicalConstants & phys = m_grid.GetModel().GetPhysicalConstants();

	// Compute values of topography
	for (int i = 0; i < m_box.GetATotalWidth(); i++) {
	for (int j = 0; j < m_box.GetBTotalWidth(); j++) {

		double dX = m_box.GetANode(i);
		double dY = m_box.GetBNode(j);

		m_dataTopography[i][j] = test.EvaluateTopography(phys, dX, dY);

		if (m_dataTopography[i][j] >= m_grid.GetZtop()) {
			_EXCEPTIONT("TestCase topography exceeds model top.");
		}
	}
	}

	// Get derivatves from basis
	GridCartesianGLL & gridCartesianGLL =
		dynamic_cast<GridCartesianGLL &>(m_grid);

	const DataMatrix<double> & dDxBasis1D =
		gridCartesianGLL.GetDxBasis1D();

	// Compute derivatives of topography
	for (int a = 0; a < GetElementCountA(); a++) {
	for (int b = 0; b < GetElementCountB(); b++) {

		for (int i = 0; i < m_nHorizontalOrder; i++) {
		for (int j = 0; j < m_nHorizontalOrder; j++) {

			// Nodal points
			int iElementA = m_box.GetAInteriorBegin() + a * m_nHorizontalOrder;
			int iElementB = m_box.GetBInteriorBegin() + b * m_nHorizontalOrder;

			int iA = iElementA + i;
			int iB = iElementB + j;

			// Topography height and its derivatives
			double dZs = m_dataTopography[iA][iB];

			double dDaZs = 0.0;
			double dDbZs = 0.0;

			for (int s = 0; s < m_nHorizontalOrder; s++) {
				dDaZs += dDxBasis1D[s][i] * m_dataTopography[iElementA+s][iB];
				dDbZs += dDxBasis1D[s][j] * m_dataTopography[iA][iElementB+s];
			}

			dDaZs /= GetElementDeltaA();
			dDbZs /= GetElementDeltaB();

			m_dataTopographyDeriv[0][iA][iB] = dDaZs;
			m_dataTopographyDeriv[1][iA][iB] = dDbZs;
		}
		}
	}
	}
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::EvaluateGeometricTerms() {

	// Physical constants
	const PhysicalConstants & phys = m_grid.GetModel().GetPhysicalConstants();

	// Obtain Gauss Lobatto quadrature nodes and weights
	DataVector<double> dGL;
	DataVector<double> dWL;

	GaussLobattoQuadrature::GetPoints(m_nHorizontalOrder, 0.0, 1.0, dGL, dWL);

	// Obtain normalized areas in the vertical
	const DataVector<double> & dWNode =
		m_grid.GetREtaLevelsNormArea();
	const DataVector<double> & dWREdge =
		m_grid.GetREtaInterfacesNormArea();

	// Verify that normalized areas are correct
	double dWNodeSum = 0.0;
	for (int k = 0; k < dWNode.GetRows(); k++) {
		dWNodeSum += dWNode[k];
	}
	if (fabs(dWNodeSum - 1.0) > 1.0e-13) {
		_EXCEPTION1("Error in normalized areas (%1.15e)", dWNodeSum);
	}

	if (m_grid.GetVerticalStaggering() !=
	    Grid::VerticalStaggering_Interfaces
	) {
		double dWREdgeSum = 0.0;
		for (int k = 0; k < dWREdge.GetRows(); k++) {
			dWREdgeSum += dWREdge[k];
		}
		if (fabs(dWREdgeSum - 1.0) > 1.0e-13) {
			_EXCEPTION1("Error in normalized areas (%1.15e)", dWREdgeSum);
		}
	}

	// Derivatives of basis functions
	GridCartesianGLL & gridCartesianGLL =
		dynamic_cast<GridCartesianGLL &>(m_grid);

	const DataMatrix<double> & dDxBasis1D = gridCartesianGLL.GetDxBasis1D();
	
	double dy0 = 0.5 * fabs(m_dGDim[3] - m_dGDim[2]);
	double dfp = 2.0 * phys.GetOmega() * sin(m_dRefLat);
	double dbetap = 2.0 * phys.GetOmega() * cos(m_dRefLat) / 
					phys.GetEarthRadius();
	// Initialize the Coriolis force at each node
	for (int i = 0; i < m_box.GetATotalWidth(); i++) {
	for (int j = 0; j < m_box.GetBTotalWidth(); j++) {
		// Coriolis force by beta approximation
		//m_dataCoriolisF[i][j] = dfp + dbetap * (m_dataLat[i][j] - dy0);
		//m_dataCoriolisF[i][j] = dfp;
		//m_dataCoriolisF[i][j] = 0.0;
	}
	}

	// Initialize metric and Christoffel symbols in terrain-following coords
	for (int a = 0; a < GetElementCountA(); a++) {
	for (int b = 0; b < GetElementCountB(); b++) {

		for (int i = 0; i < m_nHorizontalOrder; i++) {
		for (int j = 0; j < m_nHorizontalOrder; j++) {

			// Nodal points
			int iElementA = m_box.GetAInteriorBegin() + a * m_nHorizontalOrder;
			int iElementB = m_box.GetBInteriorBegin() + b * m_nHorizontalOrder;

			int iA = iElementA + i;
			int iB = iElementB + j;

			// Topography height and its derivatives
			double dZs = m_dataTopography[iA][iB];
			double dDaZs = m_dataTopographyDeriv[0][iA][iB];
			double dDbZs = m_dataTopographyDeriv[1][iA][iB];

			// Initialize 2D Jacobian
			m_dataJacobian2D[iA][iB] = 1.0;

			// Initialize 2D contravariant metric
			m_dataContraMetric2DA[iA][iB][0] = 1.0;
			m_dataContraMetric2DA[iA][iB][1] = 0.0;

			m_dataContraMetric2DB[iA][iB][0] = 0.0;
			m_dataContraMetric2DB[iA][iB][1] = 1.0;

			// Initialize 2D covariant metric
			m_dataCovMetric2DA[iA][iB][0] = 1.0;
			m_dataCovMetric2DA[iA][iB][1] = 0.0;

			m_dataCovMetric2DB[iA][iB][0] = 0.0;
			m_dataCovMetric2DB[iA][iB][1] = 1.0;

			// Vertical coordinate transform and its derivatives
			for (int k = 0; k < m_grid.GetRElements(); k++) {

				// Gal-Chen and Somerville (1975) terrain following coord
				// Schar Exponential Decay terrain following coord
				double dREta = m_grid.GetREtaLevel(k);

				double dREtaStretch;
				double dDxREtaStretch;
				m_grid.EvaluateVerticalStretchF(
					dREta, dREtaStretch, dDxREtaStretch);

				//double dZ = dZs + (m_grid.GetZtop() - dZs) * dREtaStretch;
				//double dbZ = sinh(m_grid.GetZtop() * (1.0 - dREtaStretch) / m_dSL)
				//	/ sinh(m_grid.GetZtop() / m_dSL);
				//double dZ = m_grid.GetZtop() * dREtaStretch + dZs; // * dbZ;

				double dZ = dZs + (m_grid.GetZtop() - dZs) * dREtaStretch;

				double dDaZ = (1.0 - dREtaStretch) * dDaZs;
				double dDbZ = (1.0 - dREtaStretch) * dDbZs;
				double dDxZ = (m_grid.GetZtop() - dZs) * dDxREtaStretch;

/*
				double dDaZ = dbZ * dDaZs;
				double dDbZ = dbZ * dDbZs;
				double dDxZ = m_grid.GetZtop() - dZs * m_grid.GetZtop() * 
					cosh(m_grid.GetZtop() * (1.0 - dREtaStretch) / m_dSL) /
					(m_dSL * sinh(m_grid.GetZtop() / m_dSL));
				dDxZ *= dDxREtaStretch;
*/
				// Calculate pointwise Jacobian
				m_dataJacobian[k][iA][iB] =
					dDxZ * m_dataJacobian2D[iA][iB];

				// Element area associated with each model level GLL node
				m_dataElementArea[k][iA][iB] =
					m_dataJacobian[k][iA][iB]
					* dWL[i] * GetElementDeltaA()
					* dWL[j] * GetElementDeltaB()
					* dWNode[k];

				// Contravariant metric components
				m_dataContraMetricA[k][iA][iB][0] =
					m_dataContraMetric2DA[iA][iB][0];
				m_dataContraMetricA[k][iA][iB][1] =
					m_dataContraMetric2DA[iA][iB][1];
				m_dataContraMetricA[k][iA][iB][2] =
					- dDaZ / dDxZ;

				m_dataContraMetricB[k][iA][iB][0] =
					m_dataContraMetric2DB[iA][iB][0];
				m_dataContraMetricB[k][iA][iB][1] =
					m_dataContraMetric2DB[iA][iB][1];
				m_dataContraMetricB[k][iA][iB][2] =
					- dDbZ / dDxZ;

				m_dataContraMetricXi[k][iA][iB][0] =
					m_dataContraMetricA[k][iA][iB][2];
				m_dataContraMetricXi[k][iA][iB][1] =
					m_dataContraMetricB[k][iA][iB][2];
				m_dataContraMetricXi[k][iA][iB][2] =
					(1.0 + dDaZ * dDaZ + dDbZ * dDbZ) / (dDxZ * dDxZ);

				// Covariant metric components
				m_dataCovMetricA[k][iA][iB][0] =
					m_dataCovMetric2DA[iA][iB][0] + dDaZ * dDaZ;
				m_dataCovMetricA[k][iA][iB][1] =
					m_dataCovMetric2DA[iA][iB][1] + dDaZ * dDbZ;
				m_dataCovMetricA[k][iA][iB][2] =
					dDaZ * dDxZ;

				m_dataCovMetricB[k][iA][iB][0] =
					m_dataCovMetric2DB[iA][iB][0] + dDbZ * dDaZ;
				m_dataCovMetricB[k][iA][iB][1] =
					m_dataCovMetric2DB[iA][iB][1] + dDbZ * dDbZ;
				m_dataCovMetricB[k][iA][iB][2] =
					dDbZ * dDxZ;

				m_dataCovMetricXi[k][iA][iB][0] =
					dDaZ * dDxZ;
				m_dataCovMetricXi[k][iA][iB][1] =
					dDbZ * dDxZ;
				m_dataCovMetricXi[k][iA][iB][2] =
					dDxZ * dDxZ;

				// Derivatives of the vertical coordinate transform
				m_dataDerivRNode[k][iA][iB][0] = dDaZ;
				m_dataDerivRNode[k][iA][iB][1] = dDbZ;
				m_dataDerivRNode[k][iA][iB][2] = dDxZ;
			}

			// Metric terms at vertical interfaces
			for (int k = 0; k <= m_grid.GetRElements(); k++) {

				// Gal-Chen and Somerville (1975) terrain following coord
				// Schar Exponential decay terrain following coord
				double dREta = m_grid.GetREtaInterface(k);
/*				
				double dREtaStretch;
				double dDxREtaStretch;
				m_grid.EvaluateVerticalStretchF(
					dREta, dREtaStretch, dDxREtaStretch);

				double dZ = dZs + (m_grid.GetZtop() - dZs) * dREtaStretch;

				double dDaZ = (1.0 - dREtaStretch) * dDaZs;
				double dDbZ = (1.0 - dREtaStretch) * dDbZs;
				double dDxZ = (m_grid.GetZtop() - dZs) * dDxREtaStretch;
*/
				double dREtaStretch;
				double dDxREtaStretch;
				m_grid.EvaluateVerticalStretchF(
					dREta, dREtaStretch, dDxREtaStretch);
/*
				//double dZ = dZs + (m_grid.GetZtop() - dZs) * dREtaStretch;
				double dbZ = sinh(m_grid.GetZtop() * (1.0 - dREtaStretch) / m_dSL) / 
					sinh(m_grid.GetZtop() / m_dSL);
				double dZ = m_grid.GetZtop() * dREtaStretch + dZs * dbZ;
*/

				double dDaZ = (1.0 - dREtaStretch) * dDaZs;
				double dDbZ = (1.0 - dREtaStretch) * dDbZs;
		     	double dDxZ = (m_grid.GetZtop() - dZs) * dDxREtaStretch;
/*
				double dDaZ = dbZ * dDaZs;
				double dDbZ = dbZ * dDbZs;
				double dDxZ = m_grid.GetZtop() - dZs * m_grid.GetZtop() * 
					cosh(m_grid.GetZtop() * (1.0 - dREtaStretch) / m_dSL) /
					(m_dSL * sinh(m_grid.GetZtop() / m_dSL));
				dDxZ *= dDxREtaStretch;
*/
				// Calculate pointwise Jacobian
				m_dataJacobianREdge[k][iA][iB] =
					dDxZ * m_dataJacobian2D[iA][iB];

				// Element area associated with each model interface GLL node
				m_dataElementAreaREdge[k][iA][iB] =
					m_dataJacobianREdge[k][iA][iB]
					* dWL[i] * GetElementDeltaA()
					* dWL[j] * GetElementDeltaB()
					* dWREdge[k];

				// Components of the contravariant metric
				m_dataContraMetricAREdge[k][iA][iB][0] =
					m_dataContraMetric2DA[iA][iB][0];
				m_dataContraMetricAREdge[k][iA][iB][1] =
					m_dataContraMetric2DA[iA][iB][1];
				m_dataContraMetricAREdge[k][iA][iB][2] =
					- dDaZ / dDxZ;

				m_dataContraMetricBREdge[k][iA][iB][0] =
					m_dataContraMetric2DB[iA][iB][0];
				m_dataContraMetricBREdge[k][iA][iB][1] =
					m_dataContraMetric2DB[iA][iB][1];
				m_dataContraMetricBREdge[k][iA][iB][2] =
					- dDbZ / dDxZ;

				m_dataContraMetricXiREdge[k][iA][iB][0] =
					- dDaZ / dDxZ;
				m_dataContraMetricXiREdge[k][iA][iB][1] =
					- dDbZ / dDxZ;
				m_dataContraMetricXiREdge[k][iA][iB][2] =
					(1.0 + dDaZ * dDaZ + dDbZ * dDbZ) / (dDxZ * dDxZ);

				// Derivatives of the vertical coordinate transform
				m_dataDerivRREdge[k][iA][iB][0] = dDaZ;
				m_dataDerivRREdge[k][iA][iB][1] = dDbZ;
				m_dataDerivRREdge[k][iA][iB][2] = dDxZ;
			}
		}
		}
	}
	}
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::EvaluateTestCase(
	const TestCase & test,
	const Time & time,
	int iDataIndex
) {
	// Initialize the data at each node
	if (m_datavecStateNode.size() == 0) {
		_EXCEPTIONT("InitializeData must be called before InitialConditions");
	}
	if (iDataIndex >= m_datavecStateNode.size()) {
		_EXCEPTIONT("Invalid iDataIndex (out of range)");
	}

	// Check dimensionality
	if ((m_grid.GetModel().GetEquationSet().GetDimensionality() == 2) &&
		(m_nVerticalOrder != 1)
	) {
		_EXCEPTIONT("VerticalOrder / Dimensionality mismatch:\n"
			"For 2D problems vertical order must be 1.");
	}

	// Evaluate topography
	EvaluateTopography(test);

	// Physical constants
	const PhysicalConstants & phys = m_grid.GetModel().GetPhysicalConstants();
	
	// Initialize the topography at each node
	for (int i = 0; i < m_box.GetATotalWidth(); i++) {
	for (int j = 0; j < m_box.GetBTotalWidth(); j++) {
		m_dataTopography[i][j] =
			test.EvaluateTopography(
				phys,
				m_dataLon[i][j],
				m_dataLat[i][j]);

		if (m_dataTopography[i][j] >= m_grid.GetZtop()) {
			_EXCEPTIONT("TestCase topography exceeds model top.");
		}

		// Gal-Chen and Sommerville vertical coordinate
		for (int k = 0; k < m_grid.GetRElements(); k++) {
			m_dataZLevels[k][i][j] =
				m_dataTopography[i][j]
					+ m_grid.GetREtaLevel(k)
						* (m_grid.GetZtop() - m_dataTopography[i][j]);
		}
		for (int k = 0; k <= m_grid.GetRElements(); k++) {
			m_dataZInterfaces[k][i][j] =
				m_dataTopography[i][j]
					+ m_grid.GetREtaInterface(k)
						* (m_grid.GetZtop() - m_dataTopography[i][j]);
		}

/*
		// Schar Exponential Decay vertical coordinate
		for (int k = 0; k < m_grid.GetRElements(); k++) {
			m_dataZLevels[k][i][j] = m_grid.GetZtop() * m_grid.GetREtaLevel(k) + 
			m_dataTopography[i][j] * sinh(m_grid.GetZtop() * (1.0 - m_grid.GetREtaLevel(k)) / m_dSL) / 
			sinh(m_grid.GetZtop() / m_dSL);
		}
		for (int k = 0; k <= m_grid.GetRElements(); k++) {
			m_dataZInterfaces[k][i][j] = m_grid.GetZtop() * m_grid.GetREtaInterface(k) + 
			m_dataTopography[i][j] * sinh(m_grid.GetZtop() * (1.0 - m_grid.GetREtaInterface(k)) / m_dSL) / 
			sinh(m_grid.GetZtop() / m_dSL);
		}
*/
	}
	}

	// Initialize the Rayleigh friction strength at each node
	if (test.HasRayleighFriction()) {
		for (int i = 0; i < m_box.GetATotalWidth(); i++) {
		for (int j = 0; j < m_box.GetBTotalWidth(); j++) {
			for (int k = 0; k < m_grid.GetRElements(); k++) {
				m_dataRayleighStrengthNode[k][i][j] =
					test.EvaluateRayleighStrength(
						m_dataZLevels[k][i][j],
						m_dataLon[i][j],
						m_dataLat[i][j]);
			}
			for (int k = 0; k < m_grid.GetRElements(); k++) {
				m_dataRayleighStrengthREdge[k][i][j] =
					test.EvaluateRayleighStrength(
						m_dataZInterfaces[k][i][j],
						m_dataLon[i][j],
						m_dataLat[i][j]);
			}
		}
		}
	}

	// Buffer vector for storing pointwise states
	const EquationSet & eqns = m_grid.GetModel().GetEquationSet();

	int nComponents = eqns.GetComponents();
	int nTracers = eqns.GetTracers();

	DataVector<double> dPointwiseState;
	dPointwiseState.Initialize(nComponents);

	DataVector<double> dPointwiseRefState;
	dPointwiseRefState.Initialize(nComponents);

	DataVector<double> dPointwiseTracers;
	if (m_datavecTracers.size() > 0) {
		dPointwiseTracers.Initialize(nTracers);
	}

	// Evaluate the state on model levels
	for (int k = 0; k < m_grid.GetRElements(); k++) {
	for (int i = 0; i < m_box.GetATotalWidth(); i++) {
	for (int j = 0; j < m_box.GetBTotalWidth(); j++) {

		// Evaluate pointwise state
		test.EvaluatePointwiseState(
			m_grid.GetModel().GetPhysicalConstants(),
			time,
			m_dataZLevels[k][i][j],
			m_dataLon[i][j],
			m_dataLat[i][j],
			dPointwiseState,
			dPointwiseTracers);

		eqns.ConvertComponents(phys, dPointwiseState);

		for (int c = 0; c < dPointwiseState.GetRows(); c++) {
			m_datavecStateNode[iDataIndex][c][k][i][j] = dPointwiseState[c];
		}

		// Evaluate reference state
		if (m_grid.HasReferenceState()) {
			test.EvaluateReferenceState(
				m_grid.GetModel().GetPhysicalConstants(),
				m_dataZLevels[k][i][j],
				m_dataLon[i][j],
				m_dataLat[i][j],
				dPointwiseRefState);

			eqns.ConvertComponents(phys, dPointwiseRefState);

			for (int c = 0; c < dPointwiseState.GetRows(); c++) {
				m_dataRefStateNode[c][k][i][j] = dPointwiseRefState[c];
			}
		}

		// Evaluate tracers
		for (int c = 0; c < dPointwiseTracers.GetRows(); c++) {
			m_datavecTracers[iDataIndex][c][k][i][j] = dPointwiseTracers[c];
		}
	}
	}
	}

	// Evaluate the state on model interfaces
	for (int k = 0; k <= m_grid.GetRElements(); k++) {
	for (int i = 0; i < m_box.GetATotalWidth(); i++) {
	for (int j = 0; j < m_box.GetBTotalWidth(); j++) {

		// Evaluate pointwise state
		test.EvaluatePointwiseState(
			phys,
			time,
			m_dataZInterfaces[k][i][j],
			m_dataLon[i][j],
			m_dataLat[i][j],
			dPointwiseState,
			dPointwiseTracers);

		eqns.ConvertComponents(phys, dPointwiseState);

		for (int c = 0; c < dPointwiseState.GetRows(); c++) {
			m_datavecStateREdge[iDataIndex][c][k][i][j] = dPointwiseState[c];
		}

		if (m_grid.HasReferenceState()) {
			test.EvaluateReferenceState(
				phys,
				m_dataZInterfaces[k][i][j],
				m_dataLon[i][j],
				m_dataLat[i][j],
				dPointwiseRefState);

			eqns.ConvertComponents(phys, dPointwiseRefState);

			for (int c = 0; c < dPointwiseState.GetRows(); c++) {
				m_dataRefStateREdge[c][k][i][j] = dPointwiseRefState[c];
			}
		}
	}
	}
	}
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::ApplyBoundaryConditions(
	int iDataIndex,
	DataType eDataType
) {
	// Indices of EquationSet variables
	const int UIx = 0;
	const int VIx = 1;
	const int TIx = 2;
	const int WIx = 3;
	const int RIx = 4;

	// Impose boundary conditions (everything on levels)
	if (m_grid.GetVerticalStaggering() ==
		Grid::VerticalStaggering_Levels
	) {
		_EXCEPTIONT("Not implemented");

	// Impose boundary conditions (everything on interfaces)
	} else if (m_grid.GetVerticalStaggering() ==
		Grid::VerticalStaggering_Interfaces
	) {
		const int UIx = 0;
		const int VIx = 1;
		const int WIx = 3;

		for (int i = 0; i < m_box.GetATotalWidth(); i++) {
		for (int j = 0; j < m_box.GetBTotalWidth(); j++) {

#ifdef USE_COVARIANT_VELOCITIES
			m_datavecStateNode[iDataIndex][WIx][0][i][j] =
				- ( m_dataContraMetricXi[0][i][j][0]
					* m_datavecStateNode[iDataIndex][UIx][0][i][j]
				  + m_dataContraMetricXi[0][i][j][1]
					* m_datavecStateNode[iDataIndex][VIx][0][i][j])
				/ m_dataContraMetricXi[0][i][j][2]
				/ m_dataDerivRNode[0][i][j][2];


#else
			m_datavecStateNode[iDataIndex][WIx][0][i][j] =
				CalculateNoFlowUrNode(
					0, i, j,
					m_datavecStateNode[iDataIndex][UIx][0][i][j],
					m_datavecStateNode[iDataIndex][VIx][0][i][j]);
#endif
		}
		}

	// Impose boundary conditions (vertical velocity on interfaces)
	} else {
		const int UIx = 0;
		const int VIx = 1;
		const int WIx = 3;

		for (int i = 0; i < m_box.GetATotalWidth(); i++) {
		for (int j = 0; j < m_box.GetBTotalWidth(); j++) {

#ifdef USE_COVARIANT_VELOCITIES
			m_datavecStateREdge[iDataIndex][WIx][0][i][j] =
				- ( m_dataContraMetricXi[0][i][j][0]
					* m_datavecStateNode[iDataIndex][UIx][0][i][j]
				  + m_dataContraMetricXi[0][i][j][1]
					* m_datavecStateNode[iDataIndex][VIx][0][i][j])
				/ m_dataContraMetricXi[0][i][j][2]
				/ m_dataDerivRNode[0][i][j][2];


#else
			m_datavecStateNode[iDataIndex][WIx][0][i][j] =
				CalculateNoFlowUrNode(
					0, i, j,
					m_datavecStateNode[iDataIndex][UIx][0][i][j],
					m_datavecStateNode[iDataIndex][VIx][0][i][j]);
#endif

		}
		}
	}


/*
	// Check number of components
	if (m_grid.GetModel().GetEquationSet().GetComponents() != 5) {
		_EXCEPTIONT("Unimplemented");
	}

	// Working data
	GridData4D & dataREdge = GetDataState(iDataUpdate, DataLocation_REdge);
	GridData4D & dataNode  = GetDataState(iDataUpdate, DataLocation_Node);

	// Apply boundary conditions on model levels
	for (int k = 0; k < m_grid.GetRElements(); k++) {
		int i;
		int j;

		// Evaluate boundary conditions along right edge
		i = m_box.GetAInteriorEnd();
		for (j = m_box.GetBInteriorBegin(); j < m_box.GetBInteriorEnd(); j++) {
			dataNode[UIx][k][i][j] =   dataNode[UIx][k][i-1][j];
			dataNode[VIx][k][i][j] =   dataNode[VIx][k][i-1][j];
			dataNode[TIx][k][i][j] =   dataNode[TIx][k][i-1][j];
			dataNode[WIx][k][i][j] =   dataNode[WIx][k][i-1][j];
			dataNode[RIx][k][i][j] =   dataNode[RIx][k][i-1][j];
		}

		// Evaluate boundary conditions along left edge
		i = m_box.GetAInteriorBegin()-1;
		for (j = m_box.GetBInteriorBegin(); j < m_box.GetBInteriorEnd(); j++) {
			dataNode[UIx][k][i][j] =   dataNode[UIx][k][i+1][j];
			dataNode[VIx][k][i][j] =   dataNode[VIx][k][i+1][j];
			dataNode[TIx][k][i][j] =   dataNode[TIx][k][i+1][j];
			dataNode[WIx][k][i][j] =   dataNode[WIx][k][i+1][j];
			dataNode[RIx][k][i][j] =   dataNode[RIx][k][i+1][j];
		}

		// Evaluate boundary conditions along top edge
		j = m_box.GetBInteriorEnd();
		for (i = m_box.GetAInteriorBegin()-1; i < m_box.GetAInteriorEnd()+1; i++) {
			dataNode[UIx][k][i][j] =   dataNode[UIx][k][i][j-1];
			dataNode[VIx][k][i][j] = - dataNode[VIx][k][i][j-1];
			dataNode[TIx][k][i][j] =   dataNode[TIx][k][i][j-1];
			dataNode[WIx][k][i][j] =   dataNode[WIx][k][i][j-1];
			dataNode[RIx][k][i][j] =   dataNode[RIx][k][i][j-1];
		}

		// Evaluate boundary conditions along bottom edge
		j = m_box.GetBInteriorBegin()-1;
		for (i = m_box.GetAInteriorBegin()-1; i < m_box.GetAInteriorEnd()+1; i++) {
			dataNode[UIx][k][i][j] =   dataNode[UIx][k][i][j+1];
			dataNode[VIx][k][i][j] = - dataNode[VIx][k][i][j+1];
			dataNode[TIx][k][i][j] =   dataNode[TIx][k][i][j+1];
			dataNode[WIx][k][i][j] =   dataNode[WIx][k][i][j+1];
			dataNode[RIx][k][i][j] =   dataNode[RIx][k][i][j+1];
		}
	}

	// Apply boundary conditions on interfaces
	for (int k = 0; k <= m_grid.GetRElements(); k++) {
		int i;
		int j;

		// Evaluate boundary conditions along right edge
		i = m_box.GetAInteriorEnd();
		for (j = m_box.GetBInteriorBegin(); j < m_box.GetBInteriorEnd(); j++) {
			dataREdge[UIx][k][i][j] =   dataREdge[UIx][k][i-1][j];
			dataREdge[VIx][k][i][j] =   dataREdge[VIx][k][i-1][j];
			dataREdge[TIx][k][i][j] =   dataREdge[TIx][k][i-1][j];
			dataREdge[WIx][k][i][j] =   dataREdge[WIx][k][i-1][j];
			dataREdge[RIx][k][i][j] =   dataREdge[RIx][k][i-1][j];
		}

		// Evaluate boundary conditions along left edge
		i = m_box.GetAInteriorBegin()-1;
		for (j = m_box.GetBInteriorBegin(); j < m_box.GetBInteriorEnd(); j++) {
			dataREdge[UIx][k][i][j] =   dataREdge[UIx][k][i+1][j];
			dataREdge[VIx][k][i][j] =   dataREdge[VIx][k][i+1][j];
			dataREdge[TIx][k][i][j] =   dataREdge[TIx][k][i+1][j];
			dataREdge[WIx][k][i][j] =   dataREdge[WIx][k][i+1][j];
			dataREdge[RIx][k][i][j] =   dataREdge[RIx][k][i+1][j];
		}

		// Evaluate boundary conditions along top edge
		j = m_box.GetBInteriorEnd();
		for (i = m_box.GetAInteriorBegin()-1; i < m_box.GetAInteriorEnd()+1; i++) {
			dataREdge[UIx][k][i][j] =   dataREdge[UIx][k][i][j-1];
			dataREdge[VIx][k][i][j] = - dataREdge[VIx][k][i][j-1];
			dataREdge[TIx][k][i][j] =   dataREdge[TIx][k][i][j-1];
			dataREdge[WIx][k][i][j] =   dataREdge[WIx][k][i][j-1];
			dataREdge[RIx][k][i][j] =   dataREdge[RIx][k][i][j-1];
		}

		// Evaluate boundary conditions along bottom edge
		j = m_box.GetBInteriorBegin()-1;
		for (i = m_box.GetAInteriorBegin()-1; i < m_box.GetAInteriorEnd()+1; i++) {
			dataREdge[UIx][k][i][j] =   dataREdge[UIx][k][i][j+1];
			dataREdge[VIx][k][i][j] = - dataREdge[VIx][k][i][j+1];
			dataREdge[TIx][k][i][j] =   dataREdge[TIx][k][i][j+1];
			dataREdge[WIx][k][i][j] =   dataREdge[WIx][k][i][j+1];
			dataREdge[RIx][k][i][j] =   dataREdge[RIx][k][i][j+1];
		}
	}
*/
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::ComputeCurlAndDiv(
	const GridData3D & dataUa,
	const GridData3D & dataUb
) const {
	// Parent grid
	const GridCartesianGLL & gridCSGLL =
		dynamic_cast<const GridCartesianGLL &>(m_grid);

	// Compute derivatives of the field
	const DataMatrix<double> & dDxBasis1D = gridCSGLL.GetDxBasis1D();

	// Number of finite elements in each direction
	int nAFiniteElements = m_box.GetAInteriorWidth() / m_nHorizontalOrder;
	int nBFiniteElements = m_box.GetBInteriorWidth() / m_nHorizontalOrder;

	// Contravariant velocity within an element
	DataMatrix<double> dConUa(m_nHorizontalOrder, m_nHorizontalOrder);
	DataMatrix<double> dConUb(m_nHorizontalOrder, m_nHorizontalOrder);

	// Loop over all elements in the box
	for (int k = 0; k < gridCSGLL.GetRElements(); k++) {
	for (int a = 0; a < nAFiniteElements; a++) {
	for (int b = 0; b < nBFiniteElements; b++) {

		// Index of lower-left corner node
		int iA = a * m_nHorizontalOrder + m_box.GetHaloElements();
		int iB = b * m_nHorizontalOrder + m_box.GetHaloElements();

		// Calculate contravariant velocity at each node within the element
		for (int i = 0; i < m_nHorizontalOrder; i++) {
		for (int j = 0; j < m_nHorizontalOrder; j++) {
			dConUa[i][j] =
				  m_dataContraMetric2DA[iA+i][iB+j][0]
				  	* dataUa[k][iA+i][iB+j]
				+ m_dataContraMetric2DA[iA+i][iB+j][1]
					* dataUb[k][iA+i][iB+j];

			dConUb[i][j] =
				  m_dataContraMetric2DB[iA+i][iB+j][0]
				  	* dataUa[k][iA+i][iB+j]
				+ m_dataContraMetric2DB[iA+i][iB+j][1]
					* dataUb[k][iA+i][iB+j];
		}
		}

		// Calculate divergance and curl
		for (int i = 0; i < m_nHorizontalOrder; i++) {
		for (int j = 0; j < m_nHorizontalOrder; j++) {

			// Compute derivatives at each node
			double dDaJUa = 0.0;
			double dDbJUb = 0.0;

			double dCovDaUb = 0.0;
			double dCovDbUa = 0.0;

			for (int s = 0; s < m_nHorizontalOrder; s++) {
				dDaJUa += dDxBasis1D[s][i]
					* m_dataJacobian2D[iA+s][iB+j]
					* dConUa[s][j];

				dDbJUb += dDxBasis1D[s][j]
					* m_dataJacobian2D[iA+i][iB+s]
					* dConUb[i][s];

				dCovDaUb += dDxBasis1D[s][i] * dataUb[k][iA+s][iB+j];
					//( m_dataCovMetric2DB[iA+s][iB+j][0] * dataUa[k][iA+s][iB+j]
					//+ m_dataCovMetric2DB[iA+s][iB+j][1] * dataUb[k][iA+s][iB+j]);
				dCovDbUa += dDxBasis1D[s][j] * dataUa[k][iA+i][iB+s];
					//( m_dataCovMetric2DA[iA+i][iB+s][0] * dataUa[k][iA+i][iB+s]
					//+ m_dataCovMetric2DA[iA+i][iB+s][1] * dataUb[k][iA+i][iB+s]);
			}

			dDaJUa /= GetElementDeltaA();
			dDbJUb /= GetElementDeltaB();

			dCovDaUb /= GetElementDeltaA();
			dCovDbUa /= GetElementDeltaB();

			m_dataVorticity[k][iA+i][iB+j] =
				(dCovDaUb - dCovDbUa) / m_dataJacobian2D[iA+i][iB+j];

			// Compute the divergence at node
			m_dataDivergence[k][iA+i][iB+j] =
				(dDaJUa + dDbJUb) / m_dataJacobian2D[iA+i][iB+j];

/*
			// Compute covariant derivatives at node
			double dCovDaUa = dDaUa
				+ m_dataChristoffelA[iA+i][iB+j][0] * dUa
				+ m_dataChristoffelA[iA+i][iB+j][1] * 0.5 * dUb;

			double dCovDaUb = dDaUb
				+ m_dataChristoffelB[iA+i][iB+j][0] * dUa
				+ m_dataChristoffelB[iA+i][iB+j][1] * 0.5 * dUb;

			double dCovDbUa = dDbUa
				+ m_dataChristoffelA[iA+i][iB+j][1] * 0.5 * dUa
				+ m_dataChristoffelA[iA+i][iB+j][2] * dUb;

			double dCovDbUb = dDbUb
				+ m_dataChristoffelB[iA+i][iB+j][1] * 0.5 * dUa
				+ m_dataChristoffelB[iA+i][iB+j][2] * dUb;

			// Compute curl at node
			m_dataVorticity[k][iA+i][iB+j] = m_dataJacobian2D[iA+i][iB+j] * (
				+ m_dataContraMetric2DA[iA+i][iB+j][0] * dDaUb
				+ m_dataContraMetric2DA[iA+i][iB+j][1] * dDbUb
				- m_dataContraMetric2DB[iA+i][iB+j][0] * dDaUa
				- m_dataContraMetric2DB[iA+i][iB+j][1] * dDbUa);

			// Compute the divergence at node
			m_dataDivergence[k][iA+i][iB+j] = dDaUa + dDbUb;
*/
		}
		}
	}
	}
	}
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::ComputeVorticityDivergence(
	int iDataIndex
) {
	// Physical constants
	const PhysicalConstants & phys = m_grid.GetModel().GetPhysicalConstants();

	// Working data
	const GridData4D & dataState = GetDataState(iDataIndex, DataLocation_Node);

	if (dataState.GetComponents() < 2) {
		_EXCEPTIONT(
			"Insufficient components for vorticity calculation");
	}

	// Get the alpha and beta components of vorticity
	GridData3D dataUa;
	GridData3D dataUb;

	dataState.GetAsGridData3D(0, dataUa);
	dataState.GetAsGridData3D(1, dataUb);

	// Compute the radial component of the curl of the velocity field
	ComputeCurlAndDiv(dataUa, dataUb);
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::InterpolateData(
	const DataVector<double> & dAlpha,
	const DataVector<double> & dBeta,
	const DataVector<int> & iPatch,
	DataType eDataType,
	DataLocation eDataLocation,
	bool fInterpAllVariables,
	DataMatrix3D<double> & dInterpData,
	bool fIncludeReferenceState,
	bool fConvertToPrimitive
) {
	if (dAlpha.GetRows() != dBeta.GetRows()) {
		_EXCEPTIONT("Point vectors must have equivalent length.");
	}

	// Vector for storage interpolated points
	DataVector<double> dAInterpCoeffs;
	dAInterpCoeffs.Initialize(m_nHorizontalOrder);

	DataVector<double> dBInterpCoeffs;
	dBInterpCoeffs.Initialize(m_nHorizontalOrder);

	DataVector<double> dADiffCoeffs;
	dADiffCoeffs.Initialize(m_nHorizontalOrder);

	DataVector<double> dBDiffCoeffs;
	dBDiffCoeffs.Initialize(m_nHorizontalOrder);

	DataVector<double> dAInterpPt;
	dAInterpPt.Initialize(m_nHorizontalOrder);

	// Element-wise grid spacing
	double dDeltaA = GetElementDeltaA();
	double dDeltaB = GetElementDeltaB();

	// Physical constants
	const PhysicalConstants & phys = m_grid.GetModel().GetPhysicalConstants();

	// Loop throught all points
	for (int i = 0; i < dAlpha.GetRows(); i++) {

		// Element index
		if (iPatch[i] != GetPatchIndex()) {
			continue;
		}

		// Verify point lies within domain of patch
		const double Eps = 1.0e-10;
		if ((dAlpha[i] < m_box.GetAEdge(m_box.GetAInteriorBegin()) - Eps) ||
			(dAlpha[i] > m_box.GetAEdge(m_box.GetAInteriorEnd()) + Eps) ||
			(dBeta[i] < m_box.GetBEdge(m_box.GetBInteriorBegin()) - Eps) ||
			(dBeta[i] > m_box.GetBEdge(m_box.GetBInteriorEnd()) + Eps)
		) {
			_EXCEPTIONT("Point out of range");
		}

		// Determine finite element index
		int iA =
			(dAlpha[i] - m_box.GetAEdge(m_box.GetAInteriorBegin())) / dDeltaA;

		int iB =
			(dBeta[i] - m_box.GetBEdge(m_box.GetBInteriorBegin())) / dDeltaB;

		// Bound the index within the element
		if (iA < 0) {
			iA = 0;
		}
		if (iA >= (m_box.GetAInteriorWidth() / m_nHorizontalOrder)) {
			iA = m_box.GetAInteriorWidth() / m_nHorizontalOrder - 1;
		}
		if (iB < 0) {
			iB = 0;
		}
		if (iB >= (m_box.GetBInteriorWidth() / m_nHorizontalOrder)) {
			iB = m_box.GetBInteriorWidth() / m_nHorizontalOrder - 1;
		}

		iA = m_box.GetHaloElements() + iA * m_nHorizontalOrder;
		iB = m_box.GetHaloElements() + iB * m_nHorizontalOrder;

		// Compute interpolation coefficients
		PolynomialInterp::LagrangianPolynomialCoeffs(
			m_nHorizontalOrder,
			&(m_box.GetAEdges()[iA]),
			dAInterpCoeffs,
			dAlpha[i]);

		PolynomialInterp::LagrangianPolynomialCoeffs(
			m_nHorizontalOrder,
			&(m_box.GetBEdges()[iB]),
			dBInterpCoeffs,
			dBeta[i]);

		int nComponents;
		int nRElements = m_grid.GetRElements();

		double ** pData2D;

		// State Data: Perform interpolation on all variables
		if (eDataType == DataType_State) {
			if (eDataLocation == DataLocation_Node) {
				nComponents = m_datavecStateNode[0].GetComponents();
			} else {
				nComponents = m_datavecStateREdge[0].GetComponents();
				nRElements = m_grid.GetRElements() + 1;
			}

		// Tracer Data: Perform interpolation on all variables
		} else if (eDataType == DataType_Tracers) {
			nComponents = m_datavecTracers[0].GetComponents();

		// Topography Data: Special handling due to 2D nature of data
		} else if (eDataType == DataType_Topography) {
			nComponents = 1;
			pData2D = (double**)(m_dataTopography);

		// Vorticity Data
		} else if (eDataType == DataType_Vorticity) {
			nComponents = 1;

		// Divergence Data
		} else if (eDataType == DataType_Divergence) {
			nComponents = 1;

		// Temperature Data
		} else if (eDataType == DataType_Temperature) {
			nComponents = 1;

		} else {
			_EXCEPTIONT("Invalid DataType");
		}

		// Number of radial elements
		for (int c = 0; c < nComponents; c++) {

			const double *** pData;
			if (eDataType == DataType_State) {
				if (eDataLocation == DataLocation_Node) {
					pData = (const double ***)(m_datavecStateNode[0][c]);
				} else {
					pData = (const double ***)(m_datavecStateREdge[0][c]);
				}
	
			} else if (eDataType == DataType_Topography) {
				pData = (const double ***)(&pData2D);
				nRElements = 1;

			} else if (eDataType == DataType_Tracers) {
				pData = (const double ***)(m_datavecTracers[0][c]);

			} else if (eDataType == DataType_Vorticity) {
				pData = (const double ***)(double ***)(m_dataVorticity);

			} else if (eDataType == DataType_Divergence) {
				pData = (const double ***)(double ***)(m_dataDivergence);

			} else if (eDataType == DataType_Temperature) {
				pData = (const double ***)(double ***)(m_dataTemperature);
			}

			// Perform interpolation on all levels
			for (int k = 0; k < nRElements; k++) {

				dInterpData[c][k][i] = 0.0;

				for (int m = 0; m < m_nHorizontalOrder; m++) {
				for (int n = 0; n < m_nHorizontalOrder; n++) {
					dInterpData[c][k][i] +=
						  dAInterpCoeffs[m]
						* dBInterpCoeffs[n]
						* pData[k][iA+m][iB+n];
				}
				}

				// Do not include the reference state
				if ((eDataType == DataType_State) &&
					(!fIncludeReferenceState)
				) {
					if (eDataLocation == DataLocation_Node) {
						for (int m = 0; m < m_nHorizontalOrder; m++) {
						for (int n = 0; n < m_nHorizontalOrder; n++) {
							dInterpData[c][k][i] -=
								  dAInterpCoeffs[m]
								* dBInterpCoeffs[n]
								* m_dataRefStateNode[c][k][iA+m][iB+n];
						}
						}

					} else {
						for (int m = 0; m < m_nHorizontalOrder; m++) {
						for (int n = 0; n < m_nHorizontalOrder; n++) {
							dInterpData[c][k][i] -=
								  dAInterpCoeffs[m]
								* dBInterpCoeffs[n]
								* m_dataRefStateREdge[c][k][iA+m][iB+n];
						}
						}
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::TransformHaloVelocities(
	int iDataUpdate
) {
	// Transform not necessary on Cartesian grid
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchCartesianGLL::TransformTopographyDeriv() {

	// Transform not necessary on Cartesian grid
}

///////////////////////////////////////////////////////////////////////////////

