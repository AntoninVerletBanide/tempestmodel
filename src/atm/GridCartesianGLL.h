///////////////////////////////////////////////////////////////////////////////
///
///	\file    GridCartesianGLL.h
///	\author  Paul Ullrich, Jorge Guerra
///	\version September 19, 2013
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

#ifndef _GRIDCARTESIANGLL_H_
#define _GRIDCARTESIANGLL_H_

#include "GridGLL.h"

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		The CubedSphere grid with degrees of freedom at Gauss-Lobatto-Legendre
///		quadrature nodes.
///	</summary>
class GridCartesianGLL : public GridGLL {

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	GridCartesianGLL(
		Model & model,
		int nBaseResolutionA,
		int nBaseResolutionB,
		int nRefinementRatio,
		int nHorizontalOrder,
		int nVerticalOrder,
		int nRElements,
		double dGDim[],
		double dRefLat,
		double dTopoHeight,
		VerticalStaggering eVerticalStaggering =
			VerticalStaggering_CharneyPhillips
	);

	///	<summary>
	///		Initialize grid patches.
	///	</summary>
	virtual void Initialize();
	
public:
	///	<summary
	///		Get the bounds on the reference grid.
	///	</summary>
	virtual void GetReferenceGridBounds(
		double & dX0,
		double & dX1,
		double & dY0,
		double & dY1
	);
	
	///	<summary>
	///		Add the default set of patches.
	///	</summary>
	virtual void AddDefaultPatches();

	///	<summary>
	///		Convert an array of coordinate variables to coordinates on the
	///		reference grid (RLL on the sphere)
	///	</summary>
	virtual void ConvertReferenceToPatchCoord(
		const DataVector<double> & dXReference,
		const DataVector<double> & dYReference,
		DataVector<double> & dAlpha,
		DataVector<double> & dBeta,
		DataVector<int> & iPanel
	) const;
	
	///	<summary>
	///		Get the patch and coordinate index for the specified node.
	///	</summary>
	virtual void GetPatchFromCoordinateIndex(
		int iRefinementLevel,
		const DataVector<int> & vecIxA,
		const DataVector<int> & vecIxB,
		const DataVector<int> & vecPanel,
		DataVector<int> & vecPatchIndex,
		int nVectorLength = (-1)
	);
	
	///	<summary>
	///		Get the relation between coordinate vectors across panel
	///		boundaries.
	///	</summary>
	virtual void GetOpposingDirection(
		int ixPanelSrc,
		int ixPanelDest,
		Direction dir,
		Direction & dirOpposing,
		bool & fSwitchParallel,
		bool & fSwitchPerpendicular
	) const;

public:
	///	<summary>
	///		Apply the boundary conditions.
	///	</summary>
	void ApplyBoundaryConditions(
		int iDataUpdate,
		DataType eDataType
	);

	///	<summary>
	///		Apply the direct stiffness summation (DSS) operation on the grid.
	///	</summary>
	virtual void ApplyDSS(
		int iDataUpdate,
		DataType eDataType = DataType_State
	);
	
private:
	///	<summary>
	///		Dimension of the grid - private to cartesian grids.
	///	</summary>
	double m_dGDim[6];

	///	<summary>
	///		Referece latitude (for beta plane cases)
	///	</summary>
	double m_dRefLat;

	///	<summary>
	///		Maximum height of a topography feature
	///	</summary>
	double m_dTopoHeight;

};

///////////////////////////////////////////////////////////////////////////////

#endif
