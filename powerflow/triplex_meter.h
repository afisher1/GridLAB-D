// $Id: triplex_meter.h 942 2008-09-19 20:03:17Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _TRIPLEXMETER_H
#define _TRIPLEXMETER_H

#include "powerflow.h"
#include "triplex_node.h"

class triplex_meter : public triplex_node
{
protected:
	TIMESTAMP last_t;

public:
	complex measured_voltage[3];	///< measured voltage
	complex measured_current[3];	///< measured current
	double measured_energy; //< metered energy
	double measured_power; //< metered power
	double measured_demand; //< metered demand (peak of power)
	double measured_real_power; //< metered real power

public:
	static CLASS *oclass;
	static CLASS *pclass;
public:
	triplex_meter(MODULE *mod);
	inline triplex_meter(CLASS *cl=oclass):triplex_node(cl){};
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
	int isa(char *classname);
};

#endif // _TRIPLEXMETER_H

