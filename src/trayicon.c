/* trayicon.c
 *
 * Copyright (C) 2006 Jörgen Scheibengruber
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

#include <config.h>
#include <gnome.h>
#include "gui.h"
#include "trayicon.h"
#include "../pixmaps/radio.xpm"

static GtkWidget *showwindow_menuitem;

static void mute_menuitem_toggled_cb(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	toggle_volume();
}

static void record_menuitem_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	rec_button_clicked_cb(NULL, user_data);
}

static void showwindow_menuitem_toggled_cb(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	GtkWidget* app = GTK_WIDGET(user_data);
	toggle_mainwindow_visibility(app);
}

static void quit_menuitem_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	exit_gnome_radio();
}

void preset_menuitem_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	preset* ps;
	mom_ps = (int)user_data;
	
	g_assert(mom_ps >= 0 &&	mom_ps < g_list_length(settings.presets));
	
	ps = (preset*)g_list_nth_data(settings.presets, mom_ps);
	gtk_adjustment_set_value(adj, ps->freq * STEPS);
}

void create_tray_menu(GtkWidget *app) {
	GList *node = settings.presets;
	int i;
	
	tray_menu = gtk_menu_new();

	for (i = 0; node; i++, node = node->next)
	{
		preset *ps = (preset*)node->data;
		GtkWidget *menuitem = gtk_menu_item_new_with_label(ps->title); 
		
		gtk_menu_shell_insert(GTK_MENU_SHELL(tray_menu), menuitem, i);		
		g_signal_connect(G_OBJECT(menuitem), "activate", (GCallback)preset_menuitem_activate_cb, (gpointer)i);
		gtk_widget_show(menuitem);
	}
	
	gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), gtk_separator_menu_item_new());

	mute_menuitem = gtk_check_menu_item_new_with_label(_("Muted"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mute_menuitem), mixer_get_volume() == 0);
	gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), mute_menuitem);
	mute_menuitem_toggled_cb_id = 
	g_signal_connect(G_OBJECT(mute_menuitem), "toggled", (GCallback)mute_menuitem_toggled_cb, (gpointer)app);
	gtk_widget_show(mute_menuitem);

	GtkWidget *record_menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_RECORD, NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), record_menuitem);		
	g_signal_connect(G_OBJECT(record_menuitem), "activate", (GCallback)record_menuitem_activate_cb, app);
	gtk_widget_show(record_menuitem);

	gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), gtk_separator_menu_item_new());
	
	showwindow_menuitem = gtk_check_menu_item_new_with_label(_("Show Window"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showwindow_menuitem), TRUE);
	/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showwindow_menuitem), GTK_WIDGET_VISIBLE(app));*/
	gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), showwindow_menuitem);
	g_signal_connect(G_OBJECT(showwindow_menuitem), "activate", (GCallback)showwindow_menuitem_toggled_cb, (gpointer)app);
	gtk_widget_show(showwindow_menuitem);

	GtkWidget *quit_menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), quit_menuitem);		
	g_signal_connect(G_OBJECT(quit_menuitem), "activate", (GCallback)quit_menuitem_activate_cb, NULL);
	gtk_widget_show(quit_menuitem);

	gtk_widget_show_all(tray_menu);
}

static gboolean tray_destroyed_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	create_tray_icon(GTK_WIDGET(data));
	return TRUE;
}

static gboolean tray_clicked_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	gboolean active;
	switch (event->button)
	{
		case 1:
			if (event->type != GDK_BUTTON_PRESS)
				break;
			active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(showwindow_menuitem));
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showwindow_menuitem), !active);
			break;
		case 3:
			if (event->type != GDK_BUTTON_PRESS)
				break;
			gtk_menu_popup(GTK_MENU(tray_menu), NULL, NULL, NULL, NULL, event->button, event->time);
			break;
	}			
	return FALSE;
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
		G_CALLBACK(tray_clicked_cb), (gpointer)app);
	g_signal_connect(G_OBJECT(tray_icon), "destroy-event",
		G_CALLBACK(tray_destroyed_cb), (gpointer)app);
	gtk_widget_show_all(GTK_WIDGET(tray_icon));
	
	text = g_strdup_printf(_("Gnomeradio - %.2f MHz"), adj->value/STEPS);
	gtk_tooltips_set_tip(tooltips, tray_icon, text, NULL);
	g_free(text);
}
