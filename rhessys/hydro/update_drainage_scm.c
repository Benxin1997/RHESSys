/*--------------------------------------------------------------*/
/* 											*/
/*					update_drainage_land			*/
/*											*/
/*	update_drainage_scm.c - creates a patch object				*/
/*											*/
/*	NAME										*/
/*	update_drainage_scm.c - creates a patch object				*/
/*											*/
/*	SYNOPSIS									*/
/*	void update_drainage_scm( 							*/
/*				  struct zone_object *zone,
 /*                   struct patch_object *patch,			*/
/*				 			double,			 	*/
/*				 			double,			 	*/
/*				 			double,			 	*/
/*							int,				*/
/*							int)				*/
/*											*/
/* 											*/
/*											*/
/*	OPTIONS										*/
/*											*/
/*											*/
/*	DESCRIPTION									*/
/*											*/
/*											*/
/*											*/
/*											*/
/*	PROGRAMMER NOTES								*/
/*											*/
/*											*/
/*--------------------------------------------------------------*/
#include <stdio.h>
#include "rhessys.h"


void  update_drainage_scm(
					struct zone_object *zone,
                         struct patch_object *patch,
                         struct command_line_object *command_line,
					double time_int,
					int verbose_flag,
                         struct date current_date)
{
	/*--------------------------------------------------------------*/
	/*	Local function definition.				*/
	/*--------------------------------------------------------------*/
	double  compute_delta_water(
		int,
		double,
		double,
		double,
		double,
		double);
	
	double compute_varbased_returnflow(
		double,
		double,
		double,
		struct litter_object *);


	double compute_varbased_flow(
		int,
		double,
		double,
		double,
		double,
		double *);


	double compute_N_leached(int,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double *);
	
	double recompute_gamma(	
		struct patch_object *,
		double);


	double compute_infiltration( int,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double,
		double);

	/*--------------------------------------------------------------*/
	/*	Local variable definition.				*/
	/*--------------------------------------------------------------*/
	int k, j, d, idx, avek, mins, intused;
	double step; /* minutes */
	double m, Ksat, std_scale;
	double NH4_leached_to_patch, NH4_leached_to_stream;
	double NO3_leached_to_patch, NO3_leached_to_stream;
	double DON_leached_to_patch, DON_leached_to_stream;
	double DOC_leached_to_patch, DOC_leached_to_stream;
	double NO3_leached_to_surface; /* kg/m2 */
	double NH4_leached_to_surface; /* kg/m2 */
	double DON_leached_to_surface; /* kg/m2 */
	double DOC_leached_to_surface; /* kg/m2 */
	double N_leached_total; /* kg/m2 */
	double DON_leached_total; /* kg/m2 */
	double DOC_leached_total; /* kg/m2 */
	double route_to_surface;  /* m3 */
	double return_flow,route_to_patch ;  /* m3 */
	double available_sat_water; /* m3 */
	double Qin, Qout;  /* m */
	double innundation_depth, infiltration; /* m */
	double total_gamma;
	double Nin, Nout; /* kg/m2 */ 
	double t1,t2,t3;
     double scm_ave_height; /* m */
     double scm_max_H; /* m */
     double scm_max_area, scm_max_volume; /* m^2 | m^3 */
     double out_hi, out_low, h_hi, h_low, vol_hi, vol_low, LHS, LHS_delta; /* m | m | m/time int | m/time int | m/minute | m/minute */
     double preday_scm_volume, preday_scm_inflow, preday_scm_outflow;
     double scm_H, scm_outflow, scm_outflow_tmp, scm_volume; /* m | m^3/s | m */

	struct patch_object *neigh;
	route_to_patch = 0.0;
	route_to_surface = 0.0;
	return_flow=0.0;

	DON_leached_to_patch = 0.0;
	DON_leached_to_stream = 0.0;
	DOC_leached_to_patch = 0.0;
	DOC_leached_to_stream = 0.0;
	NH4_leached_to_patch = 0.0;
	NH4_leached_to_stream = 0.0;
	NO3_leached_to_patch = 0.0;
	NO3_leached_to_stream = 0.0;
	NO3_leached_to_surface = 0.0;
	NH4_leached_to_surface = 0.0;
	DOC_leached_to_surface = 0.0;
	DON_leached_to_surface = 0.0;
     
     // Patch SCM varibles
     scm_max_area       = patch[0].area;
     scm_max_volume     = patch[0].scm_stage_storage[1000][1];
     scm_max_H          = patch[0].scm_stage_storage[1000][0];
     
     // Write over soil default detention storage
     patch[0].soil_defaults[0][0].detention_store_size = patch[0].scm_stage_storage[1000][6];
	/*--------------------------------------------------------------*/
	/*	m and K are multiplied by sensitivity analysis variables */
	/*--------------------------------------------------------------*/

	m = patch[0].m ;
	Ksat = patch[0].soil_defaults[0][0].Ksat_0 ;
	d=0;

	/*--------------------------------------------------------------*/
	/*	recalculate gamma based on current saturation deficits  */
	/*      to account the effect of changes in water table slope 	*/
	/*--------------------------------------------------------------*/
	total_gamma = recompute_gamma(patch, patch[0].innundation_list[d].gamma);

	available_sat_water = max(((patch[0].soil_defaults[0][0].soil_water_cap
			- max(patch[0].sat_deficit,0.0))
			* patch[0].area),0.0);

	/*------------------------------------------------------------*/
	/*	calculate amuount of water output to patches			*/
	/*	this only computes subsurface flow, not overland flow	*/
	/*-----------------------------------------------------------*/

	std_scale = command_line[0].std_scale;

	route_to_patch =  time_int * compute_varbased_flow(
		patch[0].num_soil_intervals,
		patch[0].std * std_scale, 
		patch[0].sat_deficit,
		total_gamma, 
		patch[0].soil_defaults[0][0].interval_size,
		patch[0].transmissivity_profile);

	if (route_to_patch < 0.0) route_to_patch = 0.0;
	if ( route_to_patch > available_sat_water) 
		route_to_patch *= (available_sat_water)/(route_to_patch);
	/*--------------------------------------------------------------*/
	/* compute Nitrogen leaching amount				*/
	/*--------------------------------------------------------------*/
	if (command_line[0].grow_flag > 0) {
		Nout = compute_N_leached(
			verbose_flag,
			patch[0].soil_ns.nitrate,
			route_to_patch / patch[0].area,
			patch[0].sat_deficit,
			patch[0].soil_defaults[0][0].soil_water_cap,
			m,
			total_gamma / patch[0].area * time_int,
			patch[0].soil_defaults[0][0].porosity_0,
			patch[0].soil_defaults[0][0].porosity_decay,
			patch[0].soil_defaults[0][0].N_decay_rate,
			patch[0].soil_defaults[0][0].active_zone_z,
			patch[0].soil_defaults[0][0].soil_depth,
			patch[0].soil_defaults[0][0].NO3_adsorption_rate,
			patch[0].transmissivity_profile);
		NO3_leached_to_patch = Nout * patch[0].area;
		patch[0].soil_ns.NO3_Qout += Nout;


		Nout = compute_N_leached(
			verbose_flag,
			patch[0].soil_ns.sminn,
			route_to_patch / patch[0].area,
			patch[0].sat_deficit,
			patch[0].soil_defaults[0][0].soil_water_cap,
			m,
			total_gamma / patch[0].area * time_int,
			patch[0].soil_defaults[0][0].porosity_0,
			patch[0].soil_defaults[0][0].porosity_decay,
			patch[0].soil_defaults[0][0].N_decay_rate,
			patch[0].soil_defaults[0][0].active_zone_z,
			patch[0].soil_defaults[0][0].soil_depth,
			patch[0].soil_defaults[0][0].NH4_adsorption_rate,
			patch[0].transmissivity_profile);
		NH4_leached_to_patch = Nout * patch[0].area;
		patch[0].soil_ns.NH4_Qout += Nout;

		Nout = compute_N_leached(
			verbose_flag,
			patch[0].soil_ns.DON,
			route_to_patch / patch[0].area,
			patch[0].sat_deficit,
			patch[0].soil_defaults[0][0].soil_water_cap,
			m,
			total_gamma / patch[0].area * time_int,
			patch[0].soil_defaults[0][0].porosity_0,
			patch[0].soil_defaults[0][0].porosity_decay,
			patch[0].soil_defaults[0][0].DOM_decay_rate,
			patch[0].soil_defaults[0][0].active_zone_z,
			patch[0].soil_defaults[0][0].soil_depth,
			patch[0].soil_defaults[0][0].DON_adsorption_rate,
			patch[0].transmissivity_profile);
		DON_leached_to_patch = Nout * patch[0].area;
		patch[0].soil_ns.DON_Qout += Nout;

		Nout = compute_N_leached(
			verbose_flag,
			patch[0].soil_cs.DOC,
			route_to_patch / patch[0].area,
			patch[0].sat_deficit,
			patch[0].soil_defaults[0][0].soil_water_cap,
			m,
			total_gamma / patch[0].area * time_int,
			patch[0].soil_defaults[0][0].porosity_0,
			patch[0].soil_defaults[0][0].porosity_decay,
			patch[0].soil_defaults[0][0].DOM_decay_rate,
			patch[0].soil_defaults[0][0].active_zone_z,
			patch[0].soil_defaults[0][0].soil_depth,
			patch[0].soil_defaults[0][0].DOC_adsorption_rate,
			patch[0].transmissivity_profile);
		DOC_leached_to_patch = Nout * patch[0].area;
		patch[0].soil_cs.DOC_Qout += Nout;


	}

	
	patch[0].Qout += (route_to_patch / patch[0].area);


	/*--------------------------------------------------------------*/
	/*	calculate any return flow associated with this patch	*/
	/*	and route any infiltration excess			*/
	/*	return flow is flow leaving patch (i.e surface_Qout)  	*/
	/*	note that return flow that becomes detention storage   */
	/*	is added to surface_Qin					*/
	/*	similarly with associated nitrogen			*/
	/* 	note we move unsat_storage into saturated storage in this case */
	/*	saturated zone will be updated in compute_subsurface_routing	*/
	/*	i.e becomes part of Qout				*/
	/*--------------------------------------------------------------*/
	if ((patch[0].sat_deficit-patch[0].rz_storage-patch[0].unsat_storage) < -1.0*ZERO) {
		return_flow = compute_varbased_returnflow(patch[0].std * std_scale, 
			patch[0].rz_storage+patch[0].unsat_storage,
			patch[0].sat_deficit, &(patch[0].litter));
		patch[0].detention_store += return_flow;  
		patch[0].sat_deficit += (return_flow - (patch[0].unsat_storage+patch[0].rz_storage));
		patch[0].unsat_storage = 0.0;
		patch[0].rz_storage = 0.0;
	}
	/*--------------------------------------------------------------*/
	/*	calculated any N-transport associated with return flow  */
	/*	-note available N reduced by what has already been 	*/
	/*	we assume that only nitrate follows return flow		*/
	/*	lost in subsurface flow routing				*/
	/*--------------------------------------------------------------*/
		if (command_line[0].grow_flag > 0) {
			Nout = compute_N_leached(
				verbose_flag,
				patch[0].soil_ns.nitrate - (NO3_leached_to_patch/patch[0].area),
				return_flow,
				0.0,
				0.0,
				m,
				total_gamma / patch[0].area * time_int,
				patch[0].soil_defaults[0][0].porosity_0,
				patch[0].soil_defaults[0][0].porosity_decay,
				patch[0].soil_defaults[0][0].N_decay_rate,
				patch[0].soil_defaults[0][0].active_zone_z,
				patch[0].soil_defaults[0][0].soil_depth,
				patch[0].soil_defaults[0][0].NO3_adsorption_rate,
				patch[0].transmissivity_profile);
			patch[0].surface_NO3 += Nout;
			patch[0].soil_ns.NO3_Qout += Nout;

			Nout = compute_N_leached(
				verbose_flag,
				patch[0].soil_ns.sminn - (NH4_leached_to_patch/patch[0].area),
				return_flow,
				0.0,
				0.0,
				m,
				total_gamma / patch[0].area * time_int,
				patch[0].soil_defaults[0][0].porosity_0,
				patch[0].soil_defaults[0][0].porosity_decay,
				patch[0].soil_defaults[0][0].N_decay_rate,
				patch[0].soil_defaults[0][0].active_zone_z,
				patch[0].soil_defaults[0][0].soil_depth,
				patch[0].soil_defaults[0][0].NH4_adsorption_rate,
				patch[0].transmissivity_profile);
			patch[0].surface_NH4 += Nout;
			patch[0].soil_ns.NH4_Qout += Nout;


			Nout = compute_N_leached(
				verbose_flag,
				patch[0].soil_ns.DON - (DON_leached_to_patch/patch[0].area),
				return_flow,
				0.0,
				0.0,
				m,
				total_gamma / patch[0].area * time_int,
				patch[0].soil_defaults[0][0].porosity_0,
				patch[0].soil_defaults[0][0].porosity_decay,
				patch[0].soil_defaults[0][0].DOM_decay_rate,
				patch[0].soil_defaults[0][0].active_zone_z,
				patch[0].soil_defaults[0][0].soil_depth,
				patch[0].soil_defaults[0][0].DON_adsorption_rate,
				patch[0].transmissivity_profile);
			patch[0].surface_DON += Nout;
			patch[0].soil_ns.DON_Qout += Nout;

			Nout = compute_N_leached(
				verbose_flag,
				patch[0].soil_cs.DOC - (DOC_leached_to_patch/patch[0].area),
				return_flow,
				0.0,
				0.0,
				m,
				total_gamma / patch[0].area * time_int,
				patch[0].soil_defaults[0][0].porosity_0,
				patch[0].soil_defaults[0][0].porosity_decay,
				patch[0].soil_defaults[0][0].DOM_decay_rate,
				patch[0].soil_defaults[0][0].active_zone_z,
				patch[0].soil_defaults[0][0].soil_depth,
				patch[0].soil_defaults[0][0].DOC_adsorption_rate,
				patch[0].transmissivity_profile);
			patch[0].surface_DOC += Nout;
			patch[0].soil_cs.DOC_Qout += Nout;
		}
	
     

	/*--------------------------------------------------------------*/
	/*	route water and nitrogen lossed due to surface routing    */
	/*--------------------------------------------------------------*/
	
     /*-----------------------------------------------------------*/
     /* Need to compute total outflow at a sub-houlry time step   */
     /* Use one minute                                            */
     /* Assume inflow in linearaly distributed across the time    */
     /*-----------------------------------------------------------*/
     preday_scm_volume  = patch[0].preday_detention_store;
     preday_scm_inflow  = patch[0].preday_scm_inflow;
     preday_scm_outflow = patch[0].preday_scm_outflow;

 
     /*-----------------------------------------------------------*/
     /* A) IF HOURLY                                              */
     /*-----------------------------------------------------------*/
     
     if (zone[0].hourly_rain_flag!=1) {
          intused = 4;
          step = 60;
     } else {
          intused = 5;
          step = 60*24;
     }

     /*-----------------------------------------------------------*/
     /* SUB-HOURLY FOR LOOP */
     /*-----------------------------------------------------------*/
     scm_outflow = 0;

     for (mins=0; mins<step; mins++){
          
         scm_outflow_tmp = 0;
         
         // Calculate LHS in m/minute
         // LHS = Ii + Ij + (2*Si/t) - Oi        [t=1 minute]
         LHS = preday_scm_inflow/step + patch[0].Qin_total/step + (2*preday_scm_volume)/1 - preday_scm_outflow/step;
         /*-----------------------------------------------------------*/
         /* calculate amuount of water output to stream (SCM routing) */
         /*-----------------------------------------------------------*/

         /* for loop to lookup scm depth and outflow based on LHS */
         for (k=0; k<1001; k++) {
               // Check to see if LHS is above max
               if (LHS >= patch[0].scm_stage_storage[1000][9]) {
                    scm_outflow_tmp  = (scm_volume - patch[0].scm_stage_storage[1000][6])/patch[0].area; // immediately route excess volume to stream
                    scm_outflow_tmp += (patch[0].scm_stage_storage[1000][intused]/step) + patch[0].Qin_total/step ;  // set outflow rate to that of the maximum depth water and short-circuit extra water out
                    scm_H = scm_max_H;             // set water level to maximum
                    avek = 1000;
                    break;
               } else if (LHS >= patch[0].scm_stage_storage[k][9] && LHS < patch[0].scm_stage_storage[k+1][9]) {
                    h_low = patch[0].scm_stage_storage[k][0];
                    h_hi  = patch[0].scm_stage_storage[k+1][0];
                    out_low = patch[0].scm_stage_storage[k][intused]/step;
                    out_hi  = patch[0].scm_stage_storage[k+1][intused]/step;
                    vol_low = patch[0].scm_stage_storage[k][6];
                    vol_hi  = patch[0].scm_stage_storage[k+1][6];
                    
                    LHS_delta = (LHS - patch[0].scm_stage_storage[k][9])/(patch[0].scm_stage_storage[k+1][9]-patch[0].scm_stage_storage[k][9]);
                    scm_H = h_low + (h_hi-h_low)*LHS_delta;
                    scm_outflow_tmp = out_low + (out_hi-out_low)*LHS_delta;
                    //scm_volume = vol_low + (vol_hi-vol_low)*LHS_delta;
                    avek = k;
                    break;
               } else {
                    
               }
             
         }
         
         scm_outflow_tmp    = max(scm_outflow_tmp,0);
         scm_volume         = preday_scm_volume - scm_outflow_tmp + patch[0].Qin_total/step;
         
         if(patch[0].ID == 1){
               fprintf(stderr, "\npre-I: %f | pre-volume: %f | volume: %f | pre-height: %f | O: %f  | LHS %f | avek: %d", preday_scm_inflow/step, preday_scm_volume, scm_volume, scm_H, scm_outflow_tmp, LHS, avek);
         }
          
         preday_scm_volume  = scm_volume;
         preday_scm_outflow = scm_outflow_tmp;
         preday_scm_inflow  = patch[0].Qin_total; // after the first step, preday inflow is just the inflow
         scm_outflow       += scm_outflow_tmp;
 
     }

     Qout = scm_outflow;
     patch[0].scm_H = scm_H;

    
     if (command_line[0].grow_flag > 0) {
          Nout = (min(1.0, (Qout/ patch[0].detention_store))) * patch[0].surface_DOC;
          DOC_leached_to_surface = Nout * patch[0].area;
          patch[0].surface_DOC -= Nout;
          Nout = (min(1.0, (Qout/ patch[0].detention_store))) * patch[0].surface_DON;
          DON_leached_to_surface = Nout * patch[0].area;
          patch[0].surface_DON -= Nout;
          Nout = (min(1.0, (Qout/ patch[0].detention_store))) * patch[0].surface_NO3;
          NO3_leached_to_surface = Nout * patch[0].area;
          patch[0].surface_NO3 -= Nout;
          Nout = (min(1.0, (Qout/ patch[0].detention_store))) * patch[0].surface_NH4;
          NH4_leached_to_surface = Nout * patch[0].area;
          patch[0].surface_NH4 -= Nout;
     }

     //patch[0].detention_store -= Qout;  // IS THIS DONE SOMEWHERE ELSE AT THE END OF THE HOUR?
     patch[0].detention_store = preday_scm_volume;
     patch[0].surface_Qout = Qout;
     route_to_surface = Qout * patch[0].area;
     /*if(patch[0].ID == 1){
     fprintf(stderr, "\nTEST: LHS: %f | k: %d | hr: %d", LHS, avek, current_date.hour);
     }*/
     
     if(patch[0].ID ==1){
          fprintf(stderr, "\nS0: %f | S1: %f | I0: %f | I1: %f | Surf. O: %f  |  SubSurf O: %f  |  Qout T: %f | hour: %d | ET: %f", patch[0].preday_detention_store, patch[0].detention_store, patch[0].preday_scm_inflow, patch[0].Qin_total, Qout, patch[0].Qout, (patch[0].Qout_total), current_date.hour, (patch[0].evaporation + patch[0].evaporation_surf  + patch[0].transpiration_unsat_zone + patch[0].transpiration_sat_zone));
     }
     
     if(avek > 0){
     patch[0].scm_ave_height = (patch[0].detention_store*patch[0].area)/((patch[0].scm_stage_storage[0][3]+patch[0].scm_stage_storage[avek][3])/2);
     } else {
     patch[0].scm_ave_height = 0;
     }


     patch[0].preday_scm_inflow  = patch[0].Qin_total;
     patch[0].preday_scm_volume  = preday_scm_volume;
     patch[0].preday_scm_outflow = preday_scm_outflow;

  	if (NO3_leached_to_surface < 0.0)
		printf("WARNING %d %lf",patch[0].ID, NO3_leached_to_surface);


	/*--------------------------------------------------------------*/
	/*	route flow to neighbours				*/
	/*	route n_leaching if grow flag specfied			*/
	/*--------------------------------------------------------------*/

	/*--------------------------------------------------------------*/
	/* regular downslope routing */
	/*--------------------------------------------------------------*/
	if (command_line[0].noredist_flag == 0) {
	d=0;
	for (j = 0; j < patch[0].innundation_list[d].num_neighbours; j++) {
		neigh = patch[0].innundation_list[d].neighbours[j].patch;  
		/*--------------------------------------------------------------*/
		/* first transfer subsurface water and nitrogen */
		/*--------------------------------------------------------------*/
		Qin =	(patch[0].innundation_list[d].neighbours[j].gamma * route_to_patch) / neigh[0].area;
		if (Qin < 0) printf("\n warning negative routing from patch %d with gamma %lf", patch[0].ID, total_gamma);
		if (command_line[0].grow_flag > 0) {
			Nin = (patch[0].innundation_list[d].neighbours[j].gamma * DON_leached_to_patch) 
				/ neigh[0].area;
			neigh[0].soil_ns.DON_Qin += Nin;
			Nin = (patch[0].innundation_list[d].neighbours[j].gamma * DOC_leached_to_patch) 
				/ neigh[0].area;
			neigh[0].soil_cs.DOC_Qin += Nin;
			Nin = (patch[0].innundation_list[d].neighbours[j].gamma * NO3_leached_to_patch) 
				/ neigh[0].area;
			neigh[0].soil_ns.NO3_Qin += Nin;
			Nin = (patch[0].innundation_list[d].neighbours[j].gamma * NH4_leached_to_patch) 
				/ neigh[0].area;
			neigh[0].soil_ns.NH4_Qin += Nin;
			}
		neigh[0].Qin += Qin;
	}

	/*--------------------------------------------------------------*/
	/* surface downslope routing */
	/*--------------------------------------------------------------*/
	/*--------------------------------------------------------------*/
	/* determine which innundation depth to consider		*/
	/*--------------------------------------------------------------*/
	if (patch[0].num_innundation_depths > 0) {
		innundation_depth = patch[0].detention_store + route_to_surface/patch[0].area; 
		d=0;
		while ((innundation_depth > patch[0].innundation_list[d].critical_depth) 
			&& (d < patch[0].num_innundation_depths-1)) {
			d++;}
		}
	else d=0;
	for (j = 0; j < patch[0].surface_innundation_list[d].num_neighbours; j++) {

		neigh = patch[0].surface_innundation_list[d].neighbours[j].patch;

		/*--------------------------------------------------------------*/
		/* now transfer surface water and nitrogen */
		/*	- first nitrogen					*/
		/*--------------------------------------------------------------*/
		if (command_line[0].grow_flag > 0) {
			Nin = (patch[0].surface_innundation_list[d].neighbours[j].gamma * NO3_leached_to_surface) / neigh[0].area;
			neigh[0].surface_NO3 += Nin;
			Nin = (patch[0].surface_innundation_list[d].neighbours[j].gamma * NH4_leached_to_surface) / neigh[0].area;
			neigh[0].surface_NH4 += Nin;
			Nin = (patch[0].surface_innundation_list[d].neighbours[j].gamma * DON_leached_to_surface) / neigh[0].area;
			neigh[0].surface_DON += Nin;
			Nin = (patch[0].surface_innundation_list[d].neighbours[j].gamma * DOC_leached_to_surface) / neigh[0].area;
			neigh[0].surface_DOC += Nin;
			}
		
		/*--------------------------------------------------------------*/
		/*	- now surface water 					*/
		/*	surface stores should be updated to facilitate transfer */
		/* added net surface water transfer to detention store		*/
		/*--------------------------------------------------------------*/

		Qin = (patch[0].surface_innundation_list[d].neighbours[j].gamma * route_to_surface) / neigh[0].area;
		neigh[0].detention_store += Qin;
		neigh[0].surface_Qin += Qin;
		
		/*--------------------------------------------------------------*/
		/* try to infiltrate this water					*/ 
		/* use time_int as duration */
		/*--------------------------------------------------------------*/
		if (neigh[0].detention_store > ZERO) {
			if (neigh[0].rootzone.depth > ZERO) {
			infiltration = compute_infiltration(
				verbose_flag,
				neigh[0].sat_deficit_z,
				neigh[0].rootzone.S,
				neigh[0].Ksat_vertical,
				neigh[0].soil_defaults[0][0].Ksat_0_v,
				neigh[0].soil_defaults[0][0].mz_v,
				neigh[0].soil_defaults[0][0].porosity_0,
				neigh[0].soil_defaults[0][0].porosity_decay,
				(neigh[0].detention_store),	
				time_int,
				neigh[0].soil_defaults[0][0].psi_air_entry);
			}
			else {
			infiltration = compute_infiltration(
				verbose_flag,
				neigh[0].sat_deficit_z,
				neigh[0].S,
				neigh[0].Ksat_vertical,
				neigh[0].soil_defaults[0][0].Ksat_0_v,
				neigh[0].soil_defaults[0][0].mz_v,
				neigh[0].soil_defaults[0][0].porosity_0,
				neigh[0].soil_defaults[0][0].porosity_decay,
				(neigh[0].detention_store),	
				time_int,
				neigh[0].soil_defaults[0][0].psi_air_entry);
			}
		}
		else infiltration = 0.0;
		/*--------------------------------------------------------------*/
		/* added an surface N flux to surface N pool	and		*/
		/* allow infiltration of surface N				*/
		/*--------------------------------------------------------------*/
		if ((command_line[0].grow_flag > 0 ) && (infiltration > ZERO)) {
			neigh[0].soil_cs.DOC_Qin += ((infiltration / neigh[0].detention_store) * neigh[0].surface_DOC);
			neigh[0].surface_DOC -= ((infiltration / neigh[0].detention_store) * neigh[0].surface_DOC);
			neigh[0].soil_ns.DON_Qin += ((infiltration / neigh[0].detention_store) * neigh[0].surface_DON);
			neigh[0].surface_DON -= ((infiltration / neigh[0].detention_store) * neigh[0].surface_DON);
			neigh[0].soil_ns.NO3_Qin += ((infiltration / neigh[0].detention_store) * neigh[0].surface_NO3);
			neigh[0].surface_NO3 -= ((infiltration / neigh[0].detention_store) * neigh[0].surface_NO3);
			neigh[0].soil_ns.NH4_Qin += ((infiltration / neigh[0].detention_store) * neigh[0].surface_NH4);
			neigh[0].surface_NH4 -= ((infiltration / neigh[0].detention_store) * neigh[0].surface_NH4);
		}

		if (infiltration > neigh[0].sat_deficit - neigh[0].unsat_storage - neigh[0].rz_storage) {
			neigh[0].sat_deficit -= (infiltration + neigh[0].unsat_storage + neigh[0].rz_storage);
			neigh[0].unsat_storage = 0.0; 
			neigh[0].rz_storage = 0.0; 
			neigh[0].field_capacity = 0.0; 
			neigh[0].rootzone.field_capacity = 0.0; 
		}

		else if ((neigh[0].sat_deficit > neigh[0].rootzone.potential_sat) &&
			(infiltration > neigh[0].rootzone.potential_sat - neigh[0].rz_storage)) {
		/*------------------------------------------------------------------------------*/
		/*		Just add the infiltration to the rz_storage and unsat_storage	*/
		/*------------------------------------------------------------------------------*/
			neigh[0].unsat_storage += infiltration - (neigh[0].rootzone.potential_sat - neigh[0].rz_storage);
			neigh[0].rz_storage = neigh[0].rootzone.potential_sat;
		}								
		/* Only rootzone layer saturated - perched water table case */
		else if ((neigh[0].sat_deficit > neigh[0].rootzone.potential_sat) &&
			(infiltration <= neigh[0].rootzone.potential_sat - neigh[0].rz_storage)) {
			/*--------------------------------------------------------------*/
			/*		Just add the infiltration to the rz_storage	*/
			/*--------------------------------------------------------------*/
			neigh[0].rz_storage += infiltration;
		}
		else if ((neigh[0].sat_deficit <= neigh[0].rootzone.potential_sat) &&
			(infiltration <= neigh[0].sat_deficit - neigh[0].rz_storage - neigh[0].unsat_storage)) {
			neigh[0].rz_storage += neigh[0].unsat_storage;		
			/* transfer left water in unsat storage to rootzone layer */
			neigh[0].unsat_storage = 0;
			neigh[0].rz_storage += infiltration;
			neigh[0].field_capacity = 0;
		}

		neigh[0].detention_store -= infiltration;

	}

	}
     

     /* end if redistribution flag */


     /*if(patch[0].ID == 1){
          fprintf(stderr, "\nP no:  %d  |  \Qout: %f  |  Depth:  %f", patch[0].ID, Qout, scm_H);
     }*/
     
	return;

} /*end update_drainage_land.c*/

