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
#include <gst/gst.h>

typedef struct Recording_Settings recording_settings;
struct Recording_Settings
{
	gchar *profile;
	gchar *destination;
/*	gchar *audiodevice;
	gboolean mp3;
	gchar *rate;
	gchar *sample;
	gboolean stereo;
	gchar *encoder;
	gchar *bitrate;*/
};

recording_settings rec_settings;

typedef struct {
	GstElement* pipeline;
	char* filename;
	char* station;
} Recording;

Recording*
recording_start(const char* filename);

void
recording_stop(Recording* recording);

int
check_filename(const char *filename);

int
get_file_size(char *filename);

#endif
