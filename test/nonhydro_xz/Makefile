# TempestBase directory
TEMPESTBASEDIR= ../..

# Compile with BLAS libraries
USEBLAS= true

# Load system-specific defaults
include $(TEMPESTBASEDIR)/mk/Make.defs

##
## Build instructions
##
all: atm InertialGravityCartesianXZTest ThermalBubbleCartesianTest ScharMountainCartesianTest HydrostaticMountainCartesianTest NonHydroMountainCartesianTest Baroclinic3DCartesianTest

atm:
	cd $(TEMPESTBASEDIR)/src/base; make
	cd $(TEMPESTBASEDIR)/src/atm; make

##
## Individual test case build instructions
##
ThermalBubbleCartesianTest: $(BUILDDIR)/ThermalBubbleCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/ThermalBubbleCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

InertialGravityCartesianXZTest: $(BUILDDIR)/InertialGravityCartesianXZTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/InertialGravityCartesianXZTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

ScharMountainCartesianTest: $(BUILDDIR)/ScharMountainCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/ScharMountainCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

HydrostaticMountainCartesianTest: $(BUILDDIR)/HydrostaticMountainCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/HydrostaticMountainCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

NonHydroMountainCartesianTest: $(BUILDDIR)/NonHydroMountainCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/NonHydroMountainCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

Baroclinic3DCartesianTest: $(BUILDDIR)/Baroclinic3DCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(TEMPESTLIBS)
	$(CC) $(LDFLAGS) -o $@ $(BUILDDIR)/Baroclinic3DCartesianTest.o $(FILES:%.cpp=$(BUILDDIR)/%.o) $(LDFILES)

##
## Clean
##
clean:
	rm -f ThermalBubbleCartesianTest
	rm -f InertialGravityCartesianXZTest
	rm -f ScharMountainCartesianTest
	rm -f HydrostaticMountainCartesianTest
	rm -f NonHydroMountainCartesianTest
	rm -f Baroclinic3DCartesianTest
	rm -rf $(DEPDIR)
	rm -rf $(BUILDDIR)

##
## Include dependencies
##
include $(FILES:%.cpp=$(DEPDIR)/%.d)

# DO NOT DELETE

