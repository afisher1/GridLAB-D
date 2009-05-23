// $Id: load.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _LOAD_H
#define _LOAD_H

#include "node.h"

class load : public node
{
public:
	static CLASS *oclass;
	static CLASS *pclass;
public:
	complex measured_voltage_A;	///< measured voltage
	complex measured_voltage_B;
	complex measured_voltage_C;
	complex measured_voltage_AB;	
	complex measured_voltage_BC;
	complex measured_voltage_CA;
	complex constant_power[3];		// power load
	complex constant_current[3];	// current load
	complex constant_impedance[3];	// impedance load
	enum {LC_UNKNOWN=0, LC_RESIDENTIAL, LC_COMMERCIAL, LC_INDUSTRIAL, LC_AGRICULTURAL} load_class;

	int create(void);

	load(MODULE *mod);
	TIMESTAMP sync(TIMESTAMP t0);
	TIMESTAMP presync(TIMESTAMP t0);
	inline load(CLASS *cl=oclass):node(cl){};
	int isa(char *classname);
};

#endif // _LOAD_H

