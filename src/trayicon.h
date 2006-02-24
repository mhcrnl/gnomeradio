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

#ifndef _TRAYICON_H
#define _TRAYICON_H

GtkWidget *tray_menu;
GtkWidget *tray_icon;
GtkWidget *mute_menuitem;

int mute_menuitem_toggled_cb_id;

void tray_icon_items_set_sensible(gboolean sensible);

void create_tray_icon(GtkWidget *app);

void create_tray_menu(GtkWidget *app);

#endif
