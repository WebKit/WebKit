/*
 * Copyright 2012-2015 Samy Al Bahra.
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

#include <ck_ht.h>

#include <assert.h>
#include <ck_epoch.h>
#include <ck_malloc.h>
#include <ck_pr.h>
#include <ck_spinlock.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../common.h"

static ck_ht_t ht CK_CC_CACHELINE;
static uintptr_t *keys;
static size_t keys_length = 0;
static ck_epoch_t epoch_ht;
static ck_epoch_record_t epoch_wr;
static int n_threads;
static bool next_stage;

enum state {
	HT_STATE_STOP = 0,
	HT_STATE_GET,
	HT_STATE_STRICT_REPLACEMENT,
	HT_STATE_DELETION,
	HT_STATE_REPLACEMENT,
	HT_STATE_COUNT
};

static struct affinity affinerator = AFFINITY_INITIALIZER;
static uint64_t accumulator[HT_STATE_COUNT];
static ck_spinlock_t accumulator_mutex = CK_SPINLOCK_INITIALIZER;
static int barrier[HT_STATE_COUNT];
static int state;

struct ht_epoch {
	ck_epoch_entry_t epoch_entry;
};

COMMON_ALARM_DECLARE_GLOBAL(ht_alarm, alarm_event, next_stage)

static void
alarm_handler(int s)
{

	(void)s;
	next_stage = true;
	return;
}

static void
ht_destroy(ck_epoch_entry_t *e)
{

	free(e);
	return;
}

static void *
ht_malloc(size_t r)
{
	ck_epoch_entry_t *b;

	b = malloc(sizeof(*b) + r);
	return b + 1;
}

static void
ht_free(void *p, size_t b, bool r)
{
	struct ht_epoch *e = p;

	(void)b;

	if (r == true) {
		/* Destruction requires safe memory reclamation. */
		ck_epoch_call(&epoch_wr, &(--e)->epoch_entry, ht_destroy);
	} else {
		free(--e);
	}

	return;
}

static struct ck_malloc my_allocator = {
	.malloc = ht_malloc,
	.free = ht_free
};

static void
hash_function(ck_ht_hash_t *h, const void *key, size_t key_length, uint64_t seed)
{
	const uintptr_t *value = key;

	(void)key_length;
	(void)seed;
	h->value = *value;
	return;
}

static void
table_init(void)
{

	ck_epoch_init(&epoch_ht);
	ck_epoch_register(&epoch_ht, &epoch_wr, NULL);
	common_srand48((long int)time(NULL));
	if (ck_ht_init(&ht, CK_HT_MODE_DIRECT, hash_function, &my_allocator, 8, common_lrand48()) == false) {
		perror("ck_ht_init");
		exit(EXIT_FAILURE);
	}

	return;
}

static bool
table_remove(uintptr_t value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;

	ck_ht_hash_direct(&h, &ht, value);
	ck_ht_entry_key_set_direct(&entry, value);
	return ck_ht_remove_spmc(&ht, h, &entry);
}

static bool
table_replace(uintptr_t value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;

	ck_ht_hash_direct(&h, &ht, value);
	ck_ht_entry_set_direct(&entry, h, value, 6605241);
	return ck_ht_set_spmc(&ht, h, &entry);
}

static uintptr_t
table_get(uintptr_t value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;

	ck_ht_hash_direct(&h, &ht, value);
	ck_ht_entry_key_set_direct(&entry, value);
	if (ck_ht_get_spmc(&ht, h, &entry) == true)
		return ck_ht_entry_value_direct(&entry);

	return 0;
}

static bool
table_insert(uintptr_t value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;

	ck_ht_hash_direct(&h, &ht, value);
	ck_ht_entry_set_direct(&entry, h, value, value);
	return ck_ht_put_spmc(&ht, h, &entry);
}

static size_t
table_count(void)
{

	return ck_ht_count(&ht);
}

static bool
table_reset(void)
{

	return ck_ht_reset_spmc(&ht);
}

static void *
ht_reader(void *unused)
{
	size_t i;
	ck_epoch_record_t epoch_record;
	int state_previous = HT_STATE_STOP;
	int n_state;
	uint64_t s, j, a;

	(void)unused;
	if (aff_iterate(&affinerator) != 0)
		perror("WARNING: Failed to affine thread");

	s = j = a = 0;
	ck_epoch_register(&epoch_ht, &epoch_record, NULL);
	for (;;) {
		j++;
		ck_epoch_begin(&epoch_record, NULL);
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			uintptr_t r;

			r = table_get(keys[i]);
			if (r == 0)
				continue;

			if (r == 6605241)
				continue;

			if (r == keys[i])
				continue;

			ck_error("ERROR: Found invalid value: [%ju]\n",
			    (uintmax_t)r);
		}
		a += rdtsc() - s;
		ck_epoch_end(&epoch_record, NULL);

		n_state = ck_pr_load_int(&state);
		if (n_state != state_previous) {
			ck_spinlock_lock(&accumulator_mutex);
			accumulator[state_previous] += a / (j * keys_length);
			ck_spinlock_unlock(&accumulator_mutex);
			ck_pr_inc_int(&barrier[state_previous]);
			while (ck_pr_load_int(&barrier[state_previous]) != n_threads + 1)
				ck_pr_stall();

			state_previous = n_state;
			s = j = a = 0;
		}
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	size_t i, j, r;
	unsigned int d = 0;
	uint64_t s, e, a, repeated;
	pthread_t *readers;
	double p_r, p_d;

	COMMON_ALARM_DECLARE_LOCAL(ht_alarm, alarm_event)

	r = 20;
	s = 8;
	p_d = 0.5;
	p_r = 0.5;
	n_threads = CORES - 1;

	if (argc < 2) {
		fprintf(stderr, "Usage: parallel <#entries> [<interval length> <initial size> <readers>\n"
		    " <probability of replacement> <probability of deletion> <epoch threshold>]\n");
		exit(EXIT_FAILURE);
	}

	if (argc >= 3)
		r = atoi(argv[2]);

	if (argc >= 4)
		s = (uint64_t)atoi(argv[3]);

	if (argc >= 5) {
		n_threads = atoi(argv[4]);
		if (n_threads < 1) {
			ck_error("ERROR: Number of readers must be >= 1.\n");
		}
	}

	if (argc >= 6) {
		p_r = atof(argv[5]) / 100.00;
		if (p_r < 0) {
			ck_error("ERROR: Probability of replacement must be >= 0 and <= 100.\n");
		}
	}

	if (argc >= 7) {
		p_d = atof(argv[6]) / 100.00;
		if (p_d < 0) {
			ck_error("ERROR: Probability of deletion must be >= 0 and <= 100.\n");
		}
	}

	COMMON_ALARM_INIT(ht_alarm, alarm_event, r)

	affinerator.delta = 1;
	readers = malloc(sizeof(pthread_t) * n_threads);
	assert(readers != NULL);

	keys_length = (size_t)atoi(argv[1]);
	keys = malloc(sizeof(uintptr_t) * keys_length);
	assert(keys != NULL);

	table_init();

	for (i = 0; i < keys_length; i++) {
		keys[i] = (uintptr_t)common_lrand48();
		while (keys[i] == 2)
			keys[i] = (uintptr_t)common_lrand48();
	}

	for (i = 0; i < (size_t)n_threads; i++) {
		if (pthread_create(&readers[i], NULL, ht_reader, NULL) != 0) {
			ck_error("ERROR: Failed to create thread %zu.\n", i);
		}
	}

	for (i = 0; i < keys_length; i++)
		d += table_insert(keys[i]) == false;

	fprintf(stderr, " [S] %zu entries stored and %u duplicates.\n\n",
	    table_count(), d);

	fprintf(stderr, " ,- BASIC TEST\n");
	fprintf(stderr, " | Executing SMR test...");
	a = 0;
	for (j = 0; j < r; j++) {
		if (table_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			d += table_insert(keys[i]) == false;
		e = rdtsc();
		a += e - s;
	}
	fprintf(stderr, "done (%" PRIu64 " ticks)\n", a / (r * keys_length));

	fprintf(stderr, " | Executing replacement test...");
	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			table_replace(keys[i]);
		e = rdtsc();
		a += e - s;
	}
	fprintf(stderr, "done (%" PRIu64 " ticks)\n", a / (r * keys_length));

	fprintf(stderr, " | Executing get test...");
	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			if (table_get(keys[i]) == 0) {
				ck_error("ERROR: Unexpected 0 value.\n");
			}
		}
		e = rdtsc();
		a += e - s;
	}
	fprintf(stderr, "done (%" PRIu64 " ticks)\n", a / (r * keys_length));

	a = 0;
	fprintf(stderr, " | Executing removal test...");
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			table_remove(keys[i]);
		e = rdtsc();
		a += e - s;

		for (i = 0; i < keys_length; i++)
			table_insert(keys[i]);
	}
	fprintf(stderr, "done (%" PRIu64 " ticks)\n", a / (r * keys_length));

	fprintf(stderr, " | Executing negative look-up test...");
	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			table_get(2);
		}
		e = rdtsc();
		a += e - s;
	}
	fprintf(stderr, "done (%" PRIu64 " ticks)\n", a / (r * keys_length));

	ck_epoch_record_t epoch_temporary = epoch_wr;
	ck_epoch_synchronize(&epoch_wr);

	fprintf(stderr, " '- Summary: %u pending, %u peak, %u reclamations -> "
	    "%u pending, %u peak, %u reclamations\n\n",
	    epoch_temporary.n_pending, epoch_temporary.n_peak, epoch_temporary.n_dispatch,
	    epoch_wr.n_pending, epoch_wr.n_peak, epoch_wr.n_dispatch);

	fprintf(stderr, " ,- READER CONCURRENCY\n");
	fprintf(stderr, " | Executing reader test...");

	ck_pr_store_int(&state, HT_STATE_GET);
	while (ck_pr_load_int(&barrier[HT_STATE_STOP]) != n_threads)
		ck_pr_stall();
	ck_pr_inc_int(&barrier[HT_STATE_STOP]);
	common_sleep(r);
	ck_pr_store_int(&state, HT_STATE_STRICT_REPLACEMENT);
	while (ck_pr_load_int(&barrier[HT_STATE_GET]) != n_threads)
		ck_pr_stall();
	fprintf(stderr, "done (reader = %" PRIu64 " ticks)\n",
	    accumulator[HT_STATE_GET] / n_threads);

	fprintf(stderr, " | Executing strict replacement test...");

	a = repeated = 0;
	common_alarm(alarm_handler, &alarm_event, r);

	ck_pr_inc_int(&barrier[HT_STATE_GET]);
	for (;;) {
		repeated++;
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			table_replace(keys[i]);
		e = rdtsc();
		a += e - s;

		if (next_stage == true) {
			next_stage = false;
			break;
		}
	}

	ck_pr_store_int(&state, HT_STATE_DELETION);
	while (ck_pr_load_int(&barrier[HT_STATE_STRICT_REPLACEMENT]) != n_threads)
		ck_pr_stall();
	table_reset();
	ck_epoch_synchronize(&epoch_wr);
	fprintf(stderr, "done (writer = %" PRIu64 " ticks, reader = %" PRIu64 " ticks)\n",
	    a / (repeated * keys_length), accumulator[HT_STATE_STRICT_REPLACEMENT] / n_threads);

	common_alarm(alarm_handler, &alarm_event, r);

	fprintf(stderr, " | Executing deletion test (%.2f)...", p_d * 100);
	a = repeated = 0;
	ck_pr_inc_int(&barrier[HT_STATE_STRICT_REPLACEMENT]);
	for (;;) {
		double delete;

		repeated++;
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			table_insert(keys[i]);
			if (p_d != 0.0) {
				delete = common_drand48();
				if (delete <= p_d)
					table_remove(keys[i]);
			}
		}
		e = rdtsc();
		a += e - s;

		if (next_stage == true) {
			next_stage = false;
			break;
		}
	}
	ck_pr_store_int(&state, HT_STATE_REPLACEMENT);
	while (ck_pr_load_int(&barrier[HT_STATE_DELETION]) != n_threads)
		ck_pr_stall();

	table_reset();
	ck_epoch_synchronize(&epoch_wr);
	fprintf(stderr, "done (writer = %" PRIu64 " ticks, reader = %" PRIu64 " ticks)\n",
	    a / (repeated * keys_length), accumulator[HT_STATE_DELETION] / n_threads);

	common_alarm(alarm_handler, &alarm_event, r);

	fprintf(stderr, " | Executing replacement test (%.2f)...", p_r * 100);
	a = repeated = 0;
	ck_pr_inc_int(&barrier[HT_STATE_DELETION]);
	for (;;) {
		double replace, delete;

		repeated++;
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			table_insert(keys[i]);
			if (p_d != 0.0) {
				delete = common_drand48();
				if (delete <= p_d)
					table_remove(keys[i]);
			}
			if (p_r != 0.0) {
				replace = common_drand48();
				if (replace <= p_r)
					table_replace(keys[i]);
			}
		}
		e = rdtsc();
		a += e - s;

		if (next_stage == true) {
			next_stage = false;
			break;
		}
	}
	ck_pr_store_int(&state, HT_STATE_STOP);
	while (ck_pr_load_int(&barrier[HT_STATE_REPLACEMENT]) != n_threads)
		ck_pr_stall();
	table_reset();
	ck_epoch_synchronize(&epoch_wr);
	fprintf(stderr, "done (writer = %" PRIu64 " ticks, reader = %" PRIu64 " ticks)\n",
	    a / (repeated * keys_length), accumulator[HT_STATE_REPLACEMENT] / n_threads);

	ck_pr_inc_int(&barrier[HT_STATE_REPLACEMENT]);
	epoch_temporary = epoch_wr;
	ck_epoch_synchronize(&epoch_wr);

	fprintf(stderr, " '- Summary: %u pending, %u peak, %u reclamations -> "
	    "%u pending, %u peak, %u reclamations\n\n",
	    epoch_temporary.n_pending, epoch_temporary.n_peak, epoch_temporary.n_dispatch,
	    epoch_wr.n_pending, epoch_wr.n_peak, epoch_wr.n_dispatch);
	return 0;
}
