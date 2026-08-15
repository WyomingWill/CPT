/* File: TraceGen.h
 * Athr: Dimitri Zarzhitsky
 * Date: November 18, 2004
 *
 * Desc: test generator for the CPT lattice algorithms.  This generator reads
 *       velocity and plume density from the data file produced by PlumeGen, 
 *       and executes a CPT algorithm, writing the results into a new file.
 */
#ifndef TRACE_GEN_H
#define TRACE_GEN_H

#include "Common.h"
#include "Scenario.h"


// ##### DIVERGENCE #####
// These functions are not used by any of the CPT algorithms, and were
// implemented only for the purpose of fluxotaxis research and development.
void extrapolate_div(flt_t *div);
void compute_divCNTR(const flt_t *u, const flt_t *v, const flt_t *rho, flt_t *div);
void compute_flux(const flt_t *u, const flt_t *v, const flt_t *rho, flt_t *div);


// ##### LATTICE #####
typedef struct agent {
  // agent position (x,y), velocity (u,v), and desired radius of separation (r)
  flt_t x;
  flt_t y;
  flt_t u;
  flt_t v;
  flt_t r;

  // current x/y directions for casting:
  //   longitude > 0: the casting motion is ++x
  //   longitude < 0: the casting motion is --x
  //   latitude  > 0: the casting motion is ++y
  //   latitude  < 0: the casting motion is --y
  char cast_longitude;
  char cast_latitude;

  // performance enhancement: each agent keeps an updated
  // list of its neighbors (those vehicles within SENS_RADIUS)
  // this list results in a significant speedup of the collision
  // avoidance code
   GPtrArray *neighbors;
} agent_t;

typedef struct swarm {
  GArray *agents;

  //flt_t anemo_history_sum;
  //flt_t chemo_history_sum;
  //flt_t dmf_threshold;

  // lowest airspeed resolved by the vehicle's anemometer
  flt_t anemo_threshold;

  // lowest chemical concentration detectable with the vehicles chemical sensor
  flt_t chemo_threshold;

  // time it took a swarm vehicle to come within SENS_RADIUS of the emitter
  int best_time;

  // total amount of time each vehicle in the swarm spent casting
  flt_t cast_time;

  // cumulative sum of times that a swarm vehicle was within SENS_RADIUS of the emitter
  flt_t successes;

  // cumulative distance from each swarm vehicle to the emitter over the course of the simulation
  flt_t pos_error;

  // total distance traveled by each robot during the run (assuming delT = 1)
  flt_t odometer;
} swarm_t;

// ----- GENERAL -----
// Initializes the swarm's position and each agent's state; the agents are placed in a circular pattern around (lx,ly)
// taking care not to place agents too close (or inside) obstacles.
void place_agents(guint num_agents, flt_t lx, flt_t ly, swarm_t *swarm);

// updates the local neighbor map of each agent in the swarm
void locate_neighbors(swarm_t *swarm);

// ensures that an agent never travels faster than AP_VMAX
void limit_speed(agent_t *agent);

// agent's position is a real-valued function, but the simulation world is a collection of discrete cells;
// this function takes care of mapping the agent's location to the right cell, and returning the correct
// sensor reading from that cell.
flt_t read_sensor(const flt_t *data, const agent_t *agent);

// these two functions implement a recursive broadcast of a casting direction through a swarm
void sync_cast_longitude(char new_longitude, agent_t *agent);
void sync_cast_latitude(char new_latitude, agent_t *agent);

// ----- MOVEMENT -----
// These two functions use a modified Lennard-Jones potential to move the agent swarm
// using Artificial Physics as the control mechanism. Present architecture uses
// a separate computation for formation and goal forces.  These functions 
// preserve the swarm's formation. A performance enhancement uses each vehicle's 
// "neighbor" list (a membership in this list is determined by the SENS_RADIUS parameter)
// to ensure that only local force interactions are computed.
void ap_compute_new_velocity(const agent_t *other, agent_t *agent);
void ap_maintain_formation(swarm_t *swarm);

// A simplistic attempt at obstacle avoidance (a more advanced algorithm would be
// beneficial).  Currently, this function enforces the speed limit on each vehicle,
// and uses the blocked MAP routines to see if an obstacle occupies the vehicle's
// intended location; note that this does not check for "reachability" constraint --
// if the vehicle speed is very large, and obstacle is very small, the code will not
// detect the problem of the robot driving through the obstacle.  This is not a problem
// for the current set of parameters, but this limitation should be kept in mind.
// The actual avoidance behavior consists of sliding the agent along a surface of the
// obstacle:
//  1. check if reversing the Y direction of motion avoids collision
//  2. check if reversing the X direction of motion helps
//  3. check if reversing both directions ("full" reverse) is enough
//  4. if checks above fail, try to find the center of obstacle "mass" (within a
//     a square region around the vehicle), and move away from it.  This virtual
//     mass is a function of the distance between a cell-by-cell discretization of
//     the obstacle and the center of the robot.
//  5. if everything failed to resolve the impending collision (really should only
//     happen in the case when a vehicle is completely surrounded by obstacles), stop
//     the vehicle.  In the current version, the vehicle's velocity is set to something
//     extremely small in the opposite direction of its current motion, in the faint 
//     hope that somehow, in the next step, an obstacle-free passage will be found.
void avoid_obstacles_reflection(swarm_t *swarm);

// A simple method for avoiding a collision between two robots by succesively reducing
// their velocities until the collision does not occur.  A check is made to make sure
// that this function returns (via a limit on the number of times that speed reduction
// is applied), hence it is still possible, but unlikely, for two vehicles to undergo
// a collision, which is defined as two agents being less than AGENT_CLEARANCE away from
// each other.
gboolean avoid_agent_collision(agent_t *other, agent_t* agent);

// This function will move each vehicle according to it's preferred velocity, but will
// ensure that obstacle and collision avoidance constraints are met.  When the program
// is executed in evaluation mode, this function will collect CPT performance statistics
// for each member of the swarm (hence the "cumulative" nature of the statistical 
// measures that have been implemented).
void move_agents_realistically(swarm_t *swarm);

// The set of agent_* functions is the implementation of the respective CPT algorithms
// ("casto" is the diagonal casting method we implemented).  These functions may make
// use of the global arrays U (x-component of wind velocity), V (y-component of wind
// velocity), and RHO (chemical density map).  From the viewpoint of the physicomimetics
// framework, these functions provide the environment goal forces to the agents.  Note
// that in this implementation, formation and goal forces are computed by different 
// modules: the formation forces are computed inside 'ap_maintain_formation', and the
// goal forces are added here.
void agent_anemo(swarm_t *swarm, agent_t *agent);
void agent_casto(swarm_t *swarm, agent_t *agent);
void agent_chemo(swarm_t *swarm, agent_t *agent);

// These two functions provide the implementation of our fluxotaxis algorithm: the
// 'compute_agent_flux' method computes the assymetric in/out fluxes from the perspective
// of the 'agent' at the location occupied by 'other'.  This is basically a hybrid view of
// the global values, such as wind velocity and chemical density, and a local perspective:
// the flux computed depends not only on the absolute value of the physical measurements,
// but also on the arrangement of the neighboring vehicles with respect to the agent that's
// peforming the flux computation.  The basic principle that underlies this design is the
// use of chemical mass flux as a pointer to "interesting" (e.g., potential emitter) locations
// within the chemical flow.  Conditional compilation flags 'FLUXO_MODE_*' enable different 
// versions of the flux computation algorithm (basically different weight factors).
flt_t compute_agent_flux(flt_t anemo_threshold, flt_t chemo_threshold, const agent_t *other, const agent_t *agent);
void  agent_fluxo(swarm_t *swarm, agent_t *agent);


// ##### CPT ALGORITHMS #####
// These are basic simulation wrappers/adapters that invoke the correct CPT algorithm
// for moving the swarm.  These have been separated out to simplfy test and development
// of different ideas for each algorithm; otherwise, the functions are almost identical.
// The "stayo" method doesn't move the swarm at all -- it's used for rapid trace generation,
// to help visualize the flow.  In typical usage, the stayo method is used to check CFD
// components, and the rest of the functions help validate CPT algorithms.
typedef gboolean (*cpt_alg_t)(swarm_t*);
gboolean cpt_anemo(swarm_t *swarm);
gboolean cpt_casto(swarm_t *swarm);
gboolean cpt_chemo(swarm_t *swarm);
gboolean cpt_fluxo(swarm_t *swarm);
gboolean cpt_stayo(swarm_t *swarm);


// ##### PERFORMANCE EVALUATION #####
// (these parameters are used only in the evaluation mode)

// how many CPT algorithms to test: anemotaxis, casting, chemotaxis, fluxotaxis
#define EVAL_NUM_ALGORS 4

// how many different starting locations to evaluate for each algorithm/swarm size combo
#define EVAL_NUM_STARTS 50

// swarm sizes that should be tested
#define EVAL_NUM_GROUPS      10
#define EVAL_INIT_GROUP_SIZE 7
#define EVAL_FINL_GROUP_SIZE 70
#define EVAL_DELT_GROUP_SIZE ((EVAL_FINL_GROUP_SIZE - EVAL_INIT_GROUP_SIZE)/(EVAL_NUM_GROUPS - 1))

// convenience user interface and data archive macros
#define EVAL_ALG_NAME(alg) (alg==0 ? "anemo" : (alg==1 ? "casto" : (alg==2 ? "chemo" : "fluxo")))
#define EVAL_ALG_FUNC(alg) (alg==0 ? cpt_anemo : (alg==1 ? cpt_casto : (alg==2 ? cpt_chemo : cpt_fluxo)))

// evaluation data storage -- this is the internal representation of the CSV
// files that will be written out at the end of the evaluation run.
typedef struct eval_data {
  FILE *out_best_time[EVAL_NUM_ALGORS];
  FILE *out_cast_time[EVAL_NUM_ALGORS];
  FILE *out_successes[EVAL_NUM_ALGORS];
  FILE *out_pos_error[EVAL_NUM_ALGORS];
  FILE *out_odometer[EVAL_NUM_ALGORS];
  swarm_t swarms[EVAL_NUM_ALGORS][EVAL_NUM_STARTS][EVAL_NUM_GROUPS];
} eval_data_t;

// Initializes all the different swarm combos for CPT evaluation; initial swarm starting
// locations are selected at random (INIT_LX, INIT_LY is ignored).  Instead, position 
// placement is determined by a successively increasing distance from the emitter.  This
// routine also ensures that all agents are placed in a valid location (not on top of each
// other, away from obstacles, etc). 
void init_eval_data(const char *plume_file, int seed);

// Writes out CPT performance statistics to comma-separated-values (CSV) files and
// deallocates the associated memory.
void release_eval_data();


// ##### MEMORY UTILS #####
void alloc_mem();
void free_mem();
void free_swarm(swarm_t *swarm);


// ##### FILE IO #####
void error_exit(const char *template, ...) __attribute__ ((noreturn, format(printf, 1, 2)));
void read_plume_header(FILE *in);
void read_plume_step(int t, FILE *in);
void write_cpt_header(FILE *out);
void write_cpt_step(int step, FILE *out);
void update_cpt_NT(int nt, FILE *out);


// ##### DRIVERS #####
// Sets threshold values for wind speed detection and chemical sensor sensitivity,
// and allocates agent memory structures, and then places the agents into the
// simulated world in a circular pattern around (lx,ly).
void init_swarm(guint num_agents, flt_t lx, flt_t ly, swarm_t *swarm);

// Creates the output CPT trace file by reading the plume file, executing the CPT
// algorithm selected by the user, and then saving the result into an output file.
void      generate_cpt_file(cpt_alg_t cpt_algorithm, FILE *in, FILE *out);
cpt_alg_t lookup_cpt_algorithm(char choice);
gpointer  thread_eval_worker(gpointer data);
int       main(int argc, char *argv[]);


// ##### TEST & DEVELOPMENT #####
//void monitor_mass_flux(FILE *in, FILE *out);


#endif
