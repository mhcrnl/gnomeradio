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


#include "rec_tech.h"
#include <gnome.h>
#include <profiles/audio-profile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static GstElement*
my_gst_gconf_render_bin_from_description(const gchar * description);

static GstPad*
my_gst_bin_find_unconnected_pad(GstBin* bin, GstPadDirection direction);

static void error_cb(GstBus *bus, GstMessage *message, gpointer user_data)
{
	GError *error = NULL;
	GstElement *pipeline = user_data;
	g_assert(pipeline);
	
	/* Make sure the pipeline is not running any more */
	gst_element_set_state (pipeline, GST_STATE_NULL);
	
	gst_message_parse_error(message, &error, NULL);
	show_error_message(_("GStreamer runtime error."), error->message);
	g_error_free(error);
}

Recording*
recording_start(const char* filename)
{
	GMAudioProfile *profile;
	GstElement *pipeline, *oss_src, *encoder, *filesink;
	pipeline = oss_src = encoder = filesink = NULL;
	
	profile = gm_audio_profile_lookup(rec_settings.profile);
	g_assert(profile);
	
	pipeline = gst_pipeline_new("gnomeradio-record-pipeline");
	if (!pipeline) {
		show_error_message(_("Could not create GStreamer pipeline."),
		_("Check your Gstreamer installation!"));
		goto error;
	}		
	
	oss_src = gst_element_factory_make("osssrc", "oss-source");
	if (!oss_src) {
		show_error_message(_("Could not open Gstreamer OSS Source."),
		_("Verify your Gstreamer OSS subsystem installation!"));
		goto error;
	}
	
	GstBus *bus = gst_element_get_bus(pipeline);
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message::error", G_CALLBACK(error_cb), pipeline);

	char* pipeline_str = g_strdup_printf("audioconvert ! %s", gm_audio_profile_get_pipeline(profile));
	encoder = my_gst_gconf_render_bin_from_description(pipeline_str);
	g_free(pipeline_str);
	if (!encoder) {
		char *caption = g_strdup_printf(_("Could not create encoder \"%s\"."), gm_audio_profile_get_name (profile));
		show_error_message(caption,
		_("Verify your Gstreamer plugins installation!"));
		g_free(caption);
		goto error;
	}
	
	/* Write to disk */
	filesink = gst_element_factory_make("filesink", "file-sink");
	if (!filesink) {	
		show_error_message(_("Could not create Gstreamer filesink."),
		_("Check your Gstreamer installation!"));
		goto error;
	}
	
	/* Add the elements to the pipeline */
	gst_bin_add_many(GST_BIN(pipeline), oss_src, encoder, filesink, NULL);
	
	/* Link it all together */
	if (!gst_element_link_many(oss_src, encoder, filesink, NULL)) {
		g_warning("Could not link elements. This is bad!\n");
		goto error;
	}
	char* path = g_strdup_printf("%s.%s", filename, gm_audio_profile_get_extension(profile));	
	g_object_set(G_OBJECT(filesink), "location", path, NULL);
	
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	
	Recording *recording = g_malloc0(sizeof(Recording));
	recording->filename = path;
	recording->pipeline = pipeline;
	
	return recording;
	
error:
	if (pipeline)
		gst_object_unref(GST_OBJECT(pipeline));
	if (oss_src)
		gst_object_unref(GST_OBJECT(oss_src));
	if (encoder)
		gst_object_unref(GST_OBJECT(encoder));
	if (filesink)
		gst_object_unref(GST_OBJECT(filesink));
	
	return NULL;
}		

void
recording_stop(Recording *recording)
{
	g_assert(recording);

	GstState state;
	gst_element_get_state(recording->pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
	if (state != GST_STATE_PLAYING) {
    	g_print("Ups!\n");
	} else {
		gst_element_set_state(recording->pipeline, GST_STATE_NULL);
	}
	gst_object_unref(GST_OBJECT(recording->pipeline));
	g_free(recording->filename);
	g_free(recording->station);
	g_free(recording);
}	

/* Stolen from gst-plugins-good/ext/gconf/gconf.c */
static GstPad *
my_gst_bin_find_unconnected_pad (GstBin * bin, GstPadDirection direction)
{
  GstPad *pad = NULL;
  GList *elements = NULL;
  const GList *pads = NULL;
  GstElement *element = NULL;

  GST_OBJECT_LOCK (bin);
  elements = bin->children;
  /* traverse all elements looking for unconnected pads */
  while (elements && pad == NULL) {
    element = GST_ELEMENT (elements->data);
    GST_OBJECT_LOCK (element);
    pads = element->pads;
    while (pads) {
      GstPad *testpad = GST_PAD (pads->data);

      /* check if the direction matches */
      if (GST_PAD_DIRECTION (testpad) == direction) {
        GST_OBJECT_LOCK (testpad);
        if (GST_PAD_PEER (testpad) == NULL) {
          GST_OBJECT_UNLOCK (testpad);
          /* found it ! */
          pad = testpad;
          break;
        }
        GST_OBJECT_UNLOCK (testpad);
      }
      pads = g_list_next (pads);
    }
    GST_OBJECT_UNLOCK (element);
    elements = g_list_next (elements);
  }
  GST_OBJECT_UNLOCK (bin);

  return pad;
}

/* Stolen from gst-plugins-good/ext/gconf/gconf.c */
static GstElement *
my_gst_gconf_render_bin_from_description (const gchar * description)
{
  GstElement *bin = NULL;
  GstPad *pad = NULL;
  GError *error = NULL;
  gchar *desc = NULL;

  /* parse the pipeline to a bin */
  desc = g_strdup_printf ("bin.( %s )", description);
  bin = GST_ELEMENT (gst_parse_launch (desc, &error));
  g_free (desc);
  if (error) {
    GST_ERROR ("gstgconf: error parsing pipeline %s\n%s\n",
        description, error->message);
    g_error_free (error);
    return NULL;
  }

  /* find pads and ghost them if necessary */
  if ((pad = my_gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SRC))) {
    gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
  }
  if ((pad = my_gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SINK))) {
    gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  }
  return bin;
}


/*
 * Miscellanelous functions
 */

int get_file_size(char *filename)
{
	struct stat buffer;
	g_assert(filename);
	
	if (lstat(filename, &buffer))
		return -1;
	return (int)buffer.st_size;
}

int check_filename(const char *filename)
{
	int flags, retval;
	
	g_assert(filename);
	
	flags = O_RDWR | O_CREAT | O_APPEND;
	
	retval = open(filename, flags, 0664);
	
	if (retval < 0)
	{
		return 0;
	}

	close(retval);
	return 1;
}
