// StaticOptimization.cpp
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//	AUTHOR: Jeff Reinbolt
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/* Copyright (c)  2006 Stanford University
* Use of the OpenSim software in source form is permitted provided that the following
* conditions are met:
* 	1. The software is used only for non-commercial research and education. It may not
*     be used in relation to any commercial activity.
* 	2. The software is not distributed or redistributed.  Software distribution is allowed 
*     only through https://simtk.org/home/opensim.
* 	3. Use of the OpenSim software or derivatives must be acknowledged in all publications,
*      presentations, or documents describing work in which OpenSim or derivatives are used.
* 	4. Credits to developers may not be removed from executables
*     created from modifications of the source.
* 	5. Modifications of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer. 
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
*  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
*  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
*  SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
*  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
*  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR BUSINESS INTERRUPTION) OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
*  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


//=============================================================================
// INCLUDES
//=============================================================================
#include <iostream>
#include <string>
#include <OpenSim/Common/IO.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/SimbodyEngine/SimbodyEngine.h>
#include <OpenSim/Simulation/Model/BodySet.h>
#include <OpenSim/Simulation/Model/CoordinateSet.h>
#include <OpenSim/Simulation/Model/ForceSet.h>
#include <OpenSim/Simulation/Model/Muscle.h>
#include <OpenSim/Common/GCVSplineSet.h>
#include <OpenSim/Actuators/CoordinateActuator.h>
#include <OpenSim/Simulation/Control/ControlSet.h>
#include <SimTKmath.h>
#include <SimTKlapack.h>
#include "StaticOptimization.h"
#include "StaticOptimizationTarget.h"


using namespace OpenSim;
using namespace std;

//=============================================================================
// CONSTRUCTOR(S) AND DESTRUCTOR
//=============================================================================
//_____________________________________________________________________________
/**
 * Destructor.
 */
StaticOptimization::~StaticOptimization()
{
	deleteStorage();
	delete _modelWorkingCopy;
	if(_ownsForceSet) delete _forceSet;
}
//_____________________________________________________________________________
/**
 */
StaticOptimization::StaticOptimization(Model *aModel) :
	Analysis(aModel),
	_useModelForceSet(_useModelForceSetProp.getValueBool()),
	_activationExponent(_activationExponentProp.getValueDbl()),
	_useMusclePhysiology(_useMusclePhysiologyProp.getValueBool()),
	_modelWorkingCopy(NULL),
	_numCoordinateActuators(0)
{
	setNull();

	if(aModel) setModel(*aModel);
	else allocateStorage();
}
// Copy constrctor and virtual copy 
//_____________________________________________________________________________
/**
 * Copy constructor.
 *
 */
StaticOptimization::StaticOptimization(const StaticOptimization &aStaticOptimization):
	Analysis(aStaticOptimization),
	_useModelForceSet(_useModelForceSetProp.getValueBool()),
	_activationExponent(_activationExponentProp.getValueDbl()),
	_useMusclePhysiology(_useMusclePhysiologyProp.getValueBool()),
	_modelWorkingCopy(NULL),
	_numCoordinateActuators(aStaticOptimization._numCoordinateActuators)
{
	setNull();
	// COPY TYPE AND NAME
	*this = aStaticOptimization;
}
//_____________________________________________________________________________
/**
 * Clone
 *
 */
Object* StaticOptimization::copy() const
{
	StaticOptimization *object = new StaticOptimization(*this);
	return(object);

}
//=============================================================================
// OPERATORS
//=============================================================================
//-----------------------------------------------------------------------------
// ASSIGNMENT
//-----------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Assign this object to the values of another.
 *
 * @return Reference to this object.
 */
StaticOptimization& StaticOptimization::
operator=(const StaticOptimization &aStaticOptimization)
{
	// BASE CLASS
	Analysis::operator=(aStaticOptimization);

	_modelWorkingCopy = aStaticOptimization._modelWorkingCopy;
	_numCoordinateActuators = aStaticOptimization._numCoordinateActuators;
	_useModelForceSet = aStaticOptimization._useModelForceSet;
	_activationExponent=aStaticOptimization._activationExponent;

	_useMusclePhysiology=aStaticOptimization._useMusclePhysiology;
	return(*this);
}

//_____________________________________________________________________________
/**
 * SetNull().
 */
void StaticOptimization::
setNull()
{
	setupProperties();

	// OTHER VARIABLES
	_useModelForceSet = true;
	_activationStorage = NULL;
	_forceStorage = NULL;
	_ownsForceSet = false;
	_forceSet = NULL;
	_activationExponent=2;
	_useMusclePhysiology=true;
	_numCoordinateActuators = 0;

	setType("StaticOptimization");
	setName("StaticOptimization");
}
//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 */
void StaticOptimization::
setupProperties()
{
	_useModelForceSetProp.setComment("If true, the model's own force set will be used in the static optimization computation.  "
													"Otherwise, inverse dynamics for coordinate actuators will be computed for all unconstrained degrees of freedom.");
	_useModelForceSetProp.setName("use_model_force_set");
	_propertySet.append(&_useModelForceSetProp);

	_activationExponentProp.setComment(
		"A double indicating the exponent to raise activations to when solving static optimization.  ");
	_activationExponentProp.setName("activation_exponent");
	_propertySet.append(&_activationExponentProp);

	
	_useMusclePhysiologyProp.setComment(
		"If true muscle force-length curve is observed while running optimization.");
	_useMusclePhysiologyProp.setName("use_muscle_physiology");
	_propertySet.append(&_useMusclePhysiologyProp);
}

//=============================================================================
// CONSTRUCTION METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Construct a description for the static optimization files.
 */
void StaticOptimization::
constructDescription()
{
	string descrip = "This file contains static optimization results.\n\n";
	setDescription(descrip);
}

//_____________________________________________________________________________
/**
 * Construct column labels for the static optimization files.
 */
void StaticOptimization::
constructColumnLabels()
{
	Array<string> labels;
	labels.append("time");
	if(_model) 
		for (int i=0; i < _forceSet->getSize(); i++) labels.append(_forceSet->get(i).getName());
	setColumnLabels(labels);
}

//_____________________________________________________________________________
/**
 * Allocate storage for the static optimization.
 */
void StaticOptimization::
allocateStorage()
{
	_activationStorage = new Storage(1000,"Static Optimization");
	_activationStorage->setDescription(getDescription());
	_activationStorage->setColumnLabels(getColumnLabels());

	_forceStorage = new Storage(1000,"Static Optimization");
	_forceStorage->setDescription(getDescription());
	_forceStorage->setColumnLabels(getColumnLabels());

}


//=============================================================================
// DESTRUCTION METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Delete storage objects.
 */
void StaticOptimization::
deleteStorage()
{
	delete _activationStorage; _activationStorage = NULL;
	delete _forceStorage; _forceStorage = NULL;
}

//=============================================================================
// GET AND SET
//=============================================================================
//_____________________________________________________________________________
/**
 * Set the model for which the static optimization is to be computed.
 *
 * @param aModel Model pointer
 */
void StaticOptimization::
setModel(Model& aModel)
{
	Analysis::setModel(aModel);
	//SimTK::State& s = aModel->getMultibodySystem()->updDefaultState();
}

//-----------------------------------------------------------------------------
// STORAGE
//-----------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Get the activation storage.
 *
 * @return Activation storage.
 */
Storage* StaticOptimization::
getActivationStorage()
{
	return(_activationStorage);
}
//_____________________________________________________________________________
/**
 * Get the force storage.
 *
 * @return Force storage.
 */
Storage* StaticOptimization::
getForceStorage()
{
	return(_forceStorage);
}

//-----------------------------------------------------------------------------
// STORAGE CAPACITY
//-----------------------------------------------------------------------------
//_____________________________________________________________________________
/**
 * Set the capacity increments of all storage instances.
 *
 * @param aIncrement Increment by which storage capacities will be increased
 * when storage capacities run out.
 */
void StaticOptimization::
setStorageCapacityIncrements(int aIncrement)
{
	_activationStorage->setCapacityIncrement(aIncrement);
	_forceStorage->setCapacityIncrement(aIncrement);
}

//=============================================================================
// ANALYSIS
//=============================================================================
//_____________________________________________________________________________
/**
 * Record the results.
 */
int StaticOptimization::
record(const SimTK::State& s)
{
	if(!_modelWorkingCopy) return -1;

	// Set model Q's and U's
	SimTK::State& sWorkingCopy = _modelWorkingCopy->updMultibodySystem().updDefaultState();

	sWorkingCopy.setTime(s.getTime());
	sWorkingCopy.setQ(s.getQ());
	sWorkingCopy.setU(s.getU());
	_modelWorkingCopy->computeEquilibriumForAuxiliaryStates(sWorkingCopy);

    const Set<Actuator>& fs = _modelWorkingCopy->getActuators();

	int na = fs.getSize();
	int nacc = _accelerationIndices.getSize();

	// IPOPT
	_numericalDerivativeStepSize = 0.0001;
	_optimizerAlgorithm = "ipopt";
	_printLevel = 0;
	_optimizationConvergenceTolerance = 1e-004;
	_maxIterations = 2000;

	// Optimization target
	_modelWorkingCopy->setAllControllersEnabled(false);
	StaticOptimizationTarget target(sWorkingCopy,_modelWorkingCopy,na,nacc,_useMusclePhysiology);
	target.setStatesStore(_statesStore);
	target.setStatesSplineSet(_statesSplineSet);
	target.setActivationExponent(_activationExponent);
	target.setDX(_numericalDerivativeStepSize);

	// Pick optimizer algorithm
	SimTK::OptimizerAlgorithm algorithm = SimTK::InteriorPoint;
	//SimTK::OptimizerAlgorithm algorithm = SimTK::CFSQP;

	// Optimizer
	SimTK::Optimizer *optimizer = new SimTK::Optimizer(target, algorithm);

	// Optimizer options
	//cout<<"\nSetting optimizer print level to "<<_printLevel<<".\n";
	optimizer->setDiagnosticsLevel(_printLevel);
	//cout<<"Setting optimizer convergence criterion to "<<_optimizationConvergenceTolerance<<".\n";
	optimizer->setConvergenceTolerance(_optimizationConvergenceTolerance);
	//cout<<"Setting optimizer maximum iterations to "<<_maxIterations<<".\n";
	optimizer->setMaxIterations(_maxIterations);
	optimizer->useNumericalGradient(false);
	optimizer->useNumericalJacobian(false);
	if(algorithm == SimTK::InteriorPoint) {
		// Some IPOPT-specific settings
		optimizer->setLimitedMemoryHistory(500); // works well for our small systems
		optimizer->setAdvancedBoolOption("warm_start",true);
		optimizer->setAdvancedRealOption("obj_scaling_factor",1);
		optimizer->setAdvancedRealOption("nlp_scaling_max_gradient",1);
	}

	// Parameter bounds
	SimTK::Vector lowerBounds(na), upperBounds(na);
	for(int i=0,j=0;i<fs.getSize();i++) {
		Actuator& act = fs.get(i);
		lowerBounds(j) = act.getMinControl();
	    upperBounds(j) = act.getMaxControl();
        j++;
	}
	
	target.setParameterLimits(lowerBounds, upperBounds);

	_parameters = 0; // Set initial guess to zeros

	// Static optimization
	_modelWorkingCopy->getMultibodySystem().realize(sWorkingCopy,SimTK::Stage::Velocity);
	target.prepareToOptimize(sWorkingCopy, &_parameters[0]);

	//LARGE_INTEGER start;
	//LARGE_INTEGER stop;
	//LARGE_INTEGER frequency;

	//QueryPerformanceFrequency(&frequency);
	//QueryPerformanceCounter(&start);

	try {
		target.setCurrentState( &sWorkingCopy );
		optimizer->optimize(_parameters);
	}
	catch (const SimTK::Exception::Base &ex) {
		cout << ex.getMessage() << endl;
		cout << "OPTIMIZATION FAILED..." << endl;
		cout << endl;
		cout << "StaticOptimization.record:  WARN- The optimizer could not find a solution at time = " << s.getTime() << endl;
		cout << endl;

		double tolBounds = 1e-1;
		bool weakModel = false;
		string msgWeak = "The model appears too weak for static optimization.\nTry increasing the strength and/or range of the following force(s):\n";
		for(int a=0;a<na;a++) {
			Actuator* act = dynamic_cast<Actuator*>(&_forceSet->get(a));
            if( act ) {
			    Muscle*  mus = dynamic_cast<Muscle*>(&_forceSet->get(a));
 			    if(mus==NULL) {
			    	if(_parameters(a) < (lowerBounds(a)+tolBounds)) {
			    		msgWeak += "   ";
			    		msgWeak += act->getName();
			    		msgWeak += " approaching lower bound of ";
			    		ostringstream oLower;
			    		oLower << lowerBounds(a);
			    		msgWeak += oLower.str();
			    		msgWeak += "\n";
			    		weakModel = true;
			    	} else if(_parameters(a) > (upperBounds(a)-tolBounds)) {
			    		msgWeak += "   ";
			    		msgWeak += act->getName();
			    		msgWeak += " approaching upper bound of ";
			    		ostringstream oUpper;
			    		oUpper << upperBounds(a);
			    		msgWeak += oUpper.str();
			    		msgWeak += "\n";
			    		weakModel = true;
			    	} 
			    } else {
			    	if(_parameters(a) > (upperBounds(a)-tolBounds)) {
			    		msgWeak += "   ";
			    		msgWeak += mus->getName();
			    		msgWeak += " approaching upper bound of ";
			    		ostringstream o;
			    		o << upperBounds(a);
			    		msgWeak += o.str();
			    		msgWeak += "\n";
			    		weakModel = true;
			    	}
			    }
            }
		}
		if(weakModel) cout << msgWeak << endl;

		if(!weakModel) {
			double tolConstraints = 1e-6;
			bool incompleteModel = false;
			string msgIncomplete = "The model appears unsuitable for static optimization.\nTry appending the model with additional force(s) or locking joint(s) to reduce the following acceleration constraint violation(s):\n";
			SimTK::Vector constraints;
			target.constraintFunc(_parameters,true,constraints);
			const CoordinateSet& coordSet = _modelWorkingCopy->getCoordinateSet();
			for(int acc=0;acc<nacc;acc++) {
				if(fabs(constraints(acc)) > tolConstraints) {
					const Coordinate& coord = coordSet.get(_accelerationIndices[acc]);
					msgIncomplete += "   ";
					msgIncomplete += coord.getName();
					msgIncomplete += ": constraint violation = ";
					ostringstream o;
					o << constraints(acc);
					msgIncomplete += o.str();
					msgIncomplete += "\n";
					incompleteModel = true;
				}
			}
			if(incompleteModel) cout << msgIncomplete << endl;
		}
	}

	//QueryPerformanceCounter(&stop);
	//double duration = (double)(stop.QuadPart-start.QuadPart)/(double)frequency.QuadPart;
	//cout << "optimizer time = " << (duration*1.0e3) << " milliseconds" << endl;

	target.printPerformance(sWorkingCopy, &_parameters[0]);

	_activationStorage->append(sWorkingCopy.getTime(),na,&_parameters[0]);

	SimTK::Vector forces(na);
	target.getActuation(const_cast<SimTK::State&>(sWorkingCopy), _parameters,forces);

	_forceStorage->append(sWorkingCopy.getTime(),na,&forces[0]);

	return 0;
}
//_____________________________________________________________________________
/**
 * This method is called at the beginning of an analysis so that any
 * necessary initializations may be performed.
 *
 * This method is meant to be called at the begining of an integration 
 *
 * @param s Current state .
 *
 * @return -1 on error, 0 otherwise.
 */
int StaticOptimization::
begin(SimTK::State& s )
{
	if(!proceed()) return(0);

	// Make a working copy of the model
	delete _modelWorkingCopy;
	_modelWorkingCopy =  dynamic_cast<Model*>(_model->copy());
	//_modelWorkingCopy = _model->clone();
	_modelWorkingCopy->initSystem();

	// Replace model force set with only generalized forces
	if(_model) {
		SimTK::State& sWorkingCopyTemp = _modelWorkingCopy->updMultibodySystem().updDefaultState();
		// Update the _forceSet we'll be computing inverse dynamics for
		if(_ownsForceSet) delete _forceSet;
		if(_useModelForceSet) {
			// Set pointer to model's internal force set
			_forceSet = &_modelWorkingCopy->updForceSet();
			_ownsForceSet = false;
		} else {
			ForceSet& as = _modelWorkingCopy->updForceSet();
			// Keep a copy of forces that are not muscles to restore them back.
			ForceSet* saveForces = (ForceSet*)as.copy();
			// Generate an force set consisting of a coordinate actuator for every unconstrained degree of freedom
			_forceSet = CoordinateActuator::CreateForceSetOfCoordinateActuatorsForModel(sWorkingCopyTemp,*_modelWorkingCopy,1,false);
			_ownsForceSet = false;
			_modelWorkingCopy->setAllControllersEnabled(false);
			_numCoordinateActuators = _forceSet->getSize();
			// Copy whatever forces that are not muscles back into the model
			
			for(int i=0; i<saveForces->getSize(); i++){
				const Force& f=saveForces->get(i);
				if ((dynamic_cast<const Muscle*>(&saveForces->get(i)))==NULL)
					as.append((Force*)saveForces->get(i).copy());
			}
		}

		SimTK::State& sWorkingCopy = _modelWorkingCopy->initSystem();

		// Set modeiling options for Actuators to be overriden
		for(int i=0,j=0; i<_forceSet->getSize(); i++) {
			Actuator* act = dynamic_cast<Actuator*>(&_forceSet->get(i));
			if( act ) {
				act->overrideForce(sWorkingCopy,true);
			}
		}

		sWorkingCopy.setQ(s.getQ());
		sWorkingCopy.setU(s.getU());
		sWorkingCopy.setZ(s.getZ());
		_modelWorkingCopy->getMultibodySystem().realize(s,SimTK::Stage::Velocity);
		_modelWorkingCopy->computeEquilibriumForAuxiliaryStates(sWorkingCopy);
		// Gather indices into speed set corresponding to the unconstrained degrees of freedom (for which we will set acceleration constraints)
		_accelerationIndices.setSize(0);
		const CoordinateSet& coordSet = _model->getCoordinateSet();
		for(int i=0; i<coordSet.getSize(); i++) {
			const Coordinate& coord = coordSet.get(i);
			if(!coord.isConstrained(sWorkingCopy)) {
				_accelerationIndices.append(i);
			}
		}

		int na = _forceSet->getSize();
		int nacc = _accelerationIndices.getSize();

		if(na < nacc) 
			throw(Exception("StaticOptimization: ERROR- overconstrained system -- need at least as many forces as there are degrees of freedom.\n"));

		_parameters.resize(na);
		_parameters = 0;
	}

	_statesSplineSet=GCVSplineSet(5,_statesStore);

	// DESCRIPTION AND LABELS
	constructDescription();
	constructColumnLabels();

	deleteStorage();
	allocateStorage();

	// RESET STORAGE
	_activationStorage->reset(s.getTime());
	_forceStorage->reset(s.getTime());

	// RECORD
	int status = 0;
	if(_activationStorage->getSize()<=0 && _forceStorage->getSize()<=0) {
		status = record(s);
		const Set<Actuator>& fs = _modelWorkingCopy->getActuators();
		for(int k=0;k<fs.getSize();k++) {
			Actuator& act = fs.get(k);
			cout << "Bounds for " << act.getName() << ": " << act.getMinControl()<< " to "<< act.getMaxControl() << endl;
		}
	}

	return(status);
}
//_____________________________________________________________________________
/**
 * This method is called to perform the analysis.  It can be called during
 * the execution of a forward integrations or after the integration by
 * feeding it the necessary data.
 *
 * This method should be overriden in derived classes.  It is
 * included here so that the derived class will not have to implement it if
 * it is not necessary.
 *
 * @param s Current state .
 *
 * @return -1 on error, 0 otherwise.
 */
int StaticOptimization::
step(const SimTK::State& s, int stepNumber )
{
	if(!proceed(stepNumber)) return(0);

	record(s);

	return(0);
}
//_____________________________________________________________________________
/**
 * This method is called at the end of an analysis so that any
 * necessary finalizations may be performed.
 *
 * @param s Current state 
 *
 * @return -1 on error, 0 otherwise.
 */
int StaticOptimization::
end( SimTK::State& s )
{
	if(!proceed()) return(0);

	record(s);

	return(0);
}


//=============================================================================
// IO
//=============================================================================
//_____________________________________________________________________________
/**
 * Print results.
 * 
 * The file names are constructed as
 * aDir + "/" + aBaseName + "_" + ComponentName + aExtension
 *
 * @param aDir Directory in which the results reside.
 * @param aBaseName Base file name.
 * @param aDT Desired time interval between adjacent storage vectors.  Linear
 * interpolation is used to print the data out at the desired interval.
 * @param aExtension File extension.
 *
 * @return 0 on success, -1 on error.
 */
int StaticOptimization::
printResults(const string &aBaseName,const string &aDir,double aDT,
				 const string &aExtension)
{
	// ACTIVATIONS
	Storage::printResult(_activationStorage,aBaseName+"_"+getName()+"_activation",aDir,aDT,aExtension);

	// FORCES
	Storage::printResult(_forceStorage,aBaseName+"_"+getName()+"_force",aDir,aDT,aExtension);

	// Make a ControlSet out of activations for use in forward dynamics
	ControlSet cs(*_activationStorage);
	std::string path = (aDir=="") ? "." : aDir;
	std::string name = path + "/" + aBaseName+"_"+getName()+"_controls.xml";
	cs.print(name);
	return(0);
}
