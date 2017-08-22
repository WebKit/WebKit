/*
 * Copyright 2012 Hendrik Donner
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

#include <ck_hp.h>
#include <ck_hp_fifo.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include "../../common.h"

/* FIFO queue */
static ck_hp_fifo_t fifo;

/* Hazard pointer global */
static ck_hp_t fifo_hp;

/* thread local element count */
static unsigned long *count;

static unsigned long thread_count;

static unsigned int start_barrier;
static unsigned int end_barrier;

/* destructor for FIFO queue */
static void
destructor(void *p)
{

	free(p);
	return;
}

/* entry struct for FIFO queue entries */
struct entry {
	unsigned long value;
};

/* function for thread */
static void *
queue_50_50(void *elements)
{
        struct entry *entry;
        ck_hp_fifo_entry_t *fifo_entry;
	ck_hp_record_t *record;
	void *slots;
        unsigned long j, element_count = *(unsigned long *)elements;
	unsigned int seed;

	record = malloc(sizeof(ck_hp_record_t));
	assert(record);

	slots = malloc(CK_HP_FIFO_SLOTS_SIZE);
	assert(slots);

        /* different seed for each thread */
	seed = 1337; /*(unsigned int) pthread_self(); */

        /*
         * This subscribes the thread to the fifo_hp state using the thread-owned
         * record.
         * FIFO queue needs 2 hazard pointers.
         */
        ck_hp_register(&fifo_hp, record, slots);

	/* start barrier */
	ck_pr_inc_uint(&start_barrier);
	while (ck_pr_load_uint(&start_barrier) < thread_count + 1)
		ck_pr_stall();

	/* 50/50 enqueue-dequeue */
	for(j = 0; j < element_count; j++) {
		/* rand_r with thread local state should be thread safe */
		if( 50 < (1+(int) (100.0*common_rand_r(&seed)/(RAND_MAX+1.0)))) {
			/* This is the container for the enqueued data. */
        		fifo_entry = malloc(sizeof(ck_hp_fifo_entry_t));

        		if (fifo_entry == NULL) {
        	        	exit(EXIT_FAILURE);
			}

        		/* This is the data. */
        		entry = malloc(sizeof(struct entry));
        		if (entry != NULL) {
        	        	entry->value = j;
			}

        	       /*
        	 	* Enqueue the value of the pointer entry into FIFO queue using the
        	 	* container fifo_entry.
        	 	*/
        		ck_hp_fifo_enqueue_mpmc(record, &fifo, fifo_entry, entry);
		} else {
			/*
        		 * ck_hp_fifo_dequeue_mpmc will return a pointer to the first unused node and store
        		 * the value of the first pointer in the FIFO queue in entry.
        		 */
  		      	fifo_entry = ck_hp_fifo_dequeue_mpmc(record, &fifo, &entry);
        		if (fifo_entry != NULL) {
               		 	/*
               		 	 * Safely reclaim memory associated with fifo_entry.
                		 * This inserts garbage into a local list. Once the list (plist) reaches
      			       	 * a length of 100, ck_hp_free will attempt to reclaim all references
                		 * to objects on the list.
        		       	 */
                		ck_hp_free(record, &fifo_entry->hazard, fifo_entry, fifo_entry);
        		}
		}
	}

	/* end barrier */
	ck_pr_inc_uint(&end_barrier);
	while (ck_pr_load_uint(&end_barrier) < thread_count + 1)
		ck_pr_stall();

       	return NULL;
}

int
main(int argc, char** argv)
{
        ck_hp_fifo_entry_t *stub;
        unsigned long element_count, i;
	pthread_t *thr;

        if (argc != 3) {
        	ck_error("Usage: cktest <thread_count> <element_count>\n");
        }

        /* Get element count from argument */
        element_count = atoi(argv[2]);

	/* Get element count from argument */
        thread_count = atoi(argv[1]);

	/* pthread handles */
	thr = malloc(sizeof(pthread_t) * thread_count);

	/* array for local operation count */
	count = malloc(sizeof(unsigned long *) * thread_count);

        /*
         * Initialize global hazard pointer safe memory reclamation to execute free()
         * when a fifo_entry is safe to be deleted.
         * Hazard pointer scan routine will be called when the thread local intern plist's
         * size exceed 100 entries.
         */

	/* FIFO queue needs 2 hazard pointers */
	ck_hp_init(&fifo_hp, CK_HP_FIFO_SLOTS_COUNT, 100, destructor);

        /* The FIFO requires one stub entry on initialization. */
        stub = malloc(sizeof(ck_hp_fifo_entry_t));

        /* Behavior is undefined if stub is NULL. */
        if (stub == NULL) {
                exit(EXIT_FAILURE);
	}

        /* This is called once to initialize the fifo. */
        ck_hp_fifo_init(&fifo, stub);

	/* Create threads */
	for (i = 0; i < thread_count; i++) {
		count[i] = (element_count + i) / thread_count;
		if (pthread_create(&thr[i], NULL, queue_50_50, (void *) &count[i]) != 0) {
                	exit(EXIT_FAILURE);
                }
	}

	/* start barrier */
	ck_pr_inc_uint(&start_barrier);
	while (ck_pr_load_uint(&start_barrier) < thread_count + 1);

	/* end barrier */
	ck_pr_inc_uint(&end_barrier);
	while (ck_pr_load_uint(&end_barrier) < thread_count + 1);

	/* Join threads */
	for (i = 0; i < thread_count; i++)
		pthread_join(thr[i], NULL);

        return 0;
}

