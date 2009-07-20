/** $Id: microwave.cpp,v 1.14 2008/02/13 02:22:35 d3j168 Exp $
	Copyright (C) 2008 Battelle Memorial Institute
	@file microwave.cpp
	@addtogroup microwave
	@ingroup residential

	The microwave simulation is based on a demand profile attached to the object.
	The internal heat gain is calculated using a specified fraction of installed power.

 @{
 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "house_a.h"
#include "microwave.h"

//////////////////////////////////////////////////////////////////////////
// microwave CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
CLASS* microwave::oclass = NULL;
microwave *microwave::defaults = NULL;

microwave::microwave(MODULE *module) 
{
	// first time init
	if (oclass==NULL)
	{
		// register the class definition
		oclass = gl_register_class(module,"microwave",sizeof(microwave),PC_BOTTOMUP);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_double,"installed_power[W]",PADDR(installed_power),
			PT_double,"standby_power[W]",PADDR(standby_power),
			PT_double,"circuit_split",PADDR(circuit_split),
			PT_double,"demand[unit]",PADDR(demand),
			PT_complex,"enduse_load[kW]",PADDR(load.total),
			PT_complex,"constant_power[kW]",PADDR(load.power),
			PT_complex,"constant_current[A]",PADDR(load.current),
			PT_complex,"constant_admittance[1/Ohm]",PADDR(load.admittance),
			PT_double,"internal_gains[kW]",PADDR(load.heatgain),
			PT_complex,"energy_meter[kWh]",PADDR(load.energy),
			PT_double,"heat_fraction",PADDR(heat_fraction),
			PT_double,"cycle_length",PADDR(cycle_time),
			PT_enumeration,"state",PADDR(state),
				PT_KEYWORD,"OFF",OFF,
				PT_KEYWORD,"ON",ON,
			PT_double,"runtime[s]",PADDR(runtime),
			PT_double,"state_time[s]",PADDR(state_time),
			NULL)<1) 
			GL_THROW("unable to publish properties in %s",__FILE__);

		// setup the default values
		defaults = this;
		memset(this,0,sizeof(microwave));
		load.power = load.admittance = load.current = load.total = complex(0,0,J);
	}
}

microwave::~microwave()
{
}

int microwave::create() 
{
	// copy the defaults
	memcpy(this,defaults,sizeof(microwave));

	// default properties
	return 1;
}

int microwave::init(OBJECT *parent)
{
	if (heat_fraction==0) heat_fraction = 0.25;
	if (power_factor==0) power_factor = 0.95;
	if (installed_power==0) installed_power = gl_random_uniform(700,1500);
	if (standby_power==0) standby_power = installed_power/100*gl_random_uniform(0.99,1.01);
	if (demand==0) demand = gl_random_uniform(0, 0.1);  // assuming a default maximum 10% of the sync time 

	OBJECT *hdr = OBJECTHDR(this);
	hdr->flags |= OF_SKIPSAFE;

	if (parent==NULL || (!gl_object_isa(parent,"house") && !gl_object_isa(parent,"house_e")))
	{
		gl_error("microwave must have a parent house");
		/*	TROUBLESHOOT
			The microwave object, being an enduse for the house model, must have a parent house
			that it is connected to.  Create a house object and set it as the parent of the
			offending microwave object.
		*/
		return 0;
	}

	//	pull parent attach_enduse and attach the enduseload
	FUNCTIONADDR attach = 0;
	load.end_obj = hdr;
	attach = (gl_get_function(parent, "attach_enduse"));
	if(attach == NULL){
		gl_error("microwave parent must publish attach_enduse()");
		/*	TROUBLESHOOT
			The Microwave object attempt to attach itself to its parent, which
			must implement the attach_enduse function.
		*/
		return 0;
	}
	pVoltage = ((CIRCUIT *(*)(OBJECT *, ENDUSELOAD *, double, int))(*attach))(hdr->parent, &(this->load), 20, false)->pV;

	if(installed_power < 0){
		GL_THROW("microwave power must be positive (read as %f)", installed_power);
	} else if (installed_power > 4000){
		GL_THROW("microwave power can not exceed 4 kW (and most don't exceed 2 kW)");
	}
	if(installed_power < 700){
		gl_error("microwave installed power is smaller than traditional microwave ovens");
	} else if(installed_power > 1800){
		gl_error("microwave installed power is greater than traditional microwave ovens");
	}

	if(standby_power < 0){
		gl_error("negative standby power, reseting to 1%% of installed power");
		standby_power = installed_power * 0.01;
	} else if(standby_power > installed_power){
		gl_error("standby power exceeds installed power, reseting to 1%% of installed power");
		standby_power = installed_power * 0.01;
	}

	if(cycle_time < 0){
		GL_THROW("negative cycle_length is an invalid value");
	}
	if(cycle_time > 14400){
		gl_warning("cycle_length is abnormally long and may give unusual results");
	}

	load.total = load.power = standby_power/1000;
	// initial demand
	update_state(0.0);

	return 1;
}

// periodically activates for the tail demand % of a cycle_time period.  Has a random offset to prevent
//	lock-step behavior across uniform devices
// start ....... on .. off
TIMESTAMP microwave::update_state_cycle(TIMESTAMP t0, TIMESTAMP t1){
	double ti0 = (double)t0, ti1 = (double)t1;

	if(demand == 0){
		state = OFF;
		cycle_start = 0;
		return TS_NEVER;
	}

	if(demand == 1){
		state = ON;
		cycle_start = 0;
		return TS_NEVER;
	}

	if(cycle_start == 0){
		double off = gl_random_uniform(0, this->cycle_time);
		cycle_start = ti1 + off;
		cycle_on = (1 - demand) * cycle_time + cycle_start;
		cycle_off = cycle_time + cycle_start;
		state = OFF;
		return (TIMESTAMP)cycle_on;
	}
	
	if(t0 == cycle_on){
		state = ON;
	}
	if(t0 == cycle_off){
		state = OFF;
		cycle_start = cycle_off;
	}
	if(t0 == cycle_start){
		cycle_on = (1 - demand) * cycle_time + cycle_start;
		state = OFF;
		cycle_off = cycle_time + cycle_start;
	}

	if(state == ON)
		return (TIMESTAMP)cycle_off;
	if(state == OFF)
		return (TIMESTAMP)cycle_on;
	return TS_NEVER; // from ambiguous state
}

double microwave::update_state(double dt)
{
	// run times (used for gl_random_sample()) - DPC: this is an educated guess, the true PDF needs to be researched
	static double rt[] = {30,30,30,30,30,30,30,30,30,30,60,60,60,60,90,90,120,150,180,450,600};
	static double sumrt = 2520; // sum(pdf) -- you do the math
	static double avgrt = sumrt/sizeof(rt);

	if(demand < 0.0){
		gl_error("microwave demand less than 0, reseting to zero");
		demand = 0.0;
	}
	if(demand > 1.0){
		gl_error("microwave demand greater than 1, reseting to one");
		demand = 1.0;
	}
	switch (state) {
	case OFF:
		// new OFF state or demand changed
		if (state_time==0 || prev_demand!=demand) 
		{
			if(demand != 0.0){
				runtime = avgrt * (1-demand)/demand;
			} 
			else {
				runtime = 0.0;
				return 0; /* don't run the microwave */
			}
			prev_demand = demand;
			state_time = 0; // important that state time be reset to prevent increase in demand from causing immediate state change
		}

		// time for state change
		if (state_time>runtime)
		{
			state = ON;
			runtime = gl_random_sampled(sizeof(rt)/sizeof(rt[0]),rt);
			state_time = 0;
		}
		else
			state_time += dt;
		break;
	case ON:
		// power outage or runtime expired
		runtime = floor(runtime);
		if (*pVoltage < 0.25 || state_time>runtime)
		{
			state = OFF;
			state_time = 0;
		}
		else
			state_time += dt;
		break;
	default:
		throw "unknown microwave state";
		/*	TROUBLESHOOT
			The microwave is neither on nor off.  Please initialize the "state" variable
			to either "ON" or "OFF".
		*/
	}

	return runtime;
}

TIMESTAMP microwave::sync(TIMESTAMP t0, TIMESTAMP t1) 
{
	if (t0 <= 0)
		return TS_NEVER;

	TIMESTAMP ct = 0;
	double dt = 0;
	
	if(cycle_time > 0){
		ct = update_state_cycle(t0, t1);
	} else {
		dt = update_state(gl_toseconds(t1-t0));
	}

	/* before we update our power for the next state */
	if (t1 > t0 && t0 > 0) load.energy += load.total * ((double)(t1 - t0))/3600.0; /* dt in seconds */
	
	load.power.SetPowerFactor( (state==ON?installed_power:standby_power)/1000,power_factor);
	load.total = load.power;
	
	load.heatgain = load.total.Mag()*(state==ON?heat_fraction:1.0);

	if(cycle_time == 0)
		return dt>0?-(TIMESTAMP)(t1 + dt*TS_SECOND) : TS_NEVER; // negative time means soft transition
	else
		return ct == TS_NEVER ? TS_NEVER : -ct;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_microwave(OBJECT **obj, OBJECT *parent)
{
	*obj = gl_create_object(microwave::oclass);
	if (*obj!=NULL)
	{
		microwave *my = OBJECTDATA(*obj,microwave);;
		gl_set_parent(*obj,parent);
		my->create();
		return 1;
	}
	return 0;
}

EXPORT int init_microwave(OBJECT *obj)
{
	microwave *my = OBJECTDATA(obj,microwave);
	return my->init(obj->parent);
}

EXPORT TIMESTAMP sync_microwave(OBJECT *obj, TIMESTAMP t0)
{
	microwave *my = OBJECTDATA(obj, microwave);
	TIMESTAMP t1 = my->sync(obj->clock, t0);
	obj->clock = t0;
	return t1;
}

/**@}**/
