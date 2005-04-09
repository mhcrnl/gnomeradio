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
#include <gnome.h>
#include "gui.h"
#include "tech.h"
#include "rec_tech.h"
#include "prefs.h"

static int timeout_id = -1, wav_io_id = -1, mp3_io_id = -1;
static GtkWidget *file_lbl, *size_lbl;
#if 0
static GtkWidget *audiodev_entry, *path_entry, *mp3_rb, *wav_rb, *encoder_combo;
static GtkWidget *rate_combo, *sample_combo, *stereo_rb, *mono_rb, *bitrate_combo; 
#endif
static GtkWidget *status_dialog;
static gchar *filename;

#if 0
static gboolean audiodev_entry_changed_cb(GtkWidget *widget, gpointer data)
{
	if (rec_settings.audiodevice)
		g_free(rec_settings.audiodevice);
	rec_settings.audiodevice = g_strdup(gtk_entry_get_text(GTK_ENTRY(audiodev_entry)));
	return TRUE;
}

static gboolean path_entry_changed_cb(GtkWidget *widget, gpointer data)
{
	if (rec_settings.filename)
		g_free(rec_settings.filename);
	rec_settings.filename = gnome_file_entry_get_full_path(GNOME_FILE_ENTRY(path_entry), FALSE);
	if (!rec_settings.filename)
		rec_settings.filename = g_strdup("");
	return TRUE;
}

static void mp3_rb_toggled_cb(GtkWidget* widget, gpointer data)
{
	gboolean state;
	//g_print("mp3 %s\n", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mp3_rb)) ? "TRUE":"FALSE");
	state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mp3_rb));
	
	rec_settings.mp3 = state;
	gtk_widget_set_sensitive(encoder_combo, state);
	gtk_widget_set_sensitive(bitrate_combo, state);

}	

static gboolean rate_combo_changed_cb(GtkWidget *widget, gpointer data)
{
	if (rec_settings.rate)
		g_free(rec_settings.rate);
	rec_settings.rate = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(rate_combo)->entry)));
	g_strdelimit(rec_settings.rate, " Hz", 0);
	return TRUE;
}

static gboolean sample_combo_changed_cb(GtkWidget *widget, gpointer data)
{
	if (rec_settings.sample)
		g_free(rec_settings.sample);
	rec_settings.sample = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(sample_combo)->entry)));
	g_strdelimit(rec_settings.sample, " bit", 0);
	return TRUE;
}

static void stereo_rb_toggled_cb(GtkWidget* widget, gpointer data)
{
	//g_print("stereo %s\n", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stereo_rb)) ? "TRUE":"FALSE");
	rec_settings.stereo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stereo_rb));
}	

static gboolean encoder_combo_changed_cb(GtkWidget *widget, gpointer data)
{
	if (rec_settings.encoder)
		g_free(rec_settings.encoder);
	rec_settings.encoder = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(encoder_combo)->entry)));
	return TRUE;
}

static gboolean bitrate_combo_changed_cb(GtkWidget *widget, gpointer data)
{
	if (rec_settings.bitrate)
		g_free(rec_settings.bitrate);
	rec_settings.bitrate = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(bitrate_combo)->entry)));
	g_strdelimit(rec_settings.bitrate, " kb/s", 0);
	return TRUE;
}

/*
static void filesel_ok_clicked_cb(GtkWidget *widget, gpointer filesel)
{
	gtk_entry_set_text(GTK_ENTRY(path_entry), gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel)));
}
	
static void choose_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *filesel;
	
	filesel = gtk_file_selection_new(_("Choose a filename"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), gtk_entry_get_text(GTK_ENTRY(path_entry)));
	
	//gtk_file_selection_complete(GTK_FILE_SELECTION(filesel), "*.mp3");
	
	g_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION(filesel)->ok_button), "clicked", 
						GTK_SIGNAL_FUNC(filesel_ok_clicked_cb), (gpointer)filesel);
	g_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button), "clicked",
						GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer) filesel, G_CONNECT_SWAPPED);
	g_signal_connect_object (GTK_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button), "clicked",
						GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer) filesel, G_CONNECT_SWAPPED);

	gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
	gtk_widget_show_all(filesel);
}	
*/

GtkWidget* record_prefs_window(void)
{
	GtkWidget *dialog;
	GtkWidget *gen_frame, *wav_frame, *mp3_frame;
	GtkWidget *gen_table, *wav_table, *mp3_table;
	GtkWidget *audiodev_label, *path_label, *type_label, *stereo_label;
	GtkWidget *rate_label, *sample_label, *encoder_label, *bitrate_label; 
	GtkWidget *record_button, *record_pixmap, *record_label, *record_box1, *record_box2;
	GList *rates = NULL, *samples = NULL, *bitrates = NULL, *encoders = NULL;
	char *buffer, **enc_list;
	int i;
	
	/* Create the record button */
	record_button = gtk_button_new();
	record_box1 = gtk_hbox_new(FALSE, 0);
	record_box2 = gtk_hbox_new(FALSE, 0);
	record_label = gtk_label_new(_("Start"));
	record_pixmap = gtk_image_new_from_stock(GNOME_STOCK_MIC, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(record_box2), record_pixmap, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(record_box2), record_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(record_box1), record_box2, TRUE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(record_button), record_box1);
	GTK_WIDGET_SET_FLAGS(record_button, GTK_CAN_DEFAULT);

	dialog = gtk_dialog_new_with_buttons(_("Gnomeradio Record"), NULL, 
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_HELP, GTK_RESPONSE_HELP,
			NULL);
	
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), record_button, GTK_RESPONSE_OK); 

	gen_frame = gtk_frame_new(_("General"));
	wav_frame = gtk_frame_new(_("Wave Settings"));
	mp3_frame = gtk_frame_new(_("Mp3/Ogg Settings"));
	gtk_container_set_border_width(GTK_CONTAINER(gen_frame), 5);
	gtk_container_set_border_width(GTK_CONTAINER(wav_frame), 5);
	gtk_container_set_border_width(GTK_CONTAINER(mp3_frame), 5);

	gen_table = gtk_table_new(3, 4, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(gen_table), 5);
	gtk_table_set_col_spacing(GTK_TABLE(gen_table), 0, 8);
	gtk_table_set_col_spacing(GTK_TABLE(gen_table), 1, 8);
	gtk_table_set_row_spacing(GTK_TABLE(gen_table), 0, 5);
	gtk_table_set_row_spacing(GTK_TABLE(gen_table), 1, 5);

	audiodev_label = gtk_label_new(_("Audio device:"));
	audiodev_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(audiodev_entry),rec_settings.audiodevice);
	path_label = gtk_label_new(_("Filename:"));
	//path_entry = gtk_entry_new();
	path_entry = gnome_file_entry_new("gnomeradio_filename", _("Choose a filename"));
	gtk_entry_set_text(GTK_ENTRY(
		gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(path_entry))), rec_settings.filename);
	//gnome_entry_load_history("gnomeradio_filename");
	gnome_file_entry_set_modal(GNOME_FILE_ENTRY(path_entry), TRUE);
	//choose_button = gtk_button_new_with_label(_("Choose..."));
	type_label = gtk_label_new(_("Record as:"));
	wav_rb = gtk_radio_button_new_with_label(NULL, _("Wave (.wav)"));
	mp3_rb = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(wav_rb),_("MP3/Ogg (.mp3/.ogg)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mp3_rb), rec_settings.mp3);		

	gtk_misc_set_alignment(GTK_MISC(audiodev_label), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(path_label), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(type_label), 0.0f, 0.5f);
	
	gtk_table_attach_defaults(GTK_TABLE(gen_table), audiodev_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(gen_table), audiodev_entry, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(gen_table), path_label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(gen_table), path_entry, 1, 3, 1, 2);
	//gtk_table_attach_defaults(GTK_TABLE(gen_table), choose_button, 2, 3, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(gen_table), type_label, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(gen_table), wav_rb, 1, 3, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(gen_table), mp3_rb, 1, 3, 3, 4);

	gtk_container_add(GTK_CONTAINER(gen_frame), gen_table);
	
	wav_table = gtk_table_new(2, 3, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(wav_table), 5);
	gtk_table_set_col_spacing(GTK_TABLE(wav_table), 0, 20);
	gtk_table_set_row_spacing(GTK_TABLE(wav_table), 0, 5);
	gtk_table_set_row_spacing(GTK_TABLE(wav_table), 1, 5);
	
	rate_label = gtk_label_new(_("Sample rate:"));
	sample_label = gtk_label_new(_("Sample format:"));
	stereo_label = gtk_label_new(_("Record in:"));
	gtk_misc_set_alignment(GTK_MISC(rate_label), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(sample_label), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(stereo_label), 0.0f, 0.5f); 
	
	rates = g_list_append(rates, "8000 Hz");
	rates = g_list_append(rates, "11025 Hz");
	rates = g_list_append(rates, "16000 Hz");
	rates = g_list_append(rates, "22050 Hz");
	rates = g_list_append(rates, "32000 Hz");
	rates = g_list_append(rates, "44100 Hz");
	rates = g_list_append(rates, "48000 Hz");

	samples = g_list_append(samples, "8 bit");
	samples = g_list_append(samples, "16 bit");

	rate_combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(rate_combo), rates);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(rate_combo)->entry), FALSE);
	buffer = g_strdup_printf("%s Hz", rec_settings.rate);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(rate_combo)->entry), buffer);
	g_free(buffer);

	sample_combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(sample_combo), samples);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(sample_combo)->entry), FALSE);
	buffer = g_strdup_printf("%s bit", rec_settings.sample);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(sample_combo)->entry), buffer);
	g_free(buffer);

	gtk_widget_set_size_request(rate_combo, 80, -1);
	gtk_widget_set_size_request(sample_combo, 80, -1);

	mono_rb = gtk_radio_button_new_with_label(NULL, _("Mono"));
	stereo_rb = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(mono_rb),_("Stereo"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stereo_rb), rec_settings.stereo);		

	gtk_table_attach_defaults(GTK_TABLE(wav_table), rate_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(wav_table), rate_combo, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(wav_table), sample_label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(wav_table), sample_combo, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(wav_table), stereo_label, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(wav_table), stereo_rb, 1, 2, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(wav_table), mono_rb, 1, 2, 3, 4);

	gtk_container_add(GTK_CONTAINER(wav_frame), wav_table);

	mp3_table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(mp3_table), 5);
	gtk_table_set_col_spacing(GTK_TABLE(mp3_table), 0, 20);
	gtk_table_set_row_spacing(GTK_TABLE(mp3_table), 0, 5);
	
	encoder_label = gtk_label_new(_("Encoder:"));
	bitrate_label = gtk_label_new(_("Bitrate:"));
	gtk_misc_set_alignment(GTK_MISC(encoder_label), 0.0f, 0.5f); 
	gtk_misc_set_alignment(GTK_MISC(bitrate_label), 0.0f, 0.5f); 

	bitrates = g_list_append(bitrates, "32 kb/s");
	bitrates = g_list_append(bitrates, "40 kb/s");
	bitrates = g_list_append(bitrates, "48 kb/s");
	bitrates = g_list_append(bitrates, "56 kb/s");
	bitrates = g_list_append(bitrates, "64 kb/s");
	bitrates = g_list_append(bitrates, "80 kb/s");
	bitrates = g_list_append(bitrates, "96 kb/s");
	bitrates = g_list_append(bitrates, "112 kb/s");
	bitrates = g_list_append(bitrates, "128 kb/s");
	bitrates = g_list_append(bitrates, "160 kb/s");
	bitrates = g_list_append(bitrates, "192 kb/s");
	bitrates = g_list_append(bitrates, "224 kb/s");
	bitrates = g_list_append(bitrates, "256 kb/s");
	bitrates = g_list_append(bitrates, "320 kb/s");


	encoder_combo = gtk_combo_new();
	enc_list = get_installed_encoders();
	for (i = 0; enc_list[i]; i++)
		encoders = g_list_append(encoders, enc_list[i]);
	if (!encoders)
	{
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(encoder_combo)->entry),
				_("no supported encoder installed"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wav_rb), TRUE);
		gtk_widget_set_sensitive(mp3_rb, FALSE);
	}
	else
	{
		gtk_combo_set_popdown_strings(GTK_COMBO(encoder_combo), encoders);
		if (strcmp(rec_settings.encoder, _("no supported encoder installed")))
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(encoder_combo)->entry), rec_settings.encoder);
	}
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(encoder_combo)->entry), FALSE);
	for (i = 0; enc_list[i]; i++)
		free(enc_list[i]);
	free(enc_list);

	bitrate_combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(bitrate_combo), bitrates);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(bitrate_combo)->entry), FALSE);
	buffer = g_strdup_printf("%s kb/s", rec_settings.bitrate);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(bitrate_combo)->entry), buffer);
	g_free(buffer);

	gtk_widget_set_size_request(encoder_combo, 80, -1);
	gtk_widget_set_size_request(bitrate_combo, 80, -1);

	gtk_widget_set_sensitive(encoder_combo, rec_settings.mp3);
	gtk_widget_set_sensitive(bitrate_combo, rec_settings.mp3);

	gtk_table_attach_defaults(GTK_TABLE(mp3_table), encoder_label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(mp3_table), encoder_combo, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(mp3_table), bitrate_label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(mp3_table), bitrate_combo, 1, 2, 1, 2);

	gtk_container_add(GTK_CONTAINER(mp3_frame), mp3_table);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), gen_frame, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), wav_frame, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), mp3_frame, TRUE, TRUE, 0);

	//g_signal_connect(GTK_OBJECT(choose_button), "clicked", GTK_SIGNAL_FUNC(choose_button_clicked_cb), NULL);
	g_signal_connect(GTK_OBJECT(audiodev_entry), "changed", GTK_SIGNAL_FUNC(audiodev_entry_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(path_entry), "changed", GTK_SIGNAL_FUNC(path_entry_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(mp3_rb), "toggled", GTK_SIGNAL_FUNC(mp3_rb_toggled_cb), NULL);
	g_signal_connect(GTK_OBJECT(GTK_COMBO(rate_combo)->entry), "changed", GTK_SIGNAL_FUNC(rate_combo_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(GTK_COMBO(sample_combo)->entry), "changed", GTK_SIGNAL_FUNC(sample_combo_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(stereo_rb), "toggled", GTK_SIGNAL_FUNC(stereo_rb_toggled_cb), NULL);
	g_signal_connect(GTK_OBJECT(GTK_COMBO(encoder_combo)->entry), "changed", GTK_SIGNAL_FUNC(encoder_combo_changed_cb), NULL);
	g_signal_connect(GTK_OBJECT(GTK_COMBO(bitrate_combo)->entry), "changed", GTK_SIGNAL_FUNC(bitrate_combo_changed_cb), NULL);

	gtk_tooltips_set_tip(tooltips, GTK_COMBO(rate_combo)->entry, _("The audio sample-rate of the wave-file"), NULL);
	gtk_tooltips_set_tip(tooltips, GTK_COMBO(sample_combo)->entry, _("The format (8 or 16 bit) of the wave-file"), NULL);
	gtk_tooltips_set_tip(tooltips, GTK_COMBO(encoder_combo)->entry, _("Choose the mp3 encoder that should be used"), NULL);
	gtk_tooltips_set_tip(tooltips, GTK_COMBO(bitrate_combo)->entry, _("Choose the bitrate in which the mp3 will be encoded"), NULL);
	gtk_tooltips_set_tip(tooltips, audiodev_entry, _("The audio device to use (usually /dev/audio)"), NULL);
	gtk_tooltips_set_tip(tooltips, mp3_rb, _("Whether the output should be a mp3- or a wave-file"), NULL);
	gtk_tooltips_set_tip(tooltips, wav_rb, _("Whether the output should be a mp3- or a wave-file"), NULL);
	gtk_tooltips_set_tip(tooltips, stereo_rb, _("Whether the output should be in stereo or mono"), NULL);
	gtk_tooltips_set_tip(tooltips, mono_rb, _("Whether the output should be in stereo or mono"), NULL);

	gtk_widget_show_all(dialog);

	return dialog;
}
#endif

void close_status_window(void)
{
	if (timeout_id >= 0)
	{
		gtk_timeout_remove(timeout_id);
		timeout_id = -1;
	}

	if (filename) g_free(filename);
	filename = NULL;

	if (status_dialog) gtk_widget_destroy(GTK_WIDGET(status_dialog));
	status_dialog = NULL;
}

static gboolean
monitor_ioc(GIOChannel *source, GIOCondition condition, gboolean is_mp3_chan)
{
	GError *err = NULL;
	char buffer[1024], *text;
	int n = 0;
	static char *wav_buf = NULL, *mp3_buf = NULL; 
	
	//g_print("condition is %d and is_mp3_chan is %d\n", condition, is_mp3_chan);
	if ((condition & G_IO_IN) || (condition & G_IO_PRI))
	{
		if (g_io_channel_read_chars(source, buffer, 1023, &n, &err) != G_IO_STATUS_NORMAL)
		{
			g_print("%s\n", err->message);
			g_error_free(err); 
			return FALSE;
		}
		if (n<1)
			buffer[0] = '\0';
		else
			buffer[n-1] = '\0';
		if (is_mp3_chan)
		{
			if (mp3_buf)
				g_free(mp3_buf);
			mp3_buf = g_strdup(buffer);
			//g_print("mp3_buf: %s\n", mp3_buf);
		}
		else
		{
			if (wav_buf)
				g_free(wav_buf);
			wav_buf = g_strdup(buffer);
			//g_print("wav_buf: %s\n", wav_buf);
		}
	}
	if (condition & G_IO_HUP)
	{
		int exitcode = 0, exitsignal;
		exitsignal = record_get_exit_status(is_mp3_chan, &exitcode);
		//g_print("Signal was %d and Code %d\n", -exitsignal, exitcode);
		//g_print(">>%s<<\n", is_mp3_chan ? mp3_buf : wav_buf);

		close_status_window();		
		if (exitcode)
		{
			GtkWidget *dialog;
			char *bufptr = is_mp3_chan ? mp3_buf : wav_buf;
			text = g_strdup_printf(_("%s has stopped:\n%s\n"), 
					is_mp3_chan ? _("MP3/Ogg encoder") : "sox",	
					bufptr ? bufptr : _("unknown reason"));
			if (bufptr)
				g_free(bufptr);
			if (is_mp3_chan)
				mp3_buf = NULL;
			else
				wav_buf = NULL;

			record_stop(SIGKILL);
			dialog = gtk_message_dialog_new(NULL, DIALOG_FLAGS, GTK_MESSAGE_ERROR, 
						GTK_BUTTONS_CLOSE, text);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			g_free(text);
		} else record_stop(SIGINT);
		return FALSE;
	}	
	if (condition & G_IO_ERR)
	{
		g_assert_not_reached();
		return FALSE;
	}
	return TRUE;
}

static gboolean
monitor_wav_ioc(GIOChannel *source, GIOCondition condition, gpointer data)
{
	return monitor_ioc(source, condition, FALSE);
}

static gboolean
monitor_mp3_ioc(GIOChannel *source, GIOCondition condition, gpointer data)
{
	return monitor_ioc(source, condition, TRUE);
}

static gboolean timeout_cb(gpointer data)
{
	gint s;
	gchar *size=NULL;

	g_assert(filename);	
	
	if (!GTK_WIDGET_VISIBLE(status_dialog))
		gtk_widget_show_all(status_dialog);
	
	s = get_file_size(filename);
	if (s > 0) {
		if (s < 1024) size = g_strdup_printf(_("%i byte"), s);
		
		if ((s >= 1024) && (s < 1024*1024)) size = g_strdup_printf(_("%i kB"), s>>10);
		if (s >= 1024*1024) size = g_strdup_printf(_("%.2f MB"), (float)s/1024/1024);
	} else {
		if (s)	size = g_strdup(_("Error"));
		else	size = g_strdup(_("0 byte"));	
	}	
	
	gtk_label_set_text(GTK_LABEL(file_lbl), filename);
	gtk_label_set_text(GTK_LABEL(size_lbl), size);
	g_free(size);
	
	return TRUE;
}	
	
void run_status_window(GIOChannel *wavioc, GIOChannel *mp3ioc, const gchar *fn)
{
	filename = g_strdup(fn);
	g_assert(wavioc);
	g_assert(timeout_id == -1);

	wav_io_id = g_io_add_watch(wavioc, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP, (GIOFunc)monitor_wav_ioc, NULL);
	if (mp3ioc)	mp3_io_id = g_io_add_watch(mp3ioc, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP, (GIOFunc)monitor_mp3_ioc, NULL);
	timeout_id = gtk_timeout_add(500, (GtkFunction) timeout_cb, NULL);
}

static void button_clicked_cb(GtkButton *button, gpointer data)
{
	close_status_window();
	record_stop(SIGINT);
}		

static gint delete_event_cb(GtkWidget* window, GdkEventAny* e, gpointer data)
{
	button_clicked_cb(NULL, (gpointer)window);
	return TRUE;
}

GtkWidget* record_status_window(void)
{
	GtkWidget *btn_label, *btn_pixmap, *button;
	GtkWidget *vbox, *btn_box, *hbox, *lbl_hbox, *lbl_vbox1, *lbl_vbox2;
	GtkWidget *f_lbl, *s_lbl;

	status_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(status_dialog),_("Gnomeradio recording status"));
	//gtk_window_set_resizable(GTK_WINDOW(status_dialog), FALSE);
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
	gtk_label_set_ellipsize(GTK_LABEL(file_lbl), PANGO_ELLIPSIZE_MIDDLE);
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

	g_signal_connect(GTK_OBJECT(status_dialog), "delete_event", GTK_SIGNAL_FUNC(delete_event_cb), NULL);
	g_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_clicked_cb), NULL);

	gtk_window_set_modal(GTK_WINDOW(status_dialog), TRUE);

	return status_dialog;
}
