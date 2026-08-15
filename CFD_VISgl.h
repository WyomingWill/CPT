/* File: CFD_VISgl.h
 * Athr: Dimitri Zarzhitsky
 * Date: March 3, 2004
 *
 * Desc: implements a convinience graphic library based on GTK+ for the CFD
 *       visualization.
 */
#ifndef CFD_VISGL_H
#define CFD_VISGL_H

#include "Common.h"
#include "CFD_VIS_global.h"
#include "CFD_VIS.h"

#define CONFIG_FILENAME ".farrel.cfg"

#define LEGEND_WIDTH        165
#define LEGEND_HEIGHT       267
#define LEGEND_COUNT        20
#define LEGEND_PATCH_WIDTH  64
#define LEGEND_PATCH_HEIGHT 12

#define WIN_TITLE_TEXT "CPT Dev: %s"
//#define CAPTURE_ENTIRE_APP_WINDOW
//#define CAPTURE_LATTICE_STATE_NAME

#define FIXED_FONT_DESC "mono 7"

enum { RHO, VEL, DIV, LATA, LATC, LATF };
#define NUM_PLOT_TYPES 6
const char *PLOT_TYPE_NAMES[NUM_PLOT_TYPES] = { "Density", "Velocity", "Divergence", "Anemotaxis", "Chemotaxis", "Fluxotaxis" };
			    
struct animation_data {
  GtkWidget *hscale_step;
  guint      player_id;
  short      player_direction;
  gboolean   file_info;
} ANIM;

#define SNAPSHOT_PREFIX "snapshot/%s%06.0f.png"
#define CAPTURE_PREFIX "capture/%s%06d.png"
struct capture_data {
  GtkWidget   *win_app;
  GdkColor     clr_foreground;
  GdkColor     clr_background;
  PangoLayout *layout;
  guint        capturer_id;
  int          frame;
  char         basename[PATH_MAX+1];
} CAPTURE;

#define NUM_CHK_BUTTONS 8
#define NUM_SPN_BUTTONS 3
struct gui_elements {
  GdkColor     colors[NUM_HUES];
  GdkColor     shades[NUM_HUES];
  GdkColor     clr_black;
  GdkColor     clr_white;
  GdkColor     clr_error;
  GdkColor     clr_obstacle;
  GdkColor     clr_grid;
  GdkColor     clr_emitter;
  GdkColor     clr_vector;
  GdkColor     clr_gradient;
  GdkColor     clr_agent_foot;
  GdkColor     clr_agent_head;
  GdkColor     clr_agent_sensor;
  GdkColormap *cmap;
  GdkGC       *gc;

  gint UW;
  gint UH;
  gint SW;
  gint SH;
  gint SGS;
  gint VGS;

  GdkCursor *cur_watch;
  GdkCursor *cur_cross;
  GdkPixmap *pix_agent;
  GdkPixmap *pix_out[NUM_PLOT_TYPES];
  gboolean   pix_blank[NUM_PLOT_TYPES];

  GtkWidget *img_credits;

  GtkWidget *da_out;
  GtkWidget *da_legend;
  GtkWidget *vbox_legend;

  GtkWidget *chk_grid;
  GtkWidget *chk_emit;
  GtkWidget *chk_grey;
  GtkWidget *chk_logs;
  GtkWidget *chk_hist;
  GtkWidget *chk_vect;
  GtkWidget *chk_sens;
  GtkWidget *chk_stat;

  PangoFontDescription *pfont_desc;

  GtkWidget *lbl_stat_time;
  GtkWidget *lbl_stat_mouseXY;
  GtkWidget *lbl_stat_valI;
  GtkWidget *lbl_stat_valJ;
  GtkWidget *lbl_stat_lat;
  GtkWidget *lbl_stat_avg;
  GtkWidget *lbl_stat_sdev;

  GtkWidget *mnu_plot_type;
  GtkWidget *spn_sgs;
  GtkWidget *spn_vgs;
  GtkWidget *spn_spd;

  GtkWidget *btn_play_back;
  GtkWidget *btn_step_back;
  GtkWidget *btn_stop;
  GtkWidget *btn_quit;
  GtkWidget *btn_step_fwrd;
  GtkWidget *btn_play_fwrd;
} GUI;

// ##### DATA INTERFACE ROUTINES #####
void activate_gui();

// ##### TIMED PLOT FUNCTIONS #####
gboolean tmr_cancel_file_info(gpointer data);
gboolean tmr_play_plot_fwrd(gpointer data);
gboolean tmr_play_plot_back(gpointer data);
gboolean tmr_take_capture_screenshot(gpointer data);

// ##### KEY EVENT HANDLERS #####
gboolean ek_take_snapshot(GtkWidget *widget, GdkEventKey *event, gpointer data);
gboolean ek_change_plot_type(GtkWidget *widget, GdkEventKey *event, gpointer data);

// ##### EVENT CALLBACKS #####
gboolean cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean cb_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
gboolean cb_lg_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data);
gboolean cb_da_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data);
gboolean cb_mouse_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data);
gboolean cb_mouse_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer data);

// ##### SIGNAL CALLBACKS #####
gboolean cb_grid_toggled(GtkWidget *widget, gpointer data);
gboolean cb_emit_toggled(GtkWidget *widget, gpointer data);
gboolean cb_grey_toggled(GtkWidget *widget, gpointer data);
gboolean cb_logs_toggled(GtkWidget *widget, gpointer data);
gboolean cb_hist_toggled(GtkWidget *widget, gpointer data);
gboolean cb_vect_toggled(GtkWidget *widget, gpointer data);
gboolean cb_sens_toggled(GtkWidget *widget, gpointer data);
gboolean cb_stat_toggled(GtkWidget *widget, gpointer data);
gboolean cb_plot_type_changed(GtkWidget *widget, gpointer data);
gboolean cb_sgs_changed(GtkWidget *widget, gpointer data);
gboolean cb_vgs_changed(GtkWidget *widget, gpointer data);
gboolean cb_spd_changed(GtkWidget *widget, gpointer data);
gboolean cb_anim_step_changed(GtkWidget *widget, gpointer data);
gboolean cb_play_back_button_click(GtkWidget *widget, gpointer data);
gboolean cb_step_back_button_click(GtkWidget *widget, gpointer data);
gboolean cb_stop_button_click(GtkWidget *widget, gpointer data);
gboolean cb_quit_button_click(GtkWidget *widget, gpointer data);
gboolean cb_step_fwrd_button_click(GtkWidget *widget, gpointer data);
gboolean cb_play_fwrd_button_click(GtkWidget *widget, gpointer data);

// ##### INIT FUNCTIONS ######
void build_colors();
void build_gui();
void init_capture();
void init_anim();

// ##### UTILITY FUNCTIONS #####
void       invalidate_all_plots();
void       invalidate_scalar_plots();
void       invalidate_vector_plots();
void       invalidate_lattice_plots();
GtkWidget* make_button(const char *stock_id, const char *text, gboolean left_img);
void       update_stop_button();
gboolean   stop_current_player();
guint      get_anim_speed();
gint       get_cur_plot();
gboolean   cur_plot_scalar(gint cur_plot);
gboolean   cur_plot_vector(gint cur_plot);
gboolean   cur_plot_lattice(gint cur_plot);
gboolean   next_plot();
void       last_plot();
gboolean   prev_plot();
void       frst_plot();

// ##### MISC RENDERING FUNCTIONS #####
void render_info(GdkDrawable *dest, gint width, gint height);
void render_agent(GdkDrawable *dest, gint width, gint height);
void render_error(GdkDrawable *dest, gint width, gint height);

// ##### PLOTTING FUNCTIONS #####
void update_plot(gint cur_plot);
void plot_area(GdkDrawable *dest, GdkColor *color, gint width, gint height);
void plot_obstacles(GdkDrawable *dest);
void plot_grid(GdkDrawable *dest, GdkColor *color, gint width, gint height);
void plot_emit(GdkDrawable *dest, gint width, gint height);
void plot_vector(GdkDrawable *dest, flt_t *dataI, flt_t *dataJ, flt_t maxI, flt_t maxJ);
void plot_scalar(GdkDrawable *dest, GdkColor *hues, flt_t *data, flt_t min, flt_t max);
void plot_lattice(GdkDrawable *dest, gint width, gint height);

// ##### LEGEND FUNCTIONS #####
void       update_legend_vector();
void       update_legend_scalar(gint cur_plot);
void       destroy_legend_item(GtkWidget *widget, gpointer data);
GtkWidget* build_legend_item(flt_t val);

// ##### STATISTICS DISPLAY #####
void update_stats(gint cur_plot);
void show_hover_stats(gdouble ptrX, gdouble ptrY);

// ##### SCREENSHOT UTILITIES #####
int save_screenshot(const char *template, ...) __attribute__ ((format(printf, 1, 2)));

// ##### CONFIG FILE UTILITIES #####
void read_config();
void save_config();

// ##### GROUP FUNCTIONS #####
flt_t gaverage(flt_t *data, int irow, int icol, int size);

#endif
