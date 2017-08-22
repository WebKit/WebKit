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
#include <ck_hp_stack.h>
#include <ck_stack.h>
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

static ck_stack_t stack;
static ck_hp_t stack_hp;

int
main(void)
{
	ck_hp_record_t record;
	ck_stack_entry_t entry[ENTRIES];
	uint64_t s, e, a;
	unsigned int i;
	unsigned int j;
	void *r;

	ck_hp_init(&stack_hp, CK_HP_STACK_SLOTS_COUNT, 1000000, NULL);
	r = malloc(CK_HP_STACK_SLOTS_SIZE);
	if (r == NULL) {
		ck_error("ERROR: Failed to allocate slots.\n");
	}
	ck_hp_register(&stack_hp, &record, (void *)r);

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		s = rdtsc();
		for (j = 0; j < ENTRIES; j++)
			ck_hp_stack_push_mpmc(&stack, entry + j);
		e = rdtsc();

		a += e - s;
	}
	printf("ck_hp_stack_push_mpmc: %16" PRIu64 "\n", a / STEPS / ENTRIES);

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		for (j = 0; j < ENTRIES; j++)
			ck_hp_stack_push_mpmc(&stack, entry + j);

		s = rdtsc();
		for (j = 0; j < ENTRIES; j++) {
			r = ck_hp_stack_pop_mpmc(&record, &stack);
		}
		e = rdtsc();
		a += e - s;
	}
	printf(" ck_hp_stack_pop_mpmc: %16" PRIu64 "\n", a / STEPS / ENTRIES);

	return 0;
}
