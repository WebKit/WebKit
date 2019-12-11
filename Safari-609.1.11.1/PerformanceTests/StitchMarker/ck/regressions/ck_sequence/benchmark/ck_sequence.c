/*
 * Copyright 2013-2015 Samy Al Bahra.
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

#include <ck_cc.h>
#include <ck_sequence.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS (65536 * 64)
#endif

static ck_sequence_t seqlock CK_CC_CACHELINE = CK_SEQUENCE_INITIALIZER;

int
main(void)
{
	unsigned int i = 0;
	unsigned int version;
	uint64_t a, s;

	/* Read-side latency. */
	a = 0;
	for (i = 0; i < STEPS / 4; i++) {
		s = rdtsc();
		ck_sequence_read_retry(&seqlock, ck_sequence_read_begin(&seqlock));
		ck_sequence_read_retry(&seqlock, ck_sequence_read_begin(&seqlock));
		ck_sequence_read_retry(&seqlock, ck_sequence_read_begin(&seqlock));
		ck_sequence_read_retry(&seqlock, ck_sequence_read_begin(&seqlock));
		a += rdtsc() - s;
	}
	printf("read: %" PRIu64 "\n", a / STEPS);

	a = 0;
	for (i = 0; i < STEPS / 4; i++) {
		s = rdtsc();
		CK_SEQUENCE_READ(&seqlock, &version);
		CK_SEQUENCE_READ(&seqlock, &version);
		CK_SEQUENCE_READ(&seqlock, &version);
		CK_SEQUENCE_READ(&seqlock, &version);
		a += rdtsc() - s;
	}
	printf("READ %" PRIu64 "\n", a / STEPS);

	/* Write-side latency. */
	a = 0;
	for (i = 0; i < STEPS / 4; i++) {
		s = rdtsc();
		ck_sequence_write_begin(&seqlock);
		ck_sequence_write_end(&seqlock);
		ck_sequence_write_begin(&seqlock);
		ck_sequence_write_end(&seqlock);
		ck_sequence_write_begin(&seqlock);
		ck_sequence_write_end(&seqlock);
		ck_sequence_write_begin(&seqlock);
		ck_sequence_write_end(&seqlock);
		a += rdtsc() - s;
	}
	printf("write: %" PRIu64 "\n", a / STEPS);

        return 0;
}

