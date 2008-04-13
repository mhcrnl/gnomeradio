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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>

#include <sys/soundcard.h>

#include "tech.h"

static int mixer_fd = -1, mixer_src = -1;
static char *devices[] = SOUND_DEVICE_NAMES;

/*
 * These functions handle the mixer device
 */
 
int mixer_init(char *mixer_device, char *mixer_source)
{
	int i;
	
	mixer_src = -1;
	
	for (i=0;i<SOUND_MIXER_NRDEVICES;i++)
		if (strcmp(mixer_source, devices[i]) == 0) 
			mixer_src = i;

	mixer_fd = open(mixer_device, O_RDWR);

	if (mixer_src < 0)	
		return -1;
			
	if (mixer_fd < 0)
		return 0;
	return 1;
}

char* mixer_get_sndcard_name(void)
{
	mixer_info info;
	char *result = NULL;

	if (ioctl(mixer_fd, SOUND_MIXER_INFO, &info) == -1)
		return NULL;

	result = strdup(info.name);	
	
	return result;
}

char** mixer_get_rec_devices(void)
{
	int i, o, devmask, res;
	char** result;
	
	if ((ioctl(mixer_fd, SOUND_MIXER_READ_RECMASK, &devmask)) == -1)
		return NULL;
	else
	{
		result = malloc(sizeof(char*)*SOUND_MIXER_NRDEVICES);
		o = 0;
		for (i=0;i<SOUND_MIXER_NRDEVICES;i++)
			{
				res = (devmask >> i)%2;
				if (res)
				{
					result[o] = malloc(strlen(devices[i])+1);
					sprintf(result[o], "%s", devices[i]); 
					o++;
				}
				result[o] = NULL;	
			}
	}
	return result;
}

int mixer_set_rec_device(void)
{
	int devmask, recmask;

	if (mixer_fd <= 0)
		return 0;

	if (mixer_src < 0)
		return 0;

	if ((ioctl(mixer_fd, SOUND_MIXER_READ_RECMASK, &devmask)) == -1)
		return 0;
	
	recmask = 1 << mixer_src;
	if (!(recmask & devmask))
		return 0;

	if ((ioctl(mixer_fd, SOUND_MIXER_WRITE_RECSRC, &recmask)) == -1)
		return 0;

	return 1;
}			

int mixer_close(void)
{
	if (mixer_fd > 0)
		close(mixer_fd);
	
	return 1;
}
		
int mixer_set_volume(int volume)
{
	int i_vol;
	if (mixer_fd<0)
		return -1;

	assert(volume <= 100);
	assert(volume >= 0);


	if (mixer_src<0) 
		return -1;
	
	i_vol = volume;  
	i_vol += volume << 8;

	/*printf("Setting %s to vol %i\n", devices[mixer_src], volume);*/
	if ((ioctl(mixer_fd, MIXER_WRITE(mixer_src), &i_vol)) < 0)
		return 0;
		
	return 1;
}

int mixer_get_volume(void)
{
	int i_vol = 0, r, l, volume;
	
	if (mixer_fd<0)
		return -1;

	if (mixer_src<0) 
		return -1;
	
	if ((ioctl(mixer_fd, MIXER_READ(mixer_src), &i_vol)) < 0)
		return 0;

	r = i_vol >> 8;
	l = i_vol % (1 << 8);
	volume = (r + l)/2;
	/*printf("%d %d %d %d\n", r, l, volume, i_vol);*/
	
	assert((volume >= 0) && (volume <= 100));
	
	return volume;
}
