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

#include "config.h"

#ifdef HAVE_LIRC

#include <string.h>
#include <fcntl.h>
#include <gnome.h>

#ifdef HAVE_LIRC

#include <lirc/lirc_client.h>

#endif

#include "lirc.h"
#include "gui.h"

static int fd = -1;
static struct lirc_config *config = NULL;

static void execute_lirc_command (char *cmd)
{
	printf("lirc command: %s\n", cmd);

	if (strcasecmp (cmd, "tune up") == 0) 
	{
		scfw_button_clicked_cb(NULL, NULL);
	}
	else if (strcasecmp (cmd, "tune down") == 0) 
	{
		scbw_button_clicked_cb(NULL, NULL);
	}
	else if (strcasecmp (cmd, "volume up") == 0) 
	{
		gtk_adjustment_set_value(volume, (volume->value > 95) ? 100 : volume->value+5);
	}
	else if (strcasecmp (cmd, "volume down") == 0) 
	{
		gtk_adjustment_set_value(volume,(volume->value < 5) ? 0 : volume->value-5);
	}
	else if (strcasecmp (cmd, "mute") == 0)
	{
		toggle_volume();
	}
	else if (strcasecmp (cmd, "tv") == 0)
	{
		exit_gnome_radio();
		gnome_execute_shell(NULL, "xawtv"); 
	}
	else if (strcasecmp (cmd, "quit") == 0)
	{
		exit_gnome_radio();
	}
	else if (strcasecmp (cmd, "preset up") == 0)
	{
		change_preset(TRUE);
	}
	else if (strcasecmp (cmd, "preset down") == 0)
	{
		change_preset(FALSE);
	}
	else if (strncasecmp (cmd, "preset ", 7) == 0) 
	{
		int tmp = 0, ret;
		ret = sscanf(cmd, "preset %i", &tmp);
		if (ret && (tmp < g_list_length(settings.presets)))
		{
			preset *ps;
			ps = g_list_nth_data(settings.presets, tmp);
			gtk_adjustment_set_value(adj, ps->freq*STEPS);
			mom_ps = tmp;
			preset_combo_set_item(mom_ps);
		}
	}
	else
	{
    		printf ("unrecognized lirccmd: %s\n", cmd);
	}
}

static char* map_code_to_default(char *code)
{
	char event[21];
	unsigned int dummy, repeat = 0;
	int	key = 0;
	
	if (sscanf(code,"%x %x %20s", &dummy, &repeat, event) != 3)
	{
		printf("lirc: oops, parse error: %s", code);
		return NULL;
	}
	
	if (!strcasecmp("VOL+", event))
		return (char*)strdup("volume up");
	else if (!strcasecmp("VOL-", event))
		return (char*)strdup("volume down");
	else if (!repeat && !strcasecmp("CH+", event))
		return (char*)strdup("tune up");
	else if (!repeat && !strcasecmp("CH-", event))
		return (char*)strdup("tune down");
	else if (!repeat && !strcasecmp("MUTE", event))
		return (char*)strdup("mute");
	else if (!repeat && !strcasecmp("MINIMIZE", event))
		return (char*)strdup("quit");
	
	if (sscanf(event, "%d", &key) == 1)
	{
		char *ret;
		if (repeat || (key < 0) || (key > 9))
			return NULL;
		ret = malloc(strlen("preset xx"));
		sprintf(ret, "preset %1.1d", key);
		return ret;
	}
	
	return NULL;
}
	
int my_lirc_init(void)
{
	printf("Trying to bring up lirc\n");
	if ((fd = lirc_init ("gnomeradio", 0)) <=0) 
	{
		perror("lirc_init");
		return 0;
	}
  
	if (lirc_readconfig (NULL, &config, NULL) != 0) 
	{
		perror("lirc_readconfig");
    	config = NULL;
		/*lirc_deinit ();
    	fd = -1;
    	return 0;*/
		printf("The lirc configfile (~/.lircrc) could not be opened.\n"
				"Using default config.\n");
	}
  
	fcntl (fd, F_SETFL, O_NONBLOCK);
	fcntl (fd, F_SETFD, FD_CLOEXEC);
  
	return 1;
}

void my_lirc_deinit(void)
{
	if (fd <= 0)
		return;
	printf("Shutting down lirc\n");
	/*gdk_input_remove (input_tag);*/
	lirc_freeconfig(config);
	lirc_deinit ();
}	

static gboolean lirc_has_data_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
	char *code, *cmd;
	int ret = -1;
  
	while (lirc_nextcode (&code) == 0 && code != NULL) 
	{
		ret = 0;
		if (config)
		{
			while (lirc_code2char (config, code, &cmd) == 0 && cmd != NULL) 
			{
				execute_lirc_command(cmd);
			}
		}
		else
		{
			cmd = map_code_to_default(code);
			if (cmd)
			{
				execute_lirc_command(cmd);
				free(cmd);
			}
		}	
		free (code);
	}

	/* on LIRC error, shutdown added input*/
	if (ret == -1) 
	{
		printf("An lirc error occured\n");
		/*gdk_input_remove (input_tag);*/
		lirc_freeconfig (config);
		config = NULL;
		lirc_deinit ();
		return FALSE;
	}
	return TRUE;
}

void start_lirc(void)
{
	GIOChannel *ioc;

	if (fd<0)
		return;

	ioc = g_io_channel_unix_new(fd);
	g_io_add_watch(ioc, G_IO_IN, lirc_has_data_cb, NULL);
}

#endif
