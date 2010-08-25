/** $Id: zipload.h $
	Copyright (C) 2009 Battelle Memorial Institute
	@file zipload.h
	@addtogroup ZIPload
	@ingroup residential

 @{
 **/

#ifndef _ZIPLOAD_H
#define _ZIPLOAD_H
#include "residential.h"
#include "residential_enduse.h"

class ZIPload : public residential_enduse
{
public:
	double base_power;			///< Base real power of the system
	double power_pf;			///< power factor of constant power load
	double current_pf;			///< power factor of constant current load
	double impedance_pf;		///< power factor of constant impedance load
	bool is_240;				///< load connected at 220 
	double breaker_val;			///< Amperage limit for connected breaker
	complex actual_power;		///< Actual load after adjusted for voltage factors

	bool demand_response_mode;	///< Activates equilibrium dynamic representation of demand response 
	int64 N;					///< Number of devices to model - base power is per device 
	int16 L;					///< Range of the thermostat's control operation 
	double N_off;				///< Number of devices that are off 
	double N_on;					///< Number of devices that are on 
	double noff;				///< Density of devices that are off per unit of temperature 
	double non;					///< Density of devices that are on per unit of temperature 
	double roff;				///< rate at which devices cool down 
	double ron;					///< rate at which devices heat up 
	double t; 					///< total cycle time of a thermostatic device 
	double toff;				///< total off time of device 
	double ton;					///< total on time of device 
	int16 x;					///< temperature of the device's controlled media (eg air temp or water temp) 
	double phi;					///< duty cycle of the device 
	double PHI;					///< diversity of a population of devices 
	double eta;					///< consumer demand rate that prematurely turns on a device or population 
	double rho;					///< effect rate at which devices heats up or cools down under consumer demand 
	double nominal_power;
	int64 next_time;

	typedef struct {
		double *on;
		double *off;
		int16 nbins;
	} DRMODEL;

	DRMODEL drm;
	DRMODEL previous_drm;			///< structures to save drm population and previous population

public:
	static CLASS *oclass, *pclass;
	static ZIPload *defaults;

	ZIPload(MODULE *module);
	~ZIPload();
	int create();
	int init(OBJECT *parent);
	int isa(char *classname);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);

};

#endif // _ZIPLOAD_H

/**@}**/
