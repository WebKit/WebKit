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

#include <stdio.h>
#include <stdlib.h>

#include <ck_backoff.h>
#include "../../common.h"

int
main(void)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;
	const ck_backoff_t ceiling = CK_BACKOFF_CEILING + 1;
	unsigned int i = 0;

	fprintf(stderr, "Ceiling is: %u (%#x)\n", CK_BACKOFF_CEILING, CK_BACKOFF_CEILING);

	for (;;) {
		ck_backoff_t previous = backoff;
		ck_backoff_eb(&backoff);

		printf("EB %u\n", backoff);
		if (previous == ceiling) {
			if (backoff != ceiling)
				ck_error("[C] GB: expected %u, got %u\n", ceiling, backoff);

			if (i++ >= 1)
				break;
		} else if (previous != backoff >> 1) {
			ck_error("[N] GB: expected %u (%u), got %u\n", previous << 1, previous, backoff);
		}
	}

	return 0;
}

