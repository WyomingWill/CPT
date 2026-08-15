/* File: CFD_VIS_GLOBAL.h
 * Athr: Dimitri Zarzhitsky
 * Date: March 3, 2004
 *
 * Desc: a placeholder for global variables and types used by the CFD
 *       visualization tool.
 */
#ifndef CFD_VIS_GLOBAL_H
#define CFD_VIS_GLOBAL_H

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "Common.h"

#define NUM_HUES USHRT_MAX

FILE *INPUT;

map_t MAP;
int EX, EY, NT, NA;
flt_t AGENT_RADIUS, SENS_RADIUS;
off_t FILE_HEADER_BYTES, FILE_RECORD_BYTES;

typedef struct value_range {
  flt_t min;
  flt_t max;
} value_range_t;

typedef struct range_data {
  value_range_t u;
  value_range_t v;
  value_range_t rho;
  value_range_t div;
} range_data_t;
range_data_t RANGE_ALL, RANGE_STEP;

struct visualization_data {
  int    step;
  flt_t *u;
  flt_t *v;
  flt_t *rho;
  flt_t *div;
  flt_t *lat;
} VIS_DATA;

struct cmd_line_args {
  char     input_file[PATH_MAX+1];
  gboolean capture_enabled;
} CMD_ARGS;

#endif
