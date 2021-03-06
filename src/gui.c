/* gui.c
 *
 * Copyright (C) 2001 J�rgen Scheibengruber
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*** the gui to gnomeradio */

#include <config.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <math.h>
#include "bacon-volume.h"
#include "gui.h"
#include "trayicon.h"
#include "tech.h"
#include "radio.h"
#include "rec_tech.h"
#include "lirc.h"
#include "prefs.h"
#include "record.h"
#include "../data/pixmaps/digits.xpm"
#include "../data/pixmaps/signal.xpm"
#include "../data/pixmaps/stereo.xpm"
#include "../data/pixmaps/freq_up.xpm"
#include "../data/pixmaps/freq_down.xpm"

#define DIGIT_WIDTH 20
#define DIGIT_HEIGTH 30
#define SIGNAL_WIDTH 25
#define STEREO_WIDTH 35
#define SCAN_SPEED 20

#define TRANSLATORS "TRANSLATORS"

GtkWidget* mute_button, *preset_combo;
GtkAdjustment *adj;
GtkTooltips *tooltips;
GtkWidget* app;


int mom_ps;
gnomeradio_settings settings;

gboolean main_visible;

static GtkWidget *drawing_area;
static GdkPixmap *digits, *signal_s, *stereo;
static GtkWidget *freq_scale;
static GtkWidget *rec_pixmap;

static int timeout_id, bp_timeout_id = -1, bp_timeout_steps = 0;

static gboolean is_first_start(void)
{
	GConfClient *client = gconf_client_get_default();
	if (!client)
		return TRUE;

	return !gconf_client_get_bool(client, "/apps/gnomeradio/first_time_flag", NULL);
}

static void set_first_time_flag(void)
{
	GConfClient *client = gconf_client_get_default();
	if (!client)
		return;

	gconf_client_set_bool(client, "/apps/gnomeradio/first_time_flag", TRUE, NULL);
}

typedef struct {
	GtkWidget *dialog;
	GtkWidget *progress;
	GList *stations;
	GtkWidget *label;
} FreqScanData;

static gboolean initial_frequency_scan_cb(gpointer data)
{
	static gfloat freq = FREQ_MIN - 4.0f/STEPS;
	FreqScanData *fsd = data;
	
	g_assert(fsd);
	
	if (freq > FREQ_MAX) {
		gtk_widget_destroy(fsd->dialog);
		timeout_id = 0;
		return FALSE;
	}
	
	if (radio_check_station(freq)) {
		char *text = g_strdup_printf(_("%d stations found"), g_list_length(fsd->stations) + 1);
		gfloat *f = g_malloc(sizeof(gfloat));
		gtk_label_set_text(GTK_LABEL(fsd->label), text);
		g_free(text);
		
		g_print("%.2f is a station\n", freq);
		
		*f = freq;
		fsd->stations = g_list_append(fsd->stations, f);
	}

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(fsd->progress), MAX(0, (freq - FREQ_MIN)/(FREQ_MAX - FREQ_MIN)));	
	
	freq += 1.0/STEPS;
	radio_set_freq(freq);
	
	return TRUE;
}

static void initial_frequency_scan(GtkWidget *app)
{
	FreqScanData data;
	GtkWidget *title;
	char *title_hdr;
	
	data.stations = NULL;
	
	data.dialog = gtk_dialog_new_with_buttons(_("Scanning"),
		GTK_WINDOW(app), DIALOG_FLAGS, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	gtk_dialog_set_has_separator(GTK_DIALOG(data.dialog), FALSE);
	gtk_window_set_resizable(GTK_WINDOW(data.dialog), FALSE);
	
	title_hdr = g_strconcat("<span weight=\"bold\">", _("Scanning for available stations:"), "</span>", NULL);
	title = gtk_label_new(title_hdr);
	gtk_misc_set_alignment(GTK_MISC(title), 0, 0.5);
	gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
	g_free(title_hdr);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(data.dialog)->vbox), title, FALSE, FALSE, 6);
	
	data.progress = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(data.dialog)->vbox), data.progress, TRUE, FALSE, 6);
	
	data.label = gtk_label_new(_("No stations found"));
	gtk_misc_set_alignment(GTK_MISC(data.label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(data.dialog)->vbox), data.label, TRUE, FALSE, 6);
	
	gtk_widget_show_all(data.dialog);
	
	radio_mute();
	timeout_id = g_timeout_add(1000/SCAN_SPEED, (GSourceFunc)initial_frequency_scan_cb, (gpointer)&data);	
	int response = gtk_dialog_run(GTK_DIALOG(data.dialog));

	radio_unmute();
	if (timeout_id) {
		g_source_remove(timeout_id);
		timeout_id = 0;
		gtk_widget_destroy(data.dialog);
	} else {
		if (g_list_length(data.stations) > 0) {
			gfloat f = *((gfloat*)data.stations->data);
			adj->value = f*STEPS;
			radio_set_freq(f);
			
			GtkWidget *dialog;
			GList *ptr;
			char *text;
			
			text = g_strdup_printf(_("%d stations found. \nDo you want to add them as presets?\n"),
					g_list_length(data.stations));
			
			dialog = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_YES_NO, text);
			g_free(text);
			
			int response = gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			for (ptr = data.stations; ptr; ptr = ptr->next) {
				if (response == GTK_RESPONSE_YES) {
					preset *ps = g_malloc0(sizeof(preset));
					ps->title = g_strdup(_("unnamed"));
					ps->freq = *((gfloat*)ptr->data);
					settings.presets = g_list_append(settings.presets, ps);
				}
				g_free(ptr->data);
			}	
		}
	}	
}	

static void prefs_button_clicked_cb(GtkButton *button, gpointer app)
{
	GtkWidget* dialog;
	gint choise;
	
	dialog = prefs_window(app);
	
	/* Michael Jochum <e9725005@stud3.tuwien.ac.at> proposed to not use gnome_dialog_set_parent()
	   but following instead. */
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(app));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	
	/*gnome_dialog_set_parent(GNOME_DIALOG(dialog), GTK_WINDOW(app));*/
	
	choise = GTK_RESPONSE_HELP;
	while (choise == GTK_RESPONSE_HELP)
	{
		choise = gtk_dialog_run(GTK_DIALOG(dialog)); 
		switch (choise)
		{
			case GTK_RESPONSE_HELP:
				display_help_cb("gnomeradio-settings");
			break;
			default:
				/* We need the hide signal to get the value of the device_entry */
				gtk_widget_hide_all(dialog);
				gtk_widget_destroy(dialog);
		}
	}
}

void start_radio(gboolean restart, GtkWidget *app)
{
    DriverType driver = DRIVER_ANY;
	if (restart)
		radio_stop();
	
    if (settings.driver) {
        if (0 == strcmp(settings.driver, "v4l1"))
            driver = DRIVER_V4L1;
        if (0 == strcmp(settings.driver, "v4l2"))
            driver = DRIVER_V4L2;
    }

	if (!radio_init(settings.device, driver))
	{
		char *caption = g_strdup_printf(_("Could not open device \"%s\"!"), settings.device);
		char *detail = g_strdup_printf(_("Check your settings and make sure that no other\n"
				"program is using %s.\nAlso make sure that you have read-access to it."), settings.device);
		show_error_message(caption, detail);
		g_free(caption);
		g_free(detail);
		
		if (!restart)
			prefs_button_clicked_cb(NULL, app);
	}
}

void start_mixer(gboolean restart, GtkWidget *app)
{
	gint res, vol;
	
	if (restart)
		mixer_close();
	
	res = mixer_init(settings.mixer_dev, settings.mixer);
	if (res <1) 
	{
		char *buffer;
		
		if (res == -1)
			buffer = g_strdup_printf(_("Mixer source \"%s\" is not a valid source!"), settings.mixer);
		else 
			buffer = g_strdup_printf(_("Could not open \"%s\"!"), settings.mixer_dev);
		
		show_error_message(buffer, NULL);
		
		g_free(buffer);
	}		
	vol = mixer_get_volume();
	if (vol >= 0) {
		bacon_volume_button_set_value(BACON_VOLUME_BUTTON(mute_button), vol);
		/*gtk_adjustment_set_value(volume, (double)vol);*/
	}
}

GList* get_mixer_recdev_list(void)
{
	int i;
	char **array, *dev;
	GList *result = NULL;
	
	array = mixer_get_rec_devices();
	if (!array)
		return NULL;
	
	i = 0;	
	dev = array[i];
	while (dev)
	{
		char *text = g_strdup(dev);
		result = g_list_append(result, text);
		free(dev);
		dev = array[++i];
	}			
	free(array);
	
	return result;
}

static gboolean redraw_status_window(void)
{

	GdkWindow *real_window, *window;
	GdkGC *gc;
	int win_width, win_height;
	int val, freq[5], signal_strength, is_stereo;
	
	val = (int)(rint(adj->value/STEPS * 100.0));
	
	freq[0] = val / 10000;
	freq[1] = (val % 10000) / 1000;
	freq[2] = (val % 1000) / 100; 
	freq[3] = (val % 100) / 10;
	freq[4] = val % 10;

	signal_strength = radio_get_signal();
	is_stereo = radio_get_stereo();
	
	if (signal_strength > 3) signal_strength = 3;
	if (signal_strength < 0) signal_strength = 0;
	is_stereo = (is_stereo == 1) ? 1 : 0;
	
	real_window = drawing_area->window;
	if (real_window == NULL)
		/* UI has not been realized yet */
		return TRUE;
	gc = gdk_gc_new(real_window);
	gdk_drawable_get_size(real_window, &win_width, &win_height);
	
	/* use doublebuffering to avoid flickering */
	window = gdk_pixmap_new(real_window, win_width, win_height, -1);

	gdk_draw_rectangle(window, gc, TRUE, 0, 0, win_width, win_height);
	
	win_width -= 5;
	
	if (freq[0])	gdk_draw_drawable(window, gc, digits, freq[0] * DIGIT_WIDTH, 0, win_width - DIGIT_WIDTH*6, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	else gdk_draw_rectangle(window, gc, TRUE, win_width - DIGIT_WIDTH*6, 5, DIGIT_WIDTH, DIGIT_HEIGTH);

	gdk_draw_drawable(window, gc, digits, freq[1] * DIGIT_WIDTH, 0, win_width - DIGIT_WIDTH*5, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	gdk_draw_drawable(window, gc, digits, freq[2] * DIGIT_WIDTH, 0, win_width - DIGIT_WIDTH*4, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	gdk_draw_drawable(window, gc, digits, 10 * DIGIT_WIDTH, 0, win_width - DIGIT_WIDTH*3, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	gdk_draw_drawable(window, gc, digits, freq[3] * DIGIT_WIDTH, 0, win_width - DIGIT_WIDTH*2, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	gdk_draw_drawable(window, gc, digits, freq[4] * DIGIT_WIDTH, 0, win_width - DIGIT_WIDTH*1, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	gdk_draw_drawable(window, gc, signal_s, signal_strength * SIGNAL_WIDTH, 0, win_width - DIGIT_WIDTH*6-SIGNAL_WIDTH, 5, SIGNAL_WIDTH, DIGIT_HEIGTH);
	gdk_draw_drawable(window, gc, stereo, is_stereo * STEREO_WIDTH, 0, win_width - DIGIT_WIDTH*6-SIGNAL_WIDTH-STEREO_WIDTH, 5, STEREO_WIDTH, DIGIT_HEIGTH);

	/* draw the pixmap to the real window */	
	gdk_draw_drawable(real_window, gc, window, 0, 0, 0, 0, win_width + 5, win_height);
	
	g_object_unref(G_OBJECT(gc));
	g_object_unref(G_OBJECT(window));
	
	return TRUE;	
}
	

static gboolean expose_event_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	redraw_status_window();
	return TRUE;
}	

void exit_gnome_radio(void)
{
	if (settings.mute_on_exit)
	{
		radio_mute();
		radio_stop();
	}
	mixer_close();
	save_settings();
	gtk_main_quit();
}

const char* get_preset(float freq, int *num)
{
	GList *node = settings.presets;

	int i = *num = -1;
	for (;node;)
	{
		++i;
		preset *ps = (preset*)node->data;
		if (fabs(ps->freq - freq) < 0.01)
		{
			*num = i;
			return ps->title;
		}
		node = node->next;
	}
	return NULL;
}

static void adj_value_changed_cb(GtkAdjustment* data, gpointer window)
{
	char *buffer;
	float freq = rint(adj->value)/STEPS;
	const char *preset_title = get_preset(freq, &mom_ps);

	preset_combo_set_item(mom_ps);
	
	redraw_status_window();
	
	if (preset_title)
		buffer = g_strdup_printf(_("Gnomeradio - %s"), preset_title);
	else
		buffer = g_strdup_printf(_("Gnomeradio - %.2f MHz"), freq);
	gtk_window_set_title(GTK_WINDOW(window), buffer);
	if (tray_icon) gtk_status_icon_set_tooltip(GTK_STATUS_ICON(tray_icon), buffer); //gtk_tooltips_set_tip(tooltips, tray_icon, buffer, NULL);
	g_free(buffer);
	
	buffer = g_strdup_printf(_("Frequency: %.2f MHz"), freq);
	gtk_tooltips_set_tip(tooltips, freq_scale, buffer, NULL);
	g_free(buffer);

	radio_set_freq(adj->value/STEPS);
}

static void volume_value_changed_cb(BaconVolumeButton *button, gpointer user_data)
{
	char *text;
	int vol = (int)(bacon_volume_button_get_value(BACON_VOLUME_BUTTON(mute_button)) + 0.5f);
	
	mixer_set_volume(vol);
	
/*	text = g_strdup_printf(_("Volume: %d%%"), vol);
	gtk_tooltips_set_tip(tooltips, vol_scale, text, NULL);
	g_free(text);*/
	
    if (tray_menu) {
	    g_signal_handler_block(G_OBJECT(mute_menuitem), mute_menuitem_toggled_cb_id);
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mute_menuitem), vol == 0);
	    g_signal_handler_unblock(G_OBJECT(mute_menuitem), mute_menuitem_toggled_cb_id);
    }
}

#if 0
static gboolean poll_volume_change(gpointer data)
{
	int vol;
	if ((vol = mixer_get_volume()) < 0)
		return FALSE;
	
	if (vol != (int)volume->value)
	{
		g_print("external volume change detected\n");
		gtk_adjustment_set_value(volume, (double)vol);
	}
	
	return TRUE;
}
#endif

static void change_frequency(gpointer data)
{
	gboolean increase = (gboolean)data;
	
	if (increase)
	{
		if (adj->value >= FREQ_MAX*STEPS)
			gtk_adjustment_set_value(adj, FREQ_MIN*STEPS);
		else
			gtk_adjustment_set_value(adj, adj->value+1);
	}
	else
	{
		if (adj->value <= FREQ_MIN*STEPS)
			gtk_adjustment_set_value(adj, FREQ_MAX*STEPS);
		else
			gtk_adjustment_set_value(adj, adj->value-1);
	}
}

static gboolean change_frequency_timeout(gpointer data)
{
	change_frequency(data);
	if (bp_timeout_steps < 10)
	{
		gtk_timeout_remove(bp_timeout_id);
		bp_timeout_id = gtk_timeout_add(200 - 20*bp_timeout_steps,
			(GtkFunction)change_frequency_timeout, data);
		bp_timeout_steps++; 
	}
	return TRUE;
}	

static void step_button_pressed_cb(GtkButton *button, gpointer data)
{
	bp_timeout_id = gtk_timeout_add(500, (GtkFunction)change_frequency_timeout, data);
}

static void step_button_clicked_cb(GtkButton *button, gpointer data)
{
	change_frequency(data);
}

static void step_button_released_cb(GtkButton *button, gpointer data)
{
	if (bp_timeout_id > -1)
		gtk_timeout_remove(bp_timeout_id);
	bp_timeout_id = -1;
	bp_timeout_steps = 0;
}

static gboolean scan_freq(gpointer data)
{
	static gint start, mom, max;
	gint dir = (gint)(data);
	
	if (!max) {
		max = (FREQ_MAX - FREQ_MIN) * STEPS;
	}	
		
	if (radio_check_station(adj->value/STEPS) || (start > max))	{
		start = mom = 0;
		radio_unmute();
		timeout_id = 0;
		return FALSE;
	}
	if (!mom) {
		mom = adj->value;
	}
		
	if (mom > FREQ_MAX*STEPS) 
		mom = FREQ_MIN*STEPS;
	else if (mom < FREQ_MIN*STEPS)
		mom = FREQ_MAX*STEPS;
	else	
		mom = mom + dir;
	start += 1;
	gtk_adjustment_set_value(adj, mom);

	return TRUE;
}

void scfw_button_clicked_cb(GtkButton *button, gpointer data)
{
	if (timeout_id) {
		gtk_timeout_remove(timeout_id);
		timeout_id = 0;
		radio_unmute();
		return;
	}
	radio_mute();
	timeout_id = gtk_timeout_add(1000/SCAN_SPEED, (GtkFunction)scan_freq, (gpointer)1);	
}

void scbw_button_clicked_cb(GtkButton *button, gpointer data)
{
	if (timeout_id) {
		gtk_timeout_remove(timeout_id);
		timeout_id = 0;
		radio_unmute();
		return;
	}
	radio_mute();
	timeout_id = gtk_timeout_add(1000/SCAN_SPEED, (GtkFunction)scan_freq, (gpointer)(-1));	
}

void preset_combo_set_item(gint i)
{
	if (i < -1) return;
	if (preset_combo == NULL) return;
	gtk_combo_box_set_active(GTK_COMBO_BOX(preset_combo), i + 1);
}

static void preset_combo_change_cb(GtkWidget *combo, gpointer data)
{
	preset* ps;
	mom_ps = gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) - 1;
	
	if (mom_ps < 0) return;
	
	ps = (preset*)g_list_nth_data(settings.presets, mom_ps);
	gtk_adjustment_set_value(adj, ps->freq * STEPS);
}

void change_preset(gboolean next)
{
	preset *ps;
	int len = g_list_length(settings.presets);
	if (len < 1)
		return;

	if (next)
		mom_ps = (mom_ps + 1) % len;
	else
		mom_ps = (mom_ps - 1 + len) % len;

	ps = g_list_nth_data(settings.presets, mom_ps);
	gtk_adjustment_set_value(adj, ps->freq*STEPS);
	preset_combo_set_item(mom_ps);
}

static void quit_button_clicked_cb(GtkButton *button, gpointer data)
{
	exit_gnome_radio();
}

void tray_icon_items_set_sensible(gboolean sensible)
{
	GList* menuitems;
	GtkWidget *menuitem;
	int i, cnt = g_list_length(settings.presets);
	
	
	menuitems = GTK_MENU_SHELL(tray_menu)->children;
	
	g_assert(cnt + 6 == g_list_length(menuitems));
	
	/* Disable the presets */
	for (i = 0; i < cnt; i++) {
		menuitem = g_list_nth_data(menuitems, i);
		gtk_widget_set_sensitive(menuitem, sensible);
	}	
	
	/* Disable the mute button (separator => +1) */
	menuitem = g_list_nth_data(menuitems, cnt + 1);
	gtk_widget_set_sensitive(menuitem, sensible);

	/* Disable the record button */
	menuitem = g_list_nth_data(menuitems, cnt + 2);
	gtk_widget_set_sensitive(menuitem, sensible);
	
	/* Disable the quit button */
	menuitem = g_list_nth_data(menuitems, cnt + 5);
	gtk_widget_set_sensitive(menuitem, sensible);
}

static int start_recording(const gchar *destination, const char* station, const char* time)
{
	GtkWidget *dialog;
	Recording* recording;
	char *filename;
	
	if (!mixer_set_rec_device())
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				"Could not set \"%s\" as recording Source", settings.mixer);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return -1;
	}
	
	/* You can translate the filename for a recording:
	 * args for this format are: path, station title, time 
	 */ 
	filename = g_strdup_printf(_("%s/%s_%s"), 
 		destination, 
 		station, 
 		time);
	recording = recording_start(filename);
	g_free(filename);
	if (!recording)
		return -1;
	
	tray_icon_items_set_sensible(FALSE);
	
	recording->station = g_strdup(station);
	dialog = record_status_window(recording);
	
	run_status_window(recording);

	return 1;
}

void rec_button_clicked_cb(GtkButton *button, gpointer app)
{
	GtkWidget *dialog;
	char *station;
	char time_str[100];
	time_t t;
	
	t = time(NULL);
	/* consult man strftime to translate this. This is a filename, so don't use "/" or ":", please */
	strftime(time_str, 100, _("%B-%d-%Y_%H-%M-%S"), localtime(&t));
	
	if (mom_ps < 0) {
		station = g_strdup_printf(_("%.2f MHz"), rint(adj->value)/STEPS);
	} else {
		g_assert(mom_ps < g_list_length(settings.presets));
		preset* ps = g_list_nth_data(settings.presets, mom_ps);
		g_assert(ps);
	
		station = g_strdup(ps->title);
	}	
		
/*	if (!check_filename(filename)) {
		GtkWidget *errdialog;
		errdialog = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					_("Error opening file '%s':\n%s"), filename, strerror(errno));
		gtk_dialog_run (GTK_DIALOG (errdialog));
		gtk_widget_destroy (errdialog);
	} else */
	start_recording(rec_settings.destination, station, time_str);
	g_free(station);
}

void toggle_volume(void)
{
	static int old_vol;
	int vol = mixer_get_volume();
	
	if (vol) {
		old_vol = vol;
		vol = 0;
	} else {
		vol = old_vol;
	}	
	mixer_set_volume(vol);
	bacon_volume_button_set_value(BACON_VOLUME_BUTTON(mute_button), vol);
	/*gtk_adjustment_set_value(volume, vol);*/
}	

static void mute_button_toggled_cb(GtkButton *button, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mute_button)))
	{		
		gtk_tooltips_set_tip(tooltips, mute_button, _("Unmute"), NULL);
	}
	else
	{
		gtk_tooltips_set_tip(tooltips, mute_button, _("Mute"), NULL);
	}
	toggle_volume();
}

static void about_button_clicked_cb(GtkButton *button, gpointer data)
{
	GdkPixbuf *app_icon;
	GtkIconTheme *icontheme;
	static GtkWidget *about;
	const char *authors[] = {"Jörgen Scheibengruber <mfcn@gmx.de>", NULL};
	char *text;
	
	/* Feel free to put your names here translators :-) */
	char *translators = _("TRANSLATORS");

	if (about)
	{
		gtk_window_present(GTK_WINDOW(about));
		return;
	}
	icontheme = gtk_icon_theme_get_default();
	app_icon = gtk_icon_theme_load_icon(icontheme, "gnomeradio", 48, 0, NULL);

#ifdef HAVE_LIRC	
	text =_("Gnomeradio is a FM-Tuner application for the GNOME desktop. "
							"It should work with all tuner hardware that is supported by the video4linux drivers.\n\n"
							"This version has been compiled with LIRC support.");
#else
	text =_("Gnomeradio is a FM-Tuner application for the GNOME desktop. "
							"It should work with all tuner hardware that is supported by the video4linux drivers.\n\n"
							"This version has been compiled without LIRC support.");
#endif
	
	about = gnome_about_new ("Gnomeradio", VERSION, "Copyright 2001 - 2006 Jörgen Scheibengruber",
							text, (const char **) authors, NULL, 
							strcmp("TRANSLATORS", translators) ? translators : NULL, 
							app_icon);

	gtk_widget_show(about);
	g_object_add_weak_pointer(G_OBJECT(about), (gpointer)&about);
	g_object_add_weak_pointer(G_OBJECT(about), (gpointer)&app_icon);
}

static gint delete_event_cb(GtkWidget* window, GdkEventAny* e, gpointer data)
{
	exit_gnome_radio();
	return TRUE;
}

void display_help_cb(char *topic)
{
	GError *error = NULL;

	gnome_help_display(PACKAGE, topic, &error);
	if (error)
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_error_free (error);
		error = NULL;
	}
}

void toggle_mainwindow_visibility(GtkWidget *app)
{
	static gint posx, posy;
	if (GTK_WIDGET_VISIBLE(app))
	{
		gtk_window_get_position(GTK_WINDOW(app), &posx, &posy);
		gtk_widget_hide(app);
	}
	else
	{
		if ((posx >= 0) && (posy >= 0))
			gtk_window_move(GTK_WINDOW(app), posx, posy);
		gtk_window_present(GTK_WINDOW(app));
	}
}	
	
GtkWidget* gnome_radio_gui(void)
{
	GtkWidget *app;
	GtkWidget *prefs_button, *quit_button, *scfw_button, *scbw_button;
	GtkWidget *stfw_button, *stbw_button, *about_button, *rec_button;
	GtkWidget *prefs_pixmap, *quit_pixmap, *scfw_pixmap, *scbw_pixmap;
	GtkWidget *stfw_pixmap, *stbw_pixmap, *about_pixmap;
	GtkWidget *vol_up_pixmap, *vol_down_pixmap, *freq_up_pixmap, *freq_down_pixmap;
	GdkPixbuf *vol_up_pixbuf, *vol_down_pixbuf, *freq_up_pixbuf, *freq_down_pixbuf;
	GtkWidget *hbox1, *hbox2, *vbox, *menubox, *freq_vol_box;
	GtkWidget *vseparator1, *vseparator2, *vseparator4;
	GtkWidget *label;
	GtkWidget *frame;
	gchar *text;
	
	app = gnome_app_new(PACKAGE, _("Gnomeradio"));

	gtk_window_set_resizable(GTK_WINDOW(app), FALSE);
	/*gtk_window_set_policy(GTK_WINDOW(app), FALSE, FALSE, FALSE);*/
	gtk_window_set_wmclass(GTK_WINDOW(app), "gnomeradio", "Gnomeradio");

	frame = gtk_frame_new(NULL);
	
	quit_pixmap = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
	prefs_pixmap = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_LARGE_TOOLBAR);
	scfw_pixmap = gtk_image_new_from_stock(GTK_STOCK_MEDIA_FORWARD, GTK_ICON_SIZE_LARGE_TOOLBAR);
	scbw_pixmap = gtk_image_new_from_stock(GTK_STOCK_MEDIA_REWIND, GTK_ICON_SIZE_LARGE_TOOLBAR);
	stfw_pixmap = gtk_image_new_from_stock(GTK_STOCK_MEDIA_NEXT, GTK_ICON_SIZE_LARGE_TOOLBAR);
	stbw_pixmap = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS, GTK_ICON_SIZE_LARGE_TOOLBAR);
	about_pixmap = gtk_image_new_from_stock(GTK_STOCK_ABOUT, GTK_ICON_SIZE_LARGE_TOOLBAR);
	/*mute_pixmap = gtk_image_new_from_stock(GNOME_STOCK_VOLUME, GTK_ICON_SIZE_LARGE_TOOLBAR);*/
	rec_pixmap = gtk_image_new_from_stock(GTK_STOCK_MEDIA_RECORD, GTK_ICON_SIZE_LARGE_TOOLBAR);
	/*help_pixmap = gtk_image_new_from_stock(GTK_STOCK_HELP, GTK_ICON_SIZE_LARGE_TOOLBAR);*/
	
	quit_button = gtk_button_new();
	prefs_button = gtk_button_new();
	scfw_button = gtk_button_new();
	scbw_button = gtk_button_new();
	stfw_button = gtk_button_new();
	stbw_button = gtk_button_new();
	about_button = gtk_button_new();
	/*mute_button = gtk_toggle_button_new();*/
	mute_button = bacon_volume_button_new(GTK_ICON_SIZE_LARGE_TOOLBAR, 0, 100, 1);
	gtk_button_set_relief(GTK_BUTTON(mute_button), GTK_RELIEF_NORMAL);
	rec_button = gtk_button_new();
	/*help_button = gtk_button_new();*/

	gtk_container_add(GTK_CONTAINER(quit_button), quit_pixmap);
	gtk_container_add(GTK_CONTAINER(prefs_button), prefs_pixmap);
	gtk_container_add(GTK_CONTAINER(scfw_button), scfw_pixmap);
	gtk_container_add(GTK_CONTAINER(scbw_button), scbw_pixmap);
	gtk_container_add(GTK_CONTAINER(stfw_button), stfw_pixmap);
	gtk_container_add(GTK_CONTAINER(stbw_button), stbw_pixmap);
	gtk_container_add(GTK_CONTAINER(about_button), about_pixmap);
	/*gtk_container_add(GTK_CONTAINER(mute_button), mute_pixmap);*/
	gtk_container_add(GTK_CONTAINER(rec_button), rec_pixmap);
	/*gtk_container_add(GTK_CONTAINER(help_button), help_pixmap);*/

	vbox = gtk_vbox_new(FALSE, 0);
	hbox1 = gtk_hbox_new(FALSE, 0);
	hbox2 = gtk_hbox_new(FALSE, 0);
	menubox = gtk_vbox_new(FALSE, 0);
	freq_vol_box = gtk_hbox_new(FALSE, 0);
	
	adj = GTK_ADJUSTMENT(gtk_adjustment_new(SUNSHINE*STEPS, FREQ_MIN*STEPS, FREQ_MAX*STEPS+1, 1, STEPS, 1));
/*	volume = GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 101, 1, 10, 1)); */
	
	preset_combo = gtk_combo_box_new_text();
	g_signal_connect(GTK_OBJECT(preset_combo), "changed", GTK_SIGNAL_FUNC(preset_combo_change_cb), NULL);
	
	gtk_widget_set_size_request(preset_combo, 10, -1);
	label = gtk_label_new(_("Presets:"));
	
	freq_scale = gtk_hscale_new(adj);
	/*gtk_range_set_update_policy(GTK_RANGE(freq_scale), GTK_UPDATE_DELAYED);*/
	/*vol_scale = gtk_hscale_new(volume);*/
	
	/*vol_up_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)vol_up_xpm);
	vol_down_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)vol_down_xpm);*/
	freq_up_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)freq_up_xpm);
	freq_down_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)freq_down_xpm);

	/*vol_up_pixmap = gtk_image_new_from_pixbuf(vol_up_pixbuf);
	vol_down_pixmap = gtk_image_new_from_pixbuf(vol_down_pixbuf);*/
	freq_up_pixmap = gtk_image_new_from_pixbuf(freq_up_pixbuf);
	freq_down_pixmap = gtk_image_new_from_pixbuf(freq_down_pixbuf);

	/*gtk_widget_set_usize(freq_scale, 160, 10);*/
	/*gtk_widget_set_size_request(freq_scale, 160, -1);*/

	gtk_widget_realize(app);
	drawing_area = gtk_drawing_area_new();
	digits = gdk_pixmap_create_from_xpm_d (app->window, NULL, NULL, digits_xpm);
	signal_s = gdk_pixmap_create_from_xpm_d (app->window, NULL, NULL, signal_xpm);
	stereo = gdk_pixmap_create_from_xpm_d (app->window, NULL, NULL, stereo_xpm);
	
	vseparator1 = gtk_vseparator_new();
	vseparator2 = gtk_vseparator_new();
	/*vseparator3 = gtk_vseparator_new();*/
	vseparator4 = gtk_vseparator_new();
	
	gtk_scale_set_digits(GTK_SCALE(freq_scale), 0);
	gtk_scale_set_draw_value(GTK_SCALE(freq_scale), FALSE);
/*	gtk_scale_set_digits(GTK_SCALE(vol_scale), 0);
	gtk_scale_set_draw_value(GTK_SCALE(vol_scale), FALSE);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_button), mixer_get_volume() == 0);*/

	gtk_widget_set_size_request(drawing_area, DIGIT_WIDTH*6+10+SIGNAL_WIDTH+STEREO_WIDTH, DIGIT_HEIGTH+10);

	gtk_container_add(GTK_CONTAINER(frame), drawing_area);

	gtk_box_pack_start(GTK_BOX(hbox2), scbw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), stbw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), stfw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), scfw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), vseparator1, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), mute_button, FALSE, FALSE, 2);
	/*gtk_box_pack_start(GTK_BOX(hbox2), vseparator2, TRUE, TRUE, 3);*/
	gtk_box_pack_start(GTK_BOX(hbox2), rec_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), vseparator4, FALSE, FALSE, 2);
	/*gtk_box_pack_start(GTK_BOX(hbox2), help_button, FALSE, FALSE, 2);*/
	gtk_box_pack_start(GTK_BOX(hbox2), prefs_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), about_button, FALSE, FALSE, 2);
	/*gtk_box_pack_start(GTK_BOX(hbox2), quit_button, FALSE, FALSE, 2);*/

	gtk_box_pack_start(GTK_BOX(hbox1), frame, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox1), menubox, TRUE, TRUE, 3);
	
	gtk_box_pack_start(GTK_BOX(menubox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(menubox), preset_combo, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(freq_vol_box), freq_down_pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), freq_scale, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), freq_up_pixmap, FALSE, FALSE, 2);
	/*gtk_box_pack_start(GTK_BOX(freq_vol_box), vseparator3, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vol_down_pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vol_scale, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vol_up_pixmap, FALSE, FALSE, 2);*/

	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), freq_vol_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 4);
	
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 2);

	gnome_app_set_contents(GNOME_APP(app), vbox);

	/*status = gnome_appbar_new(FALSE, TRUE, GNOME_PREFERENCES_NEVER);*/

	/*gnome_app_set_statusbar(GNOME_APP(app), status);*/

	tooltips = gtk_tooltips_new();

	g_signal_connect(GTK_OBJECT(app), "delete_event", GTK_SIGNAL_FUNC(delete_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(quit_button), "clicked", GTK_SIGNAL_FUNC(quit_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(adj), "value-changed", GTK_SIGNAL_FUNC(adj_value_changed_cb), (gpointer) app);
	g_signal_connect(GTK_OBJECT(mute_button), "value-changed", GTK_SIGNAL_FUNC(volume_value_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(stfw_button), "pressed", GTK_SIGNAL_FUNC(step_button_pressed_cb), (gpointer)TRUE);
	g_signal_connect(GTK_OBJECT(stbw_button), "pressed", GTK_SIGNAL_FUNC(step_button_pressed_cb), (gpointer)FALSE);
	g_signal_connect(GTK_OBJECT(stfw_button), "clicked", GTK_SIGNAL_FUNC(step_button_clicked_cb), (gpointer)TRUE);
	g_signal_connect(GTK_OBJECT(stbw_button), "clicked", GTK_SIGNAL_FUNC(step_button_clicked_cb), (gpointer)FALSE);
	g_signal_connect(GTK_OBJECT(stfw_button), "released", GTK_SIGNAL_FUNC(step_button_released_cb), NULL);
	g_signal_connect(GTK_OBJECT(stbw_button), "released", GTK_SIGNAL_FUNC(step_button_released_cb), NULL);
	g_signal_connect(GTK_OBJECT(scfw_button), "clicked", GTK_SIGNAL_FUNC(scfw_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(scbw_button), "clicked", GTK_SIGNAL_FUNC(scbw_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(about_button), "clicked", GTK_SIGNAL_FUNC(about_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(rec_button), "clicked", GTK_SIGNAL_FUNC(rec_button_clicked_cb), (gpointer) app);
	g_signal_connect(GTK_OBJECT(prefs_button), "clicked", GTK_SIGNAL_FUNC(prefs_button_clicked_cb), (gpointer) app);
	g_signal_connect(GTK_OBJECT(drawing_area), "expose-event", GTK_SIGNAL_FUNC(expose_event_cb), NULL);

	gtk_tooltips_set_tip(tooltips, scbw_button, _("Scan Backwards"), NULL);
	gtk_tooltips_set_tip(tooltips, scfw_button, _("Scan Forwards"), NULL);
	gtk_tooltips_set_tip(tooltips, stbw_button, _("0.05 MHz Backwards"), NULL);
	gtk_tooltips_set_tip(tooltips, stfw_button, _("0.05 MHz Forwards"), NULL);
	gtk_tooltips_set_tip(tooltips, about_button, _("About Gnomeradio"), NULL);
	gtk_tooltips_set_tip(tooltips, rec_button, _("Record radio as Wave, OGG or MP3"), NULL);
	gtk_tooltips_set_tip(tooltips, prefs_button, _("Edit your Preferences"), NULL);
	gtk_tooltips_set_tip(tooltips, mute_button, _("Adjust the Volume"), NULL);
	gtk_tooltips_set_tip(tooltips, quit_button, _("Quit"), NULL);
	text = g_strdup_printf(_("Frequency: %.2f MHz"), adj->value/STEPS);
	gtk_tooltips_set_tip(tooltips, freq_scale, text, NULL);
	g_free(text);
/*	text = g_strdup_printf(_("Volume: %d%%"), (gint)volume->value);
	gtk_tooltips_set_tip(tooltips, vol_scale, text, NULL);
	g_free(text);*/
	
	return app;
}

static void
session_die_cb(GnomeClient* client, gpointer client_data)
{
	if (settings.mute_on_exit)
	{
		radio_mute();
		radio_stop();
	}
	mixer_close();
	gtk_main_quit();
	exit (0);
}

static void 
save_session_cb(GnomeClient *client, gint phase, GnomeSaveStyle save_style,
						gint is_shutdown, GnomeInteractStyle interact_style,
						gint is_fast, gpointer client_data)
{
	save_settings();
}

static void
gconf_error_handler(GConfClient *client, GError *error)
{
	g_print("GConf error: %s\n", error->message);
}

static gboolean
key_press_event_cb(GtkWidget *app, GdkEventKey *event, gpointer data)
{
	int vol = (int)(bacon_volume_button_get_value(BACON_VOLUME_BUTTON(mute_button)) + 0.5f);
	
	switch (event->keyval)
	{
		case GDK_F1: display_help_cb(NULL);
				break;
		case GDK_m: 
				toggle_volume();
				break;
		case GDK_q: 
				exit_gnome_radio();
				break;
		case GDK_r: 
				rec_button_clicked_cb(NULL, app);
				break;
		case GDK_f: 
				scfw_button_clicked_cb(NULL, NULL);
				break;
		case GDK_b: 
				scbw_button_clicked_cb(NULL, NULL);
				break;
		case GDK_n: 
				change_preset(TRUE);
				break;
		case GDK_p: 
				change_preset(FALSE);
				break;
		case GDK_KP_Add:
		case GDK_plus:	
				bacon_volume_button_set_value(BACON_VOLUME_BUTTON(mute_button), vol > 95 ? 100 : vol + 5);
				/*gtk_adjustment_set_value(volume, (volume->value > 95) ? 100 : volume->value+5);*/
				break;
		case GDK_minus:
		case GDK_KP_Subtract: 
				bacon_volume_button_set_value(BACON_VOLUME_BUTTON(mute_button), vol < 5 ? 0 : vol - 5);
				/*gtk_adjustment_set_value(volume,(volume->value < 5) ? 0 : volume->value-5);*/
				break;
	}
	return FALSE;
}

int main(int argc, char* argv[])
{
	GList *ptr;
	GnomeClient *client;
	GError *err = NULL;
	int redraw_timeout_id;
	gboolean do_scan = FALSE;
#if GNOME_14 
	GOptionContext *ctx;
	const GOptionEntry entries[] = {
		{ "scan", 0, 0, G_OPTION_ARG_NONE, &do_scan, N_("Scan for stations"), NULL },
		{ NULL }
	};
#endif	
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);  
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	g_set_application_name(_("Gnomeradio"));
	
#if GNOME_14
    if (!g_thread_supported ()) g_thread_init(NULL);
	ctx = g_option_context_new("- Gnomeradio");
	g_option_context_add_main_entries(ctx, entries, GETTEXT_PACKAGE);  
	g_option_context_add_group(ctx, gst_init_get_option_group());
	g_option_context_set_ignore_unknown_options(ctx, TRUE);	
#endif	

	gnome_program_init(PACKAGE, VERSION, 
					LIBGNOMEUI_MODULE, argc, argv, 
					GNOME_PROGRAM_STANDARD_PROPERTIES,
#if GNOME_14
					GNOME_PARAM_GOPTION_CONTEXT, ctx,
#endif
					NULL);
	gtk_window_set_default_icon_name("gnomeradio");
	/* Main app */
	main_visible = FALSE;
	app = gnome_radio_gui();

	/* Initizialize GStreamer */
	gst_init(&argc, &argv);
	
	/* Initizialize Gconf */
	if (!gconf_init(argc, argv, &err)) {
		char *details;
		details = g_strdup_printf(_("%s\n\nChanges to the settings won't be saved."), err->message);
		show_warning_message(_("Failed to init GConf!"), details);
		g_error_free(err); 
		g_free(details);
		err = NULL;
	} else {
		gconf_client_set_global_default_error_handler((GConfClientErrorHandlerFunc)gconf_error_handler);
		gconf_client_set_error_handling(gconf_client_get_default(),  GCONF_CLIENT_HANDLE_ALL);
		gnome_media_profiles_init(gconf_client_get_default());
	}

	load_settings();
	start_mixer(FALSE, app);
	start_radio(FALSE, app);
	if (is_first_start() || do_scan) {
		if (!radio_is_init()) {
			g_message(_("Could not scan. Radio is not initialized."));
		} else {
			initial_frequency_scan(app);
			set_first_time_flag();
		}
	}
	create_tray_menu(app);
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(preset_combo), _("manual"));
	for (ptr = settings.presets; ptr; ptr = g_list_next(ptr)) {
		preset *ps = (preset*)ptr->data;
		gtk_combo_box_append_text(GTK_COMBO_BOX(preset_combo), ps->title);
	}
	preset_combo_set_item(mom_ps);

	gtk_widget_show_all(app);
	main_visible = TRUE;

	/* Create an tray icon */
	create_tray_icon(app);

	adj_value_changed_cb(NULL, (gpointer) app);
	/*volume_value_changed_cb(NULL, NULL);*/
	
#ifdef HAVE_LIRC
	if(!my_lirc_init())
	{
/*		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					_("Could not start lirc"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
*/
		g_message(_("Could not start lirc!"));
	}
	else
		start_lirc();
#endif
	
/* Connect the Session Management signals
 */
	client = gnome_master_client();
	g_signal_connect(GTK_OBJECT(client), "save_yourself",
						GTK_SIGNAL_FUNC(save_session_cb), NULL);
	g_signal_connect(GTK_OBJECT(client), "die",
						GTK_SIGNAL_FUNC(session_die_cb), NULL);
	g_signal_connect(GTK_OBJECT(app), "key-press-event",
						GTK_SIGNAL_FUNC(key_press_event_cb), NULL);

	/* Redraw the status window every 3 seconds
	 * Necessary, because the mono/stereo reception
	 * needs some time to be correctly detected
	 */
	redraw_timeout_id = gtk_timeout_add(3000, (GtkFunction)redraw_status_window, NULL);	

	/* Checks if the volume has been changed by an 
	 * external app
	 */
	/*gtk_timeout_add(100, (GtkFunction)poll_volume_change, NULL);*/

	gtk_main();
		
#ifdef HAVE_LIRC	
	my_lirc_deinit();
#endif

	return 0;
}

static show_message(GtkMessageType type, const char* text, const char* details)
{
	GtkWidget *dialog;
	
	g_assert(text);
	
	dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, type, GTK_BUTTONS_CLOSE,
			text);
	if (details) {
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), details);
	}
	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
}	

void show_error_message(const char* error, const char* details)
{
	show_message(GTK_MESSAGE_ERROR, error, details);
}	

void show_warning_message(const char* warning, const char* details)
{
	show_message(GTK_MESSAGE_WARNING, warning, details);
}
