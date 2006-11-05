/* record.c
 *
 * Copyright (C) 2001 Jörgen Scheibengruber
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

/*** the recording functionality */

#include <config.h>

#include <sys/types.h>
#include <signal.h>
#include <gnome.h>
#include "gui.h"
#include "tech.h"
#include "rec_tech.h"
#include "prefs.h"

static int timeout_id = -1, wav_io_id = -1, mp3_io_id = -1;
static GtkWidget *file_lbl, *size_lbl;
static GtkWidget *status_dialog;

void close_status_window(void)
{
	if (timeout_id >= 0)
	{
		gtk_timeout_remove(timeout_id);
		timeout_id = -1;
	}

	if (status_dialog)
		gtk_widget_destroy(GTK_WIDGET(status_dialog));
	status_dialog = NULL;
	
	tray_icon_items_set_sensible(TRUE);
}

static gboolean timeout_cb(gpointer data)
{
	Recording *recording = data;
	gint s;
	gchar *size=NULL;

	g_assert(recording);	
	
	if (!GTK_WIDGET_VISIBLE(status_dialog))
		gtk_widget_show_all(status_dialog);
	
	s = get_file_size(recording->filename);
	if (s > 0) {
		if (s < 1024) size = g_strdup_printf(_("%i byte"), s);
		
		if ((s >= 1024) && (s < 1024*1024)) size = g_strdup_printf(_("%i kB"), s>>10);
		if (s >= 1024*1024) size = g_strdup_printf(_("%.2f MB"), (float)s/1024/1024);
	} else {
		if (s)	size = g_strdup(_("Error"));
		else	size = g_strdup(_("0 byte"));
	}	
	
	gtk_label_set_text(GTK_LABEL(file_lbl), recording->filename);
	gtk_label_set_text(GTK_LABEL(size_lbl), size);
	g_free(size);
	
	return TRUE;
}	
	
void run_status_window(Recording *recording)
{
	timeout_id = gtk_timeout_add(500, (GtkFunction) timeout_cb, recording);
}

static void button_clicked_cb(GtkButton *button, gpointer data)
{
	Recording *recording = data;
	close_status_window();
	recording_stop(recording);
}		

static gint delete_event_cb(GtkWidget* window, GdkEventAny* e, gpointer data)
{
	button_clicked_cb(NULL, data);
	return TRUE;
}

GtkWidget* record_status_window(Recording *recording)
{
	GtkWidget *btn_label, *btn_pixmap, *button;
	GtkWidget *vbox, *btn_box, *hbox, *lbl_hbox, *lbl_vbox1, *lbl_vbox2;
	GtkWidget *f_lbl, *s_lbl;

	status_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(status_dialog),_("Gnomeradio recording status"));
	/*gtk_window_set_resizable(GTK_WINDOW(status_dialog), FALSE);*/
	gtk_window_set_default_size(GTK_WINDOW(status_dialog), 300, -1);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	lbl_hbox = gtk_hbox_new(FALSE, 10);	
	lbl_vbox1 = gtk_vbox_new(FALSE, 5);
	lbl_vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(lbl_hbox), lbl_vbox1, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(lbl_hbox), lbl_vbox2, TRUE, TRUE, 0);
	
	f_lbl = gtk_label_new(_("Recording:"));
	s_lbl = gtk_label_new(_("Filesize:"));
	file_lbl = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(file_lbl), PANGO_ELLIPSIZE_START);
	size_lbl = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(f_lbl), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(s_lbl), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(file_lbl), 1.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(size_lbl), 1.0f, 0.5f);

	gtk_box_pack_start(GTK_BOX(lbl_vbox1), f_lbl, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(lbl_vbox1), s_lbl, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(lbl_vbox2), file_lbl, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(lbl_vbox2), size_lbl, FALSE, FALSE, 0);

	button = gtk_button_new();
	btn_box = gtk_hbox_new(FALSE, 0);
	btn_label = gtk_label_new(_("Stop Recording"));
	btn_pixmap = gtk_image_new_from_stock(GTK_STOCK_STOP, GTK_ICON_SIZE_BUTTON);
	
	gtk_box_pack_start (GTK_BOX(btn_box), btn_pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX(btn_box), btn_label, FALSE, FALSE, 2);

	gtk_container_add(GTK_CONTAINER(button), btn_box);
	
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end (GTK_BOX(hbox), button, TRUE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX(vbox), lbl_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(status_dialog), vbox);

	g_signal_connect(GTK_OBJECT(status_dialog), "delete_event", GTK_SIGNAL_FUNC(delete_event_cb), recording);
	g_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_clicked_cb), recording);

	gtk_window_set_modal(GTK_WINDOW(status_dialog), TRUE);

	return status_dialog;
}
