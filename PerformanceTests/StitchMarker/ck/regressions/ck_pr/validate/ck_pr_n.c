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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ck_pr.h>

#include "../../common.h"
#define REPEAT 2000000

#define TEST_N(K, S, T, P, D)					\
	static void						\
	run_test_##K##_##S(void)				\
	{							\
		int i, r;					\
		T x = 0, y = 0;					\
								\
		puts("***TESTING ck_pr_"#K"_"#S"***");		\
		common_srand((unsigned int)getpid());		\
		for (i = 0; i < REPEAT; ++i) {			\
			r = common_rand();				\
			x += r;					\
			x = P x;				\
			y += r;					\
			ck_pr_##K##_##S(&y);			\
		}						\
								\
		printf("Value of operation "#K" on 2000000 "	\
			"random numbers\n"			\
			"\tusing "#P": %"#D",\n"		\
			"\tusing ck_pr_"#K"_"#S": %"#D",\n",	\
			x, y);					\
		(x == y) ? puts("SUCCESS.")			\
			 : puts("FAILURE.");			\
								\
		return;						\
	}

#define GENERATE_TEST(K, P)			\
	TEST_N(K, int, int, P, d)		\
	TEST_N(K, uint, unsigned int, P, u)	\
	static void				\
	run_test_##K(void)			\
	{					\
		run_test_##K##_int();		\
		run_test_##K##_uint();		\
						\
		return;				\
	}

GENERATE_TEST(not, ~)
GENERATE_TEST(neg, -)

#undef GENERATE_TEST
#undef TEST_N

int
main(void)
{
	run_test_not();
	run_test_neg();

	return (0);
}


