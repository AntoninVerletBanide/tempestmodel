///////////////////////////////////////////////////////////////////////////////
///
///	\file    GridLonLat.cpp
///	\author  Paul Ullrich
///	\version February 25, 2013
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

#include "GridLonLat.h"

///////////////////////////////////////////////////////////////////////////////

void GridPatchLonLat::InitializeData() {
}

///////////////////////////////////////////////////////////////////////////////

void GridPatchLonLat::EvaluateTestCase(
	const TestCase & test,
	const Time & time,
	int iDataInstance
) {
}

///////////////////////////////////////////////////////////////////////////////

GridLonLat::GridLonLat(
	Model & model,
	int nLongitudes,
	int nLatitudes,
	int nRefinementRatio,
	int nRElements
) :
	// Call up the stack
	Grid::Grid(
		model,
		nLongitudes,
		nLatitudes,
		nRefinementRatio,
		nRElements)
{
/*
	// Create master patch
	PatchBox boxMaster(0, 0, 0, nLongitudes, 0, nLatitudes);
	m_vecGridPatches.push_back(
		new GridPatchLonLat((*this), boxMaster));
*/
}

///////////////////////////////////////////////////////////////////////////////


