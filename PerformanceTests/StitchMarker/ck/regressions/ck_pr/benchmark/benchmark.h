#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_pr.h>

#include "../../common.h"

/* 8! = 40320, evenly divide 1 .. 8 processor workload. */
#define WORKLOAD (40320 * 2056)

struct block {
	unsigned int tid;
};

static struct affinity a;
static unsigned int ready;
static uint64_t *count;
static uint64_t nthr;

static uint64_t object[2] CK_CC_CACHELINE;

static void *
fairness(void *null)
{
	struct block *context = null;
	unsigned int i = context->tid;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (ck_pr_load_uint(&ready) == 0);
	while (ck_pr_load_uint(&ready)) {
		ATOMIC;
		ATOMIC;
		ATOMIC;
		ATOMIC;
		ck_pr_store_64(count + i, count[i] + 1);
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	uint64_t v, d;
	unsigned int i;
	pthread_t *threads;
	struct block *context;

	if (argc != 3) {
		ck_error("Usage: " ATOMIC_STRING " <number of threads> <affinity delta>\n");
		exit(EXIT_FAILURE);
	}

	nthr = atoi(argv[1]);
	if (nthr <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
		exit(EXIT_FAILURE);
	}

	context = malloc(sizeof(struct block) * nthr);
	if (context == NULL) {
		ck_error("ERROR: Could not allocate thread contexts\n");
		exit(EXIT_FAILURE);
	}

	a.delta = atoi(argv[2]);
	a.request = 0;

	count = malloc(sizeof(uint64_t) * nthr);
	if (count == NULL) {
		ck_error("ERROR: Could not create acquisition buffer\n");
		exit(EXIT_FAILURE);
	}
	memset(count, 0, sizeof(uint64_t) * nthr);

	fprintf(stderr, "Creating threads (fairness)...");
	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		if (pthread_create(&threads[i], NULL, fairness, context + i)) {
			ck_error("ERROR: Could not create thread %d\n", i);
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "done\n");

	ck_pr_store_uint(&ready, 1);
	common_sleep(10);
	ck_pr_store_uint(&ready, 0);

	fprintf(stderr, "Waiting for threads to finish acquisition regression...");
	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done\n\n");

	for (i = 0, v = 0; i < nthr; i++) {
		printf("%d %15" PRIu64 "\n", i, count[i]);
		v += count[i];
	}

	printf("\n# total       : %15" PRIu64 "\n", v);
	printf("# throughput  : %15" PRIu64 " a/s\n", (v /= nthr) / 10);

	for (i = 0, d = 0; i < nthr; i++)
		d += (count[i] - v) * (count[i] - v);

	printf("# average     : %15" PRIu64 "\n", v);
	printf("# deviation   : %.2f (%.2f%%)\n\n", sqrt(d / nthr), (sqrt(d / nthr) / v) * 100.00);

	return (0);
}

