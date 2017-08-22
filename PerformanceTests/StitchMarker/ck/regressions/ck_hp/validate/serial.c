/*
 * Copyright 2010-2015 Samy Al Bahra.
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

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <stdbool.h>
#include <stddef.h>
#include <ck_hp.h>

#include "../../common.h"

struct entry {
	unsigned int value;
	ck_hp_hazard_t hazard;
};

static void
destructor(void *pointer)
{

	fprintf(stderr, "Free %p\n", pointer);
	free(pointer);
	return;
}

int
main(int argc, char *argv[])
{
	ck_hp_t state;
	ck_hp_record_t record[2];
	void **pointers;
	struct entry *entry, *other;

	(void)argc;
	(void)argv;

	ck_hp_init(&state, 1, 1, destructor);

	pointers = malloc(sizeof(void *));
	if (pointers == NULL) {
		ck_error("ERROR: Failed to allocate slot.\n");
	}
	ck_hp_register(&state, &record[0], pointers);
	ck_hp_reclaim(&record[0]);

	entry = malloc(sizeof *entry);
	ck_hp_set(&record[0], 0, entry);
	ck_hp_reclaim(&record[0]);
	ck_hp_free(&record[0], &entry->hazard, entry, entry);
	ck_hp_reclaim(&record[0]);
	ck_hp_set(&record[0], 0, NULL);
	ck_hp_reclaim(&record[0]);

	entry = malloc(sizeof *entry);
	ck_hp_set(&record[0], 0, entry);
	ck_hp_reclaim(&record[0]);
	ck_hp_free(&record[0], &entry->hazard, entry, entry);
	ck_hp_reclaim(&record[0]);
	ck_hp_set(&record[0], 0, NULL);
	ck_hp_reclaim(&record[0]);

	pointers = malloc(sizeof(void *));
	if (pointers == NULL) {
		ck_error("ERROR: Failed to allocate slot.\n");
	}
	ck_hp_register(&state, &record[1], pointers);
	ck_hp_reclaim(&record[1]);

	entry = malloc(sizeof *entry);
	ck_hp_set(&record[1], 0, entry);
	ck_hp_reclaim(&record[1]);
	ck_hp_free(&record[1], &entry->hazard, entry, entry);
	ck_hp_reclaim(&record[1]);
	ck_hp_set(&record[1], 0, NULL);
	ck_hp_reclaim(&record[1]);

	printf("Allocating entry and freeing in other HP record...\n");
	entry = malloc(sizeof *entry);
	entry->value = 42;
	ck_hp_set(&record[0], 0, entry);
	ck_hp_free(&record[1], &entry->hazard, entry, entry);
	ck_pr_store_uint(&entry->value, 1);

	other = malloc(sizeof *other);
	other->value = 24;
	ck_hp_set(&record[1], 0, other);
	ck_hp_free(&record[0], &other->hazard, other, other);
	ck_pr_store_uint(&other->value, 32);
	ck_hp_set(&record[0], 0, NULL);
	ck_hp_reclaim(&record[1]);
	ck_hp_set(&record[1], 0, NULL);
	ck_hp_reclaim(&record[0]);
	ck_hp_reclaim(&record[1]);

	return 0;
}
