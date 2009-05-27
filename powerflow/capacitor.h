// $Id: capacitor.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _CAPACITOR_H
#define _CAPACITOR_H

#include "powerflow.h"
#include "node.h"

class capacitor : public node
{
public:
	typedef enum {MANUAL=0, VAR=1, VOLT=2, VARVOLT=3} CAPCONTROL;
	typedef enum {OPEN=0, CLOSED=1} CAPSWITCH;
	typedef enum {BANK=0, INDIVIDUAL=1} CAPCONTROL2;

	set pt_phase;				// phase PT on
	set phases_connected;		// phases capacitors connected to
	double voltage_set_high;    // high voltage set point for voltage control (turn off)
	double voltage_set_low;     // low voltage set point for voltage control (turn on)
	double capacitor_A;			// Capacitance value for phase A or phase AB
	double capacitor_B;			// Capacitance value for phase B or phase BC
	double capacitor_C;			// Capacitance value for phase C or phase CA
	CAPCONTROL2 control_level;  // define bank or individual control
	CAPSWITCH switchA_state;	// capacitor A switch open or close
	CAPSWITCH switchB_state;	// capacitor B switch open or close
	CAPSWITCH switchC_state;	// capacitor C switch open or close

protected:
	complex q_node[3];          // 3x1 matrix, Q of node
	complex b_node[3];          // 3x1 matrix, B of node
	complex q_cap[3];           // 3x1 matrix, Q of cap
	CAPCONTROL control;			// control operation strategy; 0 - manual, 1 - VAr, 2- voltage, 3 - VAr primary,voltage backup.
	double var_close;           // VAr close limit
	double var_open;            // VAr open limit
	double volt_close;          // volt close limit
	double volt_open;           // volt open limit
	int32 pt_ratio;             // control PT ratio
	int32 time_delay;           // control time delay
	double time_to_change;      // time for state to change

public:
	int create(void);
	TIMESTAMP sync(TIMESTAMP t0);
	capacitor(MODULE *mod);
	inline capacitor(CLASS *cl=oclass):node(cl){};
	int init(OBJECT *parent);
	int isa(char *classname);

private:
	complex cap_value[3];		// Capacitor values translated to impedance

public:
	static CLASS *pclass;
	static CLASS *oclass;
};

#endif // _CAPACITOR_H
