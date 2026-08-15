/* File: LAT_VIS.h
 * Athr: Dimitri Zarzhitsky
 * Date: July 4, 2004
 *
 * Desc: GTK+ tool for visualizing lattice movement traces.
 */
#ifndef LAT_VIS_H
#define LAT_VIS_H

#include "Common.h"
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <sys/types.h>

#define WIN_TITLE_TEXT "LAT: %s"

enum { LATA, LATC, LATF };
#define NUM_PLOT_TYPES 3
const char *PLOT_TYPE_NAMES[NUM_PLOT_TYPES] = { "Anemotaxis", "Chemotaxis", "Fluxotaxis" };

struct gui {
  GdkColor     clr_black;
  GdkColor     clr_white;
  GdkColor     clr_emitter;
  GdkColor     clr_agent_foot;
  GdkColor     clr_agent_head;
  GdkColormap *cmap;
  GdkGC       *gc;

  GdkPixmap *pix_agent;
  GdkPixmap *pix_out[NUM_PLOT_TYPES];

  GtkWidget *da_out;
  GtkWidget *mnu_plot_type;
  GtkWidget *dlg_save;

  gint width;
  gint height;
} GUI;


int main(int argc, char *argv[]);

// ##### FILE IO #####
void read_header();
void read_data();

// ##### GUI EVENT HANDLERS #####
gboolean cb_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
gboolean cb_da_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data);

// ##### GUI SIGNAL CALLBACKS #####
gboolean cb_save_button_click(GtkWidget *widget, gpointer data);
gboolean cb_plot_type_changed(GtkWidget *widget, gpointer data);

// ##### GUI HELPER FUNCTIONS #####
void build_colors();
void build_gui(char *filename);
void activate_gui(char *filename);
gint get_cur_plot();
void plot_area(GdkDrawable *dest, GdkColor *color, gint width, gint height);
void plot_emit(GdkDrawable *dest, gint width, gint height);
void plot_lattice(GdkDrawable *dest, int plot_type);

// ##### MISC UTIL #####
void hdl_interrupt_read(int signal_id);
void hdl_interrupt_gui(int signal_id);
void hdl_error(const char *template, ...) __attribute__ ((format(printf, 1, 2)));

#endif
