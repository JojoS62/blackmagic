/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements the SW-DP interface. */

#include "mbed.h"

#include "general.h"
#include "swdptap.h"

enum {
	SWDIO_STATUS_FLOAT = 0,
	SWDIO_STATUS_DRIVE
};

extern "C" {
static void swdptap_turnaround(int dir) __attribute__ ((optimize(1)));
static uint32_t swdptap_seq_in(int ticks) __attribute__ ((optimize(1)));
static bool swdptap_seq_in_parity(uint32_t *ret, int ticks)	__attribute__ ((optimize(1)));
static void swdptap_seq_out(uint32_t MS, int ticks)	__attribute__ ((optimize(1)));
static void swdptap_seq_out_parity(uint32_t MS, int ticks)	__attribute__ ((optimize(1)));
}

DigitalOut 	swdClk(PB_7, 0);
DigitalInOut swdDIO(PB_8, PIN_OUTPUT, PullNone, 1);

static void swdptap_turnaround(int dir)
{
	static int olddir = SWDIO_STATUS_DRIVE;

	/* Don't turnaround if direction not changing */
	if(dir == olddir) return;
	olddir = dir;

	if (dir == SWDIO_STATUS_FLOAT) {
		swdDIO.input();
	}
	
	swdClk = 1;
	swdClk = 1;
	swdClk = 1;
	swdClk = 0;

	if (dir == SWDIO_STATUS_DRIVE) {
		swdDIO.output();
		swdDIO=1;
	}
}

static uint32_t swdptap_seq_in(int ticks)
{
	uint32_t index = 1;
	uint32_t ret = 0;
	int len = ticks;

	swdptap_turnaround(SWDIO_STATUS_FLOAT);
	while (len--) {
		int val = swdDIO & 1;
		if (val)
			ret |= index;
		index <<= 1;
		swdClk = 1;
		swdClk = 0;
	}

	return ret;
}

static bool swdptap_seq_in_parity(uint32_t *ret, int ticks)
{
	*ret = swdptap_seq_in(ticks);
	int parity_bit = swdDIO & 1;
	swdClk = 1;
	swdClk = 0;

	int parity = __builtin_popcount(*ret);
	if (parity_bit) {
		parity++;
	}

	if (ticks == 32) {
		swdptap_turnaround(SWDIO_STATUS_DRIVE);
	}

	return (parity & 1);
}

static void swdptap_seq_out(uint32_t MS, int ticks)
{
	swdptap_turnaround(SWDIO_STATUS_DRIVE);
	bool trailing_bits = ticks==2;

	while (ticks--) {
		swdDIO = MS & 1;
		MS >>= 1;
		swdClk = 1;
		swdClk = 0;
	}

	if (trailing_bits) {
		swdDIO = 1;
	}
}

static void swdptap_seq_out_parity(uint32_t MS, int ticks)
{
	int parity = __builtin_popcount(MS);

	swdptap_seq_out(MS, ticks);
	swdDIO = parity & 1;
	swdClk = 1;
	swdClk = 0;
}

swd_proc_t swd_proc;

extern "C" int swdptap_init(void)
{
	swd_proc.swdptap_seq_in  = swdptap_seq_in;
	swd_proc.swdptap_seq_in_parity  = swdptap_seq_in_parity;
	swd_proc.swdptap_seq_out = swdptap_seq_out;
	swd_proc.swdptap_seq_out_parity  = swdptap_seq_out_parity;

	return 0;
}
