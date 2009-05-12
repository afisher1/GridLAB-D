/** $Id: meter.cpp 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file meter.cpp
	@addtogroup powerflow_meter Meter
	@ingroup powerflow

	@note The meter object now only implements polyphase meters.  For a singlephase
	meter, see the triplex_meter object.

	Distribution meter can be either single phase or polyphase meters.
	For polyphase meters, the line voltages are nominally 277V line-to-line, and
	480V line-to-ground.  The ground is not presented explicitly (it is assumed).

	Total cumulative energy, instantantenous power and peak demand are metered.

	@{
 **/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "meter.h"
#include "timestamp.h"

// useful macros
#define TO_HOURS(t) (((double)t) / (3600 * TS_SECOND))

// meter reset function
EXPORT int64 meter_reset(OBJECT *obj)
{
	meter *pMeter = OBJECTDATA(obj,meter);
	pMeter->measured_demand = 0;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// meter CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
// class management data
CLASS* meter::oclass = NULL;
CLASS* meter::pclass = NULL;

// the constructor registers the class and properties and sets the defaults
meter::meter(MODULE *mod) : node(mod)
{
	// first time init
	if (oclass==NULL)
	{
		// link to parent class (used by isa)
		pclass = node::oclass;

		// register the class definition
		oclass = gl_register_class(mod,"meter",sizeof(meter),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_INHERIT, "node",
			/// @todo three-phase meter should meter Q also (required complex)
			PT_double, "measured_energy[Wh]", PADDR(measured_energy),
			PT_double, "measured_power[W]", PADDR(measured_power),
			PT_double, "measured_demand[W]", PADDR(measured_demand),
			PT_double, "measured_real_power[W]", PADDR(measured_real_power),
			PT_double, "measured_reactive_power[VAr]", PADDR(measured_reactive_power),
			
			// added to record last voltage/current
			PT_complex, "measured_voltage_A[V]", PADDR(measured_voltage[0]),
			PT_complex, "measured_voltage_B[V]", PADDR(measured_voltage[1]),
			PT_complex, "measured_voltage_C[V]", PADDR(measured_voltage[2]),
			PT_complex, "measured_current_A[V]", PADDR(measured_current[0]),
			PT_complex, "measured_current_B[V]", PADDR(measured_current[1]),
			PT_complex, "measured_current_C[V]", PADDR(measured_current[2]),


			//PT_double, "measured_reactive[kVar]", PADDR(measured_reactive), has not implemented yet
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);

		// publish meter reset function
		if (gl_publish_function(oclass,"reset",(FUNCTIONADDR)meter_reset)==NULL)
			GL_THROW("unable to publish meter_reset function in %s",__FILE__);
		}
}

int meter::isa(char *classname)
{
	return strcmp(classname,"meter")==0 || node::isa(classname);
}

// Create a distribution meter from the defaults template, return 1 on success
int meter::create()
{
	int result = node::create();
	
	measured_voltage[0] = measured_voltage[1] = measured_voltage[2] = complex(0,0,A);
	measured_current[0] = measured_current[1] = measured_current[2] = complex(0,0,J);
	measured_energy = 0.0;
	measured_power = 0.0;
	measured_demand = 0.0;
	measured_real_power = 0.0;
	measured_reactive_power = 0.0;

	return result;
}

// Initialize a distribution meter, return 1 on success
int meter::init(OBJECT *parent)
{
	return node::init(parent);
}

// Synchronize a distribution meter
TIMESTAMP meter::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	// compute demand power
	if (measured_power>measured_demand) 
		measured_demand=measured_power;

	// compute energy use
	if (t0>0)
		measured_energy += measured_power * TO_HOURS(t1 - t0);

	return node::presync(t1);
}

TIMESTAMP meter::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	measured_voltage[0] = voltageA;
	measured_voltage[1] = voltageB;
	measured_voltage[2] = voltageC;
	measured_current[0] = current_inj[0];
	measured_current[1] = current_inj[1];
	measured_current[2] = current_inj[2];
	measured_power = (measured_voltage[0]*(~measured_current[0])).Mag() 
				   + (measured_voltage[1]*(~measured_current[1])).Mag()
				   + (measured_voltage[2]*(~measured_current[2])).Mag();
	measured_real_power = (measured_voltage[0]*(~measured_current[0])).Re()
						+ (measured_voltage[1]*(~measured_current[1])).Re()
						+ (measured_voltage[2]*(~measured_current[2])).Re();
	measured_reactive_power = (measured_voltage[0]*(~measured_current[0])).Im()
							+ (measured_voltage[1]*(~measured_current[1])).Im()
							+ (measured_voltage[2]*(~measured_current[2])).Im();
	return node::postsync(t1);
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int isa_meter(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,meter)->isa(classname);
}

EXPORT int create_meter(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(meter::oclass);
		if (*obj!=NULL)
		{
			meter *my = OBJECTDATA(*obj,meter);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (char *msg)
	{
		gl_error("create_meter: %s", msg);
	}
	return 0;
}

EXPORT int init_meter(OBJECT *obj)
{
	meter *my = OBJECTDATA(obj,meter);
	try {
		return my->init(obj->parent);
	}
	catch (char *msg)
	{
		GL_THROW("%s (meter:%d): %s", my->get_name(), my->get_id(), msg);
		return 0; 
	}
}

EXPORT TIMESTAMP sync_meter(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	meter *pObj = OBJECTDATA(obj,meter);
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
		GL_THROW("%s (meter:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return 0; 
	} catch (...) {
		GL_THROW("%s (meter:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return 0;
	}
}

/**@}**/
