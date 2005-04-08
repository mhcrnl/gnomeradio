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
#include "eggtrayicon.h"
#include "gui.h"
#include "tech.h"
#include "rec_tech.h"
#include "lirc.h"
#include "prefs.h"
#include "record.h"
#include "../pixmaps/digits.xpm"
#include "../pixmaps/signal.xpm"
#include "../pixmaps/stereo.xpm"
#include "../pixmaps/vol_up.xpm"
#include "../pixmaps/vol_down.xpm"
#include "../pixmaps/freq_up.xpm"
#include "../pixmaps/freq_down.xpm"
#include "../pixmaps/radio.xpm"

#define DIGIT_WIDTH 20
#define DIGIT_HEIGTH 30
#define SIGNAL_WIDTH 25
#define STEREO_WIDTH 35
#define SCAN_SPEED 20

#define TRANSLATORS "TRANSLATORS"

static GtkWidget *drawing_area;
static GdkPixmap *digits, *signal_s, *stereo;
static GtkWidget *preset_menu, *menu;
static GtkWidget *freq_scale, *vol_scale;
static GtkWidget *tray_icon;


static int timeout_id, bp_timeout_id = -1, bp_timeout_steps = 0;
gboolean tray_menu_disabled = FALSE;

void start_radio(gboolean restart, GtkWidget *app)
{
	if (restart)
		radio_stop();
	
	if (!radio_init(settings.device)) 
	{
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
 				_("Could not open device \"%s\" !\n\nCheck your Settings and make sure that no other\n"
				"program is using %s.\nMake also sure that you have read-access to it."), 
				settings.device, settings.device);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
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
		GtkWidget *dialog;
		char *buffer;
		
		if (res == -1)
			buffer = g_strdup_printf(_("Mixer source \"%s\" is not a valid source!"), settings.mixer);
		else 
			buffer = g_strdup_printf(_("Could not open \"%s\"!"), settings.mixer_dev);

		dialog = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, buffer);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		
		g_free(buffer);
	}		
	vol = mixer_get_volume();
	
	if (vol >= 0)
		gtk_adjustment_set_value(volume, (double)vol);
}

GList* get_mixer_recdev_list(void)
{
	int i;
	char **array, *dev;
	char *gconf_path = NULL, *sndcard_name = NULL;
	GList *result = NULL;
	GConfClient *client = NULL;
	
	array = mixer_get_rec_devices();
	if (!array)
		return NULL;

	if (!gconf_is_initialized())
		return NULL;
	
	client = gconf_client_get_default();
	
	sndcard_name = mixer_get_sndcard_name();
	if (sndcard_name)
	{
		g_strdelimit (sndcard_name, " ", '_');
		gconf_path = g_strdup_printf("/apps/gnome-volume-control/OSS-%s-%d/%%s/title", sndcard_name, 1);
		//puts(gconf_path);
		free(sndcard_name);
	}
	
	i = 0;	
	dev = array[i];
	while (dev)
	{
		char *gconf_dev = NULL;
		char *text = NULL;
		
		if (client && gconf_path)
		{
			char *path = g_strdup_printf(gconf_path, dev);
			gconf_dev = gconf_client_get_string(client, path , NULL);
			g_free(path);
		}
		if (gconf_dev)
		{
			if (strcmp(gconf_dev, dev))
				text = g_strdup_printf("%s (%s)", dev, gconf_dev);
			else
				text = g_strdup(dev);
			g_free(gconf_dev);
			gconf_dev = NULL;
		}
		else
			text = g_strdup(dev);
		result = g_list_append(result, text);
		free(dev);
		dev = array[++i];
	}			
	free(array);
	g_free(gconf_path);
	
	return result;
}

static gboolean
redraw_status_window(void)
{

	GdkWindow *real_window, *window;
	GdkGC *gc;
	int win_width, win_height;
	int val, freq[5], signal_strength, is_stereo, i;
	
	val = (int)(rint(adj->value)/STEPS * 100.0);
	
	freq[0] = val / 10000;
	freq[1] = (val % 10000) / 1000;
	freq[2] = (val % 1000) / 100; 
	freq[3] = (val % 100) / 10;
	freq[4] = val % 10;

	signal_strength = radio_getsignal();
	is_stereo = radio_getstereo();
	
	if (signal_strength > 3) signal_strength = 3;
	if (signal_strength < 0) signal_strength = 0;
	is_stereo = (is_stereo == 1) ? 1 : 0;
	
	real_window = drawing_area->window;
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
	

static gboolean
expose_event_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
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

static void adj_value_changed_cb(GtkAdjustment* data, gpointer window)
{
	gchar *buffer;
	
	redraw_status_window();
	
	buffer = g_strdup_printf(_("Gnomeradio - %.2f MHz"), rint(adj->value)/STEPS);
	gtk_window_set_title (GTK_WINDOW(window), buffer);
	g_free(buffer);
	
	buffer = g_strdup_printf(_("Frequency: %.2f MHz"), rint(adj->value)/STEPS);
	gtk_tooltips_set_tip(tooltips, freq_scale, buffer, NULL);
	g_free(buffer);
	
	buffer = g_strdup_printf(_("Gnomeradio - %.2f MHz"), rint(adj->value)/STEPS);
	gtk_tooltips_set_tip(tooltips, tray_icon, buffer, NULL);
	g_free(buffer);

	radio_setfreq(adj->value/STEPS);
}

static gboolean freq_scale_focus_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	mom_ps = -1;
	preset_menu_set_item(mom_ps);
	return FALSE;
}

static void volume_value_changed_cb(GtkAdjustment* data, gpointer window)
{
	char *text;
	
	mixer_set_volume((gint)volume->value);
	
	text = g_strdup_printf(_("Volume: %d%%"), (gint)volume->value);
	gtk_tooltips_set_tip(tooltips, vol_scale, text, NULL);
	g_free(text);

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

static void
change_frequency(gpointer data)
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

static gboolean
change_frequency_timeout(gpointer data)
{
	change_frequency(data);
	if (bp_timeout_steps < 10)
	{
		gtk_timeout_remove(bp_timeout_id);
		//g_print("adding timeout with interval %d\n", 200 - 20*bp_timeout_steps);
		bp_timeout_id = gtk_timeout_add(200 - 20*bp_timeout_steps,
			(GtkFunction)change_frequency_timeout, data);
		bp_timeout_steps++; 
	}
	return TRUE;
}	

static void step_button_pressed_cb(GtkButton *button, gpointer data)
{
	//change_frequency(data);
	bp_timeout_id = gtk_timeout_add(500, (GtkFunction)change_frequency_timeout, data);
	
	mom_ps = -1;
	preset_menu_set_item(mom_ps);
}

static void step_button_clicked_cb(GtkButton *button, gpointer data)
{
	change_frequency(data);

	mom_ps = -1;
	preset_menu_set_item(mom_ps);
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
	static gint signal, a, b, c;	
	static gint start, mom, max;
	gint dir = (gint)(data);
	
	if (!max)
		max = (FREQ_MAX - FREQ_MIN) * STEPS;

	if ((a + b + c > 8) || (start > max))	
	{
		start = mom = a = b = c = 0;
		radio_unmute();
		timeout_id = 0;
		return FALSE;
	}
	if (!mom)
	mom= adj->value;
	if (mom > FREQ_MAX*STEPS) 
		mom = FREQ_MIN*STEPS;
	else
	if (mom < FREQ_MIN*STEPS)
		mom = FREQ_MAX*STEPS;
	else	
		mom = mom + dir;
	start += 1;
	gtk_adjustment_set_value(adj, mom);
	signal = radio_getsignal();
	a = b;
	b = c;
	c = signal;
	return TRUE;
}

void scfw_button_clicked_cb(GtkButton *button, gpointer data)
{
	if (timeout_id)
		return;
	radio_mute();
	timeout_id = gtk_timeout_add(1000/SCAN_SPEED, (GtkFunction)scan_freq, (gpointer)1);	
	mom_ps = -1;
	preset_menu_set_item(mom_ps);

}

void scbw_button_clicked_cb(GtkButton *button, gpointer data)
{
	if (timeout_id)
		return;
	radio_mute();
	timeout_id = gtk_timeout_add(1000/SCAN_SPEED, (GtkFunction)scan_freq, (gpointer)(-1));	
	mom_ps = -1;
	preset_menu_set_item(mom_ps);
}

void preset_menu_set_item(gint i)
{
	gtk_option_menu_set_history(GTK_OPTION_MENU(preset_menu), i+1);
}

static void pref_item_activate_cb(GtkWidget *item, gpointer data)
{
	gint i = (gint)data;
	preset* ps = (preset*)g_list_nth_data(settings.presets,i);
	mom_ps = i;
	gtk_adjustment_set_value(adj, ps->freq*STEPS);
}

static void update_preset_menu(void)
{
	gint i, count;
	GtkWidget *item;
	preset *ps;
	
	if (menu)
		gtk_widget_destroy(menu);
	menu = gtk_menu_new();
	
	count = g_list_length(settings.presets);
	
	item = gtk_menu_item_new_with_label(_("manual"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	
	for (i=0;i<count;i++)
	{
		ps = g_list_nth_data(settings.presets, i);
		item = gtk_menu_item_new_with_label(ps->name);
		g_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(pref_item_activate_cb), (gpointer)i);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	gtk_widget_show_all(menu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(preset_menu), menu);
}

void
change_preset(gboolean next)
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
	preset_menu_set_item(mom_ps);
}

static void quit_button_clicked_cb(GtkButton *button, gpointer data)
{
	exit_gnome_radio();
}

static void prefs_button_clicked_cb(GtkButton *button, gpointer app)
{
	GtkWidget* dialog;
	gint choise;
	
	backup_settings();
	
	dialog = prefs_window();
	
	/* Michael Jochum <e9725005@stud3.tuwien.ac.at> proposed to not use gnome_dialog_set_parent()
	   but following instead. */
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(app));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	
	/*gnome_dialog_set_parent(GNOME_DIALOG(dialog), GTK_WINDOW(app));*/
	
	tray_menu_disabled = TRUE;
	choise = GTK_RESPONSE_HELP;
	while (choise == GTK_RESPONSE_HELP)
	{
		choise = gtk_dialog_run(GTK_DIALOG(dialog)); 
		switch (choise)
		{
			case GTK_RESPONSE_OK:
				gtk_widget_destroy(dialog);
				commit_settings(app);
				update_preset_menu();
				break;
		
			case GTK_RESPONSE_HELP:
				display_help_cb("gnomeradio-settings");
			break;
			default:
				gtk_widget_destroy(dialog);
				rollback_settings();
		}
	}
	tray_menu_disabled = FALSE;
}

static int
start_recording(void)
{
	GIOChannel *wavioc = NULL, *mp3ioc = NULL;
	GtkWidget *dialog;
	
	if (!mixer_set_rec_device())
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				"Could not set \"%s\" as recording Source", settings.mixer);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return -1;
	}
		
	if (rec_settings.mp3)
		record_as_mp3(&wavioc, &mp3ioc);
	else	
		record_as_wave(&wavioc);

	dialog = record_status_window();
	
	run_status_window(wavioc, mp3ioc);
	return 1;
}

static void rec_button_clicked_cb(GtkButton *button, gpointer app)
{
	GtkWidget *dialog;
	gint choise, retval=0;
	
	if (!check_sox_installation())
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				_("Sox could not be detected. Please ensure"
				" that it is installed in your PATH."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}
	
	dialog = record_prefs_window();
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(app));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	
	//g_print("choise: %d\n", choise);
	
	tray_menu_disabled = TRUE;
	choise = GTK_RESPONSE_HELP;
	while (choise == GTK_RESPONSE_HELP)
	{
		choise = gtk_dialog_run(GTK_DIALOG(dialog));
		switch (choise)
		{
			case GTK_RESPONSE_HELP:
				display_help_cb("gnomeradio-recording");
				break;

			case GTK_RESPONSE_OK:
		
				retval = 0;
				while (!retval)
				{
					if (g_file_test(rec_settings.filename, G_FILE_TEST_EXISTS ))
					{
						int ch;
						GtkWidget *question;
						question = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_QUESTION, 
									GTK_BUTTONS_YES_NO, 
									_("File '%s' exists.\nOverwrite it?"), rec_settings.filename);
						ch = gtk_dialog_run (GTK_DIALOG (question)); 
						gtk_widget_destroy (question);
						if (ch == GTK_RESPONSE_NO)
							break;
					}
					retval = check_filename(rec_settings.filename);
					if (!retval)
					{
						GtkWidget *errdialog;
						errdialog = gtk_message_dialog_new(GTK_WINDOW(app), DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
									_("Error opening file '%s':\n%s"), rec_settings.filename, strerror(errno));
						gtk_dialog_run (GTK_DIALOG (errdialog));
						gtk_widget_destroy (errdialog);
						break;
					}
				}
				if (retval > 0)
				{
					gtk_widget_destroy(dialog);
					start_recording();
				}
				else
					choise = GTK_RESPONSE_HELP;
				break;
			default:
				tray_menu_disabled = FALSE;
				gtk_widget_destroy(dialog);
		}
	}
}

void mute_button_toggled_cb(GtkButton *button, gpointer data)
{
	static gboolean muted = FALSE;
	
	if (muted)
	{
		radio_unmute();
		muted = FALSE;
		gtk_tooltips_set_tip(tooltips, mute_button, _("Mute"), NULL);
	}
	else
	{
		radio_mute();
		muted = TRUE;
		gtk_tooltips_set_tip(tooltips, mute_button, _("Unmute"), NULL);
	}
}

static void about_button_clicked_cb(GtkButton *button, gpointer data)
{
	GdkPixbuf *app_icon;
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

	app_icon = gdk_pixbuf_new_from_xpm_data((const char**)radio_xpm);

#ifdef HAVE_LIRC	
	text =_("Gnomeradio is a FM-Tuner application for the GNOME desktop. "
							"It should work with all tuner hardware that is supported by the video4linux drivers.\n\n"
							"This version has been compiled with LIRC support.");
#else
	text =_("Gnomeradio is a FM-Tuner application for the GNOME desktop. "
							"It should work with all tuner hardware that is supported by the video4linux drivers.\n\n"
							"This version has been compiled without LIRC support.");
#endif
	
	about = gnome_about_new ("Gnomeradio", VERSION, "Copyright 2001 - 2003 Jörgen Scheibengruber",
							text, (const char **) authors, NULL, 
							strcmp("TRANSLATORS", translators) ? translators : NULL, 
							app_icon);

	gtk_widget_show(about);
	g_object_add_weak_pointer(G_OBJECT(about), (gpointer*)&about);
	
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

gboolean
tray_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	static posx, posy;
	GtkWidget *app = GTK_WIDGET(data);
	switch (event->button)
	{
		case 1:
			if (event->type != GDK_BUTTON_PRESS)
				break;
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
			break;
		case 3:
			if (event->type != GDK_BUTTON_PRESS)
				break;
			if (tray_menu_disabled)
				break;
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, 
				NULL, NULL, event->button, event->time);
			break;
	}			
	return FALSE;
}	

gboolean
tray_destroyed (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	create_tray_icon(GTK_WIDGET(data));
	return TRUE;
}

void create_tray_icon(GtkWidget *app)
{
	GdkPixbuf *pixbuf, *scaled;
	GtkWidget *tray_icon_image;
	GtkWidget *eventbox;
	char *text;
	
	tray_icon = GTK_WIDGET(egg_tray_icon_new (PACKAGE));
	pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)radio_xpm);
	scaled = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_HYPER);
	gdk_pixbuf_unref(pixbuf);
	tray_icon_image = gtk_image_new_from_pixbuf(scaled);
	gdk_pixbuf_unref(scaled);

	eventbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(eventbox), tray_icon_image);
	gtk_container_add (GTK_CONTAINER(tray_icon), eventbox);

	g_signal_connect(G_OBJECT(eventbox), "button-press-event", 
		G_CALLBACK(tray_clicked), (gpointer)app);
	g_signal_connect(G_OBJECT(tray_icon), "destroy-event",
		G_CALLBACK(tray_destroyed), (gpointer)app);
	gtk_widget_show_all(GTK_WIDGET(tray_icon));
	
	text = g_strdup_printf(_("Gnomeradio - %.2f MHz"), adj->value/STEPS);
	gtk_tooltips_set_tip(tooltips, tray_icon, text, NULL);
	g_free(text);
}	

GtkWidget* gnome_radio_gui(void)
{
	GtkWidget *app;
	GtkWidget *prefs_button, *quit_button, *scfw_button, *scbw_button;
	GtkWidget *stfw_button, *stbw_button, *about_button, *rec_button;
	GtkWidget *prefs_pixmap, *quit_pixmap, *scfw_pixmap, *scbw_pixmap;
	GtkWidget *stfw_pixmap, *stbw_pixmap, *about_pixmap, *mute_pixmap, *rec_pixmap;
	GtkWidget *vol_up_pixmap, *vol_down_pixmap, *freq_up_pixmap, *freq_down_pixmap;
	GdkPixbuf *vol_up_pixbuf, *vol_down_pixbuf, *freq_up_pixbuf, *freq_down_pixbuf;
	GtkWidget *hbox1, *hbox2, *vbox, *menubox, *freq_vol_box;
	GtkWidget *hseparator1, *hseparator2, *vseparator1, *vseparator2, *vseparator3, *vseparator4;
	GtkWidget *label;
	GtkWidget *frame1, *frame2;
	gchar *text;

	app = gnome_app_new(PACKAGE, _("Gnomeradio"));

	gtk_window_set_resizable(GTK_WINDOW(app), FALSE);
	//gtk_window_set_policy(GTK_WINDOW(app), FALSE, FALSE, FALSE);
	gtk_window_set_wmclass(GTK_WINDOW(app), "gnomeradio", "Gnomeradio");

	frame1 = gtk_frame_new(NULL);
	frame2 = gtk_frame_new(NULL);

	/*quit_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_QUIT, GTK_ICON_SIZE_BUTTON);
	prefs_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_PROPERTIES, GTK_ICON_SIZE_BUTTON);
	scfw_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_LAST, GTK_ICON_SIZE_BUTTON);
	scbw_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_FIRST, GTK_ICON_SIZE_BUTTON);
	stfw_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_FORWARD, GTK_ICON_SIZE_BUTTON);
	stbw_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_BACK, GTK_ICON_SIZE_BUTTON);
	about_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_ABOUT, GTK_ICON_SIZE_BUTTON);
	mute_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_VOLUME, GTK_ICON_SIZE_BUTTON);
	rec_pixmap = gtk_image_new_from_stock(GNOME_STOCK_PIXMAP_MIC, GTK_ICON_SIZE_BUTTON);*/
	
	quit_pixmap = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
	prefs_pixmap = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_BUTTON);
	scfw_pixmap = gtk_image_new_from_stock(GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_BUTTON);
	scbw_pixmap = gtk_image_new_from_stock(GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_BUTTON);
	stfw_pixmap = gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON);
	stbw_pixmap = gtk_image_new_from_stock(GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON);
	about_pixmap = gtk_image_new_from_stock(GNOME_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON);
	mute_pixmap = gtk_image_new_from_stock(GNOME_STOCK_VOLUME, GTK_ICON_SIZE_BUTTON);
	rec_pixmap = gtk_image_new_from_stock(GNOME_STOCK_MIC, GTK_ICON_SIZE_BUTTON);
	//help_pixmap = gtk_image_new_from_stock(GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
	
	quit_button = gtk_button_new();
	prefs_button = gtk_button_new();
	scfw_button = gtk_button_new();
	scbw_button = gtk_button_new();
	stfw_button = gtk_button_new();
	stbw_button = gtk_button_new();
	about_button = gtk_button_new();
	mute_button = gtk_toggle_button_new();
	rec_button = gtk_button_new();
	//help_button = gtk_button_new();

	gtk_container_add(GTK_CONTAINER(quit_button), quit_pixmap);
	gtk_container_add(GTK_CONTAINER(prefs_button), prefs_pixmap);
	gtk_container_add(GTK_CONTAINER(scfw_button), scfw_pixmap);
	gtk_container_add(GTK_CONTAINER(scbw_button), scbw_pixmap);
	gtk_container_add(GTK_CONTAINER(stfw_button), stfw_pixmap);
	gtk_container_add(GTK_CONTAINER(stbw_button), stbw_pixmap);
	gtk_container_add(GTK_CONTAINER(about_button), about_pixmap);
	gtk_container_add(GTK_CONTAINER(mute_button), mute_pixmap);
	gtk_container_add(GTK_CONTAINER(rec_button), rec_pixmap);
	//gtk_container_add(GTK_CONTAINER(help_button), help_pixmap);

	/*gtk_widget_set_usize(quit_button, 30, 30);
	gtk_widget_set_usize(prefs_button, 30, 30);
	gtk_widget_set_usize(scfw_button, 30, 30);
	gtk_widget_set_usize(scbw_button, 30, 30);
	gtk_widget_set_usize(stfw_button, 30, 30);
	gtk_widget_set_usize(stbw_button, 30, 30);
	gtk_widget_set_usize(about_button, 30, 30);
	gtk_widget_set_usize(mute_button, 30, 30);
	gtk_widget_set_usize(rec_button, 30, 30);*/

	vbox = gtk_vbox_new(FALSE, 0);
	hbox1 = gtk_hbox_new(FALSE, 0);
	hbox2 = gtk_hbox_new(FALSE, 0);
	menubox = gtk_vbox_new(FALSE, 0);
	freq_vol_box = gtk_hbox_new(FALSE, 0);
	
	adj = GTK_ADJUSTMENT(gtk_adjustment_new(SUNSHINE*STEPS, FREQ_MIN*STEPS, FREQ_MAX*STEPS+1, 1, STEPS, 1));
	volume = GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 101, 1, 10, 1));
	
	
	preset_menu = gtk_option_menu_new();
	//gtk_widget_set_usize(preset_menu, 10, 25);	
	gtk_widget_set_size_request(preset_menu, 10, -1);
	label = gtk_label_new(_("Presets:"));
	
	freq_scale = gtk_hscale_new(adj);
	//gtk_range_set_update_policy(GTK_RANGE(freq_scale), GTK_UPDATE_DELAYED);
	vol_scale = gtk_hscale_new(volume);
	
	vol_up_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)vol_up_xpm);
	vol_down_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)vol_down_xpm);
	freq_up_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)freq_up_xpm);
	freq_down_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)freq_down_xpm);

	vol_up_pixmap = gtk_image_new_from_pixbuf(vol_up_pixbuf);
	vol_down_pixmap = gtk_image_new_from_pixbuf(vol_down_pixbuf);
	freq_up_pixmap = gtk_image_new_from_pixbuf(freq_up_pixbuf);
	freq_down_pixmap = gtk_image_new_from_pixbuf(freq_down_pixbuf);

	//gtk_widget_set_usize(freq_scale, 160, 10);
	gtk_widget_set_size_request(freq_scale, 160, -1);

	gtk_widget_realize(app);
	drawing_area = gtk_drawing_area_new();
	digits = gdk_pixmap_create_from_xpm_d (app->window, NULL, NULL, digits_xpm);
	signal_s = gdk_pixmap_create_from_xpm_d (app->window, NULL, NULL, signal_xpm);
	stereo = gdk_pixmap_create_from_xpm_d (app->window, NULL, NULL, stereo_xpm);
	
	hseparator1 = gtk_hseparator_new();
	hseparator2 = gtk_hseparator_new();
	vseparator1 = gtk_vseparator_new();
	vseparator2 = gtk_vseparator_new();
	vseparator3 = gtk_vseparator_new();
	vseparator4 = gtk_vseparator_new();
	
	gtk_scale_set_digits(GTK_SCALE(freq_scale), 0);
	gtk_scale_set_draw_value(GTK_SCALE(freq_scale), FALSE);
	gtk_scale_set_digits(GTK_SCALE(vol_scale), 0);
	gtk_scale_set_draw_value(GTK_SCALE(vol_scale), FALSE);

	//gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), DIGIT_WIDTH*6+10+SIGNAL_WIDTH+STEREO_WIDTH, DIGIT_HEIGTH+10);
	gtk_widget_set_size_request(drawing_area, DIGIT_WIDTH*6+10+SIGNAL_WIDTH+STEREO_WIDTH, DIGIT_HEIGTH+10);
	//gtk_widget_set_usize(spinbutton, 60, 25);

	gtk_container_add(GTK_CONTAINER(frame2), drawing_area);

	gtk_box_pack_start(GTK_BOX(hbox2), scbw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), stbw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), stfw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), scfw_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), vseparator1, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), mute_button, FALSE, FALSE, 2);
	//gtk_box_pack_start(GTK_BOX(hbox2), vseparator2, TRUE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(hbox2), rec_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), vseparator4, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), about_button, FALSE, FALSE, 2);
	//gtk_box_pack_start(GTK_BOX(hbox2), help_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), prefs_button, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox2), quit_button, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(hbox1), frame2, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox1), menubox, TRUE, TRUE, 3);
	
	gtk_box_pack_start(GTK_BOX(menubox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(menubox), preset_menu, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(freq_vol_box), freq_down_pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), freq_scale, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), freq_up_pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vseparator3, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vol_down_pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vol_scale, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(freq_vol_box), vol_up_pixmap, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), freq_vol_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hseparator1, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 4);
	//gtk_box_pack_start(GTK_BOX(vbox), hseparator2, FALSE, FALSE, 0);
	
	gtk_container_add(GTK_CONTAINER(frame1), vbox);
	
	gtk_frame_set_shadow_type(GTK_FRAME(frame2), GTK_SHADOW_IN);

	gtk_container_set_border_width(GTK_CONTAINER(frame1), 3);
	gtk_container_set_border_width(GTK_CONTAINER(frame2), 2);

	gnome_app_set_contents(GNOME_APP(app), frame1);

	//status = gnome_appbar_new(FALSE, TRUE, GNOME_PREFERENCES_NEVER);

	//gnome_app_set_statusbar(GNOME_APP(app), status);

	tooltips = gtk_tooltips_new();

	g_signal_connect(GTK_OBJECT(app), "delete_event", GTK_SIGNAL_FUNC(delete_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(quit_button), "clicked", GTK_SIGNAL_FUNC(quit_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(adj), "value-changed", GTK_SIGNAL_FUNC(adj_value_changed_cb), (gpointer) app);
	g_signal_connect(GTK_OBJECT(freq_scale), "button-release-event", GTK_SIGNAL_FUNC(freq_scale_focus_cb), NULL);
	g_signal_connect(GTK_OBJECT(volume), "value-changed", GTK_SIGNAL_FUNC(volume_value_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(stfw_button), "pressed", GTK_SIGNAL_FUNC(step_button_pressed_cb), (gpointer)TRUE);
	g_signal_connect(GTK_OBJECT(stbw_button), "pressed", GTK_SIGNAL_FUNC(step_button_pressed_cb), (gpointer)FALSE);
	g_signal_connect(GTK_OBJECT(stfw_button), "clicked", GTK_SIGNAL_FUNC(step_button_clicked_cb), (gpointer)TRUE);
	g_signal_connect(GTK_OBJECT(stbw_button), "clicked", GTK_SIGNAL_FUNC(step_button_clicked_cb), (gpointer)FALSE);
	g_signal_connect(GTK_OBJECT(stfw_button), "released", GTK_SIGNAL_FUNC(step_button_released_cb), NULL);
	g_signal_connect(GTK_OBJECT(stbw_button), "released", GTK_SIGNAL_FUNC(step_button_released_cb), NULL);
	g_signal_connect(GTK_OBJECT(scfw_button), "clicked", GTK_SIGNAL_FUNC(scfw_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(scbw_button), "clicked", GTK_SIGNAL_FUNC(scbw_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(about_button), "clicked", GTK_SIGNAL_FUNC(about_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(mute_button), "toggled", GTK_SIGNAL_FUNC(mute_button_toggled_cb), NULL);
	g_signal_connect(GTK_OBJECT(rec_button), "clicked", GTK_SIGNAL_FUNC(rec_button_clicked_cb), (gpointer) app);
	g_signal_connect(GTK_OBJECT(prefs_button), "clicked", GTK_SIGNAL_FUNC(prefs_button_clicked_cb), (gpointer) app);
	g_signal_connect(GTK_OBJECT(drawing_area), "expose-event", GTK_SIGNAL_FUNC(expose_event_cb), NULL);

	gtk_tooltips_set_tip(tooltips, scbw_button, _("Scan Backwards"), NULL);
	gtk_tooltips_set_tip(tooltips, scfw_button, _("Scan Forwards"), NULL);
	gtk_tooltips_set_tip(tooltips, stbw_button, _("0.05 MHz Backwards"), NULL);
	gtk_tooltips_set_tip(tooltips, stfw_button, _("0.05 MHz Forwards"), NULL);
	gtk_tooltips_set_tip(tooltips, about_button, _("About Gnomeradio"), NULL);
	gtk_tooltips_set_tip(tooltips, rec_button, _("Record radio as Wave or MP3"), NULL);
	gtk_tooltips_set_tip(tooltips, prefs_button, _("Edit your Preferences"), NULL);
	gtk_tooltips_set_tip(tooltips, mute_button, _("Mute"), NULL);
	gtk_tooltips_set_tip(tooltips, quit_button, _("Quit"), NULL);
	text = g_strdup_printf(_("Frequency: %.2f MHz"), adj->value/STEPS);
	gtk_tooltips_set_tip(tooltips, freq_scale, text, NULL);
	g_free(text);
	text = g_strdup_printf(_("Volume: %d%%"), (gint)volume->value);
	gtk_tooltips_set_tip(tooltips, vol_scale, text, NULL);
	g_free(text);
	
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
	//gnome_error_dialog(error->message);
	g_print("GConf error: %s\n", error->message);
}

static gboolean
key_press_event_cb(GtkWidget *app, GdkEventKey *event, gpointer data)
{
	GtkToggleButton *tb = GTK_TOGGLE_BUTTON(mute_button);
	gboolean state = gtk_toggle_button_get_active(tb);
	//g_print("%s key pressed: %d\n",  gdk_keyval_name(event->keyval), event->keyval);
	
	switch (event->keyval)
	{
		case GDK_F1: display_help_cb(NULL);
				break;
		case GDK_m: 
				gtk_toggle_button_set_active(tb, !state);
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
				gtk_adjustment_set_value(volume, (volume->value > 95) ? 100 : volume->value+5);
				break;
		case GDK_minus:
		case GDK_KP_Subtract: 
				gtk_adjustment_set_value(volume,(volume->value < 5) ? 0 : volume->value-5);
				break;
	}
	return FALSE;
}

int main(int argc, char* argv[])
{
	GtkWidget* app;
	GList *icons;
	GdkPixbuf *app_icon;
	GnomeClient *client;
	GError *err = NULL;
	int redraw_timeout_id;
	
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);  
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	gnome_program_init(PACKAGE, VERSION, 
					LIBGNOMEUI_MODULE, argc, argv, 
					GNOME_PROGRAM_STANDARD_PROPERTIES, 
					NULL);
		
	app_icon = gdk_pixbuf_new_from_xpm_data((const char**)radio_xpm);
	icons = g_list_append(NULL, (gpointer)app_icon);
	gtk_window_set_default_icon_list(icons);

	
	/* Main app */

	app = gnome_radio_gui();

	gtk_widget_show_all(app);

	/* Create an tray icon */
	create_tray_icon(app);
	
	/* Initizialize Gconf */
	if (!gconf_init(argc, argv, &err))
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						_("Failed to init GConf: %s\n"
						"Changes to the settings won't be saved\n"), err->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_error_free(err); 
		err = NULL;
	}
	else
	{
		gconf_client_set_global_default_error_handler((GConfClientErrorHandlerFunc)gconf_error_handler);
		gconf_client_set_error_handling(gconf_client_get_default(),  GCONF_CLIENT_HANDLE_ALL);
	}
		
	load_settings();
	update_preset_menu();
	//g_print("momps: %i\n", mom_ps);
	preset_menu_set_item(mom_ps);

	start_radio(FALSE, app);
	
	start_mixer(FALSE, app);
	adj_value_changed_cb(NULL, (gpointer) app);
	volume_value_changed_cb(NULL, NULL);

#ifdef HAVE_LIRC
	if(!my_lirc_init())
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					_("Could not start lirc"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
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
	//gtk_timeout_add(100, (GtkFunction)poll_volume_change, NULL);

	gtk_main();
		
#ifdef HAVE_LIRC	
	my_lirc_deinit();
#endif

	return 0;
}
