/*
 * Copyright 2011-2015 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ck_hp.h>
#include <ck_hp_fifo.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"

#ifndef ENTRIES
#define ENTRIES 4096
#endif

#ifndef STEPS
#define STEPS 40000
#endif

static ck_hp_fifo_t fifo;
static ck_hp_t fifo_hp;

int
main(void)
{
	void *r;
	uint64_t s, e, a;
	unsigned int i;
	unsigned int j;
	ck_hp_fifo_entry_t hp_entry[ENTRIES];
	ck_hp_fifo_entry_t hp_stub;
	ck_hp_record_t record;

	ck_hp_init(&fifo_hp, CK_HP_FIFO_SLOTS_COUNT, 1000000, NULL);

	r = malloc(CK_HP_FIFO_SLOTS_SIZE);
	if (r == NULL) {
		ck_error("ERROR: Failed to allocate slots.\n");
	}
	ck_hp_register(&fifo_hp, &record, r);

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_hp_fifo_init(&fifo, &hp_stub);

		s = rdtsc();
		for (j = 0; j < ENTRIES; j++)
			ck_hp_fifo_enqueue_mpmc(&record, &fifo, hp_entry + j, NULL);
		e = rdtsc();

		a += e - s;
	}
	printf("ck_hp_fifo_enqueue_mpmc: %16" PRIu64 "\n", a / STEPS / ENTRIES);

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_hp_fifo_init(&fifo, &hp_stub);
		for (j = 0; j < ENTRIES; j++)
			ck_hp_fifo_enqueue_mpmc(&record, &fifo, hp_entry + j, NULL);

		s = rdtsc();
		for (j = 0; j < ENTRIES; j++)
			ck_hp_fifo_dequeue_mpmc(&record, &fifo, &r);
		e = rdtsc();
		a += e - s;
	}
	printf("ck_hp_fifo_dequeue_mpmc: %16" PRIu64 "\n", a / STEPS / ENTRIES);

	return 0;
}
