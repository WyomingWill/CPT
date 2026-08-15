/* File: LAT_VIS.c
 * Athr: Dimitri Zarzhitsky
 * Date: July 4, 2004
 */
#include "LAT_VIS.h"

FILE *INPUT;
map_t MAP;
int NX, NY, EX, EY, NT;
int NXY, FILE_HEADER_BYTES, FILE_LATTICE_OFFSET_BYTES;
lattice_t LATTICE;



int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);

  char *filename = (argc > 1) ? argv[1] : DEFAULT_FILENAME_CPT;
  INPUT = fopen(filename, "r");
  if (!INPUT) hdl_error("[LAT_VIS::main] cannot open '%s' for input", filename);

  signal(SIGINT, SIG_IGN);
  read_header();
  
  signal(SIGINT, hdl_interrupt_read);
  read_data();

  signal(SIGINT, hdl_interrupt_gui);
  activate_gui(filename);

  fclose(INPUT);
  return 0;
}


// ##### FILE IO #####
void read_header() {
  char in_ver[FILE_VER_LENGTH_CPT+1];
  in_ver[FILE_VER_LENGTH_CPT] = '\0';
  fread(in_ver, sizeof(char), FILE_VER_LENGTH_CPT, INPUT);
  if (strncmp(in_ver, FILE_VER_CPT, FILE_VER_LENGTH_CPT)) hdl_error("[LAT_VIS::read_header] found version '%s', expected '%s'", in_ver, FILE_VER_CPT);

  size_t cnt = (fread(&NX, sizeof(int), 1, INPUT) +
		fread(&NY, sizeof(int), 1, INPUT) +
		fread(&NT, sizeof(int), 1, INPUT) +
		fread(&EX, sizeof(int), 1, INPUT) +
		fread(&EY, sizeof(int), 1, INPUT));
  if (cnt != 5) hdl_error("[LAT_VIS::read_header] read %zd header items, expected %d", cnt, 5);

  NXY                       = NX*NY;
  FILE_HEADER_BYTES         = FILE_VER_LENGTH_CPT*sizeof(char) + 5*sizeof(int);
  FILE_LATTICE_OFFSET_BYTES = 4*NXY*sizeof(flt_t);
  assert(NT > 0);
}

void read_data() {
  fseek(INPUT, FILE_HEADER_BYTES, SEEK_SET);
  size_t cnt = 0;
  int time = -1;
  for (int t=0; t<NT && !ferror(INPUT); t++) {
    fread(&time, sizeof(int), 1, INPUT);
    if (time != t) hdl_error("[LAT_VIS::read_data] input file is corrupted @ t=%d", t);
    fseek(INPUT, FILE_LATTICE_OFFSET_BYTES, SEEK_CUR);
    cnt += fread(&LATTICE, sizeof(lattice_t), 1, INPUT);  // XXX - fix me soon!
  }
  if (ferror(INPUT)) hdl_error("[LAT_VIS::read_data] input file is corrupted");
  if (cnt != NT) hdl_error("[LAT_VIS::read_data] read %zd lattice points, expected %d", cnt, NT);
}


// ##### GUI EVENT HANDLERS #####
gboolean cb_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
  GdkDrawable *drw_src = GDK_DRAWABLE(widget->window);

  // update cell dimensions
  GUI.width  = widget->allocation.width  / NY;
  GUI.height = widget->allocation.height / NX;

  // update agent pix
  gint agent_width = fmax(5, GUI.width), agent_height = fmax(5, GUI.height);
  if (GUI.pix_agent) g_object_unref(GUI.pix_agent);
  GUI.pix_agent = gdk_pixmap_new(drw_src, agent_width, agent_height, -1);
  gdk_gc_set_foreground(GUI.gc, &GUI.clr_agent_foot);
  gdk_draw_rectangle(GUI.pix_agent, GUI.gc, TRUE, 0, 0, agent_width, agent_height);
  gdk_gc_set_foreground(GUI.gc, &GUI.clr_agent_head);
  gdk_draw_line(GUI.pix_agent, GUI.gc, 0, 0, agent_width-1, agent_height-1);
  gdk_draw_line(GUI.pix_agent, GUI.gc, 0, agent_height-1, agent_width-1, 0);

  // update plots
  gint pix_width = GUI.width * NY, pix_height = GUI.height * NX;
  for (int i=0; i<NUM_PLOT_TYPES; i++) {
    if (GUI.pix_out[i]) g_object_unref(GUI.pix_out[i]);
    GUI.pix_out[i] = gdk_pixmap_new(drw_src, pix_width, pix_height, -1);
    plot_area(GUI.pix_out[i], &GUI.clr_white, pix_width, pix_height);
    plot_lattice(GUI.pix_out[i], i);
    plot_emit(GUI.pix_out[i], GUI.width, GUI.height);
  }

  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_da_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  gdk_draw_drawable(GDK_DRAWABLE(widget->window), GUI.gc, GDK_DRAWABLE(GUI.pix_out[get_cur_plot()]),
		    event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);
  return TRUE;
}


// ##### GUI SIGNAL CALLBACKS #####
gboolean cb_save_button_click(GtkWidget *widget, gpointer data) {
  if (gtk_dialog_run(GTK_DIALOG(GUI.dlg_save)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(GUI.dlg_save));
    if (filename) {
      GdkDrawable *src = GUI.pix_out[get_cur_plot()];
      gint width, height;
      gdk_drawable_get_size(src, &width, &height);

      GdkPixbuf *screenshot = gdk_pixbuf_get_from_drawable(NULL, src, GUI.cmap, 0, 0, 0, 0,  width, height);
      if (screenshot) {
	GError *err = NULL;
	if (!gdk_pixbuf_save(screenshot, filename, "png", &err, NULL)) {
	  g_warning("[LAT_VIS::cb_save_button_click] %s", err->message);
	  g_error_free(err);
	}
	g_object_unref(screenshot);
      } else {
	g_warning("[LAT_VIS::cb_save_button_click] failed, gdk_pixbuf_get_from_drawable() returned NULL");
      }
      g_free(filename);
    }
  }
  gtk_widget_hide(GUI.dlg_save);
  return TRUE;
}

gboolean cb_plot_type_changed(GtkWidget *widget, gpointer data) {
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}


// ##### GUI HELPER FUNCTIONS #####
void build_colors() {
  gdk_color_parse("black",      &GUI.clr_black);
  gdk_color_parse("white",      &GUI.clr_white);
  gdk_color_parse("PaleGreen4", &GUI.clr_emitter);
  gdk_color_parse("black",      &GUI.clr_agent_foot);
  gdk_color_parse("white",      &GUI.clr_agent_head);

  GUI.cmap = gdk_colormap_get_system();
  GUI.gc   = gdk_gc_new(GDK_DRAWABLE(gdk_get_default_root_window()));
  gdk_gc_set_colormap(GDK_GC(GUI.gc), GDK_COLORMAP(GUI.cmap));

  gboolean result[5];
  gint failures = gdk_colormap_alloc_colors(GUI.cmap, &GUI.clr_black, 5, FALSE, TRUE, result);
  if (failures) g_warning("[LAT_VIS::build_colors] %d colors failed allocation", failures);
}

void build_gui(char *filename) {
  // INIT SIZES
  GUI.width = GUI.height = 1;

  // INIT OFFSCREEN BUFFERS
  memset(&GUI.pix_agent, 0, (1+NUM_PLOT_TYPES)*sizeof(GdkPixmap*));

  // MAIN WINDOW
  GtkWidget *win_app = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  char win_title[256];
  win_title[255] = '\0';
  snprintf(win_title, 255, WIN_TITLE_TEXT, filename);
  gtk_window_set_title(GTK_WINDOW(win_app), win_title);
  gtk_window_set_icon_from_file(GTK_WINDOW(win_app), "icon.png", NULL);
  g_signal_connect(G_OBJECT(win_app), "delete-event", G_CALLBACK(gtk_main_quit), NULL);

  // MAIN VBOX
  GtkWidget *vbox_main = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_main), 0);
  gtk_container_add(GTK_CONTAINER(win_app), vbox_main);

  // VIS FRAME
  GtkWidget *afrm_vis = gtk_aspect_frame_new("Lattice Trace", 0.5, 0.5, -1.0, TRUE);
  gtk_container_set_border_width(GTK_CONTAINER(afrm_vis), 5);
  gtk_box_pack_start(GTK_BOX(vbox_main), afrm_vis, TRUE, TRUE, 0);

  // PLOT DRAWING AREA
  GUI.da_out = gtk_drawing_area_new();
  gtk_widget_set_size_request(GUI.da_out, NY, NX);
  g_signal_connect(G_OBJECT(GUI.da_out), "expose-event", G_CALLBACK(cb_da_expose), NULL);
  g_signal_connect(G_OBJECT(GUI.da_out), "configure-event", G_CALLBACK(cb_configure), NULL);
  gtk_container_add(GTK_CONTAINER(afrm_vis), GUI.da_out);

  // SEPARATOR
  gtk_box_pack_start(GTK_BOX(vbox_main), gtk_hseparator_new(), FALSE, FALSE, 0);

  // CONTROL BOX
  GtkWidget *hbox_ctrl = gtk_hbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(hbox_ctrl), 5);
  gtk_box_pack_start(GTK_BOX(vbox_main), hbox_ctrl, FALSE, FALSE, 0);

  // PLOT TYPE SELECTOR
  GtkWidget *lbl_menu = gtk_label_new("Lattice:");
  gtk_misc_set_alignment(GTK_MISC(lbl_menu), 1, 0.5);
  gtk_box_pack_start(GTK_BOX(hbox_ctrl), lbl_menu, TRUE, TRUE, 0);

  GUI.mnu_plot_type = gtk_combo_box_new_text();
  for (int i=0; i<NUM_PLOT_TYPES; i++) gtk_combo_box_append_text(GTK_COMBO_BOX(GUI.mnu_plot_type), PLOT_TYPE_NAMES[i]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(GUI.mnu_plot_type), 0);
  g_signal_connect(G_OBJECT(GUI.mnu_plot_type), "changed", G_CALLBACK(cb_plot_type_changed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_ctrl), GUI.mnu_plot_type, FALSE, FALSE, 0);

  GtkWidget *btn_save = gtk_button_new_from_stock(GTK_STOCK_SAVE_AS);
  g_signal_connect(btn_save, "clicked", G_CALLBACK(cb_save_button_click), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_ctrl), btn_save, FALSE, FALSE, 0);

  GtkWidget *btn_quit = gtk_button_new_from_stock(GTK_STOCK_QUIT);
  g_signal_connect(btn_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_ctrl), btn_quit, FALSE, FALSE, 0);

  gtk_widget_show_all(win_app);

  GUI.dlg_save = gtk_file_chooser_dialog_new ("Save Picture (PNG)", GTK_WINDOW(win_app), GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
}

void activate_gui(char *filename) {
  build_colors();
  build_gui(filename);
  gtk_main();
}

gint get_cur_plot() {
  gint cur_plot = gtk_combo_box_get_active(GTK_COMBO_BOX(GUI.mnu_plot_type));
  if (cur_plot<0 || cur_plot>=NUM_PLOT_TYPES) {
    g_warning("[LAT_VIS::get_cur_plot] plot selection '%d' unrecognized", cur_plot);
    cur_plot = 0;
  }
  return cur_plot;
}

void plot_area(GdkDrawable *dest, GdkColor *color, gint width, gint height) {
  gdk_gc_set_foreground(GUI.gc, color);
  gdk_draw_rectangle(dest, GUI.gc, TRUE, 0, 0, width, height); 
}

void plot_emit(GdkDrawable *dest, gint width, gint height) {
  const gint ESIZE = fmin(width, GUI.height)/2;
  GdkPoint triangle[3];
  triangle[0].x = EY*width + ESIZE;
  triangle[0].y = EX*height;
  triangle[1].x = triangle[0].x - ESIZE;
  triangle[1].y = triangle[0].y + height;
  triangle[2].x = triangle[0].x + ESIZE;
  triangle[2].y = triangle[1].y;

  gdk_gc_set_foreground(GUI.gc, &GUI.clr_emitter);
  gdk_draw_polygon(dest, GUI.gc, TRUE, triangle, 3);
}

// XXX - needs to be fixed
void plot_lattice(GdkDrawable *dest, int plot_type) {
  agent_t *agents = LATTICE.agents;
  switch (plot_type) {
  case LATA:
  case LATC:   // dimzar-20040411: same as ANEMO for now
    for (int t=0; t<NT; t++) {
      for (int id=0; id<NA; id++) {
	int index = t*NA + id;
	gdk_draw_drawable(dest, GUI.gc, GUI.pix_agent, 0, 0, GUI.width*agents[index].y, GUI.height*agents[index].x, -1, -1);
      }
    }
    break;

  case LATF:
    for (int t=0; t<NT; t++) {
      for (int id=0; id<NA; id+=500) {
	int index = t*NA + id;
	gdk_draw_drawable(dest, GUI.gc, GUI.pix_agent, 0, 0, GUI.width*agents[index].y, GUI.height*agents[index].x, -1, -1);
      }
    }
    break;

  default:
    g_warning("[LAT_VIS::plot_lattice] plot type '%d' unrecognized", plot_type);
    break;
  }
}


// ##### MISC UTIL #####
void hdl_interrupt_read(int signal_id) {
  if (INPUT) fclose(INPUT);
  // XXX - free the lattice?
  abort();
}

void hdl_interrupt_gui(int signal_id) {
  gtk_main_quit();
}

void hdl_error(const char *template, ...) {
  if (INPUT) fclose(INPUT);
  // XXX - free the lattice?

  va_list ap;
  va_start(ap, template);
  char msg[256];
  msg[255] = '\0';
  vsnprintf(msg, 255, template, ap);
  va_end(ap);

  g_error(msg);
}
