/** $Id: underground_line_conductor.cpp 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file underground_line_conductor.cpp
	@addtogroup underground_line_conductor 
	@ingroup line

	@{
**/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>
using namespace std;

#include "line.h"

CLASS* underground_line_conductor::oclass = NULL;
CLASS* underground_line_conductor::pclass = NULL;

underground_line_conductor::underground_line_conductor(MODULE *mod) : powerflow_library(mod)
{
	if(oclass == NULL)
	{
		oclass = gl_register_class(mod,"underground_line_conductor",sizeof(underground_line_conductor),0x00);
		if (oclass==NULL)
			throw "unable to register class underground_line_conductor";
		else
			oclass->trl = TRL_PROVEN;

        if(gl_publish_variable(oclass,
			PT_double, "outer_diameter[in]",PADDR(outer_diameter),
			PT_double, "conductor_gmr[ft]", PADDR(conductor_gmr),
			PT_double, "conductor_diameter[in]",PADDR(conductor_diameter),
			PT_double, "conductor_resistance[Ohm/mile]",PADDR(conductor_resistance),
			PT_double, "neutral_gmr[ft]",PADDR(neutral_gmr),
			PT_double, "neutral_diameter[in]",PADDR(neutral_diameter),
			PT_double, "neutral_resistance[Ohm/mile]",PADDR(neutral_resistance),
			PT_int16,  "neutral_strands",PADDR(neutral_strands),
			PT_double, "shield_gmr[ft]",PADDR(shield_gmr),
			PT_double, "shield_resistance[Ohm/mile]",PADDR(shield_resistance),
			PT_double, "rating.summer.continuous[A]", PADDR(summer.continuous),
			PT_double, "rating.summer.emergency[A]", PADDR(summer.emergency),
			PT_double, "rating.winter.continuous[A]", PADDR(winter.continuous),
			PT_double, "rating.winter.emergency[A]", PADDR(winter.emergency),
            NULL) < 1) GL_THROW("unable to publish underground_line_conductor properties in %s",__FILE__);
    }
}
int underground_line_conductor::create(void)
{
	int result = powerflow_library::create();
    outer_diameter = conductor_gmr = conductor_diameter = 0.0;
	conductor_resistance = neutral_gmr = neutral_diameter = 0.0;
	neutral_resistance = shield_gmr = 0.0;
	neutral_strands = 0;
	shield_resistance = 0.0;
	summer.continuous = winter.continuous = 1000;
	summer.emergency = winter.emergency = 2000;
	return result;
}

int underground_line_conductor::isa(char *classname)
{
	return strcmp(classname,"underground_line_conductor")==0;
}


//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: underground_line_conductor
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_underground_line_conductor(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(underground_line_conductor::oclass);
		if (*obj!=NULL)
		{
			underground_line_conductor *my = OBJECTDATA(*obj,underground_line_conductor);
			gl_set_parent(*obj,parent);
			return my->create();
		}
		else
			return 0;
	}
	CREATE_CATCHALL(underground_line_conductor);
}

EXPORT TIMESTAMP sync_underground_line_conductor(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	return TS_NEVER;
}

EXPORT int isa_underground_line_conductor(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,underground_line_conductor)->isa(classname);
}

/**@}**/
