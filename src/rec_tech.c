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

#include <glib.h>
#include "rec_tech.h"

static pid_t wav_pid = -1, mp3_pid = -1;

#define FAIL_STRING "wrzlbrmft"
#define FIFO_NAME "/tmp/gnomeradio_fifo"

static GIOChannel*
execute_command(int *pid, const char *arg1, ...)
{
	GIOChannel *ioc = NULL;
	int i, pipefds[2];
	va_list args;
	char **cmd, *str;
	
	g_assert(arg1);
	i = 2;
	
	va_start(args, arg1);
	while(va_arg(args, char*))
		i++;
	va_end(args);

	cmd = g_malloc(i*sizeof(char*));
	
	i = 1;
	cmd[0] = (char*)arg1;
	g_print("execute_command(): %s ", cmd[0]);
	va_start(args, arg1);
	while((str = va_arg(args, char*)))
	{
		cmd[i] = str;
		g_print("%s ", cmd[i++]);	
	}
	va_end(args);
	g_print("\n");	
	cmd[i] = NULL;

	if (pipe(pipefds) < 0)
	{
		perror("pipe");
		return NULL;
	}
	
	switch (*pid = fork())
	{
		case 0: 
			close(pipefds[0]);
			close(1);
			close(2);
			dup(pipefds[1]);
			dup(pipefds[1]);
			close(pipefds[1]);
			execvp(cmd[0], cmd);
			perror(FAIL_STRING);
			_exit(1);
		case -1: 
			close(pipefds[0]);
			close(pipefds[1]);
			g_free(cmd);
			return NULL; 
		default:
			//g_print("fd for %s is %d\n", cmd[0], pipefds[0]);
			g_free(cmd);
	}	
	close(pipefds[1]);
	ioc = g_io_channel_unix_new(pipefds[0]);
	return ioc;
}

/*
 * These functions handle recording
 */

GList* 
get_installed_encoders(void)
{
	int i = 0;
	const char* encoders[] =	{ "lame", "bladeenc", "oggenc", NULL};
	GList *result = NULL;
	
	for (i=0; encoders[i]; i++) {
		gchar *path = g_find_program_in_path(encoders[i]); 		
		if (path) { 
			result = g_list_append(result, g_strdup(encoders[i]));
			g_free(path);
		}
	}

	return result;
}

int 
check_sox_installation(void)
{
	gchar *path = g_find_program_in_path("sox"); 
	if (path) {
		g_free(path);
		return 1;
	}
	return 0;
}

void
record_as_wave(GIOChannel **wavioc, const gchar *filename)
{
	char *ster, *sample;	
	GError *err = NULL;
	
	ster = rec_settings.stereo ? "2" : "1";
	if (strcmp(rec_settings.sample, "8"))
		sample = "-w";
	else 
		sample = "-b";
	
	*wavioc = execute_command(&wav_pid, 
				"sox", "-c2", "-w", "-r32000","-tossdsp", rec_settings.audiodevice,
				"-r", rec_settings.rate, "-c", ster, sample, 
				"-twav", filename,
				NULL);
	
	if (g_io_channel_set_flags(*wavioc, G_IO_FLAG_NONBLOCK, &err) != G_IO_STATUS_NORMAL)
	{
		g_print("%s\n", err->message);
		g_error_free(err); 
		return;
	}
}	

void record_as_mp3(GIOChannel **wavioc, GIOChannel **mp3ioc, const gchar *filename)
{
	char *ster, *sample;	
	GError *err = NULL;

	if (mkfifo(FIFO_NAME, 0700))
		perror("mkfifo");

	ster = rec_settings.stereo ? "2" : "1";
	if (strcmp(rec_settings.sample, "8"))
		sample = "-w";
	else 
		sample = "-b";
	
	*wavioc = execute_command(&wav_pid, 
				"sox", "-c2", "-w", "-r32000","-tossdsp", rec_settings.audiodevice,
				"-r", rec_settings.rate, "-c", ster, sample, 
				"-twav", FIFO_NAME,
				NULL);
	
	if (!strcmp("lame", rec_settings.encoder))
	{
		/* lame -S -h -b bitrate pipename filename */
		*mp3ioc = execute_command(&mp3_pid,
				"lame", "-S", "-h", "-b", rec_settings.bitrate,
				FIFO_NAME, filename, 
				NULL);
	}
	else if (!strcmp("bladeenc", rec_settings.encoder))
	{
		/* bladeenc -br bitrate pipename filename */
		*mp3ioc = execute_command(&mp3_pid,
				"bladeenc", "-quiet", "-br", rec_settings.bitrate,
				FIFO_NAME, filename,
				NULL);
	}		
	else if (!strcmp("oggenc", rec_settings.encoder))
	{
		/* oggenc -Q -b bitrate -o filename pipename */
		*mp3ioc = execute_command(&mp3_pid,
				"oggenc", "-Q", "-b", rec_settings.bitrate,
				"-o", filename, FIFO_NAME,
				NULL);
	}		
	else
	{
		g_assert_not_reached();
	}
	
	if (g_io_channel_set_flags(*wavioc, G_IO_FLAG_NONBLOCK, &err) != G_IO_STATUS_NORMAL)
	{
		g_print("%s\n", err->message);
		g_error_free(err); 
		return;
	}

	if (g_io_channel_set_flags(*mp3ioc, G_IO_FLAG_NONBLOCK, &err) != G_IO_STATUS_NORMAL)
	{
		g_print("%s\n", err->message);
		g_error_free(err); 
		return;
	}
}	

int 
record_get_exit_status(gboolean mp3, int *exitcode)
{
	int status = 0;
	
	if (mp3)
		waitpid(mp3_pid, &status, 0);
	else
		waitpid(wav_pid, &status, 0);
	
	if (!WIFEXITED(status))
		return 0;
	*exitcode = WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return (-1 * WTERMSIG(status));
	else
		return 1;
}

void record_stop(int sig)
{
	//g_print("wav_pid is %d mp3_pid is %d\n", wav_pid, mp3_pid);
	if (wav_pid>-1)
	{
		if (kill(wav_pid, sig))
			perror("Kill sox");
	}
	if (mp3_pid>-1)
	{
		if (kill(mp3_pid, sig))
			perror("Kill mp3 encoder");
		if (remove(FIFO_NAME))
			perror("remove");
	}
	
	wav_pid = mp3_pid = -1;
}	
	
	
/*
 * Miscellanelous functions
 */

int get_file_size(char *filename)
{
	struct stat buffer;
	assert(filename);
	
	if (lstat(filename, &buffer))
		return -1;
	return (int)buffer.st_size;
}

int check_filename(const char *filename)
{
	int flags, retval;
	
	assert(filename);
	
	flags = O_RDWR | O_CREAT | O_APPEND;
	
	retval = open(filename, flags, 0664);
	
	if (retval < 0)
	{
		return 0;
	}

	close(retval);
	return 1;
}
