#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define SIZE 252 // Was 32
#define SCALE 1
#define YTRANSLATE 100 // Was 0

int world[5001][5001];

void add_object (int x, int y, int w, int h)
{
  int i, j;

  for (i = x; i <= x + w; i++) {
    for (j = y; j <= y + h; j++) {
      world[SCALE*i][SCALE*j + YTRANSLATE] = 1;
    }
  }
}

/*-----------------------------------------------------------------------*/

int main (argc, argv)
int argc;
char *argv[];
{
   FILE *fp;
   int i, j, temp, seed, status;
   char command[100];


   // Open map data file for writing
   if ((fp = fopen("map.textron", "w")) == NULL) {
     printf("Cannot open file\n");
     exit(1);
   }

   for (i = 0; i < SIZE; i++) {
     for (j = 0; j < SIZE; j++) {
       world[i][j] = 0;
     }
   }

   add_object(0, 27, 3, 5);  // 1
   add_object(5, 28, 5, 2);  // 2
   add_object(15, 27, 5, 3); // 3
   add_object(23, 27, 5, 1); // 4
   add_object(23, 30, 9, 4); // 5
   add_object(30, 22, 2, 8); // 6
   add_object(19, 22, 9, 2); // 7
   add_object(15, 21, 1, 3); // 8
   add_object(7, 18, 5, 6);  // 9
   add_object(1, 23, 4, 1);  // 10
   add_object(1, 19, 1, 3);  // 11
   add_object(0, 12, 2, 5);  // 12
   add_object(4, 12, 1, 1);  // 13
   add_object(7, 12, 5, 1);  // 14
   add_object(7, 15, 5, 1);  // 15
   add_object(17, 9, 2, 4);  // 16
   add_object(17, 15, 2, 1); // 17
   add_object(17, 18, 2, 1); // 18
   add_object(21, 15, 3, 4); // 19
   add_object(26, 15, 2, 1); // 20
   add_object(26, 18, 2, 1); // 21
   add_object(26, 12, 2, 1); // 22
   add_object(26, 9, 2, 1);  // 23
   add_object(21, 9, 3, 1);  // 24
   add_object(21, 12, 3, 1); // 25
   add_object(30, 10, 2, 5); // 26
   add_object(1, 6, 4, 2);   // 27
   add_object(0, -1, 6, 2);  // 28
   add_object(8, 3, 5, 2);   // 29
   add_object(19, -1, 13, 2);// 30
   add_object(17, 3, 2, 2);  // 31
   add_object(22, 3, 9, 2);  // 32


   // Print in row major order
   for (j = 0; j < SIZE; j++) {   
     for (i = 0; i < SIZE; i++) {
       if (world[i][j] == 0) fprintf(fp, " ");
       else fprintf(fp, "b");
     }
     fprintf(fp, "\n");
   }

   return 0;
}
