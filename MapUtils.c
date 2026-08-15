/* File: MapUtils.h
 * Date: January 28, 2004
 * Athr: Dimitri Zarzhitsky
 */
#include "MapUtils.h"

extern map_t MAP;

void map_show(FILE *stream) {
  fprintf(stream, "World Map:\n");
  for (int index=0, i=0; i<MAP.nx; i++) {
    for (int j=0; j<MAP.ny; j++) fprintf(stream, "%c", MAP.world[index++]);
    fprintf(stream, "\n");
  }
}

gboolean map_is_world_point(double x, double y) {
  int ix = lround(x), iy = lround(y);
  return ix>=0 && ix<MAP.nx && iy>=0 && iy<MAP.ny;
}

gboolean map_is_world_region_square(double x, double y, double rad) {
  return floor(x-rad)>=0 && ceil(x+rad)<MAP.nx && floor(y-rad)>=0 && ceil(y+rad)<MAP.ny;
}

gboolean map_is_blocked_point(double x, double y) {
  return map_is_world_point(x, y) && MAP.world[lround(x)*MAP.ny + lround(y)]==MAP_SYM_BLOCKED;
}

gboolean map_is_blocked_region_rectangle(double x1, double y1, double x2, double y2) {
  double x_min = floor(fmin(x1, x2)), x_max = ceil(fmax(x1, x2)), y_min = floor(fmin(y1, y2)), y_max = ceil(fmax(y1, y2));
  for (double i=x_min; i<=x_max; i++) for (double j=y_min; j<=y_max; j++) if (map_is_blocked_point(i, j)) return TRUE;
  return FALSE;
}

gboolean map_is_blocked_region_square(double x, double y, double rad) {
  double x_min = floor(x-rad), x_max = ceil(x+rad), y_min = floor(y-rad), y_max = ceil(y+rad);
  for (double i=x_min; i<=x_max; i++) for (double j=y_min; j<=y_max; j++) if (map_is_blocked_point(i, j)) return TRUE;
  return FALSE;
}

double map_percent_blocked() {
  double block_count = 0;
  for (int i=0; i<MAP.nxy; i++) if (MAP.world[i]==MAP_SYM_BLOCKED) block_count++;
  return 100 * block_count / MAP.nxy;
}
