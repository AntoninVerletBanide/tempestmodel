///////////////////////////////////////////////////////////////////////////////
///
///	\file    ThermalBubbleCartesianTest.cpp
///	\author  Paul Ullrich, Jorge Guerra
///	\version December 18, 2013
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

#include "Tempest.h"

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		Giraldo et al. (2007)
///
///		Thermal rising bubble test case.
///	</summary>
class ThermalBubbleCartesianTest : public TestCase {

public:
	/// <summary>
	///		Grid dimension array (FOR CARTESIAN GRIDS).
	///	</summary>
	double m_dGDim[6];

private:
	///	<summary>
	///		Background height field.
	///	</summary>
	double m_dH0;

	///	<summary>
	///		Reference constant background pontential temperature
	///	</summary>
	double m_dThetaBar;

	///	<summary>
	///		Parameter factor for temperature disturbance
	///	</summary>
	double m_dThetaC;

	///	<summary>
	///		Parameter reference bubble radius
	///	</summary>
	double m_drC;

	///	<summary>
	///		Parameter reference length x for temperature disturbance
	///	</summary>
	double m_dxC;

	///	<summary>
	///		Parameter reference length z for temperature disturbance
	///	</summary>
	double m_dzC;

	///	<summary>
	///		Parameter Archimede's Constant (essentially Pi but to some digits)
	///	</summary>
	double m_dpiC;

public:
	///	<summary>
	///		Constructor. (with physical constants defined privately here)
	///	</summary>
	ThermalBubbleCartesianTest() :
		m_dH0(10000.),
		m_dThetaBar(300.0),
		m_dThetaC(0.5),
		m_drC(250.),
		m_dxC(500.),
		m_dzC(350.),
		m_dpiC(3.14159265)
	{
		// Set the dimensions of the box
		m_dGDim[0] = 0.0;
		m_dGDim[1] = 1000.0;
		m_dGDim[2] = -1000.0;
		m_dGDim[3] = 1000.0;
		m_dGDim[4] = 0.0;
		m_dGDim[5] = 1000.0;
	}

public:
	///	<summary>
	///		Number of tracers used in this test.
	///	</summary>
	virtual int GetTracerCount() const {
		return 0;
	}

	///	<summary>
	///		Get the altitude of the model cap.
	///	</summary>
	virtual double GetZtop() const {
		return m_dGDim[5];
	}

	///	<summary>
	///		Flag indicating that a reference state is available.
	///	</summary>
	virtual bool HasReferenceState() const {
		return true;
	}

	///	<summary>
	///		Obtain test case specific physical constants.
	///	</summary>
	virtual void EvaluatePhysicalConstants(
		PhysicalConstants & phys
	) const {
		// Do nothing to the PhysicalConstants for global simulations
	}

	///	<summary>
	///		Evaluate the topography at the given point. (cartesian version)
	///	</summary>
	virtual double EvaluateTopography(
	   double dXp,
	   double dYp
	) const {
		// This test case has no topography associated with it
		return 0.0;
	}

	///	<summary>
	///		Evaluate the potential temperature field perturbation.
	///	</summary>
	double EvaluateTPrime(
		const PhysicalConstants & phys,
		double dXp,
		double dZp
	) const {

		// Potential temperature perturbation bubble using radius
		double xL2 = (dXp - m_dxC) * (dXp - m_dxC);
		double zL2 = (dZp - m_dzC) * (dZp - m_dzC);
		double dRp = sqrt(xL2 + zL2);

		double dThetaHat = 1.0;
		if (dRp <= m_drC) {
			dThetaHat = 0.5 * m_dThetaC * (1.0 + cos(m_dpiC * dRp / m_drC));
		} else if (dRp > m_drC) {
			dThetaHat = 0.0;
		}

		return dThetaHat;
	}

	///	<summary>
	///		Evaluate the reference state at the given point.
	///	</summary>
	virtual void EvaluateReferenceState(
		const PhysicalConstants & phys,
		double dZp,
		double dXp,
		double dYp,
		double * dState
	) const {
	    const double dG = phys.GetG();
		const double dCv = phys.GetCv();
		const double dCp = phys.GetCp();
		const double dRd = phys.GetR();
		const double dP0 = phys.GetP0();
		// Set the uniform U, V, W field for all time
		dState[0] = 0.0;
		dState[1] = 0.0;
		dState[3] = 0.0;

		// Set the initial potential temperature field
		dState[2] = m_dThetaBar;

		// Set the initial density based on the Exner pressure
		double dExnerP =
			- dG / (dCp * m_dThetaBar) * dZp + 1.0;
		double dRho =
			dP0 / (dRd * m_dThetaBar) *
			  pow(dExnerP, (dCv / dRd));

		dState[4] = dRho;
	}

	///	<summary>
	///		Evaluate the state vector at the given point.
	///	</summary>
	virtual void EvaluatePointwiseState(
		const PhysicalConstants & phys,
		const Time & time,
		double dZp,
		double dXp,
		double dYp,
		double * dState,
		double * dTracer
	) const {
	    const double dG = phys.GetG();
		const double dCv = phys.GetCv();
		const double dCp = phys.GetCp();
		const double dRd = phys.GetR();
		const double dP0 = phys.GetP0();
		// Set the uniform U, V, W field for all time
		dState[0] = 0.0;
		dState[1] = 0.0;
		dState[3] = 0.0;

		// Set the initial potential temperature field
		dState[2] = m_dThetaBar + EvaluateTPrime(phys, dXp, dZp);

		// Set the initial density based on the Exner pressure
		double dExnerP =
			- dG / (dCp * m_dThetaBar) * dZp + 1.0;
		double dRho =
			dP0 / (dRd * m_dThetaBar) *
			  pow(dExnerP, (dCv / dRd));

		dState[4] = dRho;
	}
};

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

	// Initialize Tempest
	TempestInitialize(&argc, &argv);

try {

	// Parse the command line
	BeginTempestCommandLine("ThermalBubbleCartesianTest");
		SetDefaultResolutionX(36);
		SetDefaultResolutionY(1);
		SetDefaultLevels(72);
		SetDefaultOutputDeltaT("10s");
		SetDefaultDeltaT("10000u");
		SetDefaultEndTime("700s");
		SetDefaultHorizontalOrder(4);
		SetDefaultVerticalOrder(1);

		ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	// Create a new instance of the test
	ThermalBubbleCartesianTest * test =
		new ThermalBubbleCartesianTest();

	// Setup the Model
	AnnounceBanner("MODEL SETUP");

	Model model(EquationSet::PrimitiveNonhydrostaticEquations);

	TempestSetupCartesianModel(model, test->m_dGDim, 0.0, 0.0);

	// Set the reference length to reduce diffusion (1100km)
	model.GetGrid()->SetReferenceLength(1100000.0);

	// Set the test case for the model
	AnnounceStartBlock("Initializing test case");
	model.SetTestCase(test);
	AnnounceEndBlock("Done");

	// Begin execution
	AnnounceBanner("SIMULATION");
	model.Go();

	// Compute error norms
	AnnounceBanner("RESULTS");
	model.ComputeErrorNorms();
	AnnounceBanner();

} catch(Exception & e) {
	std::cout << e.ToString() << std::endl;
}

	// Deinitialize Tempest
	TempestDeinitialize();
}

///////////////////////////////////////////////////////////////////////////////

