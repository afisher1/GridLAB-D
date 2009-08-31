// $Id: link.h 1211 2009-01-17 00:45:28Z d3x593 $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _LINK_H
#define _LINK_H

#include "lock.h"
#include "powerflow.h"

EXPORT int isa_link(OBJECT *obj, char *classname);

#define impedance(X) (B_mat[X][X])

typedef enum {
		NORMAL=0,			///< defines just a normal link/transformer
		REGULATOR=1,		///< defines the link is a regulator
		DELTAGWYE=2,		///< defines the link is actually a Delta-Gwye transformer
		SPLITPHASE=3,		///< defines the link is a split-phase transformer
		SWITCH=4			///< defines the link is a switch
} SPECIAL_LINK;

// flow directions (see link::flow_direction)
#define FD_UNKNOWN		0x000	///< Flow is undetermined
#define FD_A_MASK		0x00f	///< mask to isolate phase A flow information
#define FD_A_NORMAL		0x001	///< Flow over phase A is normal
#define FD_A_REVERSE	0x002	///< Flow over phase A is reversed
#define FD_A_NONE		0x003	///< No flow over of phase A 
#define FD_B_MASK		0x0f0	///< mask to isolate phase B flow information
#define FD_B_NORMAL		0x010	///< Flow over phase B is normal
#define FD_B_REVERSE	0x020	///< Flow over phase B is reversed
#define FD_B_NONE		0x030	///< No flow over of phase B 
#define FD_C_MASK		0xf00	///< mask to isolate phase C flow information
#define FD_C_NORMAL		0x100	///< Flow over phase C is normal
#define FD_C_REVERSE	0x200	///< Flow over phase C is reversed
#define FD_C_NONE		0x300	///< No flow over of phase C 

class link : public powerflow_object
{
public: /// @todo make this private and create interfaces to control values
	complex a_mat[3][3];	// a_mat - 3x3 matrix, 'a' matrix
	complex b_mat[3][3];	// b_mat - 3x3 matrix, 'b' matrix
	complex c_mat[3][3];	// c_mat - 3x3 matrix, 'c' matrix
	complex d_mat[3][3];	// d_mat - 3x3 matrix, 'd' matrix
	complex A_mat[3][3];	// A_mat - 3x3 matrix, 'A' matrix
	complex B_mat[3][3];	// B_mat - 3x3 matrix, 'B' matrix
	complex tn[3];			// Used to calculate return current
	complex To_Y[3][3];		// To_Y  - 3x3 matrix, object transition to admittance
	complex From_Y[3][3];	// From_Y - 3x3 matrix, object transition from admittance
	complex *YSfrom;		// YSfrom - Pointer to 3x3 matrix representing admittance seen from "from" side (transformers)
	complex *YSto;			// YSto - Pointer to 3x3 matrix representing admittance seen from "to" side (transformers)
	double voltage_ratio;	// voltage ratio (normally 1.0)
	int NR_branch_reference;	//Index of NR_branchdata this link is contained in
	SPECIAL_LINK SpecialLnk;	//Flag for exceptions to the normal handling
	set flow_direction;		// Flag direction of powerflow: 1 is normal, -1 is reverse flow, 0 is no flow
	void calculate_power();
	void calculate_power_splitphase();
	void set_flow_directions();

public:
	typedef enum {LS_CLOSED=0, LS_OPEN=1} LINKSTATUS;
	LINKSTATUS status;	///< link status (open disconnect nodes)
	OBJECT *from;			///< from_node - source node
	OBJECT *to;				///< to_node - load node
	complex current_in[3];		///< current flow to link (w.r.t from node)
	complex current_out[3];	///< current flow out of link (w.r.t. to node)
	complex power_in;		///< power flow in (w.r.t from node)
	complex power_out;		///< power flow out (w.r.t to node)
	complex power_loss;		///< power loss in transformer
	complex indiv_power_in[3];	///< power flow in (w.r.t. from node) - individual quantities
	complex indiv_power_out[3];	///< power flow out (w.r.t. to node) - individual quantities
	complex indiv_power_loss[3];///< power losses in the transformer - individual quantities

	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP prev_LTime;
	TIMESTAMP presync(TIMESTAMP t0);
	TIMESTAMP sync(TIMESTAMP t0);
	TIMESTAMP postsync(TIMESTAMP t0);
	link(MODULE *mod);
	link(CLASS *cl=oclass):powerflow_object(cl){};
	static CLASS *oclass;
	static CLASS *pclass;
	int isa(char *classname);
public:
	/* status values */
	set affected_phases;				/* use this to determine which phases are affected by status change */
	#define IMPEDANCE_CHANGED		1	/* use this status to indicate an impedance change (e.g., line contact) */
	double resistance;					/* use this resistance when status=IMPEDANCE_CHANGED */
	#define LINE_CONTACT			2	/* use this to indicate line contact */
	set line_contacted;					/* use this to indicate which line was contacted (N means ground) */
	#define CONTROL_FAILED			4	/* use this status to indicate a controller failure (e.g., PLC failure) */

	class node *get_from(void) const;
	class node *get_to(void) const;
	set get_flow(class node **from, class node **to) const; /* determine flow direction (return phases on which flow is reverse) */

	inline LINKSTATUS open(void) { LINKSTATUS previous=status; status=LS_OPEN; return previous;};
	inline LINKSTATUS close(void) { LINKSTATUS previous=status; status=LS_CLOSED; return previous;};
	inline bool is_open(void) const { return status==LS_OPEN;};
	inline bool is_closed(void) const { return status==LS_CLOSED;};
	inline LINKSTATUS get_status(void) const {return status;};

	bool is_frequency_nominal();
	bool is_voltage_nominal();

	int kmldump(FILE *fp);

	void *UpdateYVs(OBJECT *snode, char snodeside, complex *deltaV);
};

void inverse(complex in[3][3], complex out[3][3]);
void multiply(double a, complex b[3][3], complex c[3][3]);
void multiply(complex a[3][3], complex b[3][3], complex c[3][3]);
void subtract(complex a[3][3], complex b[3][3], complex c[3][3]);
void addition(complex a[3][3], complex b[3][3], complex c[3][3]);
void equalm(complex a[3][3], complex b[3][3]);
#endif // _LINK_H

