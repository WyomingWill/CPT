/* File: CFD_VIS.c
 * Athr: Dimitri Zarzhitsky
 * Date: March 13, 2004
 */
#include "CFD_VIS.h"


int main(int argc, char *argv[]) {
  VIS_DATA.u = VIS_DATA.v = VIS_DATA.rho = VIS_DATA.div = VIS_DATA.lat = NULL;
  gtk_init(&argc, &argv);
  parse_cmd_arguments(argc, argv);
  INPUT = fopen(CMD_ARGS.input_file, "r");
  if (!INPUT) hdl_error("[CFD_VIS::main] cannot open '%s' for input", CMD_ARGS.input_file);

  signal(SIGINT, SIG_IGN);
  read_header();
  alloc_mem();

  signal(SIGINT, hdl_interrupt_read);
  compute_data_range_file();

  signal(SIGINT, hdl_interrupt_gui);
  activate_gui();

  free_mem();
  fclose(INPUT);
  return 0;
}


// ##### MEMORY UTILS #####
void alloc_mem() {
  VIS_DATA.u   = calloc(MAP.nxy, sizeof(flt_t));
  VIS_DATA.v   = calloc(MAP.nxy, sizeof(flt_t));
  VIS_DATA.rho = calloc(MAP.nxy, sizeof(flt_t));
  VIS_DATA.div = calloc(MAP.nxy, sizeof(flt_t));
  VIS_DATA.lat = calloc(2*NA,    sizeof(flt_t));
  //g_message("[CFD_VIS::alloc_mem] successful");
}

void free_mem() {
  free(VIS_DATA.lat);
  free(VIS_DATA.div);
  free(VIS_DATA.rho);
  free(VIS_DATA.v);
  free(VIS_DATA.u);
  free(MAP.world);
  //g_message("[CFD_VIS::free_mem] successful");
}


// ##### FILE IO #####
void read_header() {
  char in_ver[FILE_VER_LENGTH_CPT+1];
  in_ver[FILE_VER_LENGTH_CPT] = '\0';
  fread(in_ver, sizeof(char), FILE_VER_LENGTH_CPT, INPUT);
  if (strncmp(in_ver, FILE_VER_CPT, FILE_VER_LENGTH_CPT)) hdl_error("[CFD_VIS::read_header] found version '%s', expected '%s'", in_ver, FILE_VER_CPT);

  if (3 != fread(&MAP, sizeof(int), 3, INPUT)) hdl_error("[CFD_VIS::read_header] failed to obtain map dimensions");
  MAP.world = calloc(MAP.nxy, sizeof(char));
  if (MAP.nxy != fread(MAP.world, sizeof(char), MAP.nxy, INPUT)) hdl_error("[CFD_VIS::read_header] failed to obtain map data");

  size_t cnt = (fread(&NT,             sizeof(int),   1, INPUT)
		+ fread(&EX,           sizeof(int),   1, INPUT)
		+ fread(&EY,           sizeof(int),   1, INPUT)
		+ fread(&NA,           sizeof(int),   1, INPUT)
		+ fread(&AGENT_RADIUS, sizeof(flt_t), 1, INPUT)
		+ fread(&SENS_RADIUS,  sizeof(flt_t), 1, INPUT));
  if (cnt != 6) hdl_error("[CFD_VIS::read_header] read %zd trace header points, expected %d", cnt, 6);

  FILE_HEADER_BYTES = FILE_VER_LENGTH_CPT*sizeof(char) + 3*sizeof(int) + MAP.nxy*sizeof(char) + 4*sizeof(int) + 2*sizeof(flt_t);
  FILE_RECORD_BYTES = sizeof(int) + 4*MAP.nxy*sizeof(flt_t) + 2*NA*sizeof(flt_t);
  assert(NT > 0);
}


void read_data(int t) {
  fseeko(INPUT, FILE_HEADER_BYTES + t*FILE_RECORD_BYTES, SEEK_SET);
  size_t cnt = fread(&VIS_DATA.step, sizeof(int), 1, INPUT);
  if (cnt!=1 || VIS_DATA.step!=t) hdl_error("[CFD_VIS::read_data] '%s' is corrupted @ t=%d", CMD_ARGS.input_file, t);
    
  cnt = (fread(VIS_DATA.u,   sizeof(flt_t), MAP.nxy, INPUT) +
	 fread(VIS_DATA.v,   sizeof(flt_t), MAP.nxy, INPUT) +
	 fread(VIS_DATA.rho, sizeof(flt_t), MAP.nxy, INPUT) +
	 fread(VIS_DATA.div, sizeof(flt_t), MAP.nxy, INPUT) +
	 fread(VIS_DATA.lat, sizeof(flt_t), 2*NA,    INPUT));
  if (cnt != 4*MAP.nxy+2*NA) hdl_error("[CFD_VIS::read_data] read %zd flow-field data points, expected %d @ t=%d", cnt, 4*MAP.nxy+2*NA, t);
}


// ##### DATA MANIPULATION #####
void compute_data_range_file() {
  fprintf(stdout, "Extracting min/max values from '%s'... ", CMD_ARGS.input_file), fflush(stdout);
  read_data(0);
  init_range_data(&RANGE_ALL);
  for (int t=0; t<NT; t++) {
    //fprintf(stdout, "%3.0f%%\b\b\b\b", 100.0*t/NT), fflush(stdout);
    read_data(t);
    get_max_vector(VIS_DATA.u, &RANGE_ALL.u);
    get_max_vector(VIS_DATA.v, &RANGE_ALL.v);
    get_min_max_scalar(VIS_DATA.rho, &RANGE_ALL.rho);
    get_min_max_scalar(VIS_DATA.div, &RANGE_ALL.div);
  }
  fprintf(stdout, "done\n");

  fprintf(stdout, "  -=-=- File Data Range Summary -=-=-\n");
  fprintf(stdout, "  U  : % .7e, % .7e\n", RANGE_ALL.u.min, RANGE_ALL.u.max);
  fprintf(stdout, "  V  : % .7e, % .7e\n", RANGE_ALL.v.min, RANGE_ALL.v.max);
  fprintf(stdout, "  RHO: % .7e, % .7e\n", RANGE_ALL.rho.min, RANGE_ALL.rho.max);
  fprintf(stdout, "  DIV: % .7e, % .7e\n", RANGE_ALL.div.min, RANGE_ALL.div.max);
  fprintf(stdout, "  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
  fflush(stdout);
}

void compute_data_range_step() {
  init_range_data(&RANGE_STEP);
  get_max_vector(VIS_DATA.u, &RANGE_STEP.u);
  get_max_vector(VIS_DATA.v, &RANGE_STEP.v);
  get_min_max_scalar(VIS_DATA.rho, &RANGE_STEP.rho);
  get_min_max_scalar(VIS_DATA.div, &RANGE_STEP.div);
}

void get_max_vector(flt_t *data, value_range_t *vrange) {
  flt_t datum;
  for (int i=0; i<MAP.nxy; i++) {
    datum = fabs(data[i]);
    if (datum > vrange->max) vrange->max = datum;
  }
}

void get_min_max_scalar(flt_t *data, value_range_t *vrange) {
  flt_t datum;
  for (int i=0; i<MAP.nxy; i++) {
    datum = data[i];
    if (datum < vrange->min)      vrange->min = datum;
    else if (datum > vrange->max) vrange->max = datum;
  }
}

void init_range_data(range_data_t *rdata) {
  rdata->u.max = fabs(VIS_DATA.u[0]);
  rdata->v.max = fabs(VIS_DATA.v[0]);
  rdata->rho.min = rdata->rho.max = VIS_DATA.rho[0];
  rdata->div.min = rdata->div.max = VIS_DATA.div[0];
}


// ##### STATISTICAL MEASURES #####
flt_t stat_avg_scalar(flt_t *data) {
  flt_t avg = 0;
  for (int i=0; i<MAP.nxy; i++) avg += data[i];
  return avg/MAP.nxy;
}

flt_t stat_avg_vector(flt_t *dataI, flt_t *dataJ) {
  flt_t avg = 0;
  for (int i=0; i<MAP.nxy; i++) avg += hypot(dataI[i], dataJ[i]);
  return avg/MAP.nxy;
}

// The square root of the bias-corrected variance S(N-1) formula
// taken from http://mathworld.wolfram.com/StandardDeviation.html 
flt_t stat_sdev_scalar(flt_t *data, flt_t mean) {
  flt_t sum2 = 0;
  for (int i=0; i<MAP.nxy; i++) sum2 += pow(data[i]-mean, 2); 
  return sqrt(sum2/(MAP.nxy-1));
}

flt_t stat_sdev_vector(flt_t *dataI, flt_t *dataJ, flt_t mean) {
  flt_t sum2 = 0;
  for (int i=0; i<MAP.nxy; i++) sum2 += pow(hypot(dataI[i], dataJ[i])-mean, 2); 
  return sqrt(sum2/(MAP.nxy-1));
}


// ##### MISC UTIL #####
void parse_cmd_arguments(int argc, char *argv[]) {
  if (argc == 1) {
    snprintf(CMD_ARGS.input_file, PATH_MAX, DEFAULT_FILENAME_CPT);
    CMD_ARGS.capture_enabled = FALSE;
  } else if (argc == 2) {
    if ('c' == argv[1][0]) {
      snprintf(CMD_ARGS.input_file, PATH_MAX, DEFAULT_FILENAME_CPT);
      CMD_ARGS.capture_enabled = TRUE;
    } else {
      snprintf(CMD_ARGS.input_file, PATH_MAX, argv[1]);
      CMD_ARGS.capture_enabled = FALSE;
    }
  } else {
    if ('c' == argv[1][0]) {
      snprintf(CMD_ARGS.input_file, PATH_MAX, argv[2]);
      CMD_ARGS.capture_enabled = TRUE;
    } else {
      hdl_error("usage: vis [c] [file]'");
    }
  }
  CMD_ARGS.input_file[PATH_MAX] = '\0';
}

void hdl_interrupt_read(int signal_id) {
  if (INPUT) fclose(INPUT);
  free_mem();
  abort();
}

void hdl_interrupt_gui(int signal_id) {
  gtk_main_quit();
}

void hdl_error(const char *template, ...) {
  if (INPUT) fclose(INPUT);
  free_mem();

  va_list ap;
  va_start(ap, template);
  char msg[256];
  msg[255] = '\0';
  vsnprintf(msg, 255, template, ap);
  va_end(ap);

  g_error(msg);
}
