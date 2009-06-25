/* $Id
 * Newton-Raphson solver
 */

#include "solver_nr.h"
#include <slu_ddefs.h>

/* access to module global variables */
#include "powerflow.h"

double *deltaI_NR;
Bus_admit *BA_diag; /// BA_diag store the diagonal elements of the bus admittance matrix, the off_diag elements of bus admittance matrix are equal to negative value of branch admittance
Y_NR *Y_offdiag_PQ; //Y_offdiag_PQ store the row,column and value of off_diagonal elements of 6n*6n Y_NR matrix. No PV bus is included.
Y_NR *Y_diag_fixed; //Y_diag_fixed store the row,column and value of fixed diagonal elements of 6n*6n Y_NR matrix. No PV bus is included
complex *Icalc;
//FILE * pFile; ////temporary output file, to be delete

SuperMatrix Test_Matrix;		//Simple initialization just to make sure SuperLU is linking - can be deleted
superlu_options_t test_options; //Simple initialization just to make sure SuperLU is linking - can be deleted

//struct Sample
//{
//int x;
//int y;
//int z;
//}s[4];

//int cmp( const void *a , const void *b )
//{
//struct Sample *c = (Sample *)a;
//struct Sample *d = (Sample *)b;
//if(c->x != d->x) return c->x - d->x;
//else return c->y - d->y;
//}

/** Newton-Raphson solver
	Solves a power flow problem using the Newton-Raphson method
	
	@return n=0 on failure to complete a single iteration, 
	n>0 to indicate success after n interations, or 
	n<0 to indicate failure after n iterations
 **/
int solver_nr(int bus_count, BUSDATA *bus, int branch_count, BRANCHDATA *branch)
{
    set_default_options(&test_options); //Simple test to make sure library linking is working - can be deleted
#ifdef DEBUG
	//Debugger output for NR solver - dumps into MATPOWER friendly format
	FILE *FP = fopen("caseMATPOWEROutput.m","w");
	int indexer, temper;
	char jindex, kindex;
	complex tempP, tempI, tempY;

	//Setup header stuffs
	fprintf(FP,"function [baseMVA, bus, gen, branch] = caseMATPOWEROutput\n");
	fprintf(FP,"%% This is a dump file of all current information about the GridLAB-D file\n");
	fprintf(FP,"%% the Newton-Raphson solver was just implementing.  This file should be\n");
	fprintf(FP,"%% directly executable inside MATPOWER.\n\n");
	fprintf(FP,"%%%%---- Power Flow Data ----%%%%\n");
	if (bus[0].mva_base==-1)
		fprintf(FP,"baseMVA = 0.000001;\n\n");	//Unspecified, so 1 W = 1 W
	else
		fprintf(FP,"baseMVA = %f;\n\n",bus[0].mva_base);	//Assume they'll all be properly based
	
	//Create busdata section
	fprintf(FP,"%%%%bus data\n%%Three separate networks to represent three phases\n");
	fprintf(FP,"%%B and C phases are set as PV generators, may need adjusting\nbus = [\n");
	for (indexer=0; indexer<bus_count; indexer++)
	{
		for (jindex=0; jindex<3; jindex++)
		{
			//Bus number and type - increment by 1 for MATLAB happiness
			temper=jindex*1000+indexer+1;
			if ((bus[indexer].type==2) && (jindex!=0))
				fprintf(FP," %d  %d",temper,(bus[indexer].type));
			else
				fprintf(FP," %d  %d",temper,(bus[indexer].type+1));
			
			//Power demand (load) - assume we're starting at nominal values (MATPOWER has no provisions for impedance or current loads)
			tempP = *bus[indexer].S[jindex];
			tempI = (*bus[indexer].V[jindex])*~(*bus[indexer].I[jindex]);
			tempY = (*bus[indexer].V[jindex])*~((*bus[indexer].V[jindex])*(*bus[indexer].Y[jindex]));

			tempP +=(tempI + tempY);
			fprintf(FP,"  %f  %f",tempP.Re(),tempP.Im());

			//Shunt conductance and susceptance - not used, so 0
			//Also area - set area to jindex (3 areas, each for a phase)
			fprintf(FP,"  0  0  %d",(jindex+1));

			//Voltage - current values
			tempP = *bus[indexer].V[jindex];
			fprintf(FP,"  %f  %f",tempP.Mag(),tempP.Arg()*180/PI);

			//Base kV
			if (bus[indexer].kv_base==-1)
				fprintf(FP,"  0.001");	//Non-based, just put 1 volt
			else
				fprintf(FP,"  %f",bus[indexer].kv_base);

			//Zone - zero us
			fprintf(FP,"  0");

			//Vmax and Vmin - set Vmin to 0 and Vmax to 4x current Vmag
			fprintf(FP,"  %f  0;\n",tempP.Mag()*4.0);
		}
	}
	fprintf(FP,"];\n\n");

	fprintf(FP,"%%%% Branch data\n");
	fprintf(FP,"%% Branches are formed from the diagonal of the admittance matrix.\n%%No easy method for putting the cross-terms in exists for this implementation.\n");
	fprintf(FP,"branch = [\n");
	//dump link data to do the same
	for (indexer=0; indexer<branch_count; indexer++)
	{
		for (jindex=0; jindex<3; jindex++)
		{
			//From/to indexing - increment by 1 for MATLAB happiness
			temper = 1000*jindex+branch[indexer].from+1;
			fprintf(FP,"  %d",temper);
			
			temper = 1000*jindex+branch[indexer].to+1;	//increment by 1 for MATLAB happiness
			fprintf(FP,"  %d",temper);

			//Admittance values (put back into impedance)
			tempY = *branch[indexer].Y[jindex][jindex];
			tempY = complex(1,0)/tempY;
			fprintf(FP,"  %f  %f  0",tempY.Re(),tempY.Im());

			//Rates (line ratings - use defaults)
			fprintf(FP,"  9900  0  0");

			//Voltage ratio
			temper = branch[indexer].v_ratio;
			if (temper==1.0)	//Normal line, encode as 0 (MATPOWER designation)
				fprintf(FP,"  0");
			else
				fprintf(FP,"  %f",temper);

			//Angle and status
			fprintf(FP,"  0  1;\n");
		}
	}
	fprintf(FP,"];\n");

	//Now create a "gen" item - put it on the swing bus
	fprintf(FP,"\n\n");
	fprintf(FP,"%%%% generator data\ngen = [\n");
	
	//Find the swing busses, put a generic generator on them
	for (indexer=0; indexer<bus_count; indexer++)
	{
		if (bus[indexer].type==2)	//Found one
		{
			for (jindex=0; jindex<3; jindex++)	//Create one on each swing bus
			{
				temper = jindex*1000+indexer+1;	//increment by 1 for MATLAB happiness
				fprintf(FP,"  %d  500  -120  Inf  -Inf",temper);
				
				if (bus[indexer].mva_base==-1)
					fprintf(FP,"  100");	//Unspecified, so set to 100 MVA for gens
				else
					fprintf(FP,"  %f",bus[indexer].mva_base);	//Specified

				tempP = *bus[indexer].V[jindex];
				fprintf(FP,"  %f  1  Inf  0;\n",tempP.Mag());
			}
		}
	}
	fprintf(FP,"];\n");

	fclose(FP);
#endif

//pFile = fopen ("myfile.txt","w"); ////////////////////////to be delete
int indexer, jindexer, tempa, tempb;
char jindex, kindex;
 //*Build the Y_NR matrix, the off-diagonal elements are identical to the corresponding elements of bus admittance matrix.
 //The off-diagonal elements of Y_NR marix are not updated at each iteration.*/
    if (BA_diag == NULL)
	{
		BA_diag = new Bus_admit[bus_count];   //BA_diag store the location and value of diagonal elements of Bus Admittance matrix
	}


complex tempY[3][3];	
for (indexer=0; indexer<bus_count; indexer++) // Construct the diagonal elements of Bus admittance matrix.
	{
		for (jindex=0; jindex<3; jindex++)
				{
					for (kindex=0; kindex<3; kindex++)
					{
					BA_diag[indexer].Y[jindex][kindex] = 0;
					tempY[jindex][kindex] = 0;
					}
				}
 

		for (jindexer=0; jindexer<branch_count;jindexer++)
		{ 
			if ( branch[jindexer].from == indexer)
			{ 
				for (jindex=0; jindex<3; jindex++)
				{
					for (kindex=0; kindex<3; kindex++)
					{
						tempY[jindex][kindex] += *branch[jindexer].Y[jindex][kindex];
						//fprintf(pFile,"Admittance of branch %d , %d %d : %4.3f + %4.3f i \n",jindexer,jindex,kindex,(*branch[jindexer].Y[jindex][kindex]).Re(), (*branch[jindexer].Y[jindex][kindex]).Im()); /////////////////////printf to be delete
					}
				}
			}
			else if ( branch[jindexer].to == indexer)
			{ 
				for (jindex=0; jindex<3; jindex++)
				{
					for (kindex=0; kindex<3; kindex++)
					{
						tempY[jindex][kindex] += *branch[jindexer].Y[jindex][kindex];
					}
				}
			}
			else {}
		}
        BA_diag[indexer].col_ind = BA_diag[indexer].row_ind = indexer; // BA_diag store the row and column information of n elements, n = bus_count
		for (jindex=0; jindex<3; jindex++)
				{
					for (kindex=0; kindex<3; kindex++)
					{
						BA_diag[indexer].Y[jindex][kindex] = tempY[jindex][kindex];// BA_diag store the 3*3 complex admittance value of n elements, n = bus_count
					//fprintf(pFile,"BA_diag %d , %d %d : %4.3f + %4.3f i \n",indexer,jindex,kindex,(BA_diag[indexer].Y[jindex][kindex]).Re(), (BA_diag[indexer].Y[jindex][kindex]).Im()); /////////////////////printf to be delete
					}
			
				}
	}

//// Build the off_diagonal_PQ bus elements of 6n*6n Y_NR matrix.Equation (12). All the value in this part will not be updated at each iteration.
int size_offdiag_PQ = 0;
for (jindexer=0; jindexer<branch_count;jindexer++) 
	{
		for (jindex=0; jindex<3; jindex++)
				{
					for (kindex=0; kindex<3; kindex++)
					{
					  tempa  = branch[jindexer].from;
					  tempb  = branch[jindexer].to;
					  if ((*branch[jindexer].Y[jindex][kindex]).Re() != 0 && bus[tempa].type != 1 && bus[tempb].type != 1)  
					  size_offdiag_PQ += 1; 
					  if ((*branch[jindexer].Y[jindex][kindex]).Im() != 0 && bus[tempa].type != 1 && bus[tempb].type != 1) 
					  size_offdiag_PQ += 1; 
					  else {}
					 }
				}
	}

if (Y_offdiag_PQ == NULL)
	{
		Y_offdiag_PQ = new Y_NR[size_offdiag_PQ*4];   //Y_offdiag_PQ store the row,column and value of off_diagonal elements of Bus Admittance matrix in which all the buses are not PV buses. 
	}


indexer = 0;
for (jindexer=0; jindexer<branch_count;jindexer++)
	{ 
		for (jindex=0; jindex<3; jindex++)
			{
				for (kindex=0; kindex<3; kindex++)
					{					
						tempa  = branch[jindexer].from;
					    tempb  = branch[jindexer].to;
						if ((*branch[jindexer].Y[jindex][kindex]).Im() != 0 && bus[tempa].type != 1 && bus[tempb].type != 1)
						{
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].from) + jindex;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].to) + kindex;
								Y_offdiag_PQ[indexer].Y_value = -((*branch[jindexer].Y[jindex][kindex]).Im()); // Note that off diagonal elements of bus admittance matrix is equal to the negative value of corresponding branch admittance.
								indexer += 1;
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].from) + jindex +3;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].to) + kindex +3;
								Y_offdiag_PQ[indexer].Y_value = (*branch[jindexer].Y[jindex][kindex]).Im();
								indexer += 1;
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].to) + jindex;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].from) + kindex;
								Y_offdiag_PQ[indexer].Y_value = -((*branch[jindexer].Y[jindex][kindex]).Im());
								indexer += 1;
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].to) + jindex +3;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].from) + kindex +3;
								Y_offdiag_PQ[indexer].Y_value = (*branch[jindexer].Y[jindex][kindex]).Im();
								indexer += 1;
						}
						if ((*branch[jindexer].Y[jindex][kindex]).Re() != 0 && bus[tempa].type != 1 && bus[tempb].type != 1)
						{
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].from) + jindex + 3;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].to) + kindex;
								Y_offdiag_PQ[indexer].Y_value = -((*branch[jindexer].Y[jindex][kindex]).Re());
								indexer += 1;
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].from) + jindex;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].to) + kindex +3;
								Y_offdiag_PQ[indexer].Y_value = -((*branch[jindexer].Y[jindex][kindex]).Re());
								indexer += 1;	
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].to) + jindex + 3;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].from) + kindex;
								Y_offdiag_PQ[indexer].Y_value = -((*branch[jindexer].Y[jindex][kindex]).Re());
								indexer += 1;
								Y_offdiag_PQ[indexer].row_ind = 6*(branch[jindexer].to) + jindex;
						        Y_offdiag_PQ[indexer].col_ind = 6*(branch[jindexer].from) + kindex +3;
								Y_offdiag_PQ[indexer].Y_value = -((*branch[jindexer].Y[jindex][kindex]).Re());
								indexer += 1;
						}
				}
			}
	}

//Build the fixed part of the diagonal PQ bus elements of 6n*6n Y_NR matrix. This part will not be updated at each iteration. 
int size_diag_fixed = 0;
for (jindexer=0; jindexer<bus_count;jindexer++) 
	{
		for (jindex=0; jindex<3; jindex++)
				{
					for (kindex=0; kindex<3; kindex++)
					{		 
					  if ((BA_diag[jindexer].Y[jindex][kindex]).Re() != 0 && bus[jindexer].type != 1 && jindex!=kindex)  
					  size_diag_fixed += 1; 
					  if ((BA_diag[jindexer].Y[jindex][kindex]).Im() != 0 && bus[jindexer].type != 1 && jindex!=kindex) 
					  size_diag_fixed += 1; 
					  else {}
					 }
				}
	}
if (Y_diag_fixed == NULL)
	{
		Y_diag_fixed = new Y_NR[size_diag_fixed*2];   //Y_diag_fixed store the row,column and value of the fixed part of the diagonal PQ bus elements of 6n*6n Y_NR matrix.
	}
indexer = 0;
for (jindexer=0; jindexer<bus_count;jindexer++)
	{ 
		for (jindex=0; jindex<3; jindex++)
			{
				for (kindex=0; kindex<3; kindex++)
					{					
							if ((BA_diag[jindexer].Y[jindex][kindex]).Im() != 0 && bus[jindexer].type != 1 && jindex!=kindex)
								{
									Y_diag_fixed[indexer].row_ind = 6*jindexer + jindex;
									Y_diag_fixed[indexer].col_ind = 6*jindexer + kindex;
									Y_diag_fixed[indexer].Y_value = (BA_diag[jindexer].Y[jindex][kindex]).Im();
									indexer += 1;
									Y_diag_fixed[indexer].row_ind = 6*jindexer + jindex +3;
									Y_diag_fixed[indexer].col_ind = 6*jindexer + kindex +3;
									Y_diag_fixed[indexer].Y_value = (BA_diag[jindexer].Y[jindex][kindex]).Im();
									indexer += 1;
								}
							if ((BA_diag[jindexer].Y[jindex][kindex]).Re() != 0 && bus[jindexer].type != 1 && jindex!=kindex)
								{
									Y_diag_fixed[indexer].row_ind = 6*jindexer + jindex;
									Y_diag_fixed[indexer].col_ind = 6*jindexer + kindex +3;
									Y_diag_fixed[indexer].Y_value = (BA_diag[jindexer].Y[jindex][kindex]).Re();
									indexer += 1;
									Y_diag_fixed[indexer].row_ind = 6*jindexer + jindex +3;
									Y_diag_fixed[indexer].col_ind = 6*jindexer + kindex;
									Y_diag_fixed[indexer].Y_value = (BA_diag[jindexer].Y[jindex][kindex]).Re();
									indexer += 1;
								}
					}
			}
	}


//Build the dynamic diagnal elements of 6n*6n Y matrix. All the elements in this part will be updated at each iteration.





//////////////////////////////////////test printing
//fprintf(pFile,"off diagnal elementes of 6n*6n Y matrix \n");
//for (indexer=0; indexer<size_offdiag_PQ*4;indexer++)
//	{
//		fprintf(pFile,"row %d , column %d , Value: %4.5f \n",Y_offdiag_PQ[indexer].row_ind ,Y_offdiag_PQ[indexer].col_ind,Y_offdiag_PQ[indexer].Y_value); /////////////////////printf to be delete
//	}
//fprintf(pFile,"fixed diagnal elementes of 6n*6n Y matrix \n");
//for (indexer=0; indexer<size_diag_fixed*2;indexer++)
//	{
//		fprintf(pFile,"row %d , column %d , Value: %4.5f \n",Y_diag_fixed[indexer].row_ind ,Y_diag_fixed[indexer].col_ind,Y_diag_fixed[indexer].Y_value); /////////////////////printf to be delete
//	}


//System load at each bus is represented by second order polynomial equations
	complex tempP;
	for (indexer=0; indexer<bus_count; indexer++)
		{
			tempP = complex(); //tempP storea the temporary value of Power load at each bus  
			for (jindex=0; jindex<3; jindex++)
			{
				tempP = *bus[indexer].S[jindex];									//Constant power portion
				tempP += *bus[indexer].V[jindex]*(~(*bus[indexer].I[jindex]));	//Constant current portion
				tempP += *bus[indexer].V[jindex]*(~(*bus[indexer].V[jindex]*(*bus[indexer].Y[jindex])));	//Constant impedance portion

				bus[indexer].PL[jindex] = tempP.Re();	//Real power portion
				bus[indexer].QL[jindex] = tempP.Im();	//Reactive power portion  
			


			}
	}

// Calculate the mismatch of three phase current injection at each bus (deltaI), 
//and store the deltaI in terms of real and reactive value in array deltaI_NR    
	if (deltaI_NR==NULL)
	{
		deltaI_NR = new double[6*bus_count];   // left_hand side of equation (11)
	}

	if (Icalc==NULL)
	{
		Icalc = new complex[bus_count];  // Calculated current injections at each bus is stored in Icalc for each iteration 
	}

	complex tempDI; //tempDI store the temporary value of deltaI at each bus  
	complex tempIcalc; // tempIcalc store the temporary calculated value of current injection at bus k equation(1)  
	double tempPbus; //tempPbus store the temporary value of active power at each bus
	double tempQbus; //tempQbus store the temporary value of reactive power at each bus
	int tempbus, tempPhase; 
	for (indexer=0; indexer<bus_count; indexer++)
	{
		tempIcalc = NULL;
		tempbus=indexer;	
		for (jindex=0; jindex<3; jindex++)
			{
					tempPbus = - bus[indexer].PL[jindex];	// @@@ PG and QG is assumed to be zero here @@@
					tempQbus = - bus[indexer].QL[jindex];	
					tempPhase = jindex;

					for (jindexer=0; jindexer<branch_count; jindexer++)
					{
							if (branch[jindexer].from == tempbus) 
								for (kindex=0; kindex<3; kindex++)
						             {
								     tempIcalc += (*branch[jindexer].Y[tempPhase][kindex]) * (*bus[branch[jindexer].to].V[kindex]);
								     }
							else if  (branch[indexer].to == tempbus)
                                for (kindex=0; kindex<3; kindex++)
						            {
								     tempIcalc += (*branch[jindexer].Y[tempPhase][kindex]) * (*bus[branch[jindexer].from].V[kindex]);
								     }
							else {}
					}				
					Icalc[tempbus] = tempIcalc; // calculated current injection  				
					tempDI =  (~complex(tempPbus, tempQbus))/(~(*bus[indexer].V[jindex])) - tempIcalc;
                   	deltaI_NR[tempbus*3+3 + tempPhase] = tempDI.Re(); // Real part of deltaI, left hand side of equation (11)
                    deltaI_NR[tempbus*3 + tempPhase] = tempDI.Im(); // Imaginary part of deltaI, left hand side of equation (11)

			}
	}

/* sorting integers using qsort() example */

//qsort(s,4,sizeof(s[0]),cmp);


//fclose (pFile); //////////////////////////////////////// to be delete

	/// @todo implement NR method
	GL_THROW("Newton-Raphson solution method is not yet supported");
	return 0;
}
