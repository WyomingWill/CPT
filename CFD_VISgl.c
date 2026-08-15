/* File: CFD_VISgl.c
 * Athr: Dimitri Zarzhitsky
 * Date: September 1, 2004
 */
#include "CFD_VISgl.h"


// ##### DATA INTERFACE ROUTINES #####
void activate_gui() {
  build_colors();
  build_gui();
  init_anim();
  init_capture();
  gtk_main();
}


// ##### TIMED PLOT FUNCTIONS #####
gboolean tmr_cancel_file_info(gpointer data) {
  if (!GUI.da_out || !gtk_widget_get_visible(GUI.da_out)) return TRUE;
  ANIM.file_info = FALSE;
  char msg[] = "<span font_desc='Serif 150' weight='bold'>?</span>";
  pango_layout_set_markup(CAPTURE.layout, msg, strlen(msg));
  invalidate_all_plots();
  gtk_widget_queue_draw(GUI.da_out);
  return FALSE;
}

gboolean tmr_play_plot_fwrd(gpointer data) {
  if (!next_plot()) {
    ANIM.player_direction = ANIM.player_id = 0;
    update_stop_button();
    return FALSE;
  }
  return TRUE;
}

gboolean tmr_play_plot_back(gpointer data) {
  if (!prev_plot()) {
    ANIM.player_direction = ANIM.player_id = 0;
    update_stop_button();
    return FALSE;
  }
  return TRUE;
}

gboolean tmr_take_capture_screenshot(gpointer data) {
  CAPTURE.frame += save_screenshot(CAPTURE_PREFIX, CAPTURE.basename, CAPTURE.frame);
  return TRUE;
}


// ##### KEY EVENT HANDLERS #####
gboolean ek_take_snapshot(GtkWidget *widget, GdkEventKey *event, gpointer data) {
  if (event->keyval == GDK_s) {
    save_screenshot(SNAPSHOT_PREFIX, PLOT_TYPE_NAMES[get_cur_plot()], gtk_range_get_value(GTK_RANGE(ANIM.hscale_step)));
    return TRUE;
  }
  return FALSE;
}

gboolean ek_change_plot_type(GtkWidget *widget, GdkEventKey *event, gpointer data) {
  if ((event->keyval >= GDK_0) && (event->keyval < GDK_0+NUM_PLOT_TYPES)) { 
    gtk_combo_box_set_active(GTK_COMBO_BOX(GUI.mnu_plot_type), event->keyval - GDK_0);
    return TRUE;
  }
  return FALSE;
}


// ##### EVENT CALLBACKS #####
gboolean cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
  save_config();
  gtk_main_quit();
  return TRUE;
}

gboolean cb_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
  GUI.SW = GUI.SGS * (GUI.UW = widget->allocation.width  / MAP.ny);
  GUI.SH = GUI.SGS * (GUI.UH = widget->allocation.height / MAP.nx);

  if (!GUI.UW || !GUI.UH) {
    gtk_widget_set_size_request(widget, MAP.ny, MAP.nx);
  } else {
    gint agent_width = ceil(fmax(2*AGENT_RADIUS*GUI.UW, GUI.UW)), agent_height = ceil(fmax(2*AGENT_RADIUS*GUI.UH, GUI.UH));
    //gint agent_width = fmax(2*AGENT_RADIUS*GUI.UW, GUI.UW), agent_height = fmax(2*AGENT_RADIUS*GUI.UH, GUI.UH);
    if (GUI.pix_agent) g_object_unref(GUI.pix_agent);
    GUI.pix_agent = gdk_pixmap_new(GDK_DRAWABLE(widget->window), agent_width, agent_height, -1);
    render_agent(GUI.pix_agent, agent_width, agent_height);
    
    gint pix_width = GUI.SW * (MAP.ny/GUI.SGS), pix_height = GUI.SH * (MAP.nx/GUI.SGS);
    if (CMD_ARGS.capture_enabled) {
      if (pix_width%2)  pix_width--;
      if (pix_height%2) pix_height--;
    }
    for (int i=0; i<NUM_PLOT_TYPES; i++) {
      if (GUI.pix_out[i]) g_object_unref(GUI.pix_out[i]);
      if (pix_width && pix_height) GUI.pix_out[i] = gdk_pixmap_new(GDK_DRAWABLE(widget->window), pix_width, pix_height, -1);
      else GUI.pix_out[i] = NULL;
    }
    invalidate_all_plots();
  }
  return TRUE;
}

gboolean cb_lg_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  GdkColor *hues = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_grey))) ? GUI.shades : GUI.colors;
  GdkDrawable *dest = GDK_DRAWABLE(widget->window);

  const flt_t STEP = (NUM_HUES-1.0)/(LEGEND_COUNT-1);
  for (int i=0; i<LEGEND_COUNT; i++) {
    gdk_gc_set_foreground(GUI.gc, hues + (int)(i*STEP));
    gdk_draw_rectangle(dest, GUI.gc, TRUE, 0, i*LEGEND_PATCH_HEIGHT, LEGEND_PATCH_WIDTH, LEGEND_PATCH_HEIGHT);
  }
  return TRUE;
}

gboolean cb_da_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  if (ANIM.file_info) {
    render_info(GDK_DRAWABLE(widget->window), widget->allocation.width, widget->allocation.height);
  } else {
    gint cur_plot = get_cur_plot();
    if (GUI.pix_out[cur_plot]) {
      if (GUI.pix_blank[cur_plot]) {
	gdk_window_set_cursor(GDK_WINDOW(CAPTURE.win_app->window), GUI.cur_watch);
	read_data(gtk_range_get_value(GTK_RANGE(ANIM.hscale_step)));
	update_plot(cur_plot);
	update_stats(get_cur_plot());
	gdk_window_set_cursor(GDK_WINDOW(CAPTURE.win_app->window), NULL);
      }
      gdk_draw_drawable(GDK_DRAWABLE(widget->window), GUI.gc, GDK_DRAWABLE(GUI.pix_out[cur_plot]),
			event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);
    } else {
      render_error(GDK_DRAWABLE(widget->window), widget->allocation.width, widget->allocation.height);
    }
  }
  return TRUE;
}

gboolean cb_mouse_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_stat))) show_hover_stats(event->x, event->y);
  return TRUE;
}

gboolean cb_mouse_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_stat))) show_hover_stats(event->x, event->y);
  return TRUE;
}


// ##### SIGNAL CALLBACKS #####     ---(dimzar, check if reductions are possible!)---
gboolean cb_grid_toggled(GtkWidget *widget, gpointer data) {
  invalidate_all_plots();
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_emit_toggled(GtkWidget *widget, gpointer data) {
  invalidate_all_plots();
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_grey_toggled(GtkWidget *widget, gpointer data) {
  invalidate_scalar_plots();
  gint cur_plot = get_cur_plot();
  if (cur_plot_scalar(cur_plot)) {
    update_legend_scalar(cur_plot);
    gtk_widget_queue_draw(GUI.da_legend);
    gtk_widget_queue_draw(GUI.da_out);
  }
  return TRUE;
}

gboolean cb_logs_toggled(GtkWidget *widget, gpointer data) {
  gint cur_plot = get_cur_plot();
  if (cur_plot_scalar(cur_plot)) update_legend_scalar(cur_plot);
  invalidate_all_plots();
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_hist_toggled(GtkWidget *widget, gpointer data) {
  gint cur_plot = get_cur_plot();
  if (cur_plot_scalar(cur_plot)) update_legend_scalar(cur_plot);
  invalidate_all_plots();
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_vect_toggled(GtkWidget *widget, gpointer data) {
  invalidate_vector_plots();
  if (cur_plot_vector(get_cur_plot())) gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_sens_toggled(GtkWidget *widget, gpointer data) {
  invalidate_lattice_plots();
  if (cur_plot_lattice(get_cur_plot())) gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_stat_toggled(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) gdk_window_set_cursor(GDK_WINDOW(GUI.da_out->window), GUI.cur_cross);
  else gdk_window_set_cursor(GDK_WINDOW(GUI.da_out->window), NULL);
  update_stats(get_cur_plot());
  return TRUE;
}

gboolean cb_plot_type_changed(GtkWidget *widget, gpointer data) {
  gint cur_plot = get_cur_plot();
  if (cur_plot_scalar(cur_plot)) update_legend_scalar(cur_plot), gtk_widget_show(GUI.da_legend);
  else if (cur_plot_vector(cur_plot)) update_legend_vector();
  else g_warning("[CFD_VISgl::cb_plot_type_changed] plot selection '%d' unrecognized", cur_plot);
  update_stats(cur_plot);
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_sgs_changed(GtkWidget *widget, gpointer data) {
  GUI.SGS = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  gtk_widget_set_size_request(GUI.da_out, MAP.ny, MAP.nx);
  return TRUE;
}

gboolean cb_vgs_changed(GtkWidget *widget, gpointer data) {
  GUI.VGS = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  invalidate_vector_plots();
  if (cur_plot_vector(get_cur_plot())) gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_spd_changed(GtkWidget *widget, gpointer data) {
  short cur_player_direction = ANIM.player_direction;
  stop_current_player();
  if (cur_player_direction < 0)      cb_play_back_button_click(NULL, NULL);
  else if (cur_player_direction > 0) cb_play_fwrd_button_click(NULL, NULL);
  return TRUE;
}

gboolean cb_anim_step_changed(GtkWidget *widget, gpointer data) {
  invalidate_all_plots();
  gtk_widget_queue_draw(GUI.da_out);
  return TRUE;
}

gboolean cb_play_back_button_click(GtkWidget *widget, gpointer data) {
  short old_player_direction = ANIM.player_direction;
  if (stop_current_player() && old_player_direction<0) {
    frst_plot();
  } else {
    ANIM.player_id = g_timeout_add(get_anim_speed(), tmr_play_plot_back, NULL);
    ANIM.player_direction = -1;
  }
  update_stop_button();
  return TRUE;  
}

gboolean cb_step_back_button_click(GtkWidget *widget, gpointer data) {
  cb_stop_button_click(NULL, NULL);
  prev_plot();
  return TRUE;  
}

gboolean cb_stop_button_click(GtkWidget *widget, gpointer data) {
  stop_current_player();
  update_stop_button();
  return TRUE;
}

gboolean cb_quit_button_click(GtkWidget *widget, gpointer data) {
  cb_delete_event(NULL, NULL, NULL);
  return TRUE;
}

gboolean cb_step_fwrd_button_click(GtkWidget *widget, gpointer data) {
  cb_stop_button_click(NULL, NULL);
  next_plot();
  return TRUE;
}

gboolean cb_play_fwrd_button_click(GtkWidget *widget, gpointer data) {
  short old_player_direction = ANIM.player_direction;
  if (stop_current_player() && old_player_direction>0) {
    last_plot();
  } else {
    ANIM.player_id = g_timeout_add(get_anim_speed(), tmr_play_plot_fwrd, NULL);
    ANIM.player_direction = 1;
  }
  update_stop_button();
  return TRUE;  
}


// ##### INIT FUNCTIONS ######
void build_colors() {
  for (int s=0; s<=NUM_HUES-1; s++) {
    GdkColor *color = GUI.shades + s;
    //color->red = color->green = color->blue = USHRT_MAX/(NUM_HUES-1.) * s;
    color->red = USHRT_MAX; color->green = color->blue = fmax(0, USHRT_MAX - (USHRT_MAX * (double)s)/(.77*NUM_HUES-1));
  }

  for (int s=0; s<NUM_HUES; s++) {
    GdkColor *color = GUI.colors + s;
    double slope    = 5. * USHRT_MAX/(NUM_HUES-1.);
    if (s < 1.*NUM_HUES/5) {
      color->blue  = slope * s;
      color->green = 0;
      color->red   = 0;
    } else if (s < 2.*NUM_HUES/5) {
      color->blue  = USHRT_MAX;
      color->green = slope * (s - 1.*NUM_HUES/5);
      color->red   = 0;
    } else if (s < 3.*NUM_HUES/5) {
      color->blue  = USHRT_MAX - slope*(s - 2.*NUM_HUES/5);
      color->green = USHRT_MAX;
      color->red   = 0;
    } else if (s < 4.*NUM_HUES/5) {
      color->blue  = 0;
      color->green = USHRT_MAX;
      color->red   = slope * (s - 3.*NUM_HUES/5);
    } else {
      color->blue  = 0;
      color->green = USHRT_MAX - slope*(s - 4.*NUM_HUES/5);
      color->red   = USHRT_MAX;
    }
  }

  gdk_color_parse("black",          &GUI.clr_black);
  gdk_color_parse("white",          &GUI.clr_white);
  gdk_color_parse("firebrick3",     &GUI.clr_error);
  gdk_color_parse("DarkSlateGray",  &GUI.clr_obstacle);
  gdk_color_parse("grey77",         &GUI.clr_grid);
  gdk_color_parse("navy",           &GUI.clr_emitter);
  gdk_color_parse("SeaGreen",       &GUI.clr_vector);
  gdk_color_parse("yellow",         &GUI.clr_gradient);
  gdk_color_parse("black",          &GUI.clr_agent_foot);
  gdk_color_parse("white",          &GUI.clr_agent_head);
  gdk_color_parse("maroon",         &GUI.clr_agent_sensor);

  GUI.cmap = gdk_colormap_get_system();
  GUI.gc   = gdk_gc_new(GDK_DRAWABLE(gdk_get_default_root_window()));
  gdk_gc_set_colormap(GDK_GC(GUI.gc), GDK_COLORMAP(GUI.cmap));

  gboolean res1[NUM_HUES];
  gint failures = (gdk_colormap_alloc_colors(GUI.cmap, GUI.shades, NUM_HUES, FALSE, TRUE, res1) +
		   gdk_colormap_alloc_colors(GUI.cmap, GUI.colors, NUM_HUES, FALSE, TRUE, res1));
  if (failures) g_warning("[CFD_VISgl::build_colors] %d hues failed allocation", failures);

  gboolean res2[11];
  failures = gdk_colormap_alloc_colors(GUI.cmap, &GUI.clr_black, 11, FALSE, TRUE, res2);
  if (failures) g_warning("[CFD_VISgl::build_colors] %d colors failed allocation", failures);
}

void build_gui() {
  // INIT SIZES
  GUI.UW = GUI.SW = GUI.UH = GUI.SH = 1;
  GUI.SGS = GUI.VGS = 1;

  // CURSOR CUES + OFFSCREEN BUFFERS
  GUI.cur_watch = gdk_cursor_new(GDK_WATCH);
  GUI.cur_cross = gdk_cursor_new(GDK_CROSSHAIR);
  memset(&GUI.pix_agent, 0, (1+NUM_PLOT_TYPES)*sizeof(GdkPixmap*));
  invalidate_all_plots();

  // INFORMATIONAL GRAPHIC
  GUI.img_credits = gtk_image_new_from_file("credits.png");
  g_object_ref(GUI.img_credits);

  // MAIN WINDOW
  CAPTURE.win_app = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  char win_title[255+1];
  win_title[255] = '\0';
  snprintf(win_title, 255, WIN_TITLE_TEXT, CMD_ARGS.input_file);
  gtk_window_set_title(GTK_WINDOW(CAPTURE.win_app), win_title);
  gtk_window_set_icon_from_file(GTK_WINDOW(CAPTURE.win_app), "icon.png", NULL);
  g_signal_connect(G_OBJECT(CAPTURE.win_app), "delete-event", G_CALLBACK(cb_delete_event), NULL);
  g_signal_connect(G_OBJECT(CAPTURE.win_app), "key-press-event", G_CALLBACK(ek_take_snapshot), NULL);

  // MAIN VBOX
  GtkWidget *vbox_main = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_main), 0);
  gtk_container_add(GTK_CONTAINER(CAPTURE.win_app), vbox_main);

  // PLOT + LEGEND HBOX
  GtkWidget *hbox_pl = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_main), hbox_pl);

  // SIM FRAME
  GtkWidget *afrm_sim = gtk_aspect_frame_new("Simulation Data", 0.5, 0.5, -1.0, TRUE);
  gtk_container_set_border_width(GTK_CONTAINER(afrm_sim), 5);
  gtk_box_pack_start(GTK_BOX(hbox_pl), afrm_sim, TRUE, TRUE, 0);
  
  // PLOT DRAWING AREA
  GUI.da_out = gtk_drawing_area_new();
  gtk_widget_set_size_request(GUI.da_out, MAP.ny, MAP.nx);
  gtk_widget_set_can_focus(GUI.da_out, TRUE);
  gtk_widget_add_events(GUI.da_out, GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(GUI.da_out), "expose-event", G_CALLBACK(cb_da_expose), NULL);
  g_signal_connect(G_OBJECT(GUI.da_out), "configure-event", G_CALLBACK(cb_configure), NULL);
  g_signal_connect(G_OBJECT(GUI.da_out), "leave-notify-event", G_CALLBACK(cb_mouse_leave), NULL);
  g_signal_connect(G_OBJECT(GUI.da_out), "motion-notify-event", G_CALLBACK(cb_mouse_motion), NULL);
  g_signal_connect(G_OBJECT(GUI.da_out), "key-press-event", G_CALLBACK(ek_change_plot_type), NULL);
  gtk_container_add(GTK_CONTAINER(afrm_sim), GUI.da_out);

  // SIDEBAR box
  GtkWidget *vbox_sidebar = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_pl), vbox_sidebar, FALSE, FALSE, 0);

  // LEGEND 
  GtkWidget *frm_legend = gtk_frame_new("Legend");
  gtk_widget_set_size_request(frm_legend, LEGEND_WIDTH, LEGEND_HEIGHT);
  gtk_container_set_border_width(GTK_CONTAINER(frm_legend), 5);
  gtk_box_pack_start(GTK_BOX(vbox_sidebar), frm_legend, FALSE, FALSE, 0);

  GtkWidget *hbox_legend = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frm_legend), hbox_legend);

  GUI.da_legend = gtk_drawing_area_new();
  gtk_widget_set_size_request(GUI.da_legend, LEGEND_PATCH_WIDTH, LEGEND_COUNT*LEGEND_PATCH_HEIGHT);
  g_signal_connect(G_OBJECT(GUI.da_legend), "expose-event", G_CALLBACK(cb_lg_expose), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_legend), GUI.da_legend, FALSE, FALSE, 0);

  GUI.vbox_legend = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_legend), GUI.vbox_legend, TRUE, FALSE, 0);

  // OPTIONS frame
  GtkWidget *frm_opts = gtk_frame_new("Options");
  gtk_container_set_border_width(GTK_CONTAINER(frm_opts), 5);
  gtk_box_pack_start(GTK_BOX(vbox_sidebar), frm_opts, FALSE, FALSE, 0);

  // OPTIONS table
  GtkWidget *tbl_opts = gtk_table_new(4, 2, TRUE);
  gtk_container_add(GTK_CONTAINER(frm_opts), tbl_opts);

  // OPTIONS check buttons
  GUI.chk_grid  = gtk_check_button_new_with_label("Grid");
  g_signal_connect(GUI.chk_grid, "toggled", G_CALLBACK(cb_grid_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_grid, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_emit  = gtk_check_button_new_with_label("Emit");
  g_signal_connect(GUI.chk_emit, "toggled", G_CALLBACK(cb_emit_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_emit, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_grey  = gtk_check_button_new_with_label("Grey");
  g_signal_connect(GUI.chk_grey, "toggled", G_CALLBACK(cb_grey_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_grey, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_logs  = gtk_check_button_new_with_label("Log");
  g_signal_connect(GUI.chk_logs, "toggled", G_CALLBACK(cb_logs_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_logs, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_hist  = gtk_check_button_new_with_label("Hist");
  g_signal_connect(GUI.chk_hist, "toggled", G_CALLBACK(cb_hist_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_hist, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_vect  = gtk_check_button_new_with_label("1:1");
  g_signal_connect(GUI.chk_vect, "toggled", G_CALLBACK(cb_vect_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_vect, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_sens = gtk_check_button_new_with_label("Sens");
  g_signal_connect(GUI.chk_sens, "toggled", G_CALLBACK(cb_sens_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_sens, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);

  GUI.chk_stat = gtk_check_button_new_with_label("Stats");
  g_signal_connect(GUI.chk_stat, "toggled", G_CALLBACK(cb_stat_toggled), NULL);
  gtk_table_attach(GTK_TABLE(tbl_opts), GUI.chk_stat, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);

  // STATISTICS frame
  GtkWidget *frm_stats = gtk_frame_new("Stats");
  gtk_container_set_border_width(GTK_CONTAINER(frm_stats), 5);
  gtk_box_pack_start(GTK_BOX(vbox_sidebar), frm_stats, FALSE, FALSE, 0);

  // STATISTICS vbox
  GtkWidget *vbox_stats = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_stats), 5);
  gtk_container_add(GTK_CONTAINER(frm_stats), vbox_stats);

  // STATISTICS labels
  GUI.pfont_desc = pango_font_description_from_string(FIXED_FONT_DESC);
  GUI.lbl_stat_time = gtk_label_new("TIME : --");
  gtk_widget_modify_font(GUI.lbl_stat_time, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_time), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_time);

  GUI.lbl_stat_mouseXY = gtk_label_new("(X,Y): (-,-)");
  gtk_widget_modify_font(GUI.lbl_stat_mouseXY, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_mouseXY), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_mouseXY);

  GUI.lbl_stat_valI = gtk_label_new("VALUE: --");
  gtk_widget_modify_font(GUI.lbl_stat_valI, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_valI), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_valI);

  GUI.lbl_stat_valJ = gtk_label_new("         ");
  gtk_widget_modify_font(GUI.lbl_stat_valJ, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_valJ), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_valJ);

  GUI.lbl_stat_lat = gtk_label_new("LATCE: --");
  gtk_widget_modify_font(GUI.lbl_stat_lat, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_lat), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_lat);

  GUI.lbl_stat_avg = gtk_label_new("AVRGE: --");
  gtk_widget_modify_font(GUI.lbl_stat_avg, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_avg), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_avg);

  GUI.lbl_stat_sdev = gtk_label_new("S.DEV: --");
  gtk_widget_modify_font(GUI.lbl_stat_sdev, GUI.pfont_desc);
  gtk_misc_set_alignment(GTK_MISC(GUI.lbl_stat_sdev), 0, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_stats), GUI.lbl_stat_sdev);

  // CONTROL FRAME
  GtkWidget *frm_cntr = gtk_frame_new(NULL);
  gtk_container_set_border_width(GTK_CONTAINER(frm_cntr), 5);
  gtk_box_pack_start(GTK_BOX(vbox_main), frm_cntr, FALSE, FALSE, 0);
  
  // CONTROL VBOX
  GtkWidget *vbox_cntr = gtk_vbox_new(FALSE, 5);
  gtk_container_add(GTK_CONTAINER(frm_cntr), vbox_cntr);

  // OPTION CONTROL HBOX
  GtkWidget *hbox_opt = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_cntr), hbox_opt);

  // OPTION PLOT TYPE MENU
  GUI.mnu_plot_type = gtk_combo_box_new_text();
  for (int i=0; i<NUM_PLOT_TYPES; i++) gtk_combo_box_append_text(GTK_COMBO_BOX(GUI.mnu_plot_type), PLOT_TYPE_NAMES[i]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(GUI.mnu_plot_type), 0);
  g_signal_connect(G_OBJECT(GUI.mnu_plot_type), "changed", G_CALLBACK(cb_plot_type_changed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_opt), GUI.mnu_plot_type, FALSE, FALSE, 0);

  // GROUP SIZE CONTROLS
  GtkWidget *lbl_sgs = gtk_label_new("Scalar GS: ");
  gtk_misc_set_alignment(GTK_MISC(lbl_sgs), 1, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(hbox_opt), lbl_sgs);
  GUI.spn_sgs = gtk_spin_button_new_with_range(1, 9, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(GUI.spn_sgs), TRUE);
  gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(GUI.spn_sgs), GTK_UPDATE_IF_VALID);
  gtk_widget_set_size_request(GUI.spn_sgs, 30, 22);
  g_signal_connect(G_OBJECT(GUI.spn_sgs), "value-changed", G_CALLBACK(cb_sgs_changed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_opt), GUI.spn_sgs, FALSE, FALSE, 0);

  GtkWidget *lbl_vgs = gtk_label_new("Vector GS: ");
  gtk_misc_set_alignment(GTK_MISC(lbl_vgs), 1, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(hbox_opt), lbl_vgs);
  GUI.spn_vgs = gtk_spin_button_new_with_range(1, 9, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(GUI.spn_vgs), TRUE);
  gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(GUI.spn_vgs), GTK_UPDATE_IF_VALID);
  gtk_widget_set_size_request(GUI.spn_vgs, 30, 22);
  g_signal_connect(G_OBJECT(GUI.spn_vgs), "value-changed", G_CALLBACK(cb_vgs_changed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_opt), GUI.spn_vgs, FALSE, FALSE, 0);
  
  // OPTION ANIMATION SPEED SPIN BUTTON
  GtkWidget *label = gtk_label_new("Speed: ");
  gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
  gtk_box_pack_start_defaults(GTK_BOX(hbox_opt), label);
  GUI.spn_spd = gtk_spin_button_new_with_range(1, 9, 1);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(GUI.spn_spd), TRUE);
  gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(GUI.spn_spd), GTK_UPDATE_IF_VALID);
  gtk_widget_set_size_request(GUI.spn_spd, 30, 22);
  g_signal_connect(G_OBJECT(GUI.spn_spd), "value-changed", G_CALLBACK(cb_spd_changed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_opt), GUI.spn_spd, FALSE, FALSE, 0);

  // ANIMATION STEP HSCALE
  ANIM.hscale_step = gtk_hscale_new_with_range(0, NT-1, 10);
  gtk_scale_set_draw_value(GTK_SCALE(ANIM.hscale_step), FALSE);
  gtk_range_set_update_policy(GTK_RANGE(ANIM.hscale_step), GTK_UPDATE_DELAYED);
  g_signal_connect(G_OBJECT(ANIM.hscale_step), "value-changed", G_CALLBACK(cb_anim_step_changed), NULL);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_cntr), ANIM.hscale_step);

  // BUTTON CONTROL HBOX
  GtkWidget *hbox_btn = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox_btn), GTK_BUTTONBOX_SPREAD);
  gtk_box_pack_start_defaults(GTK_BOX(vbox_cntr), hbox_btn);
  
  GUI.btn_play_back = make_button(GTK_STOCK_GOTO_FIRST, "Play", TRUE);
  g_signal_connect(G_OBJECT(GUI.btn_play_back), "clicked", G_CALLBACK(cb_play_back_button_click), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_btn), GUI.btn_play_back, FALSE, FALSE, 0);

  GUI.btn_step_back = make_button(GTK_STOCK_GO_BACK, "Step", TRUE);
  g_signal_connect(G_OBJECT(GUI.btn_step_back), "clicked", G_CALLBACK(cb_step_back_button_click), NULL);
  gtk_box_pack_start(GTK_BOX(hbox_btn), GUI.btn_step_back, FALSE, FALSE, 0);

  // make the stop button, but don't add it to the box just yet ...
  GUI.btn_stop = make_button(GTK_STOCK_STOP, "Stop", TRUE);
  g_signal_connect(G_OBJECT(GUI.btn_stop), "clicked", G_CALLBACK(cb_stop_button_click), NULL);

  GUI.btn_quit = make_button(GTK_STOCK_QUIT, "Quit", TRUE);
  g_signal_connect(G_OBJECT(GUI.btn_quit), "clicked", G_CALLBACK(cb_quit_button_click), NULL);
  gtk_box_pack_start_defaults(GTK_BOX(hbox_btn), GUI.btn_quit);

  GUI.btn_step_fwrd = make_button(GTK_STOCK_GO_FORWARD, "Step", FALSE);
  g_signal_connect(G_OBJECT(GUI.btn_step_fwrd), "clicked", G_CALLBACK(cb_step_fwrd_button_click), NULL);
  gtk_box_pack_start_defaults(GTK_BOX(hbox_btn), GUI.btn_step_fwrd);

  GUI.btn_play_fwrd = make_button(GTK_STOCK_GOTO_LAST, "Play", FALSE);
  g_signal_connect(G_OBJECT(GUI.btn_play_fwrd), "clicked", G_CALLBACK(cb_play_fwrd_button_click), NULL);
  gtk_box_pack_start_defaults(GTK_BOX(hbox_btn), GUI.btn_play_fwrd);

  gtk_widget_show_all(CAPTURE.win_app);

  // now it's safe to add the stop button (make it the 3rd from the left)
  gtk_box_pack_start_defaults(GTK_BOX(hbox_btn), GUI.btn_stop);
  gtk_box_reorder_child(GTK_BOX(hbox_btn), GUI.btn_stop, 2);

  read_config();
}

void init_anim() {
  gtk_range_set_value(GTK_RANGE(ANIM.hscale_step), 0);
  ANIM.player_id        = 0;
  ANIM.player_direction = 0;
  ANIM.file_info        = TRUE;
  g_timeout_add(900, tmr_cancel_file_info, NULL);
}

void init_capture() {
  gdk_color_parse("FloralWhite", &CAPTURE.clr_foreground);
  gdk_color_parse("DodgerBlue4", &CAPTURE.clr_background);
  gdk_colormap_alloc_color(GUI.cmap, &CAPTURE.clr_foreground, FALSE, TRUE);
  gdk_colormap_alloc_color(GUI.cmap, &CAPTURE.clr_background, FALSE, TRUE);
  
  CAPTURE.layout = gtk_widget_create_pango_layout(GUI.da_out, NULL);
  GString *text = g_string_new("<span font_desc='Sans 14'><span stretch='extraexpanded' size='larger' weight='bold'>CPT Sim</span>");
  g_string_append_printf(text, "\n\nData File: <tt>%s</tt>\nWidth x Height: %d x %d\nEmitter: (%d, %d)\nTime Steps: %d</span>",
			 CMD_ARGS.input_file, MAP.ny, MAP.nx, EX, EY, NT);
  pango_layout_set_markup(CAPTURE.layout, text->str, text->len);
  g_string_free(text, TRUE);
  
  CAPTURE.capturer_id = 0;
  CAPTURE.frame       = 0;
  if (CMD_ARGS.capture_enabled) {
    PARSE_FILENAME_FROM_PATH(CMD_ARGS.input_file, "GENERIC_PLUME_FILE", CAPTURE.basename);
    CAPTURE.capturer_id = g_timeout_add(100, tmr_take_capture_screenshot, NULL);
  }
}


// ##### UTILITY FUNCTIONS #####
void invalidate_all_plots() {
  memset(GUI.pix_blank, 1, NUM_PLOT_TYPES*sizeof(gboolean));
}

void invalidate_scalar_plots() {
  GUI.pix_blank[RHO] = GUI.pix_blank[DIV] = GUI.pix_blank[LATC] = GUI.pix_blank[LATF] = TRUE;
}

void invalidate_vector_plots() {
  GUI.pix_blank[VEL] = GUI.pix_blank[LATA] = GUI.pix_blank[LATF] = TRUE;
}

void invalidate_lattice_plots() {
  GUI.pix_blank[LATA] = GUI.pix_blank[LATC] = GUI.pix_blank[LATF] = TRUE;
}

GtkWidget *make_button(const char *stock_id, const char *text, gboolean left_img) {
  GtkWidget *btn          = gtk_button_new();
  GtkWidget *hbox_content = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(btn), hbox_content);
  
  GtkWidget *image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
  GtkWidget *label = gtk_label_new(text);
  if (left_img) {
    gtk_misc_set_alignment(GTK_MISC(image), 0.9, 0.5);
    gtk_misc_set_alignment(GTK_MISC(label), 0.1, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox_content), image, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_content), label, TRUE, TRUE, 0);
  } else {
    gtk_misc_set_alignment(GTK_MISC(label), 0.9, 0.5);
    gtk_misc_set_alignment(GTK_MISC(image), 0.1, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox_content), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_content), image, TRUE, TRUE, 0);
  }
  return btn;
}

void update_stop_button() {
  if (ANIM.player_id > 0) {
    gtk_widget_hide_all(GUI.btn_quit);
    gtk_widget_show_all(GUI.btn_stop);
  } else {
    gtk_widget_hide_all(GUI.btn_stop);
    gtk_widget_show_all(GUI.btn_quit);
  }
}

gboolean stop_current_player() {
  if (ANIM.player_id > 0) {
    g_source_remove(ANIM.player_id);
    ANIM.player_direction = ANIM.player_id = 0;
    return TRUE;
  }
  return FALSE;
}

guint get_anim_speed() {
  switch(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(GUI.spn_spd))) {
  case  1: return 900;
  case  2: return 650;
  case  3: return 400;
  case  4: return 200;
  case  5: return 100;
  case  6: return  70;
  case  7: return  40;
  case  8: return  15;
  case  9: 
  default: return   5;
  }
}

gint get_cur_plot() {
  return gtk_combo_box_get_active(GTK_COMBO_BOX(GUI.mnu_plot_type));
}

gboolean cur_plot_scalar(gint cur_plot) {
  return cur_plot==RHO || cur_plot==DIV || cur_plot==LATC || cur_plot==LATF;
}

gboolean cur_plot_vector(gint cur_plot) {
  return cur_plot==VEL || cur_plot==LATA || cur_plot==LATF;
}

gboolean cur_plot_lattice(gint cur_plot) {
  return cur_plot==LATA || cur_plot==LATC || cur_plot==LATF;
}

gboolean next_plot() {
  int anim_step = gtk_range_get_value(GTK_RANGE(ANIM.hscale_step)) + 1;
  if (anim_step < NT) {
    gtk_range_set_value(GTK_RANGE(ANIM.hscale_step), anim_step);
    return TRUE;
  }
  return FALSE;
}

void last_plot() {
  gtk_range_set_value(GTK_RANGE(ANIM.hscale_step), NT-1);
}

gboolean prev_plot() {
  int anim_step = gtk_range_get_value(GTK_RANGE(ANIM.hscale_step)) - 1;
  if (anim_step >= 0) {
    gtk_range_set_value(GTK_RANGE(ANIM.hscale_step), anim_step);
    return TRUE;
  }
  return FALSE;
}

void frst_plot() {
  gtk_range_set_value(GTK_RANGE(ANIM.hscale_step), 0);
}


// ##### MISC RENDERING FUNCTIONS #####
void render_info(GdkDrawable *dest, gint width, gint height) {
  plot_area(dest, &CAPTURE.clr_background, width, height);
  gdk_draw_layout_with_colors(dest, GUI.gc, 0, 0, CAPTURE.layout, &CAPTURE.clr_foreground, &CAPTURE.clr_background);
}

void render_agent(GdkDrawable *dest, gint width, gint height) {
  gdk_gc_set_foreground(GUI.gc, &GUI.clr_agent_foot);
  gdk_draw_rectangle(dest, GUI.gc, TRUE, 0, 0, width, height);
  
  gdk_gc_set_foreground(GUI.gc, &GUI.clr_agent_head);
  //gdk_gc_set_line_attributes(GUI.gc, 2, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
  gdk_draw_line(dest, GUI.gc, 0, 0, width-1, height-1);
  gdk_draw_line(dest, GUI.gc, 0, height-1, width-1, 0);
  //gdk_gc_set_line_attributes(GUI.gc, 1, GDK_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);
}

void render_error(GdkDrawable *dest, gint width, gint height) {
  gdk_gc_set_foreground(GUI.gc, &GUI.clr_error);
  gdk_draw_arc(dest, GUI.gc, TRUE, 0, 0, width, height, 0, 64*360);
  gdk_draw_layout_with_colors(dest, GUI.gc, width/2-40, height/2-115, CAPTURE.layout, &GUI.clr_white, NULL);
}


// ##### PLOTTING FUNCTIONS #####
void update_plot(gint cur_plot) {
  if (!GUI.pix_out[cur_plot])   g_warning("called update_plot on a NULL pix");
  if (!GUI.pix_blank[cur_plot]) g_warning("called update_plot on a valid pix");

  GdkDrawable *dest = GDK_DRAWABLE(GUI.pix_out[cur_plot]);
  gint width, height;
  gdk_drawable_get_size(dest, &width, &height);

  GdkColor *hues = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_grey)) ? GUI.shades : GUI.colors;
  
  range_data_t *rsrc = &RANGE_ALL;
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_hist))) {
    compute_data_range_step();
    rsrc = &RANGE_STEP;
  }
  
  switch(cur_plot) {
  case RHO:
    plot_scalar(dest, hues, VIS_DATA.rho, rsrc->rho.min, rsrc->rho.max);
    break;
    
  case VEL:
    plot_area(dest, &GUI.clr_white, width, height);
    plot_vector(dest, VIS_DATA.u, VIS_DATA.v, rsrc->u.max, rsrc->v.max);
    break;
    
  case DIV:
    plot_scalar(dest, hues, VIS_DATA.div, rsrc->div.min, rsrc->div.max);
    break;
    
  case LATA:
    plot_scalar(dest, hues, VIS_DATA.rho, rsrc->rho.min, rsrc->rho.max);
    plot_vector(dest, VIS_DATA.u, VIS_DATA.v, rsrc->u.max, rsrc->v.max);
    plot_lattice(dest, width, height);
    break;
    
  case LATC:
    plot_scalar(dest, hues, VIS_DATA.rho, rsrc->rho.min, rsrc->rho.max);
    plot_lattice(dest, width, height);
    break;

  case LATF:
    //plot_scalar(dest, hues, VIS_DATA.div, rsrc->div.min, rsrc->div.max);
    plot_scalar(dest, hues, VIS_DATA.rho, rsrc->rho.min, rsrc->rho.max);
    plot_vector(dest, VIS_DATA.u, VIS_DATA.v, rsrc->u.max, rsrc->v.max);
    plot_lattice(dest, width, height);
    break;

  default:
    g_warning("[CFD_VISgl::update_plot] plot selection '%d' unrecognized", cur_plot);
    break;
  }
  plot_obstacles(dest);
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_hist)) && cur_plot_scalar(cur_plot)) update_legend_scalar(cur_plot);
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_emit))) plot_emit(dest, GUI.UW, GUI.UH);
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_grid))) plot_grid(dest, &GUI.clr_grid, width, height);
  GUI.pix_blank[cur_plot] = FALSE;
}

void plot_area(GdkDrawable *dest, GdkColor *color, gint width, gint height) {
  gdk_gc_set_foreground(GUI.gc, color);
  gdk_draw_rectangle(dest, GUI.gc, TRUE, 0, 0, width, height);
}

void plot_obstacles(GdkDrawable *dest) {
  gdk_gc_set_foreground(GUI.gc, &GUI.clr_obstacle);
  for (int i=0; i<MAP.nx; i++) {
    for (int j=0; j<MAP.ny; j++) {
      if (map_is_blocked_point(i, j)) gdk_draw_rectangle(dest, GUI.gc, TRUE, j*GUI.UW, i*GUI.UH, GUI.UW, GUI.UH);
    }
  }
}

void plot_grid(GdkDrawable *dest, GdkColor *color, gint width, gint height) {
  gdk_gc_set_foreground(GUI.gc, color);
  for (gint x=GUI.SW; x<width; x+=GUI.SW)  gdk_draw_line(dest, GUI.gc, x, 0, x, height);
  for (gint y=GUI.SH; y<height; y+=GUI.SH) gdk_draw_line(dest, GUI.gc, 0, y, width, y);
}

#define EMIT_MAG_FACTOR 3
void plot_emit(GdkDrawable *dest, gint width, gint height) {
  const gint ESIZE = fmin(width, height)/2;
  GdkPoint triangle[3];

  // Note the reversal of x,y in the coordiantes
  triangle[0].x = EY*width + ESIZE;
  triangle[0].y = EX*height              - EMIT_MAG_FACTOR;
  triangle[1].x = triangle[0].x - ESIZE  - EMIT_MAG_FACTOR;
  triangle[1].y = triangle[0].y + height + EMIT_MAG_FACTOR;
  triangle[2].x = triangle[0].x + ESIZE  + EMIT_MAG_FACTOR;
  triangle[2].y = triangle[1].y;

  gdk_gc_set_foreground(GUI.gc, &GUI.clr_emitter);
  gdk_draw_polygon(dest, GUI.gc, TRUE, triangle, 3);
}

void plot_vector(GdkDrawable *dest, flt_t *dataI, flt_t *dataJ, flt_t maxI, flt_t maxJ) {
  const gint ARROW_XOFFSET = GUI.VGS * (GUI.UW/2), ARROW_YOFFSET = GUI.VGS * (GUI.UH/2);
  const gint ARROW_LEN     = GUI.VGS * 0.97*fmin(GUI.UW, GUI.UH);
  const gfloat ARROWHEAD_REDUCTION = 0.7, THETA_INCR = M_PI/16;

  gboolean logs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_logs));
  if (logs) {
    maxI = log(1+maxI);
    maxJ = log(1+maxJ);
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_vect))) maxI = maxJ = fmax(maxI, maxJ);

  // note the reversal of X and Y
  double arrow_xstep = maxJ ? ARROW_LEN/maxJ : 0, arrow_ystep = maxI ? ARROW_LEN/maxI : 0;

  gdk_gc_set_foreground(GUI.gc, &GUI.clr_vector);
  for (int i=0; i+GUI.VGS<=MAP.nx; i+=GUI.VGS) {
    for (int j=0; j+GUI.VGS<=MAP.ny; j+=GUI.VGS) {
      flt_t datumX = gaverage(dataJ, i, j, GUI.VGS), datumY = gaverage(dataI, i, j, GUI.VGS);
      GdkPoint a = { j*GUI.UW + ARROW_XOFFSET, i*GUI.UH + ARROW_YOFFSET };
      GdkPoint b = { a.x + round(arrow_xstep*((logs) ? SGN(datumX)*log(1 + fabs(datumX)) : datumX)),
		     a.y + round(arrow_ystep*((logs) ? SGN(datumY)*log(1 + fabs(datumY)) : datumY)) };
      gdk_draw_line(dest, GUI.gc, a.x, a.y, b.x, b.y);
      
      gint arrow_xlen = b.x - a.x, arrow_ylen = b.y - a.y;
      double base_len = ARROWHEAD_REDUCTION*hypot(arrow_xlen, arrow_ylen), theta = atan2(arrow_ylen, arrow_xlen);
      GdkPoint tipL = { a.x + round(base_len*cos(theta+THETA_INCR)), a.y + round(base_len*sin(theta+THETA_INCR)) };
      GdkPoint tipR = { a.x + round(base_len*cos(theta-THETA_INCR)), a.y + round(base_len*sin(theta-THETA_INCR)) };
      gdk_draw_line(dest, GUI.gc, b.x, b.y, tipL.x, tipL.y);
      gdk_draw_line(dest, GUI.gc, b.x, b.y, tipR.x, tipR.y);
    }
  }
}

void plot_scalar(GdkDrawable *dest, GdkColor *hues, flt_t *data, flt_t min, flt_t max) {
  gboolean logs        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_logs));
  flt_t range          = (logs) ? log(1 + max - min) : max - min;
  const flt_t HUE_STEP = (NUM_HUES-1)/(range ? range : 1);
  for (int i=0; i+GUI.SGS<=MAP.nx; i+=GUI.SGS) {
    for (int j=0; j+GUI.SGS<=MAP.ny; j+=GUI.SGS) {
      flt_t datum = gaverage(data, i, j, GUI.SGS);
      int hue_offset = fmin(NUM_HUES-1, HUE_STEP * ((logs) ? log(1 + datum - min) : datum - min));
      gdk_gc_set_foreground(GUI.gc, hues+hue_offset);
      gdk_draw_rectangle(dest, GUI.gc, TRUE, j*GUI.UW, i*GUI.UH, GUI.SW, GUI.SH);
    }
  }
}

void plot_lattice(GdkDrawable *dest, gint width, gint height) {
  flt_t *agents = VIS_DATA.lat;
  gboolean sens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_sens));
  if (sens) gdk_gc_set_foreground(GUI.gc, &GUI.clr_agent_sensor);
  for (int a=0; a<2*NA; a+=2) {
    gint y = lround(GUI.UH*agents[a]), x = lround(GUI.UW*agents[a+1]);
    gdk_draw_drawable(dest, GUI.gc, GUI.pix_agent, 0, 0, x-AGENT_RADIUS*GUI.UW, y-AGENT_RADIUS*GUI.UH, -1, -1);
    if (sens) gdk_draw_arc(dest, GUI.gc, FALSE, x-SENS_RADIUS*GUI.UW, y-SENS_RADIUS*GUI.UH, 2*SENS_RADIUS*GUI.UW+.5, 2*SENS_RADIUS*GUI.UH+.5, 0, 64*360);
  }
}


// ##### LEGEND FUNCTIONS #####
void update_legend_vector() {
  gtk_widget_hide(GUI.da_legend);
  gtk_container_foreach(GTK_CONTAINER(GUI.vbox_legend), destroy_legend_item, NULL);
  gtk_box_pack_start(GTK_BOX(GUI.vbox_legend), GUI.img_credits, FALSE, FALSE, 0);
  gtk_widget_show_all(GUI.vbox_legend);
}

void update_legend_scalar(gint cur_plot) {
  gtk_container_foreach(GTK_CONTAINER(GUI.vbox_legend), destroy_legend_item, NULL);
  
  range_data_t *data = &RANGE_ALL;
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_hist))) {
    compute_data_range_step();
    data = &RANGE_STEP;
  }

  value_range_t *dsrc;
  switch (cur_plot) {
  case RHO:
  case LATC:
    dsrc = &data->rho;
    break;

  case DIV:
  case LATF:
    dsrc = &data->rho;
    //dsrc = &data->div;
    break;

  default:
    dsrc = NULL;
    g_warning("[CFD_VISgl::update_legend_scalar] plot selection '%d' unrecognized", cur_plot);
    break;
  }

  gboolean logs     = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_logs));
  flt_t range       = (logs ? log(1 + dsrc->max - dsrc->min) : dsrc->max - dsrc->min);
  const flt_t DELTA = range/(LEGEND_COUNT-1);
  flt_t val         = (logs ? 0 : dsrc->min);
  for (int i=0; i<LEGEND_COUNT; i++, val+=DELTA) {
    gtk_box_pack_start(GTK_BOX(GUI.vbox_legend), build_legend_item((logs ? exp(val) + dsrc->min - 1 : val)), FALSE, FALSE, 0);
  }
  gtk_widget_show_all(GUI.vbox_legend);
}

void destroy_legend_item(GtkWidget *widget, gpointer data) {
  if (widget == GUI.img_credits) gtk_container_remove(GTK_CONTAINER(GUI.vbox_legend), widget);
  else gtk_widget_destroy(widget);
}

GtkWidget* build_legend_item(flt_t val) {
  char text[16]; //"-6.54e+000"
  text[15] = '\0';
  snprintf(text, 15, "% .4e", val);
  GtkWidget *label = gtk_label_new(text);
  gtk_widget_modify_font(label, GUI.pfont_desc);
  return label;
}


// ##### STATISTICS DISPLAY #####
void update_stats(gint cur_plot) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI.chk_stat))) {
    char text[32];
    text[31] = '\0';
    snprintf(text, 31, "TIME : %d", VIS_DATA.step);
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_time), text);

    gint ptrX, ptrY;
    gtk_widget_get_pointer(GUI.da_out, &ptrX, &ptrY);
    show_hover_stats(ptrX, ptrY);

    flt_t avg, sdev;
    switch (cur_plot) {
    case RHO:
    case LATC:
      avg  = stat_avg_scalar(VIS_DATA.rho);
      sdev = stat_sdev_scalar(VIS_DATA.rho, avg);
      break;
      
    case VEL:
    case LATA:
      avg  = stat_avg_vector(VIS_DATA.u, VIS_DATA.v);
      sdev = stat_sdev_vector(VIS_DATA.u, VIS_DATA.v, avg);
      break;

    case DIV:
    case LATF:
      avg  = stat_avg_scalar(VIS_DATA.rho);
      sdev = stat_sdev_scalar(VIS_DATA.rho, avg);
      //avg  = stat_avg_scalar(VIS_DATA.div);
      //sdev = stat_sdev_scalar(VIS_DATA.div, avg);
      break;
      
    default:
      g_warning("[CFD_VISgl::update_stats] plot selection '%d' unrecognized", cur_plot);
      avg = sdev = -1;
      break;
    }
    //snprintf(text, 31, "LATCE: %s", LAT_STATE_ID_TEXT[VIS_DATA.lat.s]);
    //gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_lat), text);
    snprintf(text, 31, "AVRGE:% .4e", avg);
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_avg), text);
    snprintf(text, 31, "S.DEV:% .4e", sdev);
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_sdev), text);
  } else {
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_time),    "TIME : --");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_mouseXY), "(X,Y): (-,-)");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valI),    "VALUE: --");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valJ),    "         ");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_lat),     "LATCE: --");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_avg),     "AVRGE: --");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_sdev),    "S.DEV: --");
  }
}

void show_hover_stats(gdouble ptrX, gdouble ptrY) {
  // note the reversal of X and Y
  int i = ptrY/GUI.UH, j = ptrX/GUI.UW;
  if (i<MAP.nx && j<MAP.ny) {
    char text[32];
    text[31] = '\0';
    snprintf(text, 31, "(X,Y): (%d,%d)", i, j);
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_mouseXY), text);
    
    int index = i*MAP.ny+j;
    gint cur_plot = get_cur_plot();
    switch (cur_plot) {
    case RHO:
    case LATC:
      snprintf(text, 31, "VALUE:% .4e", VIS_DATA.rho[index]);
      gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valI), text);
      break;
      
    case VEL:
    case LATA:
      snprintf(text, 31, "VALUE: <% .4e,", VIS_DATA.u[index]);
      gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valI), text);
      snprintf(text, 31, "        % .4e>", VIS_DATA.v[index]);
      gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valJ), text);
      break;
      
    case DIV:
    case LATF:
      snprintf(text, 31, "VALUE:% .4e", VIS_DATA.div[index]);
      gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valI), text);
      break;

    default:
      g_warning("[CFD_VISgl::show_hover_stats] plot selection '%d' unrecognized", cur_plot);
      break;
    }
  } else {
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_mouseXY), "(X,Y): (-,-)");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valI),     "VALUE: --");
    gtk_label_set_text(GTK_LABEL(GUI.lbl_stat_valJ),     "         ");
  }
}


// ##### SCREENSHOT UTILITIES #####
int save_screenshot(const char *template, ...) {
  //gtk_window_present(GTK_WINDOW(CAPTURE.win_app));

#ifdef CAPTURE_ENTIRE_APP_WINDOW
  GdkPixbuf *screenshot = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(CAPTURE.win_app->window), GUI.cmap, 0, 0, 0, 0,
						       CAPTURE.win_app->allocation.width, CAPTURE.win_app->allocation.height);
#else
  gint cur_plot = get_cur_plot();
  assert(cur_plot>=0 && cur_plot<NUM_PLOT_TYPES);

  if (GUI.pix_blank[cur_plot]) return 0;

  GdkDrawable *d_src = GDK_DRAWABLE(GUI.pix_out[cur_plot]);
  gint width, height;
  gdk_drawable_get_size(d_src, &width, &height);
  GdkPixbuf *screenshot = gdk_pixbuf_get_from_drawable(NULL, d_src, GUI.cmap, 0, 0, 0, 0, width, height);
#endif

  if (screenshot) {
    va_list ap;
    va_start(ap, template);
    char filename[64];
    filename[63] = '\0';
    vsnprintf(filename, 63, template, ap);
    va_end(ap);

    GError *err = NULL;
    if (!gdk_pixbuf_save(screenshot, filename, "png", &err, NULL)) {
      g_warning("[CFD_VISgl::save_screenshot] %s", err->message);
      g_error_free(err), g_object_unref(screenshot);
      return 0;
    }
    g_object_unref(screenshot);
    return 1;
  } else {
    g_warning("[CFD_VISgl::save_screenshot] failed, gdk_pixbuf_get_from_drawable() returned NULL");
    return 0;
  }
}


// ##### CONFIG FILE UTILITIES #####
// FILE FORMAT:   gboolean["grid","emit","grey","logs","hist","vect","stat"]
//                gint["plot_type", "spn_sgs", "spn_vgs", "spn_anm"]
void read_config() {
  FILE *cfg = fopen(CONFIG_FILENAME, "r");
  if (!cfg) { g_message("[CFD_VISgl::read_config] file '%s' not found", CONFIG_FILENAME); return; }

  size_t cnt;
  gboolean options[NUM_CHK_BUTTONS];
  cnt = fread(options, sizeof(gboolean), NUM_CHK_BUTTONS, cfg);
  for (size_t i=0; i<cnt; i++) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*(&GUI.chk_grid+i)), options[i]);

  gint plot_type;
  cnt += fread(&plot_type, sizeof(gint), 1, cfg);
  if (plot_type>=0 && plot_type<NUM_PLOT_TYPES) gtk_combo_box_set_active(GTK_COMBO_BOX(GUI.mnu_plot_type), plot_type);
  else g_warning("[CFD_VISgl::read_config] plot choice '%d' is invalid", plot_type);

  gint spin_vals[NUM_SPN_BUTTONS];
  size_t spn_cnt = fread(&spin_vals, sizeof(gint), NUM_SPN_BUTTONS, cfg);
  for (size_t i=0; i<spn_cnt; i++) {
    if (spin_vals[i]>=0 && spin_vals[i]<=9) gtk_spin_button_set_value(GTK_SPIN_BUTTON(*(&GUI.spn_sgs+i)), spin_vals[i]);
    else g_warning("[CFD_VISgl::read_config] spin button value '%d' is invalid", spin_vals[i]);
  }
  cnt += spn_cnt;

  //if (cnt == NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS) g_message("[CFD_VISgl::read_config] read  '%zd' settings", cnt);
  //else g_warning("[CFD_VISgl::read_config] found '%zd' settings, but expected '%zd'", cnt, NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS);
  if (cnt != NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS) g_warning("[CFD_VISgl::read_config] found '%zd' settings, but expected '%d'", cnt, NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS);
  fclose(cfg);
}

void save_config() {
  FILE *cfg = fopen(CONFIG_FILENAME, "w");
  if (!cfg) { g_warning("[CFD_VISgl::save_config] cannot create '%s' file", CONFIG_FILENAME); return; }

  size_t cnt;
  gboolean options[NUM_CHK_BUTTONS];
  for (size_t i=0; i<NUM_CHK_BUTTONS; i++) options[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(*(&GUI.chk_grid+i)));
  cnt = fwrite(options, sizeof(gboolean), NUM_CHK_BUTTONS, cfg);

  gint settings[NUM_SPN_BUTTONS+1] = { get_cur_plot(),
				       gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(GUI.spn_sgs)),
				       gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(GUI.spn_vgs)),
				       gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(GUI.spn_spd)) };
  cnt += fwrite(&settings, sizeof(gint), NUM_SPN_BUTTONS+1, cfg);

  //if (cnt == NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS) g_message("[CFD_VISgl::save_config] saved '%zd' settings", cnt);
  //else g_warning("[CFD_VISgl::save_config] saved '%zd' values, but expected to save '%zd' options", cnt, NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS);
  if (cnt != NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS) g_warning("[CFD_VISgl::save_config] saved '%zd' values, but expected to save '%d' options", cnt, NUM_CHK_BUTTONS+1+NUM_SPN_BUTTONS);
  fclose(cfg);
}


// ##### GROUP FUNCTIONS #####
flt_t gaverage(flt_t *data, int irow, int icol, int size) {
  flt_t avg = 0;
  for (int row=irow; row<irow+size; row++) for (int col=icol; col<icol+size; col++) avg += data[row*MAP.ny + col];
  return avg/(size*size);
}

