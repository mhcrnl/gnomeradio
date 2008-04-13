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

#include "radio.h"
#include "v4l1.h"
#include "v4l2.h"

static RadioDev *dev;

/*
 * These functions handle the radio device
 */
 
int radio_init(char *device, DriverType driver)
{
    int rv = -1;
	if (dev) {
		radio_stop();
	}

	switch (driver) {
		case DRIVER_V4L2:
			goto try_v4l2;
		case DRIVER_ANY:
		case DRIVER_V4L1:
		default:
			goto try_v4l1;
	}

try_v4l1:
	dev = v4l1_radio_dev_new();
	rv = dev->init (dev, device);
	if (rv == 0) {
        fprintf(stderr, "Initializing v4l1 failed\n");
		dev->finalize (dev);
		dev = NULL;
		if (driver != DRIVER_ANY)
			goto failure;
	} else {
		goto success;
	}

try_v4l2:
	dev = v4l2_radio_dev_new();
	rv = dev->init (dev, device);
	if (rv == 0) {
        fprintf(stderr, "Initializing v4l2 failed\n");
		dev->finalize (dev);
		dev = NULL;
		if (driver != DRIVER_ANY)
			goto failure;
	} else {
		goto success;
	}

success:
	radio_unmute();
failure:

	return rv;
}

int radio_is_init(void)
{
	if (dev) return dev->is_init (dev);
	else return 0;
}

void radio_stop(void)
{
	radio_mute();
	
	if (dev) dev->finalize (dev);
}

void radio_set_freq(float frequency)
{
	if (dev) dev->set_freq (dev, frequency);
}

void radio_unmute(void)
{
	if (dev) dev->mute (dev, 0);
}

void radio_mute(void)
{
	if (dev) dev->mute (dev, 1);
}

int radio_get_stereo(void)
{
	if (dev) return dev->get_stereo (dev);
	else return -1;
}

int radio_get_signal(void)
{
	if (dev) return dev->get_signal (dev);
	else return -1;
}

int radio_check_station(float freq)
{
	static int a, b;
	static float last;
	int signal;
	
	signal = radio_get_signal();
	
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

