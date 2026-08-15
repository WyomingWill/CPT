/* File: TraceGen.c
 * Athr: Dimitri Zarzhitsky
 * Date: February 20, 2005
 */
#include "TraceGen.h"

// ##### GLOBAL DATA #####
flt_t *U, *V, *RHO, *DIV;

map_t MAP;
int NT, EX, EY, TIME_STEP;

swarm_t     *SWARM;
eval_data_t *EVAL;


// ##### DIVERGENCE #####
void extrapolate_div(flt_t *div) {
  for (int i=1; i<MAP.nx-1; i++) {
    div[i*MAP.ny] = div[i*MAP.ny+1];
    div[i*MAP.ny+MAP.ny-1] = div[i*MAP.ny+MAP.ny-2];
  }
  for (int j=1; j<MAP.ny-1; j++) {
    div[j] = div[MAP.ny+j];
    div[(MAP.nx-1)*MAP.ny+j] = div[(MAP.nx-2)*MAP.ny+j];
  }
  div[0] = (div[1] + div[MAP.ny])/2;
  div[MAP.ny-1] = (div[MAP.ny-2] + div[MAP.ny+MAP.ny-1])/2;
  div[(MAP.nx-1)*MAP.ny] = (div[(MAP.nx-2)*MAP.ny] + div[(MAP.nx-1)*MAP.ny+1])/2;
  div[(MAP.nx-1)*MAP.ny+MAP.ny-1] = (div[(MAP.nx-2)*MAP.ny+MAP.ny-1] + div[(MAP.nx-1)*MAP.ny+MAP.ny-2])/2;
}

void compute_divCNTR(const flt_t *u, const flt_t *v, const flt_t *rho, flt_t *div) {
  for (int i=1; i<MAP.nx-1; i++) {
    for (int j=1; j<MAP.ny-1; j++) {
      int x = i*MAP.ny, xb = (i-1)*MAP.ny, xa = (i+1)*MAP.ny, xy = x+j;
      div[xy] = (rho[xy] * (u[xa+j]-u[xb+j] + v[xy+1]-v[xy-1])/2 +
                 u[xy] * (rho[xa+j]-rho[xb+j])/2 +
                 v[xy] * (rho[xy+1]-rho[xy-1])/2);
    }
  }
  extrapolate_div(div);
}

void compute_flux(const flt_t *u, const flt_t *v, const flt_t *rho, flt_t *div) {
  for (int i=0; i<MAP.nxy; i++) div[i] = rho[i] * hypot(u[i], v[i]);
}


// ##### LATTICE #####
// ----- GENERAL -----
void place_agents(guint num_agents, flt_t lx, flt_t ly, swarm_t *swarm) {
  guint placed = 0;
  for (flt_t r=.01; r<2*MAP.nx && placed<num_agents; r+=AGENT_RADIUS) {
    for (flt_t theta=r; theta<r+2*M_PI && placed<num_agents; theta+=AGENT_RADIUS/r) {
      flt_t i = lx + r*cos(theta), j = ly + r*sin(theta);
      if (map_is_world_region_square(i, j, AGENT_CLEARANCE) && !map_is_blocked_region_square(i, j, AGENT_CLEARANCE)) {
	agent_t *other    = (agent_t*)swarm->agents->data;
	gboolean conflict = FALSE;
	for (guint a=0; a<placed && !conflict; a++, other++) conflict = hypot(other->x - i,  other->y - j) < AGENT_CLEARANCE;
	if (!conflict) {
	  agent_t *agent = &g_array_index(swarm->agents, agent_t, placed);
	  agent->x       = i;
	  agent->y       = j;
	  agent->u       = 0;
	  agent->v       = 0;
	  agent->r       = fmax(AGENT_CLEARANCE, fmin(SENS_RADIUS, .5*SENS_RADIUS));

	  // WMS Turn Casting off?
	  agent->cast_longitude = 0; // Was 1
	  agent->cast_latitude  = 0; // Was 1

	  if (agent->neighbors) g_ptr_array_set_size(agent->neighbors, 0);
	  else agent->neighbors = g_ptr_array_sized_new(7);

	  placed++;
	}
      }
    }
  }
  assert(placed == num_agents);
}

void locate_neighbors(swarm_t *swarm) {
  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) g_ptr_array_set_size(agent->neighbors, 0);

  agent = (agent_t*)swarm->agents->data;
  for (guint a=0; a+1<num_agents; a++, agent++) {
    agent_t *other = agent + 1;
    for (guint o=a+1; o<num_agents; o++, other++) {
      if (hypot(other->x - agent->x, other->y - agent->y) <= SENS_RADIUS) g_ptr_array_add(agent->neighbors, other), g_ptr_array_add(other->neighbors, agent);
    }
  }
}

flt_t read_sensor(const flt_t *data, const agent_t *agent) {
  int ax = fmin(MAP.nx-1, fmax(0, round(agent->x))), ay = fmin(MAP.ny-1, fmax(0, round(agent->y)));
  return data[ax*MAP.ny + ay];
}

void limit_speed(agent_t *agent) {
  if (hypot(agent->u, agent->v) > AP_VMAX) {
    flt_t bearing = atan2(agent->v, agent->u);
    agent->u = AP_VMAX * cos(bearing);
    agent->v = AP_VMAX * sin(bearing);
  }
}

void sync_cast_longitude(char new_longitude, agent_t *agent) {
  if (agent->cast_longitude != new_longitude) {
    agent->cast_longitude = new_longitude;
    
    agent_t **neighbor = (agent_t**)agent->neighbors->pdata;
    for (guint n=0; n<agent->neighbors->len; n++, neighbor++) sync_cast_longitude(new_longitude, *neighbor);
  }
}

void sync_cast_latitude(char new_latitude, agent_t *agent) {
  if (agent->cast_latitude != new_latitude) {
    agent->cast_latitude = new_latitude;
    
    agent_t **neighbor = (agent_t**)agent->neighbors->pdata;
    for (guint n=0; n<agent->neighbors->len; n++, neighbor++) sync_cast_latitude(new_latitude, *neighbor);
  }
}

// ----- AP -----
void ap_compute_new_velocity(const agent_t *other, agent_t *agent) {
  flt_t del_x = other->x - agent->x, del_y = other->y - agent->y;
  flt_t r_actual = fmax(2*AGENT_RADIUS, hypot(del_x, del_y)), r_desired = (other->r + agent->r)/2;
  if (r_actual <= SENS_RADIUS) {
    flt_t theta = atan2(del_y, del_x);
    flt_t F     = pow(r_desired, 1)/pow(r_actual, 2.1) - pow(r_desired, 1.7)/pow(r_actual, 2.7);
    agent->u   += F * cos(theta);
    agent->v   += F * sin(theta);
  }
}

void ap_maintain_formation(swarm_t *swarm) {
  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) {
    const agent_t **neighbor = (const agent_t**)agent->neighbors->pdata;
    for (guint n=0; n<agent->neighbors->len; n++, neighbor++) ap_compute_new_velocity(*neighbor, agent);
  }
}

void avoid_obstacles_reflection(swarm_t *swarm) {
  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) {
    limit_speed(agent);

    flt_t old_x = agent->x, old_y = agent->y, new_x = old_x + agent->u, new_y = old_y + agent->v;
    if (map_is_blocked_region_square(new_x, new_y, AGENT_CLEARANCE) || !map_is_world_region_square(new_x, new_y, AGENT_CLEARANCE)) {
      // try a new position by "skipping" the agent
      new_x = old_x + agent->u;
      new_y = old_y - agent->v;
      if (!map_is_blocked_region_square(new_x, new_y, AGENT_CLEARANCE) && map_is_world_region_square(new_x, new_y, AGENT_CLEARANCE)) {
	agent->v *= -1;
      } else {
	new_x = old_x - agent->u;
	new_y = old_y + agent->v;
	if (!map_is_blocked_region_square(new_x, new_y, AGENT_CLEARANCE) && map_is_world_region_square(new_x, new_y, AGENT_CLEARANCE)) {
	  agent->u *= -1;
	} else {
	  new_x = old_x - agent->u;
	  new_y = old_y - agent->v;
	  if (!map_is_blocked_region_square(new_x, new_y, AGENT_CLEARANCE) && map_is_world_region_square(new_x, new_y, AGENT_CLEARANCE)) {
	    agent->u *= -1;
	    agent->v *= -1;
	  } else {
	    // otherwise repel from the obstacle
	    flt_t new_u = 0, new_v = 0;
	    for (flt_t x=floor(old_x-SENS_RADIUS); x<=ceil(old_x+SENS_RADIUS); x++) {
	      for (flt_t y=floor(old_y-SENS_RADIUS); y<=ceil(old_y+SENS_RADIUS); y++) {
		flt_t distance = fmax(0.1, hypot(x, y));
		if (distance<=SENS_RADIUS && map_is_blocked_point(x, y)) {
		  flt_t theta = atan2(y-old_y, x-old_x);
		  new_u      -= 1/distance * cos(theta);
		  new_v      -= 1/distance * sin(theta);
		}
	      }
	    }
	    flt_t theta = atan2(new_v, new_u);
	    agent->u    = AP_VMAX * cos(theta);
	    agent->v    = AP_VMAX * sin(theta);
	    
	    // if this last-chance attempt failed to avoid the obstacle, "stop" the agent at its present location
	    new_x = agent->x + agent->u;
	    new_y = agent->y + agent->v;
	    if (map_is_blocked_region_square(new_x, new_y, AGENT_CLEARANCE) || !map_is_world_region_square(new_x, new_y, AGENT_CLEARANCE)) {
	      agent->u *= -.0001;
	      agent->v *= -.0001;
	    }
	  }
	}
      }
    }
  }
}

#define SLOWDOWN_FACTOR (flt_t)0.7
#define MIN_AGENT_VEL   (flt_t)(AP_VMAX/100.)
gboolean avoid_agent_collision(agent_t *other, agent_t *agent) {
  gboolean collision = FALSE, conflict = FALSE;
  int iter = 1 + ceil(log(MIN_AGENT_VEL/fmax(hypot(agent->u, agent->v), hypot(other->u, other->v))) / log(SLOWDOWN_FACTOR));
  do {
    flt_t ax  = agent->x + agent->u, ay = agent->y + agent->v;
    flt_t ox  = other->x + other->u, oy = other->y + other->v;
    conflict = hypot(ox-ax, oy-ay) < AGENT_CLEARANCE;
    if (conflict) {
      collision = TRUE;
      agent->u *= SLOWDOWN_FACTOR;
      agent->v *= SLOWDOWN_FACTOR;
      other->u *= SLOWDOWN_FACTOR;
      other->v *= SLOWDOWN_FACTOR;
    }
  } while (conflict && --iter>0);
#if 0
  if (conflict) {
    fprintf(stderr, " (#) possible agent collision (dist=%f)\n", hypot(agent->x+agent->u - other->x+other->u, agent->y+agent->v - other->y+other->v));
  }
#endif
  return collision;
}

void move_agents_realistically(swarm_t *swarm) {
  avoid_obstacles_reflection(swarm);
  
  guint num_agents   = swarm->agents->len, iter = num_agents;
  agent_t *agent     = (agent_t*)swarm->agents->data;
  gboolean collision = FALSE;
  do {
    collision = FALSE;
    agent     = (agent_t*)swarm->agents->data;
    for (guint a=0; a<num_agents; a++, agent++) {
      agent_t **neighbor = (agent_t**)agent->neighbors->pdata;
      for (guint n=0; n<agent->neighbors->len; n++, neighbor++) {
	if (!collision) collision = avoid_agent_collision(*neighbor, agent);
	else avoid_agent_collision(*neighbor, agent);
      }
    }
  } while (collision && --iter>0);
  
  agent = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) {
    agent->x += agent->u;
    agent->y += agent->v;
#if 0
    if (map_is_blocked_region_square(agent->x, agent->y, AGENT_CLEARANCE) || !map_is_world_region_square(agent->x, agent->y, AGENT_CLEARANCE)) {
      fprintf(stderr, "%p c-fail: (%f,%f)\n", (void*)agent, agent->x, agent->y);
      //abort();
    }
#endif
    if (EVAL) {
      flt_t distance_to_emitter = hypot(EX - agent->x, EY - agent->y);
      if (distance_to_emitter <= SENS_RADIUS) {
	swarm->successes++;
	if (swarm->best_time == NT) swarm->best_time = TIME_STEP;
      }
      swarm->pos_error += distance_to_emitter;
      swarm->odometer  += hypot(agent->u, agent->v);
    }
  }
#if 0
  gboolean conflict = FALSE;
  agent = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents && !conflict; a++, agent++) {
    agent_t *other = (agent_t*)swarm->agents->data;
    for (guint o=0; o<num_agents && !conflict; o++, other++) {
      conflict = (agent!=other && hypot(other->x - agent->x, other->y - agent->y) < AGENT_CLEARANCE);
      if (conflict) {
	fprintf(stderr, " $$$ dist between conflicting agents: %f $$$\n", hypot(other->x - agent->x, other->y - agent->y));
	for (guint n=0; n<agent->neighbors->len; n++) {
	  if (other == g_ptr_array_index(agent->neighbors, n)) fprintf(stderr, " $$$ collided with neighbor $$$\n");
	}
      }
    }
  }
  if (conflict) fprintf(stderr, " (!) unable to resolve agent collision\n"), abort();
#endif
  locate_neighbors(swarm);
}

void agent_anemo(swarm_t *swarm, agent_t *agent) {
  flt_t u = read_sensor(U, agent), v = read_sensor(V, agent);
  if (read_sensor(RHO, agent) >= swarm->chemo_threshold && hypot(u, v) >= swarm->anemo_threshold) {
    flt_t theta = atan2(v, u);
    agent->u   -= AP_FGOAL * cos(theta);
    agent->v   -= AP_FGOAL * sin(theta);
  } else {
    agent_casto(swarm, agent);
  }
}

void agent_casto(swarm_t *swarm, agent_t *agent) {
  flt_t ax = agent->x, ay = agent->y;
  char longitude = agent->cast_longitude, latitude = agent->cast_latitude;
  if ((ax<SENS_RADIUS && longitude<0) || (ax>=MAP.nx-SENS_RADIUS && longitude>0) ||
      (map_is_blocked_region_rectangle(floor(ax-AGENT_RADIUS), ay-AGENT_RADIUS, ax, ay+AGENT_RADIUS) && longitude<0) || 
      (map_is_blocked_region_rectangle(ax, ay-AGENT_RADIUS, ceil(ax+AGENT_RADIUS), ay+AGENT_RADIUS) && longitude>0)) {
    longitude *= -1;
    sync_cast_longitude(longitude, agent);
  }
  if ((ay<SENS_RADIUS && latitude<0) || (ay>=MAP.ny-SENS_RADIUS && latitude>0) ||
      (map_is_blocked_region_rectangle(ax-AGENT_RADIUS, floor(ay-AGENT_RADIUS), ax+AGENT_RADIUS, ay) && latitude<0) ||
      (map_is_blocked_region_rectangle(ax-AGENT_RADIUS, ay, ax+AGENT_RADIUS, ceil(ay+AGENT_RADIUS)) && latitude>0)) {
    latitude *= -1;
    sync_cast_latitude(latitude, agent);
  }
  agent->u += longitude * AP_FGOAL;
  agent->v += latitude  * AP_FGOAL;
  
  if (EVAL && read_sensor(RHO, agent) < swarm->chemo_threshold) swarm->cast_time++;
}

void agent_chemo(swarm_t *swarm, agent_t *agent) {
  guint num_neighbors = agent->neighbors->len;
  if (num_neighbors) { 
    flt_t chemo_threshold = swarm->chemo_threshold, rho = read_sensor(RHO, agent);
    if (rho < chemo_threshold) rho = 0;

    agent_t *agent_max = *agent->neighbors->pdata;
    flt_t    rho_max   = read_sensor(RHO, agent_max);
    if (rho_max < chemo_threshold) rho_max = 0;

    for (guint n=1; n<num_neighbors; n++) {
      agent_t *neighbor = (agent_t*)g_ptr_array_index(agent->neighbors, n);
      flt_t    rho_n    = read_sensor(RHO, neighbor);
      if (rho_n>=chemo_threshold && rho_n>rho_max) rho_max = rho_n, agent_max = neighbor;
    }

    flt_t force = SGN(rho_max - rho) * AP_FGOAL;
    if (force) {
      flt_t theta = atan2(agent_max->y - agent->y, agent_max->x - agent->x);
      agent->u   += force * cos(theta);
      agent->v   += force * sin(theta);
    } else {
      agent_casto(swarm, agent);
    }
  } else {
    agent_casto(swarm, agent);
  }
}

flt_t compute_agent_flux(flt_t anemo_threshold, flt_t chemo_threshold, const agent_t *other, const agent_t *agent) {
  flt_t rho = read_sensor(RHO, other), u = read_sensor(U, other), v = read_sensor(V, other), vel = hypot(u, v);
  if (rho < chemo_threshold) return 0;
  if (vel < anemo_threshold) return 0;
  flt_t ax = agent->x, ay = agent->y, ox = other->x, oy = other->y, dx = ox-ax, dy = oy-ay;

  flt_t composite_flux_value = vel * cos(atan2(v, u) - atan2(dy, dx));

#ifdef FLUXO_MODE_GOOD_CHEM_SENSOR
  composite_flux_value *= rho;
#endif

#ifdef FLUXO_MODE_DISTANCE_BIAS
  composite_flux_value *= hypot(dx, dy);
#endif

  return composite_flux_value;
}

void agent_fluxo(swarm_t *swarm, agent_t *agent) {
  guint num_neighbors = agent->neighbors->len;
  if (num_neighbors > 1) {
    flt_t anemo_threshold = swarm->anemo_threshold, chemo_threshold = swarm->chemo_threshold;
    agent_t *agent_max_outflux = *agent->neighbors->pdata, *agent_max_influx = agent_max_outflux;
    flt_t max_outflux = compute_agent_flux(anemo_threshold, chemo_threshold, agent_max_outflux, agent), max_influx = max_outflux;
    for (guint n=1; n<num_neighbors; n++) {
      agent_t *neighbor = (agent_t*)g_ptr_array_index(agent->neighbors, n);
      flt_t    nflux    = compute_agent_flux(anemo_threshold, chemo_threshold, neighbor, agent);
      if (nflux > max_outflux) max_outflux = nflux, agent_max_outflux = neighbor;
      else if (nflux < max_influx) max_influx = nflux, agent_max_influx = neighbor;
    }
    if (max_influx < 0) {
      flt_t theta = atan2(agent_max_influx->y - agent->y, agent_max_influx->x - agent->x);
      agent->u   += AP_FGOAL * cos(theta);
      agent->v   += AP_FGOAL * sin(theta);
    } else if (max_outflux > 0) {
      flt_t theta = atan2(agent_max_outflux->y - agent->y, agent_max_outflux->x - agent->x);
      agent->u   += AP_FGOAL * cos(theta);
      agent->v   += AP_FGOAL * sin(theta);
    } else {
#ifdef FLUXO_MODE_FALLBACK_CHEMO
      agent_chemo(swarm, agent);
#endif
    }
  } else {
#ifdef FLUXO_MODE_FALLBACK_ANEMO
    agent_anemo(swarm, agent);
#endif
  }
}

// ##### CPT ALGORITHMS #####
gboolean cpt_anemo(swarm_t *swarm) {
  ap_maintain_formation(swarm);

  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) agent_anemo(swarm, agent);

  move_agents_realistically(swarm);
  return FALSE;
}

gboolean cpt_casto(swarm_t *swarm) {
  ap_maintain_formation(swarm);

  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) agent_casto(swarm, agent);

  move_agents_realistically(swarm);
  return FALSE;
}

gboolean cpt_chemo(swarm_t *swarm) {
  ap_maintain_formation(swarm);

  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) agent_chemo(swarm, agent);

  move_agents_realistically(swarm);
  return FALSE;
}

gboolean cpt_fluxo(swarm_t *swarm) {
  ap_maintain_formation(swarm);

  guint num_agents = swarm->agents->len;
  agent_t *agent   = (agent_t*)swarm->agents->data;
  for (guint a=0; a<num_agents; a++, agent++) agent_fluxo(swarm, agent);

  move_agents_realistically(swarm);
  return FALSE;
}

gboolean cpt_stayo(swarm_t *swarm) {
  return FALSE;
}


// ##### PERFORMANCE EVALUATION #####
void init_eval_data(const char *plume_file, int seed) {
  char out_file[PATH_MAX+1];
  out_file[PATH_MAX] = '\0';
  for (int alg_num=0; alg_num<EVAL_NUM_ALGORS; alg_num++) {
    snprintf(out_file, PATH_MAX, "eval/best_time_%s_%s.csv", plume_file, EVAL_ALG_NAME(alg_num));
    if (!(EVAL->out_best_time[alg_num] = fopen(out_file, "w"))) error_exit(" (!) cannot open '%s' for writing", out_file);

    snprintf(out_file, PATH_MAX, "eval/cast_time_%s_%s.csv", plume_file, EVAL_ALG_NAME(alg_num));
    if (!(EVAL->out_cast_time[alg_num] = fopen(out_file, "w"))) error_exit(" (!) cannot open '%s' for writing", out_file);

    snprintf(out_file, PATH_MAX, "eval/successes_%s_%s.csv", plume_file, EVAL_ALG_NAME(alg_num));
    if (!(EVAL->out_successes[alg_num] = fopen(out_file, "w"))) error_exit(" (!) cannot open '%s' for writing", out_file);

    snprintf(out_file, PATH_MAX, "eval/pos_error_%s_%s.csv", plume_file, EVAL_ALG_NAME(alg_num));
    if (!(EVAL->out_pos_error[alg_num] = fopen(out_file, "w"))) error_exit(" (!) cannot open '%s' for writing", out_file);

    snprintf(out_file, PATH_MAX, "eval/odometer_%s_%s.csv", plume_file, EVAL_ALG_NAME(alg_num));
    if (!(EVAL->out_odometer[alg_num] = fopen(out_file, "w"))) error_exit(" (!) cannot open '%s' for writing", out_file);
  }

  srandom(seed);
  flt_t location_range = hypot(fmax(EX, MAP.nx-1-EX), fmax(EY, MAP.ny-1-EY))-SENS_RADIUS, location_incr = location_range / EVAL_NUM_STARTS;
  assert(location_incr>0);
  for (int start_num=0; start_num<EVAL_NUM_STARTS; start_num++) {
    flt_t r = start_num * location_incr, theta = 2*M_PI*random()/RAND_MAX, lx = -1, ly = -1;
    gboolean location_blocked = TRUE;
    for (int angle_count=0; angle_count<360 && location_blocked; angle_count++, theta+=M_PI/180) {
      lx = EX + r*cos(theta);
      ly = EY + r*sin(theta);
      location_blocked = !map_is_world_region_square(lx, ly, AGENT_CLEARANCE) || map_is_blocked_region_square(lx, ly, AGENT_CLEARANCE);
    }
    assert(!location_blocked);
    for (int group=0; group<EVAL_NUM_GROUPS; group++) {
      guint num_agents = EVAL_INIT_GROUP_SIZE + group*EVAL_DELT_GROUP_SIZE;
      for (int alg_num=0; alg_num<EVAL_NUM_ALGORS; alg_num++) init_swarm(num_agents, lx, ly, EVAL->swarms[alg_num][start_num]+group);
    }
  }
}

void release_eval_data() {
  if (EVAL) {
    for (int alg_num=0; alg_num<EVAL_NUM_ALGORS; alg_num++) {
      FILE *out_best_time = EVAL->out_best_time[alg_num];
      FILE *out_cast_time = EVAL->out_cast_time[alg_num];
      FILE *out_successes = EVAL->out_successes[alg_num];
      FILE *out_pos_error = EVAL->out_pos_error[alg_num];
      FILE *out_odometer  = EVAL->out_odometer[alg_num];
      for (int start_num=0; start_num<EVAL_NUM_STARTS; start_num++) {
	for (int group=0; group<EVAL_NUM_GROUPS; group++) {
	  swarm_t *swarm = EVAL->swarms[alg_num][start_num] + group;
	  fprintf(out_best_time, "%d ", swarm->best_time);
	  fprintf(out_cast_time, "%f ", swarm->cast_time);
	  fprintf(out_successes, "%f ", swarm->successes);
	  fprintf(out_pos_error, "%f ", swarm->pos_error);
	  fprintf(out_odometer,  "%f ", swarm->odometer);

	  free_swarm(swarm);
	}
	fprintf(out_best_time, "\n");
	fprintf(out_cast_time, "\n");
	fprintf(out_successes, "\n");
	fprintf(out_pos_error, "\n");
	fprintf(out_odometer,  "\n");
      }
      fflush(out_best_time);
      if (ferror(out_best_time)) fprintf(stderr, " (!) cannot save %s best_time", EVAL_ALG_NAME(alg_num));
      else fclose(out_best_time);

      fflush(out_cast_time);
      if (ferror(out_cast_time)) fprintf(stderr, " (!) cannot save %s cast_time", EVAL_ALG_NAME(alg_num));
      else fclose(out_cast_time);

      fflush(out_successes);
      if (ferror(out_successes)) fprintf(stderr, " (!) cannot save %s successes", EVAL_ALG_NAME(alg_num));
      else fclose(out_successes);

      fflush(out_pos_error);
      if (ferror(out_pos_error)) fprintf(stderr, " (!) cannot save %s pos_error", EVAL_ALG_NAME(alg_num));
      else fclose(out_pos_error);

      fflush(out_odometer);
      if (ferror(out_odometer))  fprintf(stderr, " (!) cannot save %s odometer",  EVAL_ALG_NAME(alg_num));
      else fclose(out_odometer);
    }
    free(EVAL);
    EVAL  = NULL;
    SWARM = NULL;
  }
}


// ##### MEMORY UTILS #####
void alloc_mem() {
  assert(U   = calloc(MAP.nxy, sizeof(flt_t)));
  assert(V   = calloc(MAP.nxy, sizeof(flt_t)));
  assert(RHO = calloc(MAP.nxy, sizeof(flt_t)));
  assert(DIV = calloc(MAP.nxy, sizeof(flt_t)));
}

void free_mem() {
  if (EVAL) release_eval_data();
  else free_swarm(SWARM);

  free(DIV);
  free(RHO);
  free(V);
  free(U);

  free(MAP.world);
}

void free_swarm(swarm_t *swarm) {
  if (swarm) {
    guint num_agents = swarm->agents->len;
    agent_t *agent   = (agent_t*)swarm->agents->data;
    for (guint a=0; a<num_agents; a++, agent++) g_ptr_array_free(agent->neighbors, TRUE);
    g_array_free(swarm->agents, TRUE);
  }
}


// ##### FILE IO #####
void error_exit(const char *template, ...) {
  va_list ap;
  va_start(ap, template);
  vfprintf(stderr, template, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  free_mem();
  abort();
}

void read_plume_header(FILE *in) {
  char in_ver[FILE_VER_LENGTH_PLUME+1];
  in_ver[FILE_VER_LENGTH_PLUME] = '\0';
  fread(in_ver, sizeof(char), FILE_VER_LENGTH_PLUME, in);
  if (strncmp(in_ver, FILE_VER_PLUME, FILE_VER_LENGTH_PLUME)) error_exit("[TraceGen::read_plume_header] found version '%s', expected '%s'", in_ver, FILE_VER_PLUME);

  if (3 != fread(&MAP, sizeof(int), 3, in)) error_exit("[TraceGen::read_plume_header] failed to obtain map dimensions");
  MAP.world = calloc(MAP.nxy, sizeof(char));
  if (MAP.nxy != fread(MAP.world, sizeof(char), MAP.nxy, in)) error_exit("[TraceGen::read_plume_header] failed to obtain map data");

  size_t cnt = (fread(&NT, sizeof(int), 1, in) + fread(&EX, sizeof(int), 1, in) + fread(&EY, sizeof(int), 1, in));
  if (cnt != 3) error_exit("[TraceGen::read_plume_header] read %zd plume header points, expected %d", cnt, 3);
  assert(NT > 0);
}

void read_plume_step(int t, FILE *in) {
  int step;
  size_t cnt = fread(&step, sizeof(int), 1, in);
  if (cnt!=1 || step!=t) error_exit("[TraceGen::read_plume_step] '%s' is corrupted @ t=%d", DEFAULT_FILENAME_PLUME, t);
    
  cnt = fread(U, sizeof(flt_t), MAP.nxy, in) + fread(V, sizeof(flt_t), MAP.nxy, in) + fread(RHO, sizeof(flt_t), MAP.nxy, in);
  if (cnt != 3*MAP.nxy) error_exit("[TraceGen::read_plume_step] read %zd plume data points, expected %d @ t=%d", cnt, 3*MAP.nxy, t);
}

void write_cpt_header(FILE *out) {
  const char format_id[] = FILE_VER_CPT;
  fwrite(format_id, sizeof(char), FILE_VER_LENGTH_CPT, out);
  
  fwrite(&MAP,      sizeof(int),  3,       out);
  fwrite(MAP.world, sizeof(char), MAP.nxy, out);

  int params[] = {NT, EX, EY, SWARM->agents->len};
  fwrite(params, sizeof(int), 4, out);

  flt_t dimens[] = {AGENT_RADIUS, SENS_RADIUS};
  fwrite(dimens, sizeof(flt_t), 2, out);
}

void write_cpt_step(int step, FILE *out) {
  fwrite(&step, sizeof(int),   1,       out);
  fwrite(U,     sizeof(flt_t), MAP.nxy, out);
  fwrite(V,     sizeof(flt_t), MAP.nxy, out);
  fwrite(RHO,   sizeof(flt_t), MAP.nxy, out);
  fwrite(DIV,   sizeof(flt_t), MAP.nxy, out);

  agent_t *agents = (agent_t*)SWARM->agents->data;
  for (guint a=0; a<SWARM->agents->len; a++, agents++) fwrite(agents, sizeof(flt_t), 2, out);
}

void update_cpt_NT(int nt, FILE *out) {
  const long OFFSET_BYTES = FILE_VER_LENGTH_CPT*sizeof(char) + 3*sizeof(int) + MAP.nxy*sizeof(char);
  fseek(out,  OFFSET_BYTES, SEEK_SET);
  fwrite(&nt, sizeof(int), 1, out);
}


// ##### DRIVERS #####
void init_swarm(guint num_agents, flt_t lx, flt_t ly, swarm_t *swarm) {
  swarm->agents = g_array_sized_new(FALSE, TRUE, sizeof(agent_t), num_agents);
  g_array_set_size(swarm->agents, num_agents);
  place_agents(num_agents, lx, ly, swarm);
  locate_neighbors(swarm);

  swarm->anemo_threshold = ANEMOMETER_THRESHOLD;
  swarm->chemo_threshold = CHEMICAL_SENS_THRESHOLD;
  swarm->best_time = NT;
  swarm->cast_time = swarm->successes = swarm->pos_error = swarm->odometer  = 0;
}

void generate_cpt_file(cpt_alg_t cpt_algorithm, FILE *in, FILE *out) {
  TIME_STEP = 0;
  read_plume_step(TIME_STEP, in), write_cpt_header(out), write_cpt_step(TIME_STEP, out);
  gboolean found_emit = FALSE;
  for (TIME_STEP=1; TIME_STEP<NT && !ferror(out) && !found_emit; TIME_STEP++) {
    read_plume_step(TIME_STEP, in), found_emit = cpt_algorithm(SWARM), write_cpt_step(TIME_STEP, out);
  }
  if (!ferror(out) && TIME_STEP<NT) update_cpt_NT(TIME_STEP, out);
}

cpt_alg_t lookup_cpt_algorithm(char choice) {
  switch(choice) {
  case 'a': fprintf(stdout, " (*) CPT algorithm   : anemotaxis\n"), fflush(stdout); return cpt_anemo;
  case 'C': fprintf(stdout, " (*) CPT algorithm   : casting   \n"), fflush(stdout); return cpt_casto;
  case 'c': fprintf(stdout, " (*) CPT algorithm   : chemotaxis\n"), fflush(stdout); return cpt_chemo;
  case 'f': fprintf(stdout, " (*) CPT algorithm   : fluxotaxis\n"), fflush(stdout); return cpt_fluxo;
  default : fprintf(stdout, " (*) CPT algorithm   : -- none --\n"), fflush(stdout); return cpt_stayo;
  }
}

gpointer thread_eval_worker(gpointer data) {
  int alg_num             = GPOINTER_TO_INT(data);
  cpt_alg_t cpt_algorithm = EVAL_ALG_FUNC(alg_num);
  for (int start_num=0; start_num<EVAL_NUM_STARTS; start_num++) {
    for (int group_num=0; group_num<EVAL_NUM_GROUPS; group_num++) cpt_algorithm(EVAL->swarms[alg_num][start_num] + group_num);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 4) fprintf(stderr, "Usage: tgen plume_file num_agents {a|c|C|E|f|n} [random_seed=1]\n"), abort();

  g_thread_init(NULL);

  MAP.nx = MAP.ny = MAP.nxy = NT = EX = EY = -1; 
  MAP.world         = NULL;
  SWARM             = NULL;
  EVAL              = NULL;
  U = V = RHO = DIV = NULL;

  FILE *in = fopen(argv[1], "r"), *out = NULL;
  if (in) fprintf(stdout, "\nCPT inverse solver using plume file '%s'\n", argv[1]), fflush(stdout); 
  else error_exit(" (!) cannot open plume file '%s' for reading", argv[1]);

  const guint num_agents = atoi(argv[2]);
  fprintf(stdout, " (*) number of agents: %d\n", num_agents);

  int seed = 1;
  if (argc > 4) seed = atoi(argv[4]);
  fprintf(stdout, " (*) random seed     : %d\n", seed), fflush(stdout);
    
  read_plume_header(in);
  alloc_mem();

  switch (argv[3][0]) {
  case 'E':
    fprintf(stdout, " (*) starting evaluation\n"), fflush(stdout);
    assert(EVAL = calloc(1, sizeof(eval_data_t)));
    char plume_file[PATH_MAX+1];
    PARSE_FILENAME_FROM_PATH(argv[1], DEFAULT_FILENAME_PLUME, plume_file);
    init_eval_data(plume_file, seed);
 
    TIME_STEP = 0;
    read_plume_step(TIME_STEP, in);
    for (TIME_STEP=1; TIME_STEP<NT; TIME_STEP++) {
      fprintf(stdout, " (#) current t = %d\r", TIME_STEP), fflush(stdout);
      read_plume_step(TIME_STEP, in);

#ifdef THREADS
      GThread *worker_anemo = NULL, *worker_casto = NULL, *worker_chemo = NULL;
      assert(worker_anemo = g_thread_create(thread_eval_worker, GINT_TO_POINTER(0), TRUE, NULL));
      assert(worker_casto = g_thread_create(thread_eval_worker, GINT_TO_POINTER(1), TRUE, NULL));
      assert(worker_chemo = g_thread_create(thread_eval_worker, GINT_TO_POINTER(2), TRUE, NULL));
      
      thread_eval_worker(GINT_TO_POINTER(3));

      g_thread_join(worker_chemo);
      g_thread_join(worker_casto);
      g_thread_join(worker_anemo);
#else
      for (int alg_num=0; alg_num<EVAL_NUM_ALGORS; alg_num++) {
	cpt_alg_t cpt_algorithm = EVAL_ALG_FUNC(alg_num);
	for (int start_num=0; start_num<EVAL_NUM_STARTS; start_num++) {
	  for (int group_num=0; group_num<EVAL_NUM_GROUPS; group_num++) {
	    cpt_algorithm(EVAL->swarms[alg_num][start_num] + group_num);
	  }
	}
      }
#endif

    }
    fprintf(stdout, "\n");
    break;

  default:
    assert(SWARM = calloc(1, sizeof(swarm_t)));
    init_swarm(num_agents, INIT_LX, INIT_LY, SWARM);

    out = fopen(DEFAULT_FILENAME_CPT, "w");
    if (!out) error_exit(" (!) cannot open CPT file '%s' for writing", DEFAULT_FILENAME_CPT);
    fprintf(stdout, " (*) CPT file        : '%s'\n", DEFAULT_FILENAME_CPT), fflush(stdout);

    generate_cpt_file(lookup_cpt_algorithm(argv[3][0]), in, out);

    if (ferror(out)) error_exit(" (!) output stream %p:'%s' failed", (void*)out, DEFAULT_FILENAME_CPT);
    fclose(out);
    break;
  }
  fclose(in);
  free_mem();
  fprintf(stdout, " (*) finished\n");
  return 0;
}
