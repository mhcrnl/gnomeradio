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

#include <gconf/gconf-client.h>
#include <profiles/audio-profile-choose.h>
#include <string.h>
#include "config.h"
#include "prefs.h"
#include "trayicon.h"
#include "gui.h"
#include "rec_tech.h"

static GtkWidget *device_entry;
static GtkWidget *mixer_combo;
static GtkWidget *mute_on_exit_cb;
static GtkWidget *list_view;
static GtkListStore *list_store;
static GtkTreeSelection *selection;

gboolean save_settings(void)
{
	gint i, count;
	gchar *buffer;
	preset *ps;
	GConfClient* client = NULL;
	
	if (!gconf_is_initialized())
		return FALSE;

	client = gconf_client_get_default();
	if (!client)
		return FALSE;
	
	/* Store general settings */
	gconf_client_set_string(client, "/apps/gnomeradio/device", settings.device, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/mixer", settings.mixer, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/mixer-device", settings.mixer_dev, NULL);
	gconf_client_set_bool(client, "/apps/gnomeradio/mute-on-exit", settings.mute_on_exit, NULL);
	/*gconf_client_set_float(client, "/apps/gnomeradio/volume", volume->value, NULL);*/
	gconf_client_set_float(client, "/apps/gnomeradio/last-freq", adj->value/STEPS, NULL);

	/* Store recording settings */
/*	gconf_client_set_string(client, "/apps/gnomeradio/recording/audiodevice", rec_settings.audiodevice, NULL);
	gconf_client_set_bool(client, "/apps/gnomeradio/recording/record-as-mp3", rec_settings.mp3, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/sample-rate", rec_settings.rate, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/sample-format", rec_settings.sample, NULL);
	gconf_client_set_bool(client, "/apps/gnomeradio/recording/record-in-stereo", rec_settings.stereo, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/encoder", rec_settings.encoder, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/bitrate", rec_settings.bitrate, NULL);
*/

	gconf_client_set_string(client, "/apps/gnomeradio/recording/destination", rec_settings.destination, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/profile", rec_settings.profile, NULL);

	/* Store the presets */
	count = g_list_length(settings.presets);
	gconf_client_set_int(client, "/apps/gnomeradio/presets/presets", count, NULL);
	for (i=0;i<count;i++)
	{
		ps = g_list_nth_data(settings.presets, i);
		buffer = g_strdup_printf("/apps/gnomeradio/presets/%d/name", i);
		gconf_client_set_string(client, buffer, ps->title, NULL); 
		g_free(buffer);
		buffer = g_strdup_printf("/apps/gnomeradio/presets/%d/freqency", i);
		gconf_client_set_float(client, buffer, ps->freq, NULL); 
		g_free(buffer);
	}	
	gconf_client_set_int(client, "/apps/gnomeradio/presets/last", mom_ps, NULL);
	g_print("Storing Settings in GConf database\n");
	
	return TRUE;
}			

gboolean load_settings(void)
{
	gint i, count;
	gchar *buffer;
	preset *ps;
	GConfClient *client = NULL;
	double freq;
	
	settings.presets = NULL;
		
	if (!gconf_is_initialized())
		return FALSE;
	
	client = gconf_client_get_default();
	if (!client)
		return FALSE;

	/* Load general settings */
	settings.device = gconf_client_get_string(client, "/apps/gnomeradio/device" , NULL);
	if (!settings.device)
		settings.device = g_strdup("/dev/radio");
	settings.mixer = gconf_client_get_string(client, "/apps/gnomeradio/mixer", NULL);
	if (!settings.mixer)
		settings.mixer = g_strdup("line");
	settings.mixer_dev = gconf_client_get_string(client, "/apps/gnomeradio/mixer-device", NULL);
	if (!settings.mixer_dev)
		settings.mixer_dev = g_strdup("/dev/mixer");
	settings.mute_on_exit = gconf_client_get_bool(client, "/apps/gnomeradio/mute-on-exit", NULL);
	/*volume->value = gconf_client_get_float(client, "/apps/gnomeradio/volume", NULL);*/
	freq = gconf_client_get_float(client, "/apps/gnomeradio/last-freq", NULL);
	if ((freq < FREQ_MIN) || (freq > FREQ_MAX))
		adj->value = FREQ_MIN * STEPS;
	else
		adj->value = freq * STEPS;
	
	/* Load recording settings */
/*	rec_settings.audiodevice = gconf_client_get_string(client, "/apps/gnomeradio/recording/audiodevice", NULL);
	if (!rec_settings.audiodevice)
		rec_settings.audiodevice = g_strdup("/dev/audio");
	rec_settings.mp3 = gconf_client_get_bool(client, "/apps/gnomeradio/recording/record-as-mp3", NULL);
	rec_settings.rate = gconf_client_get_string(client, "/apps/gnomeradio/recording/sample-rate", NULL);
	if (!rec_settings.rate)
		rec_settings.rate = g_strdup("44100");
	rec_settings.sample = gconf_client_get_string(client, "/apps/gnomeradio/recording/sample-format", NULL);
	if (!rec_settings.sample)
		rec_settings.sample = g_strdup("16");
	rec_settings.stereo = gconf_client_get_bool(client, "/apps/gnomeradio/recording/record-in-stereo", NULL);
	rec_settings.encoder = gconf_client_get_string(client, "/apps/gnomeradio/recording/encoder", NULL);
	if (!rec_settings.encoder)
		rec_settings.encoder = g_strdup("oggenc");
	rec_settings.bitrate = gconf_client_get_string(client, "/apps/gnomeradio/recording/bitrate", NULL);
	if (!rec_settings.bitrate)
		rec_settings.bitrate = g_strdup("192");*/

	rec_settings.destination = gconf_client_get_string(client, "/apps/gnomeradio/recording/destination", NULL);
	if (!rec_settings.destination)
		rec_settings.destination = g_strdup(g_get_home_dir());
	rec_settings.profile = gconf_client_get_string(client, "/apps/gnomeradio/recording/profile", NULL);
	if (!rec_settings.profile)
		rec_settings.profile = g_strdup("cdlossy");
	
	/* Load the presets */
	count = gconf_client_get_int(client, "/apps/gnomeradio/presets/presets", NULL);
	for (i=0;i<count;i++)
	{
		ps = malloc(sizeof(preset));
		buffer = g_strdup_printf("/apps/gnomeradio/presets/%d/name", i);
		ps->title = gconf_client_get_string(client, buffer, NULL); 
		g_free(buffer);
		if (!ps->title)
			ps->title = g_strdup(_("unnamed"));
		buffer = g_strdup_printf("/apps/gnomeradio/presets/%d/freqency", i);
		freq = gconf_client_get_float(client, buffer, NULL); 
		if ((freq < FREQ_MIN) || (freq > FREQ_MAX))
			ps->freq = FREQ_MIN;
		else
			ps->freq = freq;
		g_free(buffer);
		settings.presets = g_list_append(settings.presets, (gpointer)ps);	
	}	
	mom_ps = gconf_client_get_int(client, "/apps/gnomeradio/presets/last", NULL);
	if (mom_ps >= count)
		mom_ps = -1;

	return TRUE;
}			
	
static void mute_on_exit_toggled_cb(GtkWidget* widget, gpointer data)
{
	settings.mute_on_exit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mute_on_exit_cb));
}	

static gboolean device_entry_activate_cb(GtkWidget *widget, gpointer data)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(device_entry));

	if (!strcmp(settings.device, text)) return FALSE;
	
	if (settings.device) g_free(settings.device);
	settings.device = g_strdup(text);
	
	start_radio(TRUE, data);
	
	return FALSE;
}

static gboolean mixer_combo_change_cb(GtkComboBox *combo, gpointer data)
{
	GList *mixer_devs;
	int active;
	gchar *mixer_dev, *tmp;
	
	g_assert(combo);
	mixer_devs = g_object_get_data(G_OBJECT(combo), "mixer_devs");
	active = gtk_combo_box_get_active(combo);
	g_assert(active > -1);
	
	mixer_dev = (gchar*)g_list_nth_data(mixer_devs, active);
	g_assert(mixer_dev);
	
	if (g_str_equal(mixer_dev, settings.mixer))
		return FALSE;

	if (settings.mixer) g_free(settings.mixer);
	settings.mixer = g_strdup(mixer_dev);
	
	if ((tmp = strstr(settings.mixer, " (")))
		tmp[0] = '\0';
	
	start_mixer(TRUE, data);
	
	return FALSE;
}

/*static gboolean bitrate_combo_change_cb(GtkComboBox *combo, gpointer data)
{
	GList *bitrates;
	gint active;
	gchar *bitrate;

	g_assert(combo);
	bitrates = g_object_get_data(G_OBJECT(combo), "bitrates");
	active = gtk_combo_box_get_active(combo);
	g_assert(active > -1);
	
	bitrate = (gchar*)g_list_nth_data(bitrates, active);
	g_assert(bitrate);

	if (rec_settings.bitrate) g_free(rec_settings.bitrate);
	rec_settings.bitrate = g_strdup(bitrate);
	
	return FALSE;
}*/

/*static gboolean encoder_combo_change_cb(GtkComboBox *combo, gpointer bitrate_combo)
{
	GList *encoders;
	gint active;
	gchar *encoder;
	
	g_assert(combo);
	encoders = g_object_get_data(G_OBJECT(combo), "encoders");
	active = gtk_combo_box_get_active(combo);
	g_assert(active > -1);
	
	encoder = (gchar*)g_list_nth_data(encoders, active);
	g_assert(encoder);

	if (g_str_equal(encoder, _("Wave file"))) rec_settings.mp3 = FALSE;
	else {
		rec_settings.mp3 = TRUE;
		if (rec_settings.encoder) g_free(rec_settings.encoder);
		rec_settings.encoder = g_strdup(encoder);
	}
	gtk_widget_set_sensitive(bitrate_combo, rec_settings.mp3);
	
	return FALSE;
}*/

static gboolean profile_combo_change_cb(GtkComboBox *combo, gpointer userdata)
{
	GMAudioProfile* profile = gm_audio_profile_choose_get_active(GTK_WIDGET(combo));

	g_assert(rec_settings.profile);
	g_free(rec_settings.profile);
	rec_settings.profile = g_strdup(gm_audio_profile_get_id(profile));

	return FALSE;
}

static void add_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	preset *ps;
	gchar *buffer;
	GtkTreeIter iter = {0};
	GtkAdjustment* v_scb;
	GtkTreePath *path = NULL;
	GList* menuitems;
	GtkWidget *menuitem;
	
	ps = malloc(sizeof(preset));
	ps->title = g_strdup(_("unnamed"));
	ps->freq = rint(adj->value) / STEPS;
	settings.presets = g_list_append(settings.presets, (gpointer) ps);
	buffer = g_strdup_printf("%.2f", ps->freq);

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, ps->title, 1, buffer, -1);

	g_free(buffer);
	gtk_tree_selection_unselect_all(selection);
	
	v_scb = gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(list_view));
	gtk_adjustment_set_value(v_scb, v_scb->upper);
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(preset_combo), ps->title);
	mom_ps = g_list_length(settings.presets) - 1;
	preset_combo_set_item(mom_ps);

	menuitems = GTK_MENU_SHELL(tray_menu)->children;
	menuitem = gtk_menu_item_new_with_label(ps->title); 
		
	gtk_menu_shell_insert(GTK_MENU_SHELL(tray_menu), menuitem, mom_ps);		
	g_signal_connect(G_OBJECT(menuitem), "activate", (GCallback)preset_menuitem_activate_cb, (gpointer)mom_ps);
	gtk_widget_show(menuitem);

	buffer = g_strdup_printf("%d", g_list_length(settings.presets) - 1);
	path = gtk_tree_path_new_from_string(buffer);
	g_free(buffer);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(list_view), path, NULL, FALSE);
	gtk_tree_path_free(path);
}


static void del_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_column = NULL;
	GtkTreeIter iter;
	preset *ps;
	int *row;
	GList* menuitems;
	GtkWidget *menuitem;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(list_view), &path, &focus_column);
	
	if (!path) return;

	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(settings.presets));

	ps = g_list_nth_data(settings.presets, *row);
	g_assert(ps);	
	settings.presets = g_list_remove(settings.presets, (gpointer)ps);
	g_free(ps->title);
	g_free(ps);
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_remove(list_store, &iter);

	gtk_combo_box_remove_text(GTK_COMBO_BOX(preset_combo), *row + 1);
	if (--mom_ps < 0) mom_ps = 0;
	if (!g_list_length(settings.presets)) mom_ps = -1;
	preset_combo_set_item(mom_ps);

	menuitems = GTK_MENU_SHELL(tray_menu)->children;
	g_assert(*row < g_list_length(menuitems));
	menuitem = g_list_nth_data(menuitems, *row);
	gtk_widget_destroy(menuitem);
	
	gtk_tree_path_prev(path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(list_view), path, NULL, FALSE);
	gtk_tree_path_free(path);	
}

static void destination_button_clicked_cb(GtkWidget *button, gpointer data)
{
	GtkWidget *dialog;
	
	dialog = gtk_file_chooser_dialog_new(_("Choose a destination folder"), NULL, 
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, 
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), rec_settings.destination);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		if (rec_settings.destination) g_free(rec_settings.destination);
		rec_settings.destination = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_button_set_label(GTK_BUTTON(button), rec_settings.destination);
	}
	
	gtk_widget_destroy (dialog);
}

static gboolean list_view_key_press_event_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_Delete)
		del_button_clicked_cb(widget, user_data);
	if (event->keyval == GDK_Insert)
		add_button_clicked_cb(widget, user_data);
	
	return FALSE;
}		

static void name_cell_edited_cb(GtkCellRendererText *cellrenderertext, gchar *path_str, gchar *new_val, gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	preset *ps;
	int *row;
	GList* menuitems;
	GtkWidget *menuitem;
	
	path = gtk_tree_path_new_from_string(path_str);

	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(settings.presets));

	ps = g_list_nth_data(settings.presets, *row);
	g_assert(ps);	
	if (ps->title) g_free(ps->title);
	ps->title = g_strdup(new_val);

	gtk_combo_box_remove_text(GTK_COMBO_BOX(preset_combo), *row + 1);
	gtk_combo_box_insert_text(GTK_COMBO_BOX(preset_combo), *row + 1, ps->title);
	preset_combo_set_item(mom_ps);
	
	menuitems = GTK_MENU_SHELL(tray_menu)->children;
	g_assert(mom_ps < g_list_length(menuitems));
	menuitem = g_list_nth_data(menuitems, mom_ps);
	gtk_widget_destroy(menuitem);
	menuitem = gtk_menu_item_new_with_label(ps->title); 
		
	gtk_menu_shell_insert(GTK_MENU_SHELL(tray_menu), menuitem, *row);		
	g_signal_connect(G_OBJECT(menuitem), "activate", (GCallback)preset_menuitem_activate_cb, (gpointer)mom_ps);
	gtk_widget_show(menuitem);
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_set(GTK_LIST_STORE(list_store), &iter, 0, new_val, -1);
	gtk_tree_path_free(path);	
}	

static void freq_cell_edited_cb(GtkCellRendererText *cellrenderertext, gchar *path_str, gchar *new_val, gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	preset *ps;
	int *row;
	double value;
	gchar *freq_str;
	
	if (sscanf(new_val, "%lf", &value) != 1) return;
	
	if (value < FREQ_MIN) value = FREQ_MIN;
	if (value > FREQ_MAX) value = FREQ_MAX;
	value = rint(value * STEPS) / STEPS;
	
	freq_str = g_strdup_printf("%.2f", value);
	
	path = gtk_tree_path_new_from_string(path_str);
	
	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(settings.presets));

	ps = g_list_nth_data(settings.presets, *row);
	g_assert(ps);	
	ps->freq = value;

	gtk_adjustment_set_value(adj, value * STEPS);
	mom_ps = *row;
	preset_combo_set_item(mom_ps);
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_set(GTK_LIST_STORE(list_store), &iter, 1, freq_str, -1);
	g_free(freq_str);
	gtk_tree_path_free(path);	
}	

static void
list_view_cursor_changed_cb(GtkWidget *widget, gpointer data)
{
	int *row;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_column = NULL;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(list_view), &path, &focus_column);
	
	if (!path) return;

	row = gtk_tree_path_get_indices(path);
	g_assert(row);

	mom_ps = *row;
	preset_combo_set_item(mom_ps);
	return;
}

static void free_string_list(GList *list)
{
	if (!list) return;
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

GtkWidget* prefs_window(GtkWidget *app)
{
	GtkWidget *dialog;
	GtkWidget *box, *sbox, *pbox, *rbox;
	GtkWidget *settings_box, *presets_box, *record_box;
	GtkWidget *settings_label, *presets_label, *record_label;
	GtkWidget *s_indent_label, *p_indent_label, *r_indent_label;
	GtkWidget *destination_label;
	GtkWidget *destination_button;
	GtkWidget *profile_combo;
	GtkWidget *mixer_eb, *profile_eb;
	GtkWidget *preset_box;
	GtkWidget *settings_table, *record_table;
	GtkWidget *device_label, *mixer_label;
	GtkWidget *button_box;
	GtkWidget *add_button, *del_button;
	GtkWidget *scrolled_window;
	GtkCellRenderer *cellrenderer;
	GtkTreeViewColumn *list_column;
	GList *mixer_devs, *profiles, *ptr;
	gint i, active;
	char *settings_hdr, *presets_hdr, *record_hdr;
	preset* ps;
	
	dialog = gtk_dialog_new_with_buttons(_("Gnomeradio Settings"), GTK_WINDOW(app), 
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
			GTK_STOCK_HELP, GTK_RESPONSE_HELP,
			NULL);
	
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	box = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), box, TRUE, TRUE, 0);
	
	settings_box = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(box), settings_box, TRUE, TRUE, 0);

	settings_hdr = g_strconcat("<span weight=\"bold\">", _("General Settings"), "</span>", NULL);
	settings_label = gtk_label_new(settings_hdr);
	gtk_misc_set_alignment(GTK_MISC(settings_label), 0, 0.5);
	gtk_label_set_use_markup(GTK_LABEL(settings_label), TRUE);
	g_free(settings_hdr);
	gtk_box_pack_start(GTK_BOX(settings_box), settings_label, TRUE, TRUE, 0);

	presets_box = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(box), presets_box, TRUE, TRUE, 0);

	presets_hdr = g_strconcat("<span weight=\"bold\">", _("Presets"), "</span>", NULL);
	presets_label = gtk_label_new(presets_hdr);
	gtk_misc_set_alignment(GTK_MISC(presets_label), 0, 0.5);
	gtk_label_set_use_markup(GTK_LABEL(presets_label), TRUE);
	g_free(presets_hdr);
	gtk_box_pack_start(GTK_BOX(presets_box), presets_label, TRUE, TRUE, 0);

	record_box = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(box), record_box, TRUE, TRUE, 0);

	record_hdr = g_strconcat("<span weight=\"bold\">", _("Record Settings"), "</span>", NULL);
	record_label = gtk_label_new(record_hdr);
	gtk_misc_set_alignment(GTK_MISC(record_label), 0, 0.5);
	gtk_label_set_use_markup(GTK_LABEL(record_label), TRUE);
	g_free(record_hdr);
	gtk_box_pack_start(GTK_BOX(record_box), record_label, TRUE, TRUE, 0);

	/* The general settings part */
	sbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(settings_box), sbox, TRUE, TRUE, 0);
	s_indent_label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(sbox), s_indent_label, FALSE, FALSE, 0);
	
	settings_table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(settings_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(settings_table), 15);
	gtk_box_pack_start(GTK_BOX(sbox), settings_table, TRUE, TRUE, 0);
	
	device_label = gtk_label_new(_("Radio Device:"));
	gtk_misc_set_alignment(GTK_MISC(device_label), 0.0f, 0.5f); 
	device_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(device_entry), settings.device);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), device_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), device_entry, 1, 2, 0, 1);

	mixer_label = gtk_label_new(_("Mixer Source:"));
	gtk_misc_set_alignment(GTK_MISC(mixer_label), 0.0f, 0.5f);
	mixer_eb = gtk_event_box_new();
	mixer_combo = gtk_combo_box_new_text();
	gtk_container_add(GTK_CONTAINER(mixer_eb), mixer_combo);
	ptr = mixer_devs = get_mixer_recdev_list();
	for (i = 0, active = 0; ptr; ptr = g_list_next(ptr)) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(mixer_combo), ptr->data);
		if (g_str_equal(ptr->data, settings.mixer)) active = i;
		++i;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(mixer_combo), active);
	g_object_set_data_full(G_OBJECT(mixer_combo), "mixer_devs", mixer_devs, (GDestroyNotify)free_string_list);
	
	gtk_table_attach_defaults(GTK_TABLE(settings_table), mixer_label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), mixer_eb, 1, 2, 1, 2);

	mute_on_exit_cb = gtk_check_button_new_with_label(_("Mute on exit?"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_on_exit_cb), settings.mute_on_exit);

	gtk_table_attach_defaults(GTK_TABLE(settings_table), mute_on_exit_cb, 0, 2, 2, 3);

	g_signal_connect(GTK_OBJECT(device_entry), "hide", GTK_SIGNAL_FUNC(device_entry_activate_cb), app);
	g_signal_connect(GTK_OBJECT(device_entry), "activate", GTK_SIGNAL_FUNC(device_entry_activate_cb), NULL);
	g_signal_connect(GTK_OBJECT(mixer_combo), "changed", GTK_SIGNAL_FUNC(mixer_combo_change_cb), app);
	g_signal_connect(GTK_OBJECT(mute_on_exit_cb), "toggled", GTK_SIGNAL_FUNC(mute_on_exit_toggled_cb), NULL);

	gtk_tooltips_set_tip(tooltips, device_entry, _("Specify the radio-device (in most cases /dev/radio)"), NULL);
	gtk_tooltips_set_tip(tooltips, mixer_eb, 
	_("Choose the mixer source (line, line1, etc.) that is able to control the volume of your radio"), NULL);
	gtk_tooltips_set_tip(tooltips, mute_on_exit_cb, _("If unchecked, gnomeradio won't mute after exiting"), NULL);

	
	/* The presets part */
	pbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(presets_box), pbox, TRUE, TRUE, 0);
	p_indent_label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(pbox), p_indent_label, FALSE, FALSE, 0);

	preset_box = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(pbox), preset_box, TRUE, TRUE, 0);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	gtk_container_add(GTK_CONTAINER(scrolled_window), list_view);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(list_view, 200, 100);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list_view), FALSE);
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	/*gtk_tree_selection_select_path(selection, gtk_tree_path_new_from_string("0"));
	gtk_tree_selection_set_select_function(selection, (GtkTreeSelectionFunc)list_view_select_cb, NULL, NULL);*/
	
	cellrenderer = gtk_cell_renderer_text_new();
	cellrenderer->mode = GTK_CELL_RENDERER_MODE_EDITABLE;
	GTK_CELL_RENDERER_TEXT(cellrenderer)->editable = TRUE;
	list_column = gtk_tree_view_column_new_with_attributes(NULL, cellrenderer, "text", 0, NULL);
	gtk_tree_view_column_set_min_width(list_column, 130);
	gtk_tree_view_column_set_max_width(list_column, 130);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), list_column);
	g_signal_connect(GTK_OBJECT(cellrenderer), "edited", GTK_SIGNAL_FUNC(name_cell_edited_cb), NULL);

	cellrenderer = gtk_cell_renderer_text_new();
	cellrenderer->mode = GTK_CELL_RENDERER_MODE_EDITABLE;
	cellrenderer->xalign = 1.0f;
	GTK_CELL_RENDERER_TEXT(cellrenderer)->editable = TRUE;
	list_column = gtk_tree_view_column_new_with_attributes(NULL, cellrenderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), list_column);
	g_signal_connect(GTK_OBJECT(cellrenderer), "edited", GTK_SIGNAL_FUNC(freq_cell_edited_cb), NULL);

	button_box = gtk_vbox_new(FALSE, 15);

	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	del_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	
	gtk_box_pack_start(GTK_BOX(button_box), add_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), del_button, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(preset_box), scrolled_window, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(preset_box), button_box, TRUE, TRUE, 0);

	for (i=0;i<g_list_length(settings.presets);i++) {
		GtkTreeIter iter = {0};
		char *buffer;
		ps = g_list_nth_data(settings.presets, i);
		buffer = g_strdup_printf("%0.2f", ps->freq);
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter, 0, ps->title, 1, buffer, -1);
		g_free(buffer);
	}

	g_signal_connect(GTK_OBJECT(add_button), "clicked", GTK_SIGNAL_FUNC(add_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(del_button), "clicked", GTK_SIGNAL_FUNC(del_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(list_view), "key-press-event", GTK_SIGNAL_FUNC(list_view_key_press_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(list_view), "cursor-changed", GTK_SIGNAL_FUNC(list_view_cursor_changed_cb), NULL);

	gtk_tooltips_set_tip(tooltips, add_button, _("Add a new preset"), NULL);
	gtk_tooltips_set_tip(tooltips, del_button, _("Remove preset from List"), NULL);


	/* The record settings part */
	rbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(record_box), rbox, TRUE, TRUE, 0);
	r_indent_label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(rbox), r_indent_label, FALSE, FALSE, 0);

	record_table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(record_table), 15);
	gtk_table_set_row_spacings(GTK_TABLE(record_table), 5);
	
	destination_label = gtk_label_new(_("Destination directory:"));
	gtk_misc_set_alignment(GTK_MISC(destination_label), 0.0f, 0.5f);

	destination_button = gtk_button_new();
	gtk_button_set_label(GTK_BUTTON(destination_button), rec_settings.destination);
	
	profile_eb = gtk_event_box_new();
	profile_combo = gm_audio_profile_choose_new();
	gtk_container_add(GTK_CONTAINER(profile_eb), profile_combo);
	gm_audio_profile_choose_set_active(profile_combo, rec_settings.profile);

	gtk_table_attach_defaults(GTK_TABLE(record_table), destination_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(record_table), destination_button, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(record_table), profile_eb, 0, 2, 1, 2);

	g_signal_connect(GTK_OBJECT(destination_button), "clicked", GTK_SIGNAL_FUNC(destination_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(profile_combo), "changed", GTK_SIGNAL_FUNC(profile_combo_change_cb), NULL);

	gtk_tooltips_set_tip(tooltips, profile_eb, _("Choose the Media Profile that should be used to record."), NULL);
	
	gtk_box_pack_start(GTK_BOX(rbox), record_table, TRUE, TRUE, 0);

	gtk_widget_show_all(dialog);

	return dialog;
}
