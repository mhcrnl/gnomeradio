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

#ifndef _TECH_H
#define _TECH_H

int mixer_init(char *mixer_device, char *mixer_source);

char* mixer_get_sndcard_name(void);

char** mixer_get_rec_devices(void);

int mixer_close(void);

int mixer_set_volume(int volume);

int mixer_get_volume(void);

int mixer_set_rec_device(void);

int radio_init(char *device);

int radio_is_init(void);

void radio_stop(void);

int radio_setfreq(float freq);

int radio_check_station(float freq);

void radio_unmute();

void radio_mute();

int radio_getstereo();

int radio_getsignal();

#endif
