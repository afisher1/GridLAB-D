// $Id: regulator.cpp 1186 2009-01-02 18:15:30Z dchassin $
/**	Copyright (C) 2008 Battelle Memorial Institute

	@file regulator.cpp
	@addtogroup powerflow_regulator Regulator
	@ingroup powerflow
		
	@{
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>
using namespace std;

#include "regulator.h"
#include "node.h"

CLASS* regulator::oclass = NULL;
CLASS* regulator::pclass = NULL;

regulator::regulator(MODULE *mod) : link(mod)
{
	if (oclass==NULL)
	{
		// link to parent class (used by isa)
		pclass = link::oclass;

		// register the class definition
		oclass = gl_register_class(mod,"regulator",sizeof(regulator),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_INHERIT, "link",
			PT_object,"configuration",PADDR(configuration),
			PT_int16, "tap_A",PADDR(tap_A),
			PT_int16, "tap_B",PADDR(tap_B),
			PT_int16, "tap_C",PADDR(tap_C),
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
	}
}

int regulator::isa(char *classname)
{
	return strcmp(classname,"regulator")==0 || link::isa(classname);
}

int regulator::create()
{
	int result = link::create();
	configuration = NULL;
	return result;
}

int regulator::init(OBJECT *parent)
{
	int result = link::init(parent);

	if (!configuration)
		throw "no regulator configuration specified.";

	if (!gl_object_isa(configuration, "regulator_configuration"))
		throw "invalid regulator configuration";

	regulator_configuration *pConfig = OBJECTDATA(configuration, regulator_configuration);

	// D_mat - 3x3 matrix, 'D' matrix
	D_mat[0][0] = D_mat[1][1] = D_mat[2][2] = complex(1,0);
	D_mat[0][1] = D_mat[2][0] = D_mat[1][2] = complex(-1,0);
	D_mat[0][2] = D_mat[2][1] = D_mat[1][0] = complex(0,0);

	//D_mat[3][3] = {{complex(1,0),complex(-1,0),complex(0,0)},
	//               {complex(0,0), complex(1,0),complex(-1,0)},
	//			   {complex(-1,0), complex(0,0), complex(1,0)}};   
	
	W_mat[0][0] = W_mat[1][1] = W_mat[2][2] = complex(2,0);
	W_mat[0][1] = W_mat[2][0] = W_mat[1][2] = complex(1,0);
	W_mat[0][2] = W_mat[2][1] = W_mat[1][0] = complex(0,0);
	
	multiply(1.0/3.0,W_mat,W_mat);
	//W_mat[3][3] = {{complex(2,0),complex(1,0),complex(0,0)},
	//               {complex(0,0),complex(2,0),complex(1,0)},
	//               {complex(1,0),complex(0,0),complex(2,0)}};
	
	tapChangePer = pConfig->regulation / (double) pConfig->raise_taps;
	Vlow = pConfig->band_center - pConfig->band_width / 2.0;
	Vhigh = pConfig->band_center + pConfig->band_width / 2.0;
	VtapChange = pConfig->band_center * tapChangePer;
	
	for (int i = 0; i < 3; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			a_mat[i][j] = b_mat[i][j] = c_mat[i][j] = d_mat[i][j] =
					A_mat[i][j] = B_mat[i][j] = 0.0;
		}
	}

	for (int i = 0; i < 3; i++) 
	{	
		tap[i] = pConfig->tap_pos[i];

		if (pConfig->Type == pConfig->A)
			a_mat[i][i] = 1/(1.0 + tap[i] * tapChangePer);
		else if (pConfig->Type == pConfig->B)
			a_mat[i][i] = 1.0 - tap[i] * tapChangePer;
		else
			throw "invalid regulator type";

	}

	complex tmp_mat[3][3] = {{complex(1,0)/a_mat[0][0],complex(0,0),complex(0,0)},
			                 {complex(0,0), complex(1,0)/a_mat[1][1],complex(0,0)},
			                 {complex(-1,0)/a_mat[0][0],complex(-1,0)/a_mat[1][1],complex(0,0)}};
	complex tmp_mat1[3][3];

	switch (pConfig->connect_type) {
		case regulator_configuration::WYE_WYE:
			for (int i = 0; i < 3; i++)
				d_mat[i][i] = complex(1.0,0) / a_mat[i][i]; 
			inverse(a_mat,A_mat);
			break;
		case regulator_configuration::OPEN_DELTA_ABBC:
			d_mat[0][0] = complex(1,0) / a_mat[0][0];
			d_mat[1][0] = complex(-1,0) / a_mat[0][0];
			d_mat[1][2] = complex(-1,0) / a_mat[1][1];
			d_mat[2][2] = complex(1,0) / a_mat[1][1];

			a_mat[2][0] = -a_mat[0][0];
			a_mat[2][1] = -a_mat[1][1];
			a_mat[2][2] = 0;

			multiply(W_mat,tmp_mat,tmp_mat1);
			multiply(tmp_mat1,D_mat,A_mat);
			break;
		case regulator_configuration::OPEN_DELTA_BCAC:
			throw "Regulator connect type not supported yet";
			break;
		case regulator_configuration::OPEN_DELTA_CABA:
			throw "Regulator connect type not supported yet";
			break;
		case regulator_configuration::CLOSED_DELTA:
			throw "Regulator connect type not supported yet";
			break;
		default:
			throw "unknown regulator connect type";
			break;
	}

	mech_t_next[0] = mech_t_next[1] = mech_t_next[2] = 0;
	dwell_t_next[0] = dwell_t_next[1] = dwell_t_next[2] = TS_NEVER;
	first_run_flag[0] = first_run_flag[1] = first_run_flag[2] = -1;

	return result;
}


TIMESTAMP regulator::presync(TIMESTAMP t0) 
{
	//Set flags correctly for each pass, 1 indicates okay to change, 0 indicates no go
	for (int i = 0; i < 3; i++) {
		if (mech_t_next[i] <= t0) {
			mech_flag[i] = 1;
		}
		if (dwell_t_next[i] <= t0) {
			dwell_flag[i] = 1;
		}
		else if (dwell_t_next[i] > t0) {
			dwell_flag[i] = 0;
		}
	}

	
	regulator_configuration *pConfig = OBJECTDATA(configuration, regulator_configuration);
	node *pTo = OBJECTDATA(to, node);

	complex tmp_mat2[3][3];
	inverse(d_mat,tmp_mat2);

	//Calculate outgoing currents
	curr[0] = tmp_mat2[0][0]*current_in[0]+tmp_mat2[0][1]*current_in[1]+tmp_mat2[0][2]*current_in[2];
	curr[1] = tmp_mat2[1][0]*current_in[0]+tmp_mat2[1][1]*current_in[1]+tmp_mat2[1][2]*current_in[2];
	curr[2] = tmp_mat2[2][0]*current_in[0]+tmp_mat2[2][1]*current_in[1]+tmp_mat2[2][2]*current_in[2];

	for (int i = 0; i < 3; i++) {
		if (first_run_flag[i] < 1) {
			if (curr[i] != 0) {
				first_run_flag[i] += 1;
			}
		}
	}

	if (pTo) 
	{
		volt[0] = pTo->voltageA;
		volt[1] = pTo->voltageB;
		volt[2] = pTo->voltageC;
	}
	else
	{	volt[0] = volt[1] = volt[2] = 0.0;
	}

	if (pConfig->Control == pConfig->AUTO) {
 		for (int i = 0; i < 3; i++) 
		{	
			if (pConfig->connect_type == pConfig->WYE_WYE) 
			{
				if (curr[i] != 0)
				{	
					V2[i] = volt[i] / ((double) pConfig->PT_ratio);
					Vcomp[i] = V2[i] - (curr[i] / (double) pConfig->CT_ratio) * complex(pConfig->ldc_R_V[i], pConfig->ldc_X_V[i]);
	
					if (Vcomp[i].Mag() < Vlow)		//raise voltage
					{	
						//hit the band center for convergence on first run, otherwise bad initial guess on tap settings 
						//can fail on the first timestep
						if (first_run_flag[i] == 0) {	
							tap[i] = tap[i] + (int16)ceil((pConfig->band_center - Vcomp[i].Mag())/VtapChange);
							mech_t_next[i] = t0 + (int64)pConfig->time_delay;
							mech_flag[i] = 0;
						}
						//if both flags say it's okay to change the tap, then change the tap and turn on a 
						//mechanical tap changing delay before the next change
						else if (mech_flag[i] == 1 && dwell_flag[i] == 1) {		 
							tap[i] = tap[i] + (int16) 1;						
							mech_t_next[i] = t0 + (int64)pConfig->time_delay;	
							mech_flag[i] = 0;
						}
						//only set the dwell time if we've reached the end of the previous dwell (in case other 
						//objects update during that time)
						else if (dwell_flag[i] == 0 && (dwell_t_next[i] - t0) >= pConfig->dwell_time) {
							dwell_t_next[i] = t0 + (int64)pConfig->dwell_time;	
						}														
					}
					else if (Vcomp[i].Mag() > Vhigh)  //lower voltage
					{
						if (first_run_flag[i] == 0) {
							tap[i] = tap[i] - (int16)ceil((Vcomp[i].Mag() - pConfig->band_center)/VtapChange);
							mech_t_next[i] = t0 + (int64)pConfig->time_delay;
							mech_flag[i] = 0;
						}
						else if (mech_flag[i] == 1 && dwell_flag[i] == 1) {
							tap[i] = tap[i] - (int16) 1;							
							mech_t_next[i] = t0 + (int64)pConfig->time_delay;
							mech_flag[i] = 0;
						}
						else if (dwell_flag[i] == 0 && (dwell_t_next[i] - t0) >= pConfig->dwell_time) {
							dwell_t_next[i] = t0 + (int64)pConfig->dwell_time;
						}
					}
					//If no tap changes were needed, then this resets dwell_flag to 0 and indicates regulator has no
					//more changes unless system changes
					else 
					{	
						dwell_t_next[i] = TS_NEVER;
					}

					//Keeps it within limits of the number of taps
					if (tap[i] < -pConfig->lower_taps)
					{	tap[i] = -pConfig->lower_taps;}
					if (tap[i] > pConfig->raise_taps)
					{	tap[i] = pConfig->raise_taps;}
			
					//Use tap positions to solve for 'a' matrix
					if (pConfig->Type == pConfig->A)
					{	a_mat[i][i] = 1/(1.0 + tap[i] * tapChangePer);}
					else if (pConfig->Type == pConfig->B)
					{	a_mat[i][i] = 1.0 - tap[i] * tapChangePer;}
					else
					{	throw "invalid regulator type";}
				}
			}

			else 
			{
				throw "Regulator connect type not supported in automatic mode yet";
			}
		}
		//Determine how far to advance the clock
		int64 nt[3];
		for (int i = 0; i < 3; i++) {
			if (mech_t_next[i] > t0)
				nt[i] = mech_t_next[i];
			if (dwell_t_next[i] > t0)
				nt[i] = dwell_t_next[i];
		}

		if (nt[0] > t0)
			next_time = nt[0];
		if (nt[1] > t0 && nt[1] < next_time)
			next_time = nt[1];
		if (nt[2] > t0 && nt[2] < next_time)
			next_time = nt[2];

		if (next_time <= t0)
			next_time = TS_NEVER;
	}

	else if (pConfig->Control == pConfig->MANUAL) {
		for (int i = 0; i < 3; i++) {
			if (curr[i] != 0) {
				if (pConfig->Type == pConfig->A)
				{	a_mat[i][i] = 1/(1.0 + tap[i] * tapChangePer);}
				else if (pConfig->Type == pConfig->B)
				{	a_mat[i][i] = 1.0 - tap[i] * tapChangePer;}
				else
				{	throw "invalid regulator type";}
			}
		}
		next_time = TS_NEVER;
	}

	//Use 'a' matrix to solve appropriate 'A' & 'd' matrices
	complex tmp_mat[3][3] = {{complex(1,0)/a_mat[0][0],complex(0,0),complex(0,0)},
			                 {complex(0,0), complex(1,0)/a_mat[1][1],complex(0,0)},
			                 {complex(-1,0)/a_mat[0][0],complex(-1,0)/a_mat[1][1],complex(0,0)}};
	complex tmp_mat1[3][3];

	switch (pConfig->connect_type) {
		case regulator_configuration::WYE_WYE:
			for (int i = 0; i < 3; i++)
			{	d_mat[i][i] = complex(1.0,0) / a_mat[i][i]; }
			inverse(a_mat,A_mat);
			break;
		case regulator_configuration::OPEN_DELTA_ABBC:
			d_mat[0][0] = complex(1,0) / a_mat[0][0];
			d_mat[1][0] = complex(-1,0) / a_mat[0][0];
			d_mat[1][2] = complex(-1,0) / a_mat[1][1];
			d_mat[2][2] = complex(1,0) / a_mat[1][1];

			a_mat[2][0] = -a_mat[0][0];
			a_mat[2][1] = -a_mat[1][1];
			a_mat[2][2] = 0;

			multiply(W_mat,tmp_mat,tmp_mat1);
			multiply(tmp_mat1,D_mat,A_mat);
			break;
		case regulator_configuration::OPEN_DELTA_BCAC:
			break;
		case regulator_configuration::OPEN_DELTA_CABA:
			break;
		case regulator_configuration::CLOSED_DELTA:
			break;
		default:
			throw "unknown regulator connect type";
			break;
		}
	


	return next_time;
}
//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: regulator
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_regulator(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(regulator::oclass);
		if (*obj!=NULL)
		{
			regulator *my = OBJECTDATA(*obj,regulator);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (char *msg)
	{
		gl_error("create_regulator: %s", msg);
	}
	return 0;
}

/**
* Object initialization is called once after all object have been created
*
* @param obj a pointer to this object
* @return 1 on success, 0 on error
*/
EXPORT int init_regulator(OBJECT *obj)
{
	regulator *my = OBJECTDATA(obj,regulator);
	try {
		return my->init(obj->parent);
	}
	catch (char *msg)
	{
		GL_THROW("%s (regulator:%d): %s", my->get_name(), my->get_id(), msg);
		return 0; 
	}
}

/**
* Sync is called when the clock needs to advance on the bottom-up pass (PC_BOTTOMUP)
*
* @param obj the object we are sync'ing
* @param t0 this objects current timestamp
* @param pass the current pass for this sync call
* @return t1, where t1>t0 on success, t1=t0 for retry, t1<t0 on failure
*/
EXPORT TIMESTAMP sync_regulator(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	regulator *pObj = OBJECTDATA(obj,regulator);
	try {
		TIMESTAMP t1 = TS_NEVER;
		switch (pass) {
		case PC_PRETOPDOWN:
			return pObj->presync(t0);
		case PC_BOTTOMUP:
			return pObj->sync(t0);
		case PC_POSTTOPDOWN:
			t1 = pObj->postsync(t0);
			obj->clock = t0;
			return t1;
		default:
			throw "invalid pass request";
		}
	} catch (const char *error) {
		GL_THROW("%s (regulator:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return 0; 
	} catch (...) {
		GL_THROW("%s (regulator:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return 0;
	}
}

EXPORT int isa_regulator(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,regulator)->isa(classname);
}



/**@}*/
