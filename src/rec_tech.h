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

#ifndef _REC_TECH_H
#define _REC_TECH_H

typedef struct Recording_Settings recording_settings;
struct Recording_Settings
{
	gchar *audiodevice;
	gchar *destination;
	gboolean mp3;
	gchar *rate;
	gchar *sample;
	gboolean stereo;
	gchar *encoder;
	gchar *bitrate;
};

recording_settings rec_settings;

GList *
get_installed_encoders(void);

int 
check_sox_installation(void);

void
record_as_wave(GIOChannel **wavioc, const gchar *filename);

void
record_as_mp3(GIOChannel **wavioc, GIOChannel **mp3ioc, const gchar *filename);

int 
record_get_exit_status(gboolean mp3, int *exitcode);

void
record_stop(int sig);

int
check_filename(const char *filename);

int
get_file_size(char *filename);

#endif
