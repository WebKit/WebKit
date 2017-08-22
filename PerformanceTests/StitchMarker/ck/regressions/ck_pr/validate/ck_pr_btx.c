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
#include <stdbool.h>
#include <ck_pr.h>

#include "../../common.h"
#define REPEAT 2000000

#define TEST_BTX(K, S, M, T, L, P, D, R)						\
	static bool									\
	test_##K##_##S(M *target, int offset)						\
	{										\
		T previous;								\
		const L change = R (0x01 << offset);					\
											\
		previous = (T)*target;							\
		*target = previous P change;						\
		return ((previous >> offset) & 0x01);					\
	}										\
	static void									\
	run_test_##K##_##S(void)							\
	{										\
		int i, offset, m;							\
		bool serial_t, ck_pr_t;							\
		T x = 65535, y = 65535;							\
											\
		common_srand((unsigned int)getpid());					\
		m = sizeof(T) * 8;							\
											\
		puts("***TESTING ck_pr_"#K"_"#S"***");					\
		for (i = 0; i < REPEAT; ++i) {						\
			offset = common_rand() % m;						\
			serial_t = test_##K##_##S(&x, offset);				\
			ck_pr_t = ck_pr_##K##_##S(&y, offset);				\
											\
			if (serial_t != ck_pr_t || x != y ) {				\
				printf("Serial(%"#D") and ck_pr(%"#D")"			\
					#K"_"#S " do not match.\n"			\
					"FAILURE.\n", 					\
					serial_t, ck_pr_t);				\
											\
				return;							\
			}								\
		}									\
		printf("\tserial_"#K"_"#S": %"#D"\n"					\
			"\tck_pr_"#K"_"#S": %"#D"\n"					\
			"SUCCESS.\n",							\
			x, y);								\
											\
		return;									\
	}

#define TEST_BTX_S(K, S, T, P, D, R) TEST_BTX(K, S, T, T, T, P, D, R)

#define GENERATE_TEST(K, P, R)				\
	TEST_BTX_S(K, int, int, P, d, R)		\
	TEST_BTX_S(K, uint, unsigned int, P, u, R)	\
	static void					\
	run_test_##K(void)				\
	{						\
		run_test_##K##_int();			\
		run_test_##K##_uint();			\
							\
		return;					\
	}

GENERATE_TEST(btc, ^, 0+)
GENERATE_TEST(btr, &, ~)
GENERATE_TEST(bts, |, 0+)

#undef GENERATE_TEST
#undef TEST_BTX_S
#undef TEST_BTX

int
main(void)
{
	run_test_btc();
	run_test_btr();
	run_test_bts();

	return (0);
}


