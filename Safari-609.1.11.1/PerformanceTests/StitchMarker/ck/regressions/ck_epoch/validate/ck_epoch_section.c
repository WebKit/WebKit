/*
 * Copyright 2015 John Esmet.
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

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <ck_epoch.h>

#include "../../common.h"

static ck_epoch_t epc;
static ck_epoch_record_t record, record2;
static unsigned int cleanup_calls;

static void
setup_test(void)
{

	ck_epoch_init(&epc);
	ck_epoch_register(&epc, &record, NULL);
	ck_epoch_register(&epc, &record2, NULL);
	cleanup_calls = 0;

	return;
}

static void
teardown_test(void)
{

	memset(&epc, 0, sizeof(ck_epoch_t));
	ck_epoch_unregister(&record);
	memset(&record, 0, sizeof(ck_epoch_record_t));
	memset(&record2, 0, sizeof(ck_epoch_record_t));
	cleanup_calls = 0;

	return;
}

static void
cleanup(ck_epoch_entry_t *e)
{
	(void) e;

	cleanup_calls++;

	return;
}

static void
test_simple_read_section(void)
{
	ck_epoch_entry_t entry;
	ck_epoch_section_t section;

	memset(&entry, 0, sizeof(ck_epoch_entry_t));
	setup_test();

	ck_epoch_begin(&record, &section);
	ck_epoch_call(&record, &entry, cleanup);
	assert(cleanup_calls == 0);
	if (ck_epoch_end(&record, &section) == false)
		ck_error("expected no more sections");
	ck_epoch_barrier(&record);
	assert(cleanup_calls == 1);

	teardown_test();
	return;
}

static void
test_nested_read_section(void)
{
	ck_epoch_entry_t entry1, entry2;
	ck_epoch_section_t section1, section2;

	memset(&entry1, 0, sizeof(ck_epoch_entry_t));
	memset(&entry2, 0, sizeof(ck_epoch_entry_t));
	setup_test();

	ck_epoch_begin(&record, &section1);
	ck_epoch_call(&record, &entry1, cleanup);
	assert(cleanup_calls == 0);

	ck_epoch_begin(&record, &section2);
	ck_epoch_call(&record, &entry2, cleanup);
	assert(cleanup_calls == 0);

	ck_epoch_end(&record, &section2);
	assert(cleanup_calls == 0);

	ck_epoch_end(&record, &section1);
	assert(cleanup_calls == 0);

	ck_epoch_barrier(&record);
	assert(cleanup_calls == 2);

	teardown_test();
	return;
}

struct obj {
	ck_epoch_entry_t entry;
	unsigned int destroyed;
};

static void *
barrier_work(void *arg)
{
	unsigned int *run;

	run = (unsigned int *)arg;
	while (ck_pr_load_uint(run) != 0) {
		/*
		 * Need to use record2, as record is local
		 * to the test thread.
		 */
		ck_epoch_barrier(&record2);
		usleep(5 * 1000);
	}

	return NULL;
}

static void *
reader_work(void *arg)
{
	ck_epoch_record_t local_record;
	ck_epoch_section_t section;
	struct obj *o;

	ck_epoch_register(&epc, &local_record, NULL);

	o = (struct obj *)arg;

	/*
	 * Begin a read section. The calling thread has an open read section,
	 * so the object should not be destroyed for the lifetime of this
	 * thread.
	 */
	ck_epoch_begin(&local_record, &section);
	usleep((common_rand() % 100) * 1000);
	assert(ck_pr_load_uint(&o->destroyed) == 0);
	ck_epoch_end(&local_record, &section);

	ck_epoch_unregister(&local_record);

	return NULL;
}

static void
obj_destroy(ck_epoch_entry_t *e)
{
	struct obj *o;

	o = (struct obj *)e;
	ck_pr_fas_uint(&o->destroyed, 1);

	return;
}

static void
test_single_reader_with_barrier_thread(void)
{
	const int num_sections = 10;
	struct obj o;
	unsigned int run;
	pthread_t thread;
	ck_epoch_section_t sections[num_sections];
	int shuffled[num_sections];

	run = 1;
	memset(&o, 0, sizeof(struct obj));
	common_srand(time(NULL));
	setup_test();

	if (pthread_create(&thread, NULL, barrier_work, &run) != 0) {
		abort();
	}

	/* Start a bunch of sections. */
	for (int i = 0; i < num_sections; i++) {
		ck_epoch_begin(&record, &sections[i]);
		shuffled[i] = i;
		if (i == num_sections / 2) {
			usleep(1 * 1000);
		}
	}

	/* Generate a shuffle. */
	for (int i = num_sections - 1; i >= 0; i--) {
		int k = common_rand() % (i + 1);
		int tmp = shuffled[k];
		shuffled[k] = shuffled[i];
		shuffled[i] = tmp;
	}

	ck_epoch_call(&record, &o.entry, obj_destroy);

	/* Close the sections in shuffle-order. */
	for (int i = 0; i < num_sections; i++) {
		ck_epoch_end(&record, &sections[shuffled[i]]);
		if (i != num_sections - 1) {
			assert(ck_pr_load_uint(&o.destroyed) == 0);
			usleep(3 * 1000);
		}
	}

	ck_pr_store_uint(&run, 0);
	if (pthread_join(thread, NULL) != 0) {
		abort();
	}

	ck_epoch_barrier(&record);
	assert(ck_pr_load_uint(&o.destroyed) == 1);

	teardown_test();

	return;
}

static void
test_multiple_readers_with_barrier_thread(void)
{
	const int num_readers = 10;
	struct obj o;
	unsigned int run;
	ck_epoch_section_t section;
	pthread_t threads[num_readers + 1];

	run = 1;
	memset(&o, 0, sizeof(struct obj));
	memset(&section, 0, sizeof(ck_epoch_section_t));
	common_srand(time(NULL));
	setup_test();

	/* Create a thread to call barrier() while we create reader threads.
	 * Each barrier will attempt to move the global epoch forward so
	 * it will make the read section code coverage more interesting. */
	if (pthread_create(&threads[num_readers], NULL,
	    barrier_work, &run) != 0) {
		abort();
	}

	ck_epoch_begin(&record, &section);
	ck_epoch_call(&record, &o.entry, obj_destroy);

	for (int i = 0; i < num_readers; i++) {
		if (pthread_create(&threads[i], NULL, reader_work, &o) != 0) {
			abort();
		}
	}

	ck_epoch_end(&record, &section);

	ck_pr_store_uint(&run, 0);
	if (pthread_join(threads[num_readers], NULL) != 0) {
		abort();
	}

	/* After the barrier, the object should be destroyed and readers
	 * should return. */
	for (int i = 0; i < num_readers; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			abort();
		}
	}

	teardown_test();
	return;
}

int
main(void)
{

	test_simple_read_section();
	test_nested_read_section();
	test_single_reader_with_barrier_thread();
	test_multiple_readers_with_barrier_thread();

	return 0;
}
