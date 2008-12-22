/** $Id: clotheswasher.h,v 1.9 2008/02/09 00:05:09 d3j168 Exp $
	Copyright (C) 2008 Battelle Memorial Institute
	@file clotheswasher.h
	@addtogroup clotheswasher
	@ingroup residential

 @{
 **/

#ifndef _clotheswasher_H
#define _clotheswasher_H

#include "residential.h"

class clotheswasher  
{
private:
	complex *pVoltage;

public:
	double circuit_split;				///< -1=100% negative, 0=balanced, +1=100% positive
	double motor_power;					///< installed clotheswasher motor power [W] (default = random uniform between 500 - 750 W)
	double power_factor;				///< power factor (default = 0.95)
	double enduse_demand;				///< amount of demand added per hour (units/hr)
	double enduse_queue;				///< accumulated demand (units)
	double cycle_duration;				///< typical cycle runtime (s)
	double cycle_time;					///< remaining time in main cycle (s)
	double state_time;					///< remaining time in current state (s)
	double stall_voltage;				///< voltage at which the motor stalls
	double start_voltage;				///< voltage at which motor can start
	complex stall_impedance;			///< impedance of motor when stalled
	double trip_delay;					///< stalled time before thermal trip
	double reset_delay;					///< trip time before thermal reset and restart
	double heat_fraction;				///< internal gain fraction of installed power
	ENDUSELOAD load;					///< total, power, current, impedance, energy, and heatgain
	TIMESTAMP time_state;				///< time in current state
	enum {	STOPPED=0,						///< motor is stopped
			RUNNING=1,						///< motor is running
			STALLED=2,						///< motor is stalled
			TRIPPED=3,						///< motor is tripped
	} state;							///< control state

public:
	static CLASS *oclass;
	static clotheswasher *defaults;

	clotheswasher(MODULE *module);
	~clotheswasher();
	int create();
	int init(OBJECT *parent);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	double update_state(double dt);		///< updates the load struct and returns the time until expected state change

};

#endif // _clotheswasher_H

/**@}**/
