/* File: Common.h
 * Date: June 30, 2004
 * Athr: Dimitri Zarzhitsky
 * 
 * Desc: collection of common typedefs and utility functions for the CPT
 *       test application.
 */
#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <float.h>
#include <glib.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "MapUtils.h"

#define DEFAULT_FILENAME_PLUME "/tmp/plume"
#define FILE_VER_PLUME         "PLUME01"
#define FILE_VER_LENGTH_PLUME  7

#define DEFAULT_FILENAME_CPT "/tmp/cpt"
#define FILE_VER_CPT         "CPT02"
#define FILE_VER_LENGTH_CPT  5

//typedef float flt_t;
typedef double flt_t;

// the __sgn() macro is defined like this in <bits/mathinline.h>, but does not seem to be accessible to user-level code
#define SGN(x) (flt_t)(x==0.0 ? 0.0 : (x > 0.0 ? 1.0 : -1.0))

// convenience macro for extracing file names from a command-line path; assumes destination filename has PATH_MAX+1 size
#define PARSE_FILENAME_FROM_PATH(path, DEFAULT, filename)		\
  if (path) {								\
    size_t marker = strlen(path);					\
    while (marker>0 && path[--marker] != '/');				\
    if (path[marker] != '/') strncpy(filename, path, PATH_MAX);		\
    else strncpy(filename, path+marker+1, PATH_MAX);			\
  } else {								\
    strncpy(filename, DEFAULT, PATH_MAX);				\
  }                                                                     \
  filename[PATH_MAX] = '\0';


#endif
