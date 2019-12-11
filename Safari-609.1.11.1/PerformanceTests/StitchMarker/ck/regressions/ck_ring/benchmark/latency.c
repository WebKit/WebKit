#include <ck_ring.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"

#ifndef ITERATIONS
#define ITERATIONS (128000)
#endif

struct entry {
	int tid;
	int value;
};

int
main(int argc, char *argv[])
{
	int i, r, size;
	uint64_t s, e, e_a, d_a;
	struct entry entry = {0, 0};
	ck_ring_buffer_t *buf;
	ck_ring_t ring;

	if (argc != 2) {
		ck_error("Usage: latency <size>\n");
	}

	size = atoi(argv[1]);
	if (size <= 4 || (size & (size - 1))) {
		ck_error("ERROR: Size must be a power of 2 greater than 4.\n");
	}

	buf = malloc(sizeof(ck_ring_buffer_t) * size);
	if (buf == NULL) {
		ck_error("ERROR: Failed to allocate buffer\n");
	}

	ck_ring_init(&ring, size);

	e_a = d_a = s = e = 0;
	for (r = 0; r < ITERATIONS; r++) {
		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_enqueue_spsc(&ring, buf, &entry);
			ck_ring_enqueue_spsc(&ring, buf, &entry);
			ck_ring_enqueue_spsc(&ring, buf, &entry);
			ck_ring_enqueue_spsc(&ring, buf, &entry);
			e = rdtsc();
		}
		e_a += (e - s) / 4;

		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_dequeue_spsc(&ring, buf, &entry);
			ck_ring_dequeue_spsc(&ring, buf, &entry);
			ck_ring_dequeue_spsc(&ring, buf, &entry);
			ck_ring_dequeue_spsc(&ring, buf, &entry);
			e = rdtsc();
		}
		d_a += (e - s) / 4;
	}

	printf("spsc %10d %16" PRIu64 " %16" PRIu64 "\n", size, e_a / ITERATIONS, d_a / ITERATIONS);

	e_a = d_a = s = e = 0;
	for (r = 0; r < ITERATIONS; r++) {
		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_enqueue_spmc(&ring, buf, &entry);
			ck_ring_enqueue_spmc(&ring, buf, &entry);
			ck_ring_enqueue_spmc(&ring, buf, &entry);
			ck_ring_enqueue_spmc(&ring, buf, &entry);
			e = rdtsc();
		}
		e_a += (e - s) / 4;

		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_dequeue_spmc(&ring, buf, &entry);
			ck_ring_dequeue_spmc(&ring, buf, &entry);
			ck_ring_dequeue_spmc(&ring, buf, &entry);
			ck_ring_dequeue_spmc(&ring, buf, &entry);
			e = rdtsc();
		}
		d_a += (e - s) / 4;
	}

	printf("spmc %10d %16" PRIu64 " %16" PRIu64 "\n", size, e_a / ITERATIONS, d_a / ITERATIONS);

	ck_ring_init(&ring, size);
	e_a = d_a = s = e = 0;
	for (r = 0; r < ITERATIONS; r++) {
		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_enqueue_mpsc(&ring, buf, &entry);
			ck_ring_enqueue_mpsc(&ring, buf, &entry);
			ck_ring_enqueue_mpsc(&ring, buf, &entry);
			ck_ring_enqueue_mpsc(&ring, buf, &entry);
			e = rdtsc();
		}
		e_a += (e - s) / 4;

		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_dequeue_mpsc(&ring, buf, &entry);
			ck_ring_dequeue_mpsc(&ring, buf, &entry);
			ck_ring_dequeue_mpsc(&ring, buf, &entry);
			ck_ring_dequeue_mpsc(&ring, buf, &entry);
			e = rdtsc();
		}
		d_a += (e - s) / 4;
	}
	printf("mpsc %10d %16" PRIu64 " %16" PRIu64 "\n", size, e_a / ITERATIONS, d_a / ITERATIONS);
	ck_ring_init(&ring, size);
	e_a = d_a = s = e = 0;
	for (r = 0; r < ITERATIONS; r++) {
		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_enqueue_mpmc(&ring, buf, &entry);
			ck_ring_enqueue_mpmc(&ring, buf, &entry);
			ck_ring_enqueue_mpmc(&ring, buf, &entry);
			ck_ring_enqueue_mpmc(&ring, buf, &entry);
			e = rdtsc();
		}
		e_a += (e - s) / 4;

		for (i = 0; i < size / 4; i += 4) {
			s = rdtsc();
			ck_ring_dequeue_mpmc(&ring, buf, &entry);
			ck_ring_dequeue_mpmc(&ring, buf, &entry);
			ck_ring_dequeue_mpmc(&ring, buf, &entry);
			ck_ring_dequeue_mpmc(&ring, buf, &entry);
			e = rdtsc();
		}
		d_a += (e - s) / 4;
	}
	printf("mpmc %10d %16" PRIu64 " %16" PRIu64 "\n", size, e_a / ITERATIONS, d_a / ITERATIONS);
	return (0);
}
