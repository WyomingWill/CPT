/* File: MapGen.c
 * Athr: Dimitri Zarzhitsky
 * Date: January 29, 2005
 *
 * Desc: Generates random obstacle maps for the CPT simulator.
 */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_SYM_BLOCKED 'b'

int main(int argc, char *argv[]) {
   if (argc < 5) fprintf(stderr, "Usage: mgen seed dimension num_blocks block_width [block_height=block_width]\n"), abort();
   int seed = atoi(argv[1]), dimension = atoi(argv[2]), num_blocks = atoi(argv[3]), block_width = atoi(argv[4]), block_height = block_width;
   if (argc == 6) block_height = atoi(argv[5]);

   if (dimension*dimension < num_blocks * block_width*block_height) fprintf(stderr, " (!) cannot fit all of the obstacles on the map\n"), abort();

   char filename[PATH_MAX+1];
   filename[PATH_MAX] = '\0';
   snprintf(filename, PATH_MAX, "map-S%d-D%d-B%d-W%d-H%d", seed, dimension, num_blocks, block_width, block_height);
   FILE *out = fopen(filename, "w");
   if (!out) fprintf(stderr, " (!) cannot create map file '%s'\n", filename), abort();

   fprintf(stdout, "Creating a %dx%d map with %d obstacles of %dx%d size (%.2f%% coverage) in '%s' ",
	   dimension, dimension, num_blocks, block_width, block_height, 100.*num_blocks*block_width*block_height/pow(dimension, 2), filename);

   char map[dimension][dimension];
   memset(map, ' ', dimension*dimension*sizeof(char));
   srandom(seed);   
   for (int i=0; i<num_blocks; i++) {
     int row, col;
     int found;
     do {
       row   = 1 + (dimension-1 - block_height) * (double)random()/RAND_MAX;
       col   = 1 + (dimension-1 - block_width)  * (double)random()/RAND_MAX;
       found = map[row][col] == ' ';
       for (int x=row; x<row+block_height && found; x++) for (int y=col; y<col+block_width && found; y++) found = map[x][y] == ' ';
     } while(!found);
     for (int x=row; x<row+block_height; x++) for (int y=col; y<col+block_width; y++) map[x][y] = MAP_SYM_BLOCKED;
     fprintf(stdout, "."), fflush(stdout);
   }

   for (int i=0; i<dimension; i++) {
     for (int j=0; j<dimension; j++) fprintf(out, "%c", map[i][j]);
     fprintf(out, "\n");
   }
   fclose(out);
   fprintf(stdout, " done.\n");
   return 0;
}
