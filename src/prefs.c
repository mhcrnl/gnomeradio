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
#include <string.h>
#include "config.h"
#include "prefs.h"
#include "gui.h"
#include "rec_tech.h"

#define UN_NAMED "unnamed"

static GtkWidget *name_entry, *device_entry, *freq_spin;
static GtkWidget *mixer_combo;
static GtkWidget *mute_on_exit_cb;
//static GtkWidget *clist;
static GtkWidget *list_view;
static GtkListStore *list_store;
static GtkTreeSelection *selection;
static GtkAdjustment *spin;
static gint selected_row = -1;

static gnomeradio_settings tmp_settings;

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
	//gconf_client_set_float(client, "/apps/gnomeradio/volume", volume->value, NULL);
	gconf_client_set_float(client, "/apps/gnomeradio/last-freq", adj->value/STEPS, NULL);

	/* Store recording settings */
	gconf_client_set_string(client, "/apps/gnomeradio/recording/audiodevice", rec_settings.audiodevice, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/last-filename", rec_settings.filename, NULL);
	gconf_client_set_bool(client, "/apps/gnomeradio/recording/record-as-mp3", rec_settings.mp3, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/sample-rate", rec_settings.rate, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/sample-format", rec_settings.sample, NULL);
	gconf_client_set_bool(client, "/apps/gnomeradio/recording/record-in-stereo", rec_settings.stereo, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/encoder", rec_settings.encoder, NULL);
	gconf_client_set_string(client, "/apps/gnomeradio/recording/bitrate", rec_settings.bitrate, NULL);

	/* Store the presets */
	count = g_list_length(settings.presets);
	gconf_client_set_int(client, "/apps/gnomeradio/presets/presets", count, NULL);
	for (i=0;i<count;i++)
	{
		ps = g_list_nth_data(settings.presets, i);
		buffer = g_strdup_printf("/apps/gnomeradio/presets/%d/name", i);
		gconf_client_set_string(client, buffer, ps->name, NULL); 
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
	//volume->value = gconf_client_get_float(client, "/apps/gnomeradio/volume", NULL);
	freq = gconf_client_get_float(client, "/apps/gnomeradio/last-freq", NULL);
	if ((freq < FREQ_MIN) || (freq > FREQ_MAX))
		adj->value = FREQ_MIN * STEPS;
	else
		adj->value = freq * STEPS;
	
	/* Load recording settings */
	rec_settings.audiodevice = gconf_client_get_string(client, "/apps/gnomeradio/recording/audiodevice", NULL);
	if (!rec_settings.audiodevice)
		rec_settings.audiodevice = g_strdup("/dev/audio");
	rec_settings.filename = gconf_client_get_string(client, "/apps/gnomeradio/recording/last-filename", NULL);
	if (!rec_settings.filename)
		rec_settings.filename = g_strdup("/");
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
		rec_settings.encoder = g_strdup("lame");
	rec_settings.bitrate = gconf_client_get_string(client, "/apps/gnomeradio/recording/bitrate", NULL);
	if (!rec_settings.bitrate)
		rec_settings.bitrate = g_strdup("128");
	
	/* Load the presets */
	count = gconf_client_get_int(client, "/apps/gnomeradio/presets/presets", NULL);
	for (i=0;i<count;i++)
	{
		char *tmp;
		ps = malloc(sizeof(preset));
		buffer = g_strdup_printf("/apps/gnomeradio/presets/%d/name", i);
		tmp = gconf_client_get_string(client, buffer, NULL); 
		if (!tmp)
			tmp = "unnamed";
		strncpy(ps->name, tmp, 20);
		ps->name[20] = '\0';
		g_free(buffer);
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
	
gint backup_settings(void)
{
	preset *ps, *tmp;
	gint i;
	
	//g_print("Making a backup of settings\n");
		
	tmp_settings.device = g_strdup(settings.device);
	tmp_settings.mixer = g_strdup(settings.mixer);
	tmp_settings.mute_on_exit = settings.mute_on_exit;
	tmp_settings.presets = NULL;
	for (i=0;i<g_list_length(settings.presets);i++)
	{
		ps = malloc(sizeof(preset));
		tmp = g_list_nth_data(settings.presets, i);
		strcpy(ps->name, tmp->name);
		ps->freq = tmp->freq;
		tmp_settings.presets = g_list_append(tmp_settings.presets, (gpointer) ps);
	}
	return 1;
}

gint commit_settings(gpointer app)
{
	preset  *tmp;
	gint i;
	gchar* ptr;
	
	//g_print("Committing Settings\n");
	
	ptr = settings.device;
	settings.device = tmp_settings.device;
	if (strcmp(ptr, tmp_settings.device))
		start_radio(TRUE, app);
	g_free(ptr);
	tmp_settings.device = NULL;
	ptr = settings.mixer;
	settings.mixer = tmp_settings.mixer;
	if (strcmp(ptr, tmp_settings.mixer))
		start_mixer(TRUE, app);
	g_free(ptr);
	tmp_settings.mixer = NULL;
	settings.mute_on_exit = tmp_settings.mute_on_exit;
	for (i=0;i<g_list_length(settings.presets);i++)
	{
		tmp = g_list_nth_data(settings.presets, i);
		g_free(tmp);
	}
	g_list_free(settings.presets);
	settings.presets = tmp_settings.presets;
	tmp_settings.presets = NULL;
	
	selected_row = -1;	
	return 1;
}

gint rollback_settings(void)
{
	preset *tmp;
	gint i;
	
	//g_print("Undo Changes\n");
	
	g_free(tmp_settings.device);
	g_free(tmp_settings.mixer);
	for (i=0;i<g_list_length(tmp_settings.presets);i++)
	{
		tmp = g_list_nth_data(tmp_settings.presets, i);
		g_free(tmp);
	}
	g_list_free(tmp_settings.presets);
	tmp_settings.presets = NULL;
	
	selected_row = -1;	
	return 1;
}
	
static void mute_on_exit_toggled_cb(GtkWidget* widget, gpointer data)
{
	tmp_settings.mute_on_exit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mute_on_exit_cb));
}	

static gboolean device_entry_focus_out_event_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	if (tmp_settings.device)
		g_free(tmp_settings.device);
	tmp_settings.device = g_strdup(gtk_entry_get_text(GTK_ENTRY(device_entry)));
	return FALSE;
}

static gboolean mixer_entry_focus_out_event_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	char *tmp;
	if (tmp_settings.mixer)
		g_free(tmp_settings.mixer);
	tmp_settings.mixer = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(mixer_combo)->entry)));
	
	if ((tmp = strstr(tmp_settings.mixer, " (")))
		tmp[0] = '\0';
	
	return FALSE;
}

static void add_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	preset *ps;
	GtkTreeIter iter = {0};
	char *text, *buffer;
	GtkAdjustment *v_scb;
	
	text = (char*)gtk_entry_get_text(GTK_ENTRY(name_entry));
	if (strlen(text)<1)
	{
		g_print("\a");
		return;
	}
	
	ps = malloc(sizeof(preset));
	strcpy(ps->name, text);
	ps->freq = spin->value;
	tmp_settings.presets = g_list_append(tmp_settings.presets, (gpointer) ps);
	buffer = g_strdup_printf("%.2f", spin->value);

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, text, 1, buffer, -1);

	gtk_entry_set_text(GTK_ENTRY(name_entry), "");
	
	g_free(buffer);
	gtk_tree_selection_unselect_all(selection);
	
	v_scb = gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(list_view));
	gtk_adjustment_set_value(v_scb, v_scb->upper);
}


static void del_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_column = NULL;
	GtkTreeIter iter;
	preset *ps;
	int *row;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(list_view), &path, &focus_column);
	
	if (!path)
	{
		g_print("\aNo row selected\n");
		return;
	}
	
	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(tmp_settings.presets));

	//g_print("path: %s row %d\n", gtk_tree_path_to_string(path), *row);
	ps = g_list_nth_data(tmp_settings.presets, *row);
	g_assert(ps);	
	tmp_settings.presets = g_list_remove(tmp_settings.presets, (gpointer)ps);
	g_free(ps);
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_remove(list_store, &iter);
	
	if (!gtk_tree_path_prev(path))
		return;
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(list_view), path, NULL, FALSE);
		
}

static void update_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_column = NULL;
	GtkTreeIter iter;
	preset *ps;
	int *row;
	char *text, *buffer;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(list_view), &path, &focus_column);
	
	if (!path)
	{
		g_print("\aNo row selected\n");
		return;
	}
	
	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(tmp_settings.presets));

	text = (char*)gtk_entry_get_text(GTK_ENTRY(name_entry));
	if (strlen(text)<1)
	{
		g_print("\a");
		return;
	}
	ps = g_list_nth_data(tmp_settings.presets, *row);
	g_assert(ps);	

	strcpy(ps->name, text);
	ps->freq = spin->value;
	buffer = g_strdup_printf("%.2f", spin->value);
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_set(list_store, &iter, 0, text, 1, buffer, -1);

	g_free(buffer);
}

static gboolean list_view_key_press_event_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_Delete)
		del_button_clicked_cb(widget, user_data);
	
	return FALSE;
}		

#if 0
static void freq_spin_activate_cb(void)
{
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_column = NULL;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(list_view), &path, &focus_column);
	
	if (!path)
		add_button_clicked_cb(NULL, NULL);
	else
		update_button_clicked_cb(NULL, NULL);
}	

static gboolean enter_key_press_event_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	//g_print("keyval = %d\n", event->keyval);
	if (event->keyval == GDK_Return)
		freq_spin_activate_cb();

	return FALSE;
}		
#endif

/*static gboolean
list_view_select_cb(GtkTreeSelection *selection, GtkTreeModel *model, 
				GtkTreePath *path, gboolean path_currently_selected, gpointer data)
{
	gchar* text;
	preset *ps;
	int *row;
	
	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(tmp_settings.presets));
	g_print("row: %d %d\n", *row, path_currently_selected);
	if (!path_currently_selected)
	{
		return TRUE;
	}
	ps = (preset*)g_list_nth_data(tmp_settings.presets, *row);
	text = ps->name;
	gtk_entry_set_text(GTK_ENTRY(name_entry), text);
	gtk_adjustment_set_value(spin, ps->freq);
	return TRUE;
}*/

static void
list_view_cursor_changed_cb(GtkWidget *widget, gpointer data)
{
	gchar* text;
	preset *ps;
	int *row;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_column = NULL;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(list_view), &path, &focus_column);
	
	if (!path)
	{
		return;
	}

	row = gtk_tree_path_get_indices(path);
	g_assert(row);
	g_assert(*row < g_list_length(tmp_settings.presets));

	ps = (preset*)g_list_nth_data(tmp_settings.presets, *row);
	text = ps->name;
	gtk_entry_set_text(GTK_ENTRY(name_entry), text);
	gtk_adjustment_set_value(spin, ps->freq);
	return;
}

/*
static void clist_select_row_cb(GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer user_data)
{
	gchar* text;
	preset *ps;

	g_assert(row < g_list_length(tmp_settings.presets));

	selected_row = row;
	ps = (preset*)g_list_nth_data(tmp_settings.presets, row);
	text = ps->name;
	gtk_entry_set_text(GTK_ENTRY(name_entry), text);
	//freq_entry_set_value(ps->freq);
	//freq_entry_activate_cb(NULL, NULL);	
	gtk_adjustment_set_value(spin, ps->freq);
}	

static void clist_unselect_row_cb(GtkWidget *widget, gint row, gint column, GdkEventButton *event, gpointer user_data)
{
	selected_row = -1;
}*/	

static void spin_value_changed_cb(GtkWidget *widget, gpointer data)
{
	double value;

	value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(freq_spin));
	gtk_adjustment_set_value(adj, value*STEPS);
	mom_ps = -1;
	preset_menu_set_item(mom_ps);
}

GtkWidget* prefs_window(void)
{
	GtkWidget *dialog;
	GtkWidget *frame1, *frame2;
	GtkWidget *misc_box, *preset_box, *hbox;
	GtkWidget *device_label, *device_box;
	GtkWidget *mixer_label, *mixer_box;
	GtkWidget *name_label, *freq_label;
	GtkWidget *button_box, *entry_box, *freq_box;
	GtkWidget *add_button, *del_button, *update_button;
	GtkWidget *add_pixmap, *del_pixmap, *update_pixmap;
	GtkWidget *scrolled_window;
	GtkWidget *separator;
	GtkCellRenderer *cellrenderer;
	GtkTreeViewColumn *list_column;
	GList *mixer_devs;
	gint i;
	preset* ps;
	
	dialog = gtk_dialog_new_with_buttons(_("Gnomeradio Settings"), NULL, 
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
			GTK_STOCK_OK, GTK_RESPONSE_OK, 
			GTK_STOCK_HELP, GTK_RESPONSE_HELP,
			NULL);
	
	frame1 = gtk_frame_new(_("Misc Settings"));
	frame2 = gtk_frame_new(_("Presets"));
	gtk_container_set_border_width(GTK_CONTAINER(frame1), 5);
	gtk_container_set_border_width(GTK_CONTAINER(frame2), 5);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame2, TRUE, TRUE, 0);

	misc_box = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(misc_box), 8);
	gtk_container_add(GTK_CONTAINER(frame1), misc_box);
	
	device_box = gtk_hbox_new(FALSE, 10);
	device_label = gtk_label_new(_("Radio Device:"));
	gtk_misc_set_alignment(GTK_MISC(device_label), 0.0f, 0.5f);
	device_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(device_entry), tmp_settings.device);
	gtk_box_pack_start(GTK_BOX(device_box), device_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(device_box), device_entry, TRUE, TRUE, 0);

	mixer_box = gtk_hbox_new(FALSE, 10);
	mixer_label = gtk_label_new(_("Mixer Source:"));
	gtk_misc_set_alignment(GTK_MISC(mixer_label), 0.0f, 0.5f);
	mixer_combo = gtk_combo_new();
	mixer_devs = get_mixer_recdev_list();
	if (mixer_devs)
	{
		gtk_combo_set_popdown_strings(GTK_COMBO(mixer_combo), mixer_devs);
		g_list_foreach(mixer_devs, (GFunc)g_free, NULL);
		g_list_free(mixer_devs);
	}
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(mixer_combo)->entry), tmp_settings.mixer);
	gtk_box_pack_start(GTK_BOX(mixer_box), mixer_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(mixer_box), mixer_combo, TRUE, TRUE, 0);

	mute_on_exit_cb = gtk_check_button_new_with_label(_("Mute on exit?"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_on_exit_cb), tmp_settings.mute_on_exit);

	gtk_box_pack_start(GTK_BOX(misc_box), device_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(misc_box), mixer_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(misc_box), mute_on_exit_cb, FALSE, FALSE, 2);

	preset_box = gtk_vbox_new(FALSE, 4);
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
	gtk_container_add(GTK_CONTAINER(frame2), hbox);

	/*scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	clist = gtk_clist_new(2);
	gtk_container_add(GTK_CONTAINER(scrolled_window), clist);
	gtk_widget_set_usize(clist, 50, 80);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, 120);
	//gtk_clist_set_column_width(GTK_CLIST(clist), 1, 40);*/

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	gtk_container_add(GTK_CONTAINER(scrolled_window), list_view);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	//gtk_widget_set_usize(list_view, 130, 100);
	gtk_widget_set_size_request(list_view, 130, 100);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list_view), FALSE);
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	/*gtk_tree_selection_select_path(selection, gtk_tree_path_new_from_string("0"));
	gtk_tree_selection_set_select_function(selection, (GtkTreeSelectionFunc)list_view_select_cb, NULL, NULL);*/
	
	cellrenderer = gtk_cell_renderer_text_new();
	list_column = gtk_tree_view_column_new_with_attributes(NULL, cellrenderer, "text", 0, NULL);
	gtk_tree_view_column_set_min_width(list_column, 130);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), list_column);
	cellrenderer = gtk_cell_renderer_text_new();
	list_column = gtk_tree_view_column_new_with_attributes(NULL, cellrenderer, "text", 1, NULL);
	//gtk_tree_view_column_set_min_width(list_column, 30);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), list_column);

	entry_box = gtk_hbox_new(FALSE, 10);
	button_box = gtk_vbox_new(TRUE, 3);

	name_label = gtk_label_new(_("Name:"));	
	gtk_misc_set_alignment(GTK_MISC(name_label), 0.0f, 0.5f);
	name_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(name_entry), 20);
	//gtk_entry_set_text(GTK_ENTRY(name_entry), UN_NAMED);
	gtk_editable_set_editable(GTK_EDITABLE(name_entry), TRUE);
	
	gtk_box_pack_start(GTK_BOX(entry_box), name_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(entry_box), name_entry, TRUE, TRUE, 0);

	add_pixmap = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	del_pixmap = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON);
	update_pixmap = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON);
	
	add_button = gtk_button_new();
	del_button = gtk_button_new();
	update_button = gtk_button_new();

	/*add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	del_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	update_button = gtk_button_new_from_stock(GTK_STOCK_REFRESH);*/


	separator = gtk_hseparator_new();
	
	gtk_container_add(GTK_CONTAINER(add_button), add_pixmap);
	gtk_container_add(GTK_CONTAINER(del_button), del_pixmap);
	gtk_container_add(GTK_CONTAINER(update_button), update_pixmap);

	gtk_box_pack_start(GTK_BOX(button_box), add_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), del_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), update_button, FALSE, FALSE, 0);
	
	freq_box = gtk_hbox_new(FALSE, 4);
	freq_label = gtk_label_new(_("Frequency:"));
	gtk_misc_set_alignment(GTK_MISC(freq_label), 0.0f, 0.5f);
	/*freq_entry = gtk_entry_new_with_max_length(6);
	freq_entry_set_value(adj->value/STEPS);
	gtk_widget_set_usize(freq_entry, 50, 23);
	freq_spin = my_spin();*/
	
	spin = GTK_ADJUSTMENT(gtk_adjustment_new(adj->value/STEPS, FREQ_MIN, FREQ_MAX, 0.05, 1, 1));
	freq_spin = gtk_spin_button_new(spin, 0.05, 2);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(freq_spin), TRUE);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(freq_spin), TRUE);
	//gtk_widget_set_usize(fgtk_spin_button_get_snap_to_ticksreq_spin, 120, 23);
	
	gtk_box_pack_start(GTK_BOX(freq_box), freq_label, TRUE, TRUE, 0);
	//gtk_box_pack_start(GTK_BOX(freq_box), freq_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(freq_box), freq_spin, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(preset_box), scrolled_window, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(preset_box), separator, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(preset_box), entry_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(preset_box), freq_box, TRUE, TRUE, 2);
	//gtk_box_pack_start(GTK_BOX(preset_box), button_box, TRUE, TRUE, 2);

	gtk_box_pack_start(GTK_BOX(hbox), preset_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), button_box, TRUE, FALSE, 10);

	//g_signal_connect(GTK_OBJECT(freq_spin), "button-press-event", GTK_SIGNAL_FUNC(freq_spin_button_press_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(device_entry), "focus-out-event", GTK_SIGNAL_FUNC(device_entry_focus_out_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(GTK_COMBO(mixer_combo)->entry), "focus-out-event", GTK_SIGNAL_FUNC(mixer_entry_focus_out_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(mute_on_exit_cb), "toggled", GTK_SIGNAL_FUNC(mute_on_exit_toggled_cb), NULL);
	g_signal_connect(GTK_OBJECT(add_button), "clicked", GTK_SIGNAL_FUNC(add_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(del_button), "clicked", GTK_SIGNAL_FUNC(del_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(update_button), "clicked", GTK_SIGNAL_FUNC(update_button_clicked_cb), NULL);
	//g_signal_connect(GTK_OBJECT(clist), "select-row", GTK_SIGNAL_FUNC(clist_select_row_cb), NULL);
	//g_signal_connect(GTK_OBJECT(clist), "unselect-row", GTK_SIGNAL_FUNC(clist_unselect_row_cb), NULL);
	g_signal_connect(GTK_OBJECT(list_view), "key-press-event", GTK_SIGNAL_FUNC(list_view_key_press_event_cb), NULL);
	/* FIXME: Does not really work :-( */
	/*g_signal_connect_after(GTK_OBJECT(freq_spin), "activate", GTK_SIGNAL_FUNC(freq_spin_activate_cb), NULL);
	g_signal_connect(GTK_OBJECT(name_entry), "key-press-event", GTK_SIGNAL_FUNC(enter_key_press_event_cb), NULL);*/
	g_signal_connect(GTK_OBJECT(spin), "value-changed", GTK_SIGNAL_FUNC(spin_value_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(list_view), "cursor-changed", GTK_SIGNAL_FUNC(list_view_cursor_changed_cb), NULL);


	for (i=0;i<g_list_length(tmp_settings.presets);i++)
	{
		GtkTreeIter iter = {0};
		char *buffer;
		ps = g_list_nth_data(tmp_settings.presets, i);
		buffer = g_strdup_printf("%0.2f", ps->freq);
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter, 0, ps->name, 1, buffer, -1);
		g_free(buffer);
	}

	gtk_tooltips_set_tip(tooltips, device_entry, _("Specify the radio-device (in most cases /dev/radio)"), NULL);
	gtk_tooltips_set_tip(tooltips, GTK_COMBO(mixer_combo)->entry, 
	_("Choose the mixer source (line, line1, etc.) that is able to control the volume of your radio"), NULL);
	gtk_tooltips_set_tip(tooltips, mute_on_exit_cb, _("If unchecked, gnomeradio won't mute after exiting"), NULL);
	gtk_tooltips_set_tip(tooltips, add_button, _("Add a new preset"), NULL);
	gtk_tooltips_set_tip(tooltips, del_button, _("Remove preset from List"), NULL);
	gtk_tooltips_set_tip(tooltips, update_button, _("Update the preset in the List"), NULL);

	gtk_widget_show_all(dialog);

	return dialog;
}	
	
