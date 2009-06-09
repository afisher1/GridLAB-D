/** $Id: triplex_meter.cpp 1002 2008-09-29 15:58:23Z d3m998 $
	Copyright (C) 2008 Battelle Memorial Institute
	@file triplex_meter.cpp
	@addtogroup powerflow_triplex_meter Meter
	@ingroup powerflow

	Distribution triplex_meter can be either single phase or polyphase triplex_meters.
	Single phase triplex_meters present three lines to objects
	- Line 1-G: 120V,
	- Line 2-G: 120V
	- Line 3-G: 0V
	- Line 1-2: 240V
	- Line 2-3: 120V
	- Line 1-3: 120V

	Total cumulative energy, instantantenous power and peak demand are triplex_metered.

	@{
 **/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "triplex_meter.h"
#include "timestamp.h"

// useful macros
#define TO_HOURS(t) (((double)t) / (3600 * TS_SECOND))

// meter reset function
EXPORT int64 triplex_meter_reset(OBJECT *obj)
{
	triplex_meter *pMeter = OBJECTDATA(obj,triplex_meter);
	pMeter->measured_demand = 0;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// triplex_meter CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
// class management data
CLASS* triplex_meter::oclass = NULL;
CLASS* triplex_meter::pclass = NULL;

// the constructor registers the class and properties and sets the defaults
triplex_meter::triplex_meter(MODULE *mod) : triplex_node(mod)
{
	// first time init
	if (oclass==NULL)
	{
		// link to parent class (used by isa)
		pclass = triplex_node::oclass;

		// register the class definition
		oclass = gl_register_class(mod,"triplex_meter",sizeof(triplex_meter),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_INHERIT, "triplex_node",
			PT_double, "measured_energy[Wh]", PADDR(measured_energy),
			PT_double, "measured_power[VA]", PADDR(measured_power),
			PT_double, "measured_demand[W]", PADDR(measured_demand),
			PT_double, "measured_real_power[W]", PADDR(measured_real_power),
			
			// added to record last voltage/current
			PT_complex, "measured_voltage_1[V]", PADDR(measured_voltage[0]),
			PT_complex, "measured_voltage_2[V]", PADDR(measured_voltage[1]),
			PT_complex, "measured_voltage_N[V]", PADDR(measured_voltage[2]),
			PT_complex, "measured_current_1[A]", PADDR(measured_current[0]),
			PT_complex, "measured_current_2[A]", PADDR(measured_current[1]),
			PT_complex, "measured_current_N[A]", PADDR(measured_current[2]),

			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
		}
}

int triplex_meter::isa(char *classname)
{
	return strcmp(classname,"triplex_meter")==0 || triplex_node::isa(classname);
}

// Create a distribution triplex_meter from the defaults template, return 1 on success
int triplex_meter::create()
{
	int result = triplex_node::create();
	measured_energy = 0;
	measured_power = 0;
	measured_demand = 0;
	return result;
}

// Initialize a distribution triplex_meter, return 1 on success
int triplex_meter::init(OBJECT *parent)
{
	return triplex_node::init(parent);
}

// Synchronize a distribution triplex_meter
TIMESTAMP triplex_meter::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	// compute demand power
	if (measured_power>measured_demand) 
		measured_demand=measured_power;

	//Zero out current - it is the constant current load (houses will accumulate here)
	current[0] = current[1] = current[2] = 0.0;

	// compute energy use
	if (t0>0)
		measured_energy += measured_power * TO_HOURS(t1 - t0);

	return triplex_node::presync(t1);
}

TIMESTAMP triplex_meter::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	//measured_voltage[0] = voltageA;
	measured_voltage[0].SetPolar(voltageA.Mag(),voltageA.Arg());
	measured_voltage[1].SetPolar(voltageB.Mag(),voltageB.Arg());
	measured_voltage[2].SetPolar(voltageC.Mag(),voltageC.Arg());
	measured_current[0] = current_inj[0];
	measured_current[1] = current_inj[1];
	measured_current[2] = current_inj[2];
	measured_power = (measured_voltage[0]*(~measured_current[0])
				   - measured_voltage[1]*(~measured_current[1])
				   + measured_voltage[2]*(~measured_current[2])).Mag();
	measured_real_power = (measured_voltage[0]*(~measured_current[0])).Re()
						- (measured_voltage[1]*(~measured_current[1])).Re()
						+ (measured_voltage[2]*(~measured_current[2])).Re();
	return triplex_node::postsync(t1);
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int isa_triplex_meter(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,triplex_meter)->isa(classname);
}

EXPORT int create_triplex_meter(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(triplex_meter::oclass);
		if (*obj!=NULL)
		{
			triplex_meter *my = OBJECTDATA(*obj,triplex_meter);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (char *msg)
	{
		gl_error("create_triplex_meter: %s", msg);
	}
	return 0;
}

EXPORT int init_triplex_meter(OBJECT *obj)
{
	triplex_meter *my = OBJECTDATA(obj,triplex_meter);
	try {
		return my->init(obj->parent);
	}
	catch (char *msg)
	{
		GL_THROW("%s (triplex_meter:%d): %s", my->get_name(), my->get_id(), msg);
		return 0; 
	}
}

EXPORT TIMESTAMP sync_triplex_meter(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	triplex_meter *pObj = OBJECTDATA(obj,triplex_meter);
	try {
		TIMESTAMP t1;
		switch (pass) {
		case PC_PRETOPDOWN:
			return pObj->presync(obj->clock,t0);
		case PC_BOTTOMUP:
			return pObj->sync(t0);
		case PC_POSTTOPDOWN:
			t1 = pObj->postsync(obj->clock,t0);
			obj->clock = t0;
			return t1;
		default:
			throw "invalid pass request";
		}
		throw "invalid pass request";
	} catch (const char *error) {
		GL_THROW("%s (triplex_meter:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return 0; 
	} catch (...) {
		GL_THROW("%s (triplex_meter:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return 0;
	}
}

/**@}**/
