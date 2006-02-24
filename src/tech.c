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

#include <linux/videodev.h>
#include <sys/soundcard.h>

#include "tech.h"

static int freq_fact, fd = -1, mixer_fd = -1, mixer_src = -1;
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

/*
 * These functions handle the radio device
 */
 
int radio_init(char *device)
{
	struct video_tuner tuner;
	
	if ((fd = open(device, O_RDONLY))< 0)
		return 0;
	
	tuner.tuner = 0;
	if (ioctl (fd, VIDIOCGTUNER, &tuner) < 0)
		freq_fact = 16;
	else
	{
		if ((tuner.flags & VIDEO_TUNER_LOW) == 0)
			freq_fact = 16;
		else 
			freq_fact = 16000;
	}		
	
	radio_unmute();
	
	return 1;
}

int radio_is_init(void)
{
	return (fd >= 0);
}

void radio_stop(void)
{
	radio_mute();
	
	if (fd >= 0)
		close(fd);
}

int radio_setfreq(float freq)
{
    int ifreq = (freq+1.0/32)*freq_fact;
    if (fd<0)
    	return 0;
    
#if 0
	printf("Setting to %i (= %.2f)\n", ifreq, freq);
#endif
	
	if ((freq > 108) || (freq < 65))
		return 1;

	assert ((freq <= 108) && (freq > 65));

    return ioctl(fd, VIDIOCSFREQ, &ifreq);
}

int radio_check_station(float freq)
{
	static int a, b;
	static float last;
	int signal;
	
	signal = radio_getsignal();
	
	if (last == 0.0f)
		last = freq;
	
	if ((a + b + signal > 8) && (fabsf(freq - last) > 0.25f)) {
		a = b = 0;
		last = freq;
		return 1;
	}
	a = b;
	b = signal;
	return 0;
}	


void radio_unmute(void)
{
    struct video_audio vid_aud;

    if (fd<0)
    	return;

    if (ioctl(fd, VIDIOCGAUDIO, &vid_aud))
		perror("VIDIOCGAUDIO");
    /*if (vid_aud.volume == 0)*/
	vid_aud.volume = 0xFFFF;
    vid_aud.flags &= ~VIDEO_AUDIO_MUTE;
	vid_aud.mode = VIDEO_SOUND_STEREO;
	
    if (ioctl(fd, VIDIOCSAUDIO, &vid_aud))
		perror("VIDIOCSAUDIO");
}

void radio_mute(void)
{
    struct video_audio vid_aud;

    if (fd<0)
    	return;

    if (ioctl(fd, VIDIOCGAUDIO, &vid_aud))
		perror("VIDIOCGAUDIO");
    vid_aud.flags |= VIDEO_AUDIO_MUTE;
    if (ioctl(fd, VIDIOCSAUDIO, &vid_aud))
		perror("VIDIOCSAUDIO");
}

int radio_getstereo()
{
    struct video_audio va;
    va.mode=-1;

    if (fd<0)
    	return -1;
    
    if (ioctl (fd, VIDIOCGAUDIO, &va) < 0)
		return -1;
	if (va.mode == VIDEO_SOUND_STEREO)
		return 1;
	else 
		return 0;
}

int radio_getsignal()
{
    struct video_tuner vt;
    int signal;

    if (fd<0)
    	return -1;

    memset(&vt,0,sizeof(vt));
    ioctl (fd, VIDIOCGTUNER, &vt);
    signal=vt.signal>>13;

    return signal;
}
