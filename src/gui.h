/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _GUI_H
#define _GUI_H

#define FREQ_MAX 108
#define FREQ_MIN 87.5
#define STEPS 20
#define SUNSHINE 106.15

#define DIALOG_FLAGS (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT)

typedef struct Gnomeradio_Settings gnomeradio_settings;
typedef struct Preset preset;

struct Gnomeradio_Settings
{
	gchar *device;
	gchar *mixer_dev;
	gchar *mixer;
	gboolean mute_on_exit;
		
	GList *presets;
};

struct Preset
{
	gchar name[21];
	gfloat freq;
};

GtkWidget* mute_button;
GtkAdjustment *adj, *volume;
GtkTooltips *tooltips;

int mom_ps;

gnomeradio_settings settings;

void start_radio(gboolean restart, GtkWidget* app);

void start_mixer(gboolean restart, GtkWidget* app);

GList* get_mixer_recdev_list(void);

void exit_gnome_radio(void);

void scfw_button_clicked_cb(GtkButton *button, gpointer data);

void scbw_button_clicked_cb(GtkButton *button, gpointer data);

void mute_button_toggled_cb(GtkButton *button, gpointer data);

void preset_menu_set_item(gint i);

void display_help_cb(char *topic);

void change_preset(gboolean next);

#endif

