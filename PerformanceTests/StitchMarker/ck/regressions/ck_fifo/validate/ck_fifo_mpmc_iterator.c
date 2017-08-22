/*
 * Copyright 2011 David Joseph.
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

#include <ck_fifo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef CK_F_FIFO_MPMC
struct example {
	int x;
};

static ck_fifo_mpmc_t mpmc_fifo;

int
main(void)
{
	int i, length = 3;
	struct example *examples;
	ck_fifo_mpmc_entry_t *stub, *entries, *entry, *next;

	stub = malloc(sizeof(ck_fifo_mpmc_entry_t));
	if (stub == NULL)
		exit(EXIT_FAILURE);

	ck_fifo_mpmc_init(&mpmc_fifo, stub);

	entries = malloc(sizeof(ck_fifo_mpmc_entry_t) * length);
	if (entries == NULL)
		exit(EXIT_FAILURE);

	examples = malloc(sizeof(struct example) * length);
	/* Need these for this unit test. */
	if (examples == NULL)
		exit(EXIT_FAILURE);

	for (i = 0; i < length; ++i) {
		examples[i].x = i;
		ck_fifo_mpmc_enqueue(&mpmc_fifo, entries + i, examples + i);
	}

	puts("Testing CK_FIFO_MPMC_FOREACH.");
	CK_FIFO_MPMC_FOREACH(&mpmc_fifo, entry) {
		printf("Next value in fifo: %d\n", ((struct example *)entry->value)->x);
	}

	puts("Testing CK_FIFO_MPMC_FOREACH_SAFE.");
	CK_FIFO_MPMC_FOREACH_SAFE(&mpmc_fifo, entry, next) {
		if (entry->next.pointer != next)
			exit(EXIT_FAILURE);
		printf("Next value in fifo: %d\n", ((struct example *)entry->value)->x);
	}

	free(examples);
	free(entries);
	free(stub);

	return (0);
}
#else
int
main(void)
{
	return (0);
}
#endif
