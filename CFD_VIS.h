/* File: CFD_VIS.h
 * Athr: Dimitri Zarzhitsky
 * Date: March 3, 2004
 *
 * Desc: GTK-based visualization tool for the Fluxotaxis CFD simulation.
 *       Tool reads the input file (in the format shown below) and produces
 *       a time-stamped visualization of the data.
 *
 *       The input file format is as follows:
 *       <!-- add the description once the format is stabilized -->
 */
#ifndef CFD_VIS_H
#define CFD_VIS_H

#include "Common.h"
#include "CFD_VIS_global.h"

extern void activate_gui();

int main(int argc, char *argv[]);

// ##### MEMORY UTILS #####
void alloc_mem();
void free_mem();

// ##### FILE IO #####
void read_header();
void read_data(int t);

// ##### DATA MANIPULATION #####
void compute_data_range_file();
void compute_data_range_step();
void get_max_vector(flt_t *data, value_range_t *vrange);
void get_min_max_scalar(flt_t *data, value_range_t *vrange);
void init_range_data(range_data_t *rdata);

// ##### STATISTICAL MEASURES #####
flt_t stat_avg_scalar(flt_t *data);
flt_t stat_avg_vector(flt_t *dataI, flt_t *dataJ);
flt_t stat_sdev_scalar(flt_t *data, flt_t mean);
flt_t stat_sdev_vector(flt_t *dataI, flt_t *dataJ, flt_t mean);

// ##### MISC UTIL #####
void parse_cmd_arguments(int argc, char *argv[]);
void hdl_interrupt_read(int signal_id);
void hdl_interrupt_gui(int signal_id);
void hdl_error(const char *template, ...) __attribute__ ((format(printf, 1, 2)));

#endif
