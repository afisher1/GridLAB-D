/** $Id: occupantload.h,v 1.7 2008/02/09 23:48:51 d3j168 Exp $
	Copyright (C) 2008 Battelle Memorial Institute
	@file occupantload.h
	@addtogroup occupantload
	@ingroup residential

 @{
 **/

#ifndef _OCCUPANTLOAD_H
#define _OCCUPANTLOAD_H
#include "residential.h"

class occupantload  
{

public:
	ENDUSELOAD load;					///< enduse load structure
	int number_of_occupants;			///< number of occupants of the house
	double occupancy_fraction;			///< represents occupancy schedule
	double heatgain_per_person;			///< sensible+latent loads (400 BTU) default based on DOE-2

public:
	static CLASS *oclass;
	static occupantload *defaults;

	occupantload(MODULE *module);
	~occupantload();
	int create();
	int init(OBJECT *parent);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);

};

#endif // _OCCUPANTLOAD_H

/**@}**/
