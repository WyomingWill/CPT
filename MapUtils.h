/* File: MapUtils.h
 * Date: January 28, 2004
 * Athr: Dimitri Zarzhitsky
 * 
 * Desc: the 2D world map used by the CPT simulator is stored as a 1D character
 *       array.  This file provides convenience utilities for query and look-up
 *       operations on the map.  All of the utilities assume existence of a 
 *       global map_t variable named 'MAP' which stores the map data.
 */
#ifndef MAP_UTILS_H
#define MAP_UTILS_H

#include <glib.h>
#include <math.h>
#include <stdio.h>

#define MAP_SYM_BLOCKED 'b'

typedef struct map {
  int nx;
  int ny;
  int nxy;
  char *world;
} map_t;

void map_show(FILE *stream);

gboolean map_is_world_point(double x, double y);
gboolean map_is_world_region_square(double x, double y, double rad);
gboolean map_is_blocked_point(double x, double y);
gboolean map_is_blocked_region_rectangle(double x1, double y1, double x2, double y2);
gboolean map_is_blocked_region_square(double x, double y, double rad);

double map_percent_blocked();

#endif
