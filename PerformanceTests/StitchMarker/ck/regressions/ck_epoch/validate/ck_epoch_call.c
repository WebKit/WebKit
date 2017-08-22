/*
 * Copyright 2014 Samy Al Bahra.
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

#include <stdio.h>
#include <ck_epoch.h>

#include "../../common.h"

static ck_epoch_t epoch;
static unsigned int counter;
static ck_epoch_record_t record[2];

static void
cb(ck_epoch_entry_t *p)
{

	/* Test that we can reregister the callback. */
	if (counter == 0)
		ck_epoch_call(&record[1], p, cb);

	printf("Counter value: %u -> %u\n",
	    counter, counter + 1);
	counter++;
	return;
}

int
main(void)
{
	ck_epoch_entry_t entry;
	ck_epoch_entry_t another;

	ck_epoch_register(&epoch, &record[0], NULL);
	ck_epoch_register(&epoch, &record[1], NULL);

	ck_epoch_call(&record[1], &entry, cb);
	ck_epoch_barrier(&record[1]);
	ck_epoch_barrier(&record[1]);

	/* Make sure that strict works. */
	ck_epoch_call_strict(&record[1], &entry, cb);
	ck_epoch_call_strict(&record[1], &another, cb);
	ck_epoch_barrier(&record[1]);

	if (counter != 4)
		ck_error("Expected counter value 4, read %u.\n", counter);

	return 0;
}
