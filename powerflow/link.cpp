/** $Id: link.cpp 1211 2009-01-17 00:45:28Z d3x593 $
	Copyright (C) 2008 Battelle Memorial Institute
	@file link.cpp
	@addtogroup powerflow_link Link
	@ingroup powerflow_object

	@par Fault support

	The following conditions are used to describe a fault impedance \e X (e.g., 1e-6), 
	between phase \e x and neutral or group, or between phases \e x and \e y, and leaving
	phase \e z unaffected at iteration \e t:

	- \e phase-to-phase contact on the link
		- \e Forward-sweep: \f$ 
			V_x(t) = V_y(t) = 0, 
			A = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 0 & 0 \\
				0 & 0 & 1 \end{array} \right) , 
			B = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 0 & 0 \\
				0 & 0 & p \end{array} \right)  
			\f$
		  where \e p is the previous value
		  when \e x = phase A and \e y = phase B;
		- \e Back-sweep: \f$ 
			  c = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 0 & 0 \\
				0 & 0 & 0 \end{array} \right) ,
			  d = \left( \begin{array}{ccc}
				\frac{1}{X} & 0 & 0 \\
				0 & \frac{1}{X} & 0 \\
				0 & 0 & p \end{array} \right) 
		  \f$ when \e x = phase A and \e y = phase B;
    - \e phase-to-ground contact on the link
		- \e Forward-sweep: \f$ 
			V_x(t) = 0, 
			A = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 1 & 0 \\
				0 & 0 & 1 \end{array} \right) , 
			B = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & p & p \\
				0 & p & p \end{array} \right)  
			\f$
		  where \e p is the previous value
		  when \e x = phase A;
		- \e Back-sweep: \f$ 
			  c = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 0 & 0 \\
				0 & 0 & 0 \end{array} \right) ,
			  d = \left( \begin{array}{ccc}
				\frac{1}{X} & 0 & 0 \\
				0 & p & 0 \\
				0 & 0 & p \end{array} \right) 
		  \f$ when \e x = phase A;
	- \e phase-to-neutral contact on the link
		- \e Forward-sweep: \f$ 
			V_x(t) = -V_N, 
			A = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 1 & 0 \\
				0 & 0 & 1 \end{array} \right) , 
			B = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & p & p \\
				0 & p & p \end{array} \right)  
			\f$
		  where \e p is the previous value
		  when \e x = phase A;
		- \e Back-sweep: \f$ 
			  c = \left( \begin{array}{ccc}
				0 & 0 & 0 \\
				0 & 0 & 0 \\
				0 & 0 & 0 \end{array} \right) ,
			  d = \left( \begin{array}{ccc}
				\frac{1}{X} & 0 & 0 \\
				0 & p & 0 \\
				0 & 0 & p \end{array} \right) 
		  \f$ when \e x = phase A;


	@{
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "link.h"
#include "node.h"

CLASS* link::oclass = NULL;
CLASS* link::pclass = NULL;

/**
* constructor.  Class registration is only called once to register the class with the core.
* @param mod the module struct that this class is registering in
*/
link::link(MODULE *mod) : powerflow_object(mod)
{
	// first time init
	if (oclass==NULL)
	{
		pclass = powerflow_object::oclass;
		oclass = gl_register_class(mod,"link",sizeof(link),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);
        if(gl_publish_variable(oclass,
			PT_INHERIT, "powerflow_object",
			PT_enumeration, "status", PADDR(status),
				PT_KEYWORD, "CLOSED", LS_CLOSED,
				PT_KEYWORD, "OPEN", LS_OPEN,
			PT_object, "from",PADDR(from),
			PT_object, "to", PADDR(to),
			PT_double, "power_in[W]", PADDR(power_in),
			PT_double, "power_out[W]", PADDR(power_out),
			NULL) < 1 && errno) GL_THROW("unable to publish link properties in %s",__FILE__);
	}
}

int link::isa(char *classname)
{
	return strcmp(classname,"link")==0 || powerflow_object::isa(classname);
}

int link::create(void)
{
	int result = powerflow_object::create();
	
	from = NULL;
	to = NULL;
	status = LS_CLOSED;
	power_in = 0;
	power_out = 0;
	voltage_ratio = 1.0;
	SpecialLnk = NORMAL;
	prev_LTime=0;
	NR_branch_reference=-1;

	current_in[0] = current_in[1] = current_in[2] = complex(0,0);

	return result;
}

int link::init(OBJECT *parent)
{
	OBJECT *obj = GETOBJECT(this);

	powerflow_object::init(parent);

	set phase_f_test, phase_t_test;
	node *fNode = OBJECTDATA(from,node);
	node *tNode = OBJECTDATA(to,node);


	/* check link from node */
	if (from==NULL)
		throw "link from node is not specified";
		/*  TROUBLESHOOT
		The from node for a line or link is not connected to anything.
		*/
	if (to==NULL)
		throw "link to node is not specified";
		/*  TROUBLESHOOT
		The to node for a line or link is not connected to anything.
		*/
	
	/* adjust ranks according to method in use */
	switch (solver_method) {
	case SM_FBS: /* forward backsweep method only */
		if (obj->parent==NULL)
		{
			/* make 'from' object parent of this object */
			if (gl_object_isa(from,"node")) 
			{
				if(gl_set_parent(obj, from) < 0)
					throw "error when setting parent";
					/*  TROUBLESHOOT
					An error has occurred while setting the parent field of a link.  Please
					submit a bug report and your code so this error can be diagnosed further.
					*/
			} 
			else 
				throw "link from reference not a node";
				/*  TROUBLESHOOT
				The from connection of a link is not a node-based object.  Ensure that the from object is a
				node, load, or meter object.  If not, adjust your model accordingly.
				*/
		}
		else
			/* promote 'from' object if necessary */
			gl_set_rank(from,obj->rank+1);
		
		if (to->parent==NULL)
		{
			/* make this object parent to 'to' object */
			if (gl_object_isa(to,"node"))
			{
				if(gl_set_parent(to, obj) < 0)
					throw "error when setting parent";
					//Defined above
			} 
			else 
				throw "link to reference not a node";
				//Defined above
		}
		else
			/* promote this object if necessary */
			gl_set_rank(obj,to->rank+1);

		break;
	case SM_GS: /* Gauss-Seidel */
		{
		if (obj->parent==NULL) 
			/* automatically use from as parent node */
			obj->parent = from;

		node *fNode = OBJECTDATA(from,node);
		node *tNode = OBJECTDATA(to,node);
		
		if (fNode!=NULL)
			fNode->attachlink(OBJECTHDR(this));

		if (tNode!=NULL)
			tNode->attachlink(OBJECTHDR(this));

		break;
		}
	case SM_NR:
	{
		NR_branch_count++;		//Update global value of link count

		//Use FBS code to rank objects - do this so swing ends up on one end for consistency
			if (obj->parent==NULL)
			{
				/* make 'from' object parent of this object */
				if (gl_object_isa(from,"node")) 
				{
					if(gl_set_parent(obj, from) < 0)
						throw "error when setting parent";
						//Defined above
				} 
				else 
					throw "link from reference not a node";
					//Defined above
			}
			else
				/* promote 'from' object if necessary */
				gl_set_rank(from,obj->rank+1);
			
			if (to->parent==NULL)
			{
				/* make this object parent to 'to' object */
				if (gl_object_isa(to,"node"))
				{
					if(gl_set_parent(to, obj) < 0)
						throw "error when setting parent";
					//Defined above
				} 
				else 
					throw "link to reference not a node";
					//Defined above
			}
			else
				/* promote this object if necessary */
				gl_set_rank(obj,to->rank+1);

		break;
	}
	default:
		throw "unsupported solver method";
		//Defined elsewhere
		break;
	}

	//Simple Phase checks
	phase_f_test = (fNode->phases & phases);
	phase_t_test = (tNode->phases & phases);

	if ((phase_f_test != phases) || (phase_t_test != phases))	//Phase mismatch on the line
		GL_THROW("line:%d has a phase mismatch at one or both ends",obj->id);
		/*  TROUBLESHOOT
		A line has been configured to carry a certain set of phases.  Either the input node or output
		node is not providing a source/sink for these different conductors.  The To and From nodes must
		have at least the phases of the line connecting them.
		*/

	if (nominal_voltage==0)
	{
		node *pFrom = OBJECTDATA(from,node);
		nominal_voltage = pFrom->nominal_voltage;
	}

	/* no nominal voltage */
	if (nominal_voltage==0)
		throw "nominal voltage is not specified";

	/* record this link on the nodes' incidence counts */
	OBJECTDATA(from,node)->k++;
	OBJECTDATA(to,node)->k++;
	return 1;
}

node *link::get_from(void) const
{
	node *from;
	get_flow(&from,NULL);
	return from;
}
node *link::get_to(void) const
{
	node *to;
	get_flow(NULL,&to);
	return to;
}
set link::get_flow(node **fn, node **tn) const
{
	node *f = OBJECTDATA(from, node);
	node *t = OBJECTDATA(to, node);
	set reverse = 0;
	reverse |= (f->voltage[0].Mag()<t->voltage[0].Mag())?PHASE_A:0;
	reverse |= (f->voltage[1].Mag()<t->voltage[1].Mag())?PHASE_B:0;
	reverse |= (f->voltage[2].Mag()<t->voltage[2].Mag())?PHASE_C:0;
	if (fn!=NULL) *fn=f;
	if (tn!=NULL) *tn=t;
	/* someday perhaps, reversal will be supported */
	return reverse;
}

TIMESTAMP link::presync(TIMESTAMP t0)
{
	TIMESTAMP t1 = powerflow_object::presync(t0); 

	if (solver_method==SM_NR)
	{
		if (prev_LTime==0)	//First run, build up the pointer matrices
		{
			node *fnode = OBJECTDATA(from,node);
			node *tnode = OBJECTDATA(to,node);
			char jval, kval;
			if (fnode==NULL || tnode==NULL)
				return TS_NEVER;

			if ((NR_curr_bus!=-1) && (NR_curr_branch!=-1))	//Ensure we've been initialized
			{
				//Check our end nodes first - update them if necessary
				if (fnode->NR_node_reference==-1)	//Uninitialized node
				{
					fnode->NR_populate();
				}

				if (tnode->NR_node_reference==-1)	//Unitialized node
				{
					tnode->NR_populate();
				}

				//Now populate the link's information
				
				//Start with admittance matrix
				for (jval=0; jval<3; jval++)
					for (kval=0; kval<3; kval++)
						NR_branchdata[NR_curr_branch].Y[jval][kval] = &From_Y[jval][kval];

				//Populate to/from indices
				NR_branchdata[NR_curr_branch].from = fnode->NR_node_reference;
				NR_branchdata[NR_curr_branch].to = tnode->NR_node_reference;

				//Populate voltage ratio
				NR_branchdata[NR_curr_branch].v_ratio = voltage_ratio;

				//Update our storage value
				NR_branch_reference = NR_curr_branch;

				//Update pointer
				NR_curr_branch++;
			}
			else
				GL_THROW("A link was called before NR was initialized by a node.");
				/*	TROUBLESHOOT
				This is a bug.  The Newton-Raphson solver method relies on a node being called first.  If GridLAB-D
				made it this far, you should have a swing bus defined and it should be called before any other objects.
				Please submit your code and a bug report for this problem.
				*/
		}

		if (prev_LTime!=t0)	//New timestep, recalc admittance matrix (does this really need to be done every timestep?)
		{
			complex Ylinecharge[3][3];
			complex Y[3][3];
			complex Yc[3][3];
			complex Ylefttemp[3][3];
			char jindex, kindex;

			//Create initial admittance matrix - use code from GS below - store in From_Y (for now)
			for (jindex=0; jindex<3; jindex++)
				for (kindex=0; kindex<3; kindex++)
					Y[jindex][kindex] = 0.0;
			
			// compute admittance - invert b matrix - special circumstances given different methods
			if (has_phase(PHASE_S)) //Triplexy
			{
				inverse(b_mat,Y);
			}
			else if (has_phase(PHASE_A) && !has_phase(PHASE_B) && !has_phase(PHASE_C)) //only A
				Y[0][0] = complex(1.0) / b_mat[0][0];
			else if (!has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //only B
				Y[1][1] = complex(1.0) / b_mat[1][1];
			else if (!has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //only C
				Y[2][2] = complex(1.0) / b_mat[2][2];
			else if (has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //has A & C
			{
				complex detvalue = b_mat[0][0]*b_mat[2][2] - b_mat[0][2]*b_mat[2][0];

				Y[0][0] = b_mat[2][2] / detvalue;
				Y[0][2] = b_mat[0][2] * -1.0 / detvalue;
				Y[2][0] = b_mat[2][0] * -1.0 / detvalue;
				Y[2][2] = b_mat[0][0] / detvalue;
			}
			else if (has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //has A & B
			{
				complex detvalue = b_mat[0][0]*b_mat[1][1] - b_mat[0][1]*b_mat[1][0];

				Y[0][0] = b_mat[1][1] / detvalue;
				Y[0][1] = b_mat[0][1] * -1.0 / detvalue;
				Y[1][0] = b_mat[1][0] * -1.0 / detvalue;
				Y[1][1] = b_mat[0][0] / detvalue;
			}
			else if ((has_phase(PHASE_A) && has_phase(PHASE_B) && has_phase(PHASE_C)) || (has_phase(PHASE_D))) //has ABC or D (D=ABC)
				inverse(b_mat,Y);
			// defaulted else - No phases (e.g., the line does not exist) - just = 0

			//Compute total self admittance - include line charging capacitance
			equalm(a_mat,Ylinecharge);
			Ylinecharge[0][0]-=1;
			Ylinecharge[1][1]-=1;
			Ylinecharge[2][2]-=1;
			multiply(2,Ylinecharge,Ylefttemp);
			multiply(Y,Ylefttemp,Ylinecharge);

			addition(Ylinecharge,Y,From_Y);

			//Update time variable
			prev_LTime=t0;
		}

	}
	else if ((solver_method==SM_GS) & (is_closed()) & (prev_LTime!=t0))	//Initial YVs calculations
	{
		node *fnode = OBJECTDATA(from,node);
		node *tnode = OBJECTDATA(to,node);
		if (fnode==NULL || tnode==NULL)
			return TS_NEVER;
		
		complex Ytot[3][3];
		complex Ylinecharge[3][3];
		complex Y[3][3];
		complex Yc[3][3];
		complex Ifrom[3];
		complex Ito[3];
		complex Ys[3][3];
		complex Yto[3][3];
		complex Yfrom[3][3];
		complex Ylefttemp[3][3];
		complex Yrighttemp[3][3];
		complex outcurrent[3];
		complex tempvar[3];
		double invratio;
		char jindex, kindex;
		char zerosum = 0;

		//Update t0 variable
		prev_LTime=t0;

		//Reset global convergence variable
		GS_all_converged=false;

		for (jindex=0;jindex<3;jindex++)	//Check to see if b_mat is all zeros (0 length line)
		{
			for (kindex=0;kindex<3;kindex++)
			{
				if (b_mat[jindex][kindex]==0)
				{
					zerosum++;
				}
			}
		}

		if ((zerosum==9)) // && (fnode->bustype!=2)) //Zero length line & from is not the swing bus
		{
			//put some values in the matrices first - these need to be fixed to get accurate power flow...but it's length 0!?!
			b_mat[0][0] = b_mat[1][1] = b_mat[2][2] = fault_Z;
			d_mat[0][0] = d_mat[1][1] = d_mat[2][2] = 1.0;
			A_mat[0][0] = A_mat[1][1] = A_mat[2][2] = 1.0;
			B_mat[0][0] = B_mat[1][1] = B_mat[2][2] = fault_Z;

			//Remove ourselves from from and to node's linked lists, otherwise it may reference itself
			
			//Grab the linked list from the from object
			LINKCONNECTED *parlink = &fnode->nodelinks;

			LINKCONNECTED *prevlink = new LINKCONNECTED;
			if (prevlink==NULL)
			{
				gl_error("GS: memory allocation failure zero length");
				/*  TROUBLESHOOT
				This is a bug.  Gauss-Seidel attempted to allocate memory for a zero-length substitution and failed.
				Please submit a bug report and your model files.
				*/
				return 0;
			}

			while (parlink->next!=NULL)		//Parse through the linked list
			{
				prevlink = parlink;
				parlink = parlink->next;
				
				if ((parlink->fnodeconnected==from) & (parlink->tnodeconnected==to)) //this is us
				{
					prevlink->next=parlink->next;
				}
			}
			
			prevlink = NULL;
			parlink = &tnode->nodelinks;

			if (parlink->next->next!=NULL)	//Was the only line in, so just get rid of us
			{
				while (parlink->next!=NULL)
				{
					prevlink = parlink;
					parlink = parlink->next;
					
					if ((parlink->fnodeconnected==from) & (parlink->tnodeconnected==to)) //this is us
					{
						prevlink->next=parlink->next;
					}
				}
			}
			else
				parlink->next=NULL;

			prevlink=NULL;
			delete prevlink;

			OBJECT *obj = OBJECTHDR(this);

			//Essentially setting from as parent - follow basic idea of node:init

			//See if it is a node/load/meter
			if (!(gl_object_isa(from,"load") | gl_object_isa(from,"node") | gl_object_isa(from,"meter")))
				GL_THROW("GS: Attempt to substitue 0 length line %d failed: from is not a node device!",obj->id);
				/*  TROUBLESHOOT
				The from end of a line is not a node, load, or meter.  Gauss-Seidel has failed to do a zero line
				substitution as a result.  Check the from connection of the link.
				*/

			//Make sure our phases align, otherwise become angry
			if (fnode->phases!=tnode->phases)
				GL_THROW("GS: Attempt to substitue 0 length line %d failed: endpoint phases do not match!",obj->id);
				/*  TROUBLESHOOT
				Gauss-Seidel attempted to substitute a 0 length line with a parent-child relationship.  However, the parent and child
				phases do not explicitly match.  Set them the same to enable the parent child relationship.
				*/

			//Additional check not needed in node - make sure To isn't a parent (no easy way to fix this, if multiple children gets wonky)
			if (tnode->SubNode==(SUBNODETYPE)3)
			{
				//Reference its child
				node *SubNodeObj = OBJECTDATA(tnode->SubNodeParent,node);
				gl_warning("0 Length Line %d has child-linked object as the end.  If more than one child existed, earlier children have been lost!",obj->id);
				/*  TROUBLESHOOT
				The end link of a system was attached to a node that already had a parent-child relationship.  If more than one child was connected to
				this end node, other children may have lost their connection and no longer be connected to the system.
				*/
			
				//Now have to handle based on what the from node of the line is
				if (fnode->SubNode==(SUBNODETYPE)2)	//From node is another child
				{
					GL_THROW("GS: Attempt to substitute 0 length line %d failed: Would result in great-grandchilren nesting which is unsupported in GS!",obj->id);
					/*  TROUBLESHOOT
					Gauss-Seidel is only set to implement "grandchildren" relationships.  That is, parent-child connections
					may only be nested two-deep.  Any further nesting is not supported.
					*/
				}
				else	//From node is unchilded
				{
					//Set appropriate flags (store parent name and flag from & to)
					tnode->SubNode = (SUBNODETYPE)2;
					tnode->SubNodeParent = from;
					
					fnode->SubNode = (SUBNODETYPE)3;
					fnode->SubNodeParent = to;

					//Set to's subnode (or the one we found) to from node as well
					SubNodeObj->SubNodeParent = from;
				}
			}
			else	//Normal To node
			{
				if (fnode->SubNode==(SUBNODETYPE)2)	//From node is another child
				{
					//Set appropriate flags (store parent name and flag)
					tnode->SubNode = (SUBNODETYPE)2;
					tnode->SubNodeParent = fnode->SubNodeParent;
				}
				else	//From node is unchilded
				{
					//Set appropriate flags (store parent name and flag from & to)
					tnode->SubNode = (SUBNODETYPE)2;
					tnode->SubNodeParent = from;
					
					fnode->SubNode = (SUBNODETYPE)3;
					fnode->SubNodeParent = to;
				}
			}

			for (jindex=0;jindex<3;jindex++)	//zero out load tracking matrix (just in case) (do both in case to was parent too)
			{
				for (kindex=0;kindex<3;kindex++)
				{
					fnode->last_child_power[jindex][kindex]=0.0;
					tnode->last_child_power[jindex][kindex]=0.0;
				}
			}

		}
		else	//normal b_mat (or at least, not all zeros)
		{
			//Ensure have zeroed out the Y at first
			for (jindex=0;jindex<3;jindex++)
			{
				for (kindex=0;kindex<3;kindex++)
				{
					Y[jindex][kindex] = 0.0;
				}
			}

			// compute admittance - invert b matrix - special circumstances given different methods
			if (has_phase(PHASE_S)) //Triplexy
			{
				inverse(b_mat,Y);
			}
			else if (has_phase(PHASE_A) && !has_phase(PHASE_B) && !has_phase(PHASE_C)) //only A
				Y[0][0] = complex(1.0) / b_mat[0][0];
			else if (!has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //only B
				Y[1][1] = complex(1.0) / b_mat[1][1];
			else if (!has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //only C
				Y[2][2] = complex(1.0) / b_mat[2][2];
			else if (has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //has A & C
			{
				complex detvalue = b_mat[0][0]*b_mat[2][2] - b_mat[0][2]*b_mat[2][0];

				Y[0][0] = b_mat[2][2] / detvalue;
				Y[0][2] = b_mat[0][2] * -1.0 / detvalue;
				Y[2][0] = b_mat[2][0] * -1.0 / detvalue;
				Y[2][2] = b_mat[0][0] / detvalue;
			}
			else if (has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //has A & B
			{
				complex detvalue = b_mat[0][0]*b_mat[1][1] - b_mat[0][1]*b_mat[1][0];

				Y[0][0] = b_mat[1][1] / detvalue;
				Y[0][1] = b_mat[0][1] * -1.0 / detvalue;
				Y[1][0] = b_mat[1][0] * -1.0 / detvalue;
				Y[1][1] = b_mat[0][0] / detvalue;
			}
			else if ((has_phase(PHASE_A) && has_phase(PHASE_B) && has_phase(PHASE_C)) || (has_phase(PHASE_D))) //has ABC or D (D=ABC)
				inverse(b_mat,Y);
			// defaulted else - No phases (e.g., the line does not exist) - just = 0

			//Compute total self admittance - include line charging capacitance
			equalm(a_mat,Ylinecharge);
			Ylinecharge[0][0]-=1;
			Ylinecharge[1][1]-=1;
			Ylinecharge[2][2]-=1;
			multiply(2,Ylinecharge,Ylefttemp);
			multiply(Y,Ylefttemp,Ylinecharge);

			addition(Ylinecharge,Y,Ytot);
			
			if ((voltage_ratio!=1) | (SpecialLnk!=NORMAL))	//Handle transformers slightly different
			{
				invratio=1.0/voltage_ratio;

				if (SpecialLnk==DELTAGWYE)	//Delta-Gwye implementation
				{
					complex tempImped;

					//Pre-admittancized matrix
					equalm(b_mat,Yto);

					//Adjust for To_Y
					multiply(Yto,c_mat,To_Y);

					//Scale to other size
					multiply(invratio,Yto,Ylefttemp);
					multiply(invratio,Ylefttemp,Yfrom);

					//Adjust for From_Y
					multiply(B_mat,Yto,From_Y);

					//Fix what I broke
					for(jindex=0;jindex<3;jindex++)
					{
						for(kindex=0;kindex<3;kindex++)
						{
							c_mat[jindex][kindex]=0.0;
							B_mat[jindex][kindex]=0.0;
						}
					}

					tempImped = complex(1.0) / b_mat[0][0];
					B_mat[0][0] = B_mat[1][1] = B_mat[2][2] = tempImped;

					//Calculate YVs terms
					Ifrom[0]=From_Y[0][0]*tnode->voltage[0]+
							 From_Y[0][1]*tnode->voltage[1]+
							 From_Y[0][2]*tnode->voltage[2];
					Ifrom[1]=From_Y[1][0]*tnode->voltage[0]+
							 From_Y[1][1]*tnode->voltage[1]+
							 From_Y[1][2]*tnode->voltage[2];
					Ifrom[2]=From_Y[2][0]*tnode->voltage[0]+
							 From_Y[2][1]*tnode->voltage[1]+
							 From_Y[2][2]*tnode->voltage[2];
					Ito[0] = To_Y[0][0]*fnode->voltage[0]+
							 To_Y[0][1]*fnode->voltage[1]+
							 To_Y[0][2]*fnode->voltage[2];
					Ito[1] = To_Y[1][0]*fnode->voltage[0]+
							 To_Y[1][1]*fnode->voltage[1]+
							 To_Y[1][2]*fnode->voltage[2];
					Ito[2] = To_Y[2][0]*fnode->voltage[0]+
							 To_Y[2][1]*fnode->voltage[1]+
							 To_Y[2][2]*fnode->voltage[2];

				}
				else if (SpecialLnk==REGULATOR)	//Regulator
				{
					GL_THROW("GS: Regulator not implemented in Gauss-Seidel Solver yet!");
					/*  TROUBLESHOOT
					Regulators are not coded into the Gauss-Seidel solver at this time.  Please
					use a different solver method or find a way to remove the regulator from your model.
					*/

					equalm(b_mat,Yto);	//Initial code.  Very untested
					equalm(c_mat,Yfrom);

					equalm(Yto,To_Y);

					To_Y[0][0] *= B_mat[0][0];
					To_Y[0][1] *= B_mat[0][1];
					To_Y[0][2] *= B_mat[0][2];
					To_Y[1][0] *= B_mat[1][0];
					To_Y[1][1] *= B_mat[1][1];
					To_Y[1][2] *= B_mat[1][2];
					To_Y[2][0] *= B_mat[2][0];
					To_Y[2][1] *= B_mat[2][1];
					To_Y[2][2] *= B_mat[2][2];

					equalm(Yfrom,From_Y);

					From_Y[0][0] = (B_mat[0][0]==0.0) ? 0.0 : From_Y[0][0]*B_mat[0][0];
					From_Y[0][1] = (B_mat[0][1]==0.0) ? 0.0 : From_Y[0][1]*B_mat[0][1];
					From_Y[0][2] = (B_mat[0][2]==0.0) ? 0.0 : From_Y[0][2]*B_mat[0][2];
					From_Y[1][0] = (B_mat[1][0]==0.0) ? 0.0 : From_Y[1][0]*B_mat[1][0];
					From_Y[1][1] = (B_mat[1][1]==0.0) ? 0.0 : From_Y[1][1]*B_mat[1][1];
					From_Y[1][2] = (B_mat[1][2]==0.0) ? 0.0 : From_Y[1][2]*B_mat[1][2];
					From_Y[2][0] = (B_mat[2][0]==0.0) ? 0.0 : From_Y[2][0]*B_mat[2][0];
					From_Y[2][1] = (B_mat[2][1]==0.0) ? 0.0 : From_Y[2][1]*B_mat[2][1];
					From_Y[2][2] = (B_mat[2][2]==0.0) ? 0.0 : From_Y[2][2]*B_mat[2][2];

					//Zero the used matrices
					b_mat[0][0] = b_mat[0][1] = b_mat[0][2] = complex(0,0);
					b_mat[1][0] = b_mat[1][1] = b_mat[1][2] = complex(0,0);
					b_mat[2][0] = b_mat[2][1] = b_mat[2][2] = complex(0,0);

					equalm(b_mat,c_mat);
					equalm(b_mat,B_mat);

					Ifrom[0]=From_Y[0][0]*tnode->voltage[0]+
							 From_Y[0][1]*tnode->voltage[1]+
							 From_Y[0][2]*tnode->voltage[2];
					Ifrom[1]=From_Y[1][0]*tnode->voltage[0]+
							 From_Y[1][1]*tnode->voltage[1]+
							 From_Y[1][2]*tnode->voltage[2];
					Ifrom[2]=From_Y[2][0]*tnode->voltage[0]+
							 From_Y[2][1]*tnode->voltage[1]+
							 From_Y[2][2]*tnode->voltage[2];
					Ito[0] = To_Y[0][0]*fnode->voltage[0]+
							 To_Y[0][1]*fnode->voltage[1]+
							 To_Y[0][2]*fnode->voltage[2];
					Ito[1] = To_Y[1][0]*fnode->voltage[0]+
							 To_Y[1][1]*fnode->voltage[1]+
							 To_Y[1][2]*fnode->voltage[2];
					Ito[2] = To_Y[2][0]*fnode->voltage[0]+
							 To_Y[2][1]*fnode->voltage[1]+
							 To_Y[2][2]*fnode->voltage[2];
				}
				else if (SpecialLnk==SPLITPHASE)	//Split phase - non working
				{
					equalm(b_mat,Yto);
					equalm(c_mat,Yfrom);

					for (jindex=0;jindex<3;jindex++)
					{
						for (kindex=0;kindex<3;kindex++)
						{
							c_mat[jindex][kindex] = 0.0;
						}
					}

/*					Old version - works?? - fixing assignments so can generalize
					equalm(b_mat,Yto);
					Yto[2][0] = Yto[2][1] = 0.0;

					equalm(b_mat,Ylefttemp);
					Ylefttemp[1][0] = Ylefttemp[1][1];
					Ylefttemp[1][1] = 0.0;
					multiply(invratio,Ylefttemp,To_Y);

					equalm(c_mat,Yfrom);
					equalm(c_mat,From_Y);
					Yfrom[0][0] = Yfrom[2][2];
					Yfrom[2][2] = Yfrom[0][1] = 0.0;

					c_mat[0][0] = c_mat[0][1] = c_mat[2][2] = 0.0;*/

					//multiply(invratio,Yfrom,From_Y);

					Ifrom[0]=From_Y[0][0]*tnode->voltage[0]+
							 From_Y[0][1]*tnode->voltage[1]+
							 From_Y[0][2]*tnode->voltage[2];
					Ifrom[1]=From_Y[1][0]*tnode->voltage[0]+
							 From_Y[1][1]*tnode->voltage[1]+
							 From_Y[1][2]*tnode->voltage[2];
					Ifrom[2]=From_Y[2][0]*tnode->voltage[0]+
							 From_Y[2][1]*tnode->voltage[1]+
							 From_Y[2][2]*tnode->voltage[2];
					Ito[0] = To_Y[0][0]*fnode->voltage[0]+
							 To_Y[0][1]*fnode->voltage[1]+
							 To_Y[0][2]*fnode->voltage[2];
					Ito[1] = To_Y[1][0]*fnode->voltage[0]+
							 To_Y[1][1]*fnode->voltage[1]+
							 To_Y[1][2]*fnode->voltage[2];
					Ito[2] = To_Y[2][0]*fnode->voltage[0]+
							 To_Y[2][1]*fnode->voltage[1]+
							 To_Y[2][2]*fnode->voltage[2];

				}
				else	//Other xformers
				{
					//Pre-admittancized matrix
					equalm(b_mat,Yto);

					multiply(invratio,Yto,Ylefttemp);		//Scale from admittance by turns ratio
					multiply(invratio,Ylefttemp,Yfrom);

					multiply(invratio,Yto,To_Y);		//Incorporate turns ratio information into line's admittance matrix.
					multiply(voltage_ratio,Yfrom,From_Y); //Scales voltages to same "level" for GS //uncomment me

					Ifrom[0]=From_Y[0][0]*tnode->voltage[0]+
							 From_Y[0][1]*tnode->voltage[1]+
							 From_Y[0][2]*tnode->voltage[2];
					Ifrom[1]=From_Y[1][0]*tnode->voltage[0]+
							 From_Y[1][1]*tnode->voltage[1]+
							 From_Y[1][2]*tnode->voltage[2];
					Ifrom[2]=From_Y[2][0]*tnode->voltage[0]+
							 From_Y[2][1]*tnode->voltage[1]+
							 From_Y[2][2]*tnode->voltage[2];
					Ito[0] = To_Y[0][0]*fnode->voltage[0]+
							 To_Y[0][1]*fnode->voltage[1]+
							 To_Y[0][2]*fnode->voltage[2];
					Ito[1] = To_Y[1][0]*fnode->voltage[0]+
							 To_Y[1][1]*fnode->voltage[1]+
							 To_Y[1][2]*fnode->voltage[2];
					Ito[2] = To_Y[2][0]*fnode->voltage[0]+
							 To_Y[2][1]*fnode->voltage[1]+
							 To_Y[2][2]*fnode->voltage[2];


				}

				LOCK_OBJECT(from);
				addition(fnode->Ys,Yfrom,fnode->Ys);
				fnode->YVs[0] += Ifrom[0];
				fnode->YVs[1] += Ifrom[1];
				fnode->YVs[2] += Ifrom[2];
				UNLOCK_OBJECT(from);

				LOCK_OBJECT(to);
				addition(tnode->Ys,Yto,tnode->Ys);
				tnode->YVs[0] += Ito[0];
				tnode->YVs[1] += Ito[1];
				tnode->YVs[2] += Ito[2];
				UNLOCK_OBJECT(to);
			
			}
			else					//Simple lines
			{
				//Compute "currents"
				//Individual phases
				Ifrom[0]=Ytot[0][0]*tnode->voltage[0]+
						 Ytot[0][1]*tnode->voltage[1]+
						 Ytot[0][2]*tnode->voltage[2];
				Ifrom[1]=Ytot[1][0]*tnode->voltage[0]+
						 Ytot[1][1]*tnode->voltage[1]+
						 Ytot[1][2]*tnode->voltage[2];
				Ifrom[2]=Ytot[2][0]*tnode->voltage[0]+
						 Ytot[2][1]*tnode->voltage[1]+
						 Ytot[2][2]*tnode->voltage[2];
				Ito[0] = Ytot[0][0]*fnode->voltage[0]+
						 Ytot[0][1]*fnode->voltage[1]+
						 Ytot[0][2]*fnode->voltage[2];
				Ito[1] = Ytot[1][0]*fnode->voltage[0]+
						 Ytot[1][1]*fnode->voltage[1]+
						 Ytot[1][2]*fnode->voltage[2];
				Ito[2] = Ytot[2][0]*fnode->voltage[0]+
						 Ytot[2][1]*fnode->voltage[1]+
						 Ytot[2][2]*fnode->voltage[2];
			
				//Update admittance tracking - separate matrices (transformer support)
				equalm(Ytot,To_Y);
				equalm(Ytot,From_Y);

				//Update from node YVs
				LOCK_OBJECT(from);
				addition(fnode->Ys,Ytot,fnode->Ys);
				fnode->YVs[0] += Ifrom[0];
				fnode->YVs[1] += Ifrom[1];
				fnode->YVs[2] += Ifrom[2];
				UNLOCK_OBJECT(from);

				//Update to node YVs
				LOCK_OBJECT(to);
				addition(tnode->Ys,Ytot,tnode->Ys);
				tnode->YVs[0] += Ito[0];
				tnode->YVs[1] += Ito[1];
				tnode->YVs[2] += Ito[2];
				UNLOCK_OBJECT(to);
			}
		}
	}

	return t1;
}

TIMESTAMP link::sync(TIMESTAMP t0)
{
#ifdef SUPPORT_OUTAGES
	node *fNode;
	node *tNode;
	fNode=OBJECTDATA(from,node);
	tNode=OBJECTDATA(to,node);
#endif

	if (is_closed())
	{
		if (solver_method==SM_NR)
		{
			//Nothing here yet
		}
		else if (solver_method==SM_FBS)
		{
			node *f;
			node *t;
			set reverse = get_flow(&f,&t);

#ifdef SUPPORT_OUTAGES
			tNode->condition=fNode->condition;
#endif
			/* compute currents */
			complex i;
			current_in[0] = 
			i = c_mat[0][0] * t->voltage[0] +
				c_mat[0][1] * t->voltage[1] +
				c_mat[0][2] * t->voltage[2] +
				d_mat[0][0] * t->current_inj[0] +
				d_mat[0][1] * t->current_inj[1] +
				d_mat[0][2] * t->current_inj[2];
			LOCKED(from, f->current_inj[0] += i);
			current_in[1] = 
			i = c_mat[1][0] * t->voltage[0] +
				c_mat[1][1] * t->voltage[1] +
				c_mat[1][2] * t->voltage[2] +
				d_mat[1][0] * t->current_inj[0] +
				d_mat[1][1] * t->current_inj[1] +
				d_mat[1][2] * t->current_inj[2];
			LOCKED(from, f->current_inj[1] += i);
			current_in[2] = 
			i = c_mat[2][0] * t->voltage[0] +
				c_mat[2][1] * t->voltage[1] +
				c_mat[2][2] * t->voltage[2] +
				d_mat[2][0] * t->current_inj[0] +
				d_mat[2][1] * t->current_inj[1] +
				d_mat[2][2] * t->current_inj[2];
			LOCKED(from, f->current_inj[2] += i);
		}
	}
#ifdef SUPPORT_OUTAGES
	else if (is_open_any())
	{
		if (solver_method==SM_FBS)
		{
			/* compute for consequences of open link conditions -- Only supports 3-phase fault at the moment */
			if (has_phase(PHASE_A))	a_mat[0][0] = d_mat[0][0] = A_mat[0][0] = is_open() ? 0.0 : 1.0;
			if (has_phase(PHASE_B))	a_mat[1][1] = d_mat[1][1] = A_mat[1][1] = is_open() ? 0.0 : 1.0;
			if (has_phase(PHASE_C))	a_mat[2][2] = d_mat[2][2] = A_mat[2][2] = is_open() ? 0.0 : 1.0;
			tNode->condition=!OC_NORMAL;
		}

	}
	else if (is_contact_any())
		throw "unable to handle link contact condition";
#endif

	return TS_NEVER;
}

TIMESTAMP link::postsync(TIMESTAMP t0)
{
	TIMESTAMP TRET=TS_NEVER;
	if (solver_method==SM_NR)
	{
		//Nothing here yet
	}
	//else if ((!is_open()) && (solver_method==SM_FBS))
	else if ((solver_method==SM_FBS))
	{
		node *f;
		node *t;
		set reverse = get_flow(&f,&t);

		if (!is_open())
		{
			/* compute and update voltages */
			complex v;
			v = A_mat[0][0] * f->voltage[0] +
				A_mat[0][1] * f->voltage[1] +
				A_mat[0][2] * f->voltage[2] -
				B_mat[0][0] * t->current_inj[0] -
				B_mat[0][1] * t->current_inj[1] -
				B_mat[0][2] * t->current_inj[2];
			LOCKED(to, t->voltage[0] = v);
			v = A_mat[1][0] * f->voltage[0] +
				A_mat[1][1] * f->voltage[1] +
				A_mat[1][2] * f->voltage[2] -
				B_mat[1][0] * t->current_inj[0] -
				B_mat[1][1] * t->current_inj[1] -
				B_mat[1][2] * t->current_inj[2];
			LOCKED(to, t->voltage[1] = v);
			v = A_mat[2][0] * f->voltage[0] +
				A_mat[2][1] * f->voltage[1] +
				A_mat[2][2] * f->voltage[2] -
				B_mat[2][0] * t->current_inj[0] -
				B_mat[2][1] * t->current_inj[1] -
				B_mat[2][2] * t->current_inj[2];
			LOCKED(to, t->voltage[2] = v);
#ifdef SUPPORT_OUTAGES		
			t->condition=f->condition;
		}
		else if (is_open()) //open
		{
			t->condition=!OC_NORMAL;
		}

		/* propagate voltage source flag from to-bus to from-bus */
		if (t->bustype==node::PQ)
		{
			/* keep a copy of the old flags on the to-bus */
			set of = t->busflags&NF_HASSOURCE;

			/* if the admittance is non-zero */
			if ((a_mat[0][0].Mag()>0 || a_mat[1][1].Mag()>0 || a_mat[2][2].Mag()>0))
			{
				/* the source-flag of the from-bus is copied to the to-bus */
				LOCKED(to, t->busflags |= (f->busflags&NF_HASSOURCE));
			}
			else
			{
				/* otherwise the source flag of the to-bus is cleared */
				LOCKED(to, t->busflags &= ~NF_HASSOURCE);
			}

			/* if the to-bus flags has changed */
			if ((t->busflags&NF_HASSOURCE)!=of)

				/* force the solver to make another pass */
				TRET = t0;
		}
#else
		}
#endif

		/* compute kva flows */
		power_in = (f->voltage[0]*~current_in[0]).Mag() + (f->voltage[1]*~current_in[1]).Mag() + (f->voltage[2]*~current_in[2]).Mag();
		power_out = (t->voltage[0]*~t->current_inj[0]).Mag() + (t->voltage[1]*~t->current_inj[1]).Mag() + (t->voltage[2]*~t->current_inj[2]).Mag();
	}
	else if ((!is_open()) && (solver_method==SM_GS) && GS_all_converged)
	{
		node *fnode = OBJECTDATA(from,node);
		node *tnode = OBJECTDATA(to,node);
		complex current_temp[3];
		complex Binv[3][3];
		char jindex, kindex;

		for (jindex=0; jindex<3; jindex++)
			for (kindex=0; kindex<3; kindex++)
				Binv[jindex][kindex] = 0.0;

		// invert B matrix - special circumstances given different methods
		if (has_phase(PHASE_A) && !has_phase(PHASE_B) && !has_phase(PHASE_C)) //only A
			Binv[0][0] = complex(1.0) / B_mat[0][0];
		else if (!has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //only B
			Binv[1][1] = complex(1.0) / B_mat[1][1];
		else if (!has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //only C
			Binv[2][2] = complex(1.0) / B_mat[2][2];
		else if (has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //has A & C
		{
			complex detvalue = B_mat[0][0]*B_mat[2][2] - B_mat[0][2]*B_mat[2][0];

			Binv[0][0] = B_mat[2][2] / detvalue;
			Binv[0][2] = B_mat[0][2] * -1.0 / detvalue;
			Binv[2][0] = B_mat[2][0] * -1.0 / detvalue;
			Binv[2][2] = B_mat[0][0] / detvalue;
		}
		else if (has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //has A & B
		{
			complex detvalue = B_mat[0][0]*B_mat[1][1] - B_mat[0][1]*B_mat[1][0];

			Binv[0][0] = B_mat[1][1] / detvalue;
			Binv[0][1] = B_mat[0][1] * -1.0 / detvalue;
			Binv[1][0] = B_mat[1][0] * -1.0 / detvalue;
			Binv[1][1] = B_mat[0][0] / detvalue;
		}
		else if ((has_phase(PHASE_A) && has_phase(PHASE_B) && has_phase(PHASE_C)) || (has_phase(PHASE_D))) //has ABC or D (D=ABC)
			inverse(B_mat,Binv);
		// defaulted else - No phases (e.g., the line does not exist) - just = 0

		//Calculate output currents
		current_temp[0] = A_mat[0][0]*fnode->voltage[0]+
						  A_mat[0][1]*fnode->voltage[1]+
						  A_mat[0][2]*fnode->voltage[2]-
						  tnode->voltage[0];
		current_temp[1] = A_mat[1][0]*fnode->voltage[0]+
						  A_mat[1][1]*fnode->voltage[1]+
						  A_mat[1][2]*fnode->voltage[2]-
						  tnode->voltage[1];
		current_temp[2] = A_mat[2][0]*fnode->voltage[0]+
						  A_mat[2][1]*fnode->voltage[1]+
						  A_mat[2][2]*fnode->voltage[2]-
						  tnode->voltage[2];

		current_out[0] = Binv[0][0]*current_temp[0]+
						 Binv[0][1]*current_temp[1]+
						 Binv[0][2]*current_temp[2];
		current_out[1] = Binv[1][0]*current_temp[0]+
						 Binv[1][1]*current_temp[1]+
						 Binv[1][2]*current_temp[2];
		current_out[2] = Binv[2][0]*current_temp[0]+
						 Binv[2][1]*current_temp[1]+
						 Binv[2][2]*current_temp[2];

		//Calculate input currents
		current_in[0] = c_mat[0][0]*tnode->voltage[0]+
						c_mat[0][1]*tnode->voltage[1]+
						c_mat[0][2]*tnode->voltage[2]+
						d_mat[0][0]*current_out[0]+
						d_mat[0][1]*current_out[1]+
						d_mat[0][2]*current_out[2];
		current_in[1] = c_mat[1][0]*tnode->voltage[0]+
						c_mat[1][1]*tnode->voltage[1]+
						c_mat[1][2]*tnode->voltage[2]+
						d_mat[1][0]*current_out[0]+
						d_mat[1][1]*current_out[1]+
						d_mat[1][2]*current_out[2];
		current_in[2] = c_mat[2][0]*tnode->voltage[0]+
						c_mat[2][1]*tnode->voltage[1]+
						c_mat[2][2]*tnode->voltage[2]+
						d_mat[2][0]*current_out[0]+
						d_mat[2][1]*current_out[1]+
						d_mat[2][2]*current_out[2];

		//Compute magnitude of power output
		power_in = ((fnode->voltage[0]*~current_in[0]).Mag() + (fnode->voltage[1]*~current_in[1]).Mag() + (fnode->voltage[2]*~current_in[2]).Mag());
		power_out = ((tnode->voltage[0]*~current_out[0]).Mag() + (tnode->voltage[1]*~current_out[1]).Mag() + (tnode->voltage[2]*~current_out[2]).Mag());
	}

	return TRET;
}

int link::kmldump(FILE *fp)
{
	OBJECT *obj = OBJECTHDR(this);
	if (isnan(from->latitude) || isnan(to->latitude) || isnan(from->longitude) || isnan(to->longitude))
		return 0;
	fprintf(fp,"    <Placemark>\n");
	if (obj->name)
		fprintf(fp,"      <name>%s</name>\n", obj->name);
	else
		fprintf(fp,"      <name>%s ==> %s</name>\n", from->name?from->name:"unnamed", to->name?to->name:"unnamed");
	fprintf(fp,"      <description>\n");
	fprintf(fp,"        <![CDATA[\n");
	fprintf(fp,"          <TABLE><TR>\n");
	fprintf(fp,"<TR><TD WIDTH=\"25%\">%s&nbsp;%d<HR></TD><TH WIDTH=\"25%\" ALIGN=CENTER>Phase A<HR></TH><TH WIDTH=\"25%\" ALIGN=CENTER>Phase B<HR></TH><TH WIDTH=\"25%\" ALIGN=CENTER>Phase C<HR></TH></TR>\n", obj->oclass->name, obj->id);

	// values
	node *pFrom = OBJECTDATA(from,node);
	node *pTo = OBJECTDATA(to,node);
	double vscale = primary_voltage_ratio*sqrt((double) 3.0)/(double) 1000.0;
	complex loss[3];
	complex flow[3];
	complex current[3];
	complex in[3] = {pFrom->voltageA/B_mat[0][0], pFrom->voltageB/B_mat[1][1], pFrom->voltageC/B_mat[2][2]};
	complex out[3] = {pTo->voltageA/B_mat[0][0], pTo->voltageB/B_mat[1][1], pTo->voltageC/B_mat[2][2]};
	complex Vfrom[3] = {pFrom->voltageA, pFrom->voltageB, pFrom->voltageC};
	complex Vto[3] = {pTo->voltageA,pTo->voltageB,pTo->voltageC};
	int phase[3] = {has_phase(PHASE_A),has_phase(PHASE_B),has_phase(PHASE_C)};
	int i;
	for (i=0; i<3; i++)
	{
		if (phase[i])
		{
			if (Vfrom[i].Re() > Vto[i].Re())
			{
				flow[i] = out[i]*Vto[i]*vscale;
				loss[i] = in[i]*Vfrom[i]*vscale - flow[i];
				current[i] = out[i];
			}
			else
			{
				flow[i] = in[i]*Vfrom[i]*vscale;
				loss[i] = out[i]*Vto[i]*vscale - flow[i];
				current[i] = in[i];
			}
		}
	}

	// flow
	fprintf(fp,"<TR><TH ALIGN=LEFT>Flow</TH>");
	for (i=0; i<3; i++)
	{
		if (phase[i])
			fprintf(fp,"<TD ALIGN=RIGHT STYLE=\"font-family:courier;\">%.3f&nbsp;&nbsp;kW&nbsp;&nbsp;<BR>%.3f&nbsp;&nbsp;kVAR</TD>\n",
				flow[i].Re(), flow[i].Im());
		else
			fprintf(fp,"<TD></TD>\n");
	}
	fprintf(fp,"</TR>");

	// current
	fprintf(fp,"<TR><TH ALIGN=LEFT>Current</TH>");
	for (i=0; i<3; i++)
	{
		if (phase[i])
			fprintf(fp,"<TD ALIGN=RIGHT STYLE=\"font-family:courier;\">%.3f&nbsp;&nbsp;Amps</TD>\n",
				current[i].Mag());
		else
			fprintf(fp,"<TD></TD>\n");
	}
	fprintf(fp,"</TR>");

	// loss
	fprintf(fp,"<TR><TH ALIGN=LEFT>Loss</TH>");
	for (i=0; i<3; i++)
	{
		if (phase[i])
			fprintf(fp,"<TD ALIGN=RIGHT STYLE=\"font-family:courier;\">%.2f&nbsp;&nbsp;&nbsp;%%P&nbsp;&nbsp;<BR>%.2f&nbsp;&nbsp;&nbsp;%%Q&nbsp;&nbsp;</TD>\n",
				loss[i].Re()/flow[i].Re()*100,loss[i].Im()/flow[i].Im()*100);
		else
			fprintf(fp,"<TD></TD>\n");
	}
	fprintf(fp,"</TR>");
	fprintf(fp,"</TABLE>\n");
	fprintf(fp,"        ]]>\n");
	fprintf(fp,"      </description>\n");
	fprintf(fp,"      <styleUrl>#%s</styleUrl>>\n",obj->oclass->name);
	fprintf(fp,"      <coordinates>%f,%f</coordinates>\n",
		(from->longitude+to->longitude)/2,(from->latitude+to->latitude)/2);
	fprintf(fp,"      <LineString>\n");			
	fprintf(fp,"        <extrude>0</extrude>\n");
	fprintf(fp,"        <tessellate>0</tessellate>\n");
	fprintf(fp,"        <altitudeMode>relative</altitudeMode>\n");
	fprintf(fp,"        <coordinates>%f,%f,50 %f,%f,50</coordinates>\n",
		from->longitude,from->latitude,to->longitude,to->latitude);
	fprintf(fp,"      </LineString>\n");			
	fprintf(fp,"    </Placemark>\n");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: link
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_link(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(link::oclass);
		if (*obj!=NULL)
		{
			link *my = OBJECTDATA(*obj,link);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (char *msg)
	{
		gl_error("create_link: %s", msg);
	}
	return 0;
}

/**
* Object initialization is called once after all object have been created
*
* @param obj a pointer to this object
* @return 1 on success, 0 on error
*/
EXPORT int init_link(OBJECT *obj)
{
	link *my = OBJECTDATA(obj,link);
	try {
		return my->init(obj->parent);
	}
	catch (char *msg)
	{
		GL_THROW("%s (link:%d): %s", my->get_name(), my->get_id(), msg);
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
EXPORT TIMESTAMP sync_link(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	link *pObj = OBJECTDATA(obj,link);
	try 
	{
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
	} 
	catch (const char *error) 
	{
		GL_THROW("%s (link:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return TS_INVALID; 
	} 
	catch (...) 
	{
		GL_THROW("%s (link:%d): unknown exception", pObj->get_name(), pObj->get_id());
		return TS_INVALID;
	}
}

EXPORT int isa_link(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,link)->isa(classname);
}

/**
* UpdateYVs is called by node functions when their voltage changes to update the "current"
* for future nodes.
*
* @param snode is the object to update
* @param snodesite is the type the calling node is (1=from, 2=to)
* @param deltaV the voltage update to apply to YVs terms
*/
void *link::UpdateYVs(OBJECT *snode, char snodeside, complex *deltaV)
{
	complex YVsNew[3];
	node *worknode = OBJECTDATA(snode,node);

	//Zero the accumulator
	YVsNew[0] = YVsNew[1] = YVsNew[2] = 0.0;

	//Calculate YVs update - gets done regardless of who we are
	if (snodeside==1)		//From node
	{
		if (deltaV[0]!=0.0)	//Non-zero
		{
			YVsNew[0] = To_Y[0][0]*deltaV[0];
			YVsNew[1] = To_Y[1][0]*deltaV[0];
			YVsNew[2] = To_Y[2][0]*deltaV[0];
		}

		if (deltaV[1]!=0.0)	//Non-zero
		{
			YVsNew[0] += To_Y[0][1]*deltaV[1];
			YVsNew[1] += To_Y[1][1]*deltaV[1];
			YVsNew[2] += To_Y[2][1]*deltaV[1];
		}

		if (deltaV[2]!=0.0)	//Non-zero
		{
			YVsNew[0] += To_Y[0][2]*deltaV[2];
			YVsNew[1] += To_Y[1][2]*deltaV[2];
			YVsNew[2] += To_Y[2][2]*deltaV[2];
		}
	}
	else if (snodeside==2)		//To node
	{
		if (deltaV[0]!=0.0)	//Non-zero
		{
			YVsNew[0] = From_Y[0][0]*deltaV[0];
			YVsNew[1] = From_Y[1][0]*deltaV[0];
			YVsNew[2] = From_Y[2][0]*deltaV[0];
		}

		if (deltaV[1]!=0.0)	//Non-zero
		{
			YVsNew[0] += From_Y[0][1]*deltaV[1];
			YVsNew[1] += From_Y[1][1]*deltaV[1];
			YVsNew[2] += From_Y[2][1]*deltaV[1];
		}

		if (deltaV[2]!=0.0)	//Non-zero
		{
			YVsNew[0] += From_Y[0][2]*deltaV[2];
			YVsNew[1] += From_Y[1][2]*deltaV[2];
			YVsNew[2] += From_Y[2][2]*deltaV[2];
		}
	}

	//Check to see if we are a subnode (fixes backpostings)
	if (worknode->SubNode!=1)
	{
		LOCK_OBJECT(snode);
		worknode->YVs[0] += YVsNew[0];
		worknode->YVs[1] += YVsNew[1];
		worknode->YVs[2] += YVsNew[2];
		UNLOCK_OBJECT(snode);
	}
	else	//Child update
	{
		OBJECT *newnode = worknode->SubNodeParent;
		node *newworknode = OBJECTDATA(newnode,node);

		LOCK_OBJECT(newnode);
		newworknode->YVs[0] += YVsNew[0];
		newworknode->YVs[1] += YVsNew[1];
		newworknode->YVs[2] += YVsNew[2];
		UNLOCK_OBJECT(newnode);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// MATRIX OPS FOR LINKS
//////////////////////////////////////////////////////////////////////////

void inverse(complex in[3][3], complex out[3][3])
{
	complex x = complex(1.0) / (in[0][0] * in[1][1] * in[2][2] -
                               in[0][0] * in[1][2] * in[2][1] -
                               in[0][1] * in[1][0] * in[2][2] +
                               in[0][1] * in[1][2] * in[2][0] +
                               in[0][2] * in[1][0] * in[2][1] -
                               in[0][2] * in[1][1] * in[2][0]);

	out[0][0] = x * (in[1][1] * in[2][2] - in[1][2] * in[2][1]);
	out[0][1] = x * (in[0][2] * in[2][1] - in[0][1] * in[2][2]);
	out[0][2] = x * (in[0][1] * in[1][2] - in[0][2] * in[1][1]);
	out[1][0] = x * (in[1][2] * in[2][0] - in[1][0] * in[2][2]);
	out[1][1] = x * (in[0][0] * in[2][2] - in[0][2] * in[2][0]);
	out[1][2] = x * (in[0][2] * in[1][0] - in[0][0] * in[1][2]);
	out[2][0] = x * (in[1][0] * in[2][1] - in[1][1] * in[2][0]);
	out[2][1] = x * (in[0][1] * in[2][0] - in[0][0] * in[2][1]);
	out[2][2] = x * (in[0][0] * in[1][1] - in[0][1] * in[1][0]);
}

void multiply(double a, complex b[3][3], complex c[3][3])
{
	#define MUL(i, j) c[i][j] = b[i][j] * a
	MUL(0, 0); MUL(0, 1); MUL(0, 2);
	MUL(1, 0); MUL(1, 1); MUL(1, 2);
	MUL(2, 0); MUL(2, 1); MUL(2, 2);
	#undef MUL
}

void multiply(complex a[3][3], complex b[3][3], complex c[3][3])
{
	#define MUL(i, j) c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
	MUL(0, 0); MUL(0, 1); MUL(0, 2);
	MUL(1, 0); MUL(1, 1); MUL(1, 2);
	MUL(2, 0); MUL(2, 1); MUL(2, 2);
	#undef MUL
}

void subtract(complex a[3][3], complex b[3][3], complex c[3][3])
{
	#define SUB(i, j) c[i][j] = a[i][j] - b[i][j]
	SUB(0, 0); SUB(0, 1); SUB(0, 2);
	SUB(1, 0); SUB(1, 1); SUB(1, 2);
	SUB(2, 0); SUB(2, 1); SUB(2, 2);
	#undef SUB
}

void addition(complex a[3][3], complex b[3][3], complex c[3][3])
{
	#define ADD(i, j) c[i][j] = a[i][j] + b[i][j]
	ADD(0, 0); ADD(0, 1); ADD(0, 2);
	ADD(1, 0); ADD(1, 1); ADD(1, 2);
	ADD(2, 0); ADD(2, 1); ADD(2, 2);
	#undef ADD
}

void equalm(complex a[3][3], complex b[3][3])
{
	#define MEQ(i, j) b[i][j] = a[i][j]
	MEQ(0, 0); MEQ(0, 1); MEQ(0, 2);
	MEQ(1, 0); MEQ(1, 1); MEQ(1, 2);
	MEQ(2, 0); MEQ(2, 1); MEQ(2, 2);
	#undef MEQ
}
/**@}*/
