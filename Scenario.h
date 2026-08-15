/* File: Scenario.h
 * Athr: William M. Spears
 * Date: August 13, 2008
 *
 * Desc: defines common parameters for the Plume and CPT simulation programs.
 */
#ifndef SCENARIO_H
#define SCENARIO_H

// For MakeMap.c
#define SCALE 4      // Scale of the world
#define SIZE  32     // Should be divisible by 2 (i.e., even)

// For PlumeGen.c

#define PREP_STEPS    1
#define CPT_STEPS     8000
#define VIS_REDUCTION 10

#define K              (flt_t).5
#define WIND_DAMPING   (flt_t).09
#define WIND_BANDWIDTH (flt_t).01
#define WIND_GAIN      (flt_t).77
//Wind from the West
#define MEAN_U         (flt_t)0.05
#define MEAN_V         (flt_t)0.77
#define DEL_T          (flt_t)1.

#define SOURCE_EX 71
#define SOURCE_EY 15
#define EMIT_RATE 1
#define MAX_PUFFS (EMIT_RATE*(PREP_STEPS+CPT_STEPS))
#define Q         (flt_t)(80.3e0/EMIT_RATE)
#define R0        (flt_t)3.50
#define GAMMA     (flt_t).03

#define SIGMA_NUX (flt_t)5
#define SIGMA_NUY (flt_t)5

// For TraceGen.h

// enable multi-threading during evaluation mode
#define THREADS

// the radius of a circular robot -- used for obstacle and collision avoidance
#define AGENT_RADIUS (flt_t)0.5

// how much space to leave between agents to prevent a collision
#define AGENT_CLEARANCE (flt_t)(1.5*AGENT_RADIUS)

// sensor range of each agent
#define SENS_RADIUS (flt_t)(10*AGENT_RADIUS)

// maximum speed of each agent
#define AP_VMAX (flt_t)(3/12.)

// the magnitude of the AP goal force
#define AP_FGOAL (flt_t)(AP_VMAX/2.)

// initial position of the center of the swarm (ignored in the evaluation mode)
#define INIT_LX 82
#define INIT_LY 88

// Anemometer sensitivity threshold (minimum wind velocity that can be detected)
#define ANEMOMETER_THRESHOLD (flt_t).0005

// Chemical sensor threshold (minimum concentration that can be detected)
#define CHEMICAL_SENS_THRESHOLD (flt_t).0001

// Enable full-resolution chemical sensor in fluxotaxis 
// (otherwise a binary on/off threshold sensor is used)
#define FLUXO_MODE_GOOD_CHEM_SENSOR

// Enable biasing of the flux measurement by the distance between robots;
#define FLUXO_MODE_DISTANCE_BIAS

// Enable fluxotaxis to use anemotaxis when there are no neighbors.
#define FLUXO_MODE_FALLBACK_ANEMO

// Enable fluxotaxis to use chemotaxis when there is no mass flux (e.g., still air)
#define FLUXO_MODE_FALLBACK_CHEMO


#endif
