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

#include <ck_pr.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../common.h"
#define REPEAT 2000000

#define TEST_BINARY(K, S, T, P, D)					\
	static void							\
	run_test_##K##_##S(void)					\
	{								\
		int i, r;						\
		T serial_result = 65535;				\
		T ck_result = 65535;					\
									\
		puts("***TESTING ck_pr_" #K "_" #S "***");		\
		common_srand((unsigned int)getpid());			\
		for (i = 0; i < REPEAT; ++i) {				\
			r = common_rand();					\
			serial_result = serial_result P r;		\
			ck_pr_##K##_##S(&ck_result, r);			\
		}							\
									\
		printf("Value of operation " #K " on 2000000 "		\
			"random numbers\n\tusing " #P ": %" #D ",\n"	\
			"\tusing ck_pr_"#K"_"#S": %" #D "\n",		\
				 serial_result, ck_result);		\
		(serial_result == ck_result) ? puts("SUCCESS.")		\
					     : puts("FAILURE.");	\
									\
		return;							\
	}								\

#define GENERATE_TEST(K, P)				\
	TEST_BINARY(K, int, int, P, d)			\
	TEST_BINARY(K, uint, unsigned int, P, u)	\
	static void					\
	run_test_##K(void)				\
	{						\
		run_test_##K##_int();			\
		run_test_##K##_uint();			\
							\
		return;					\
	}

GENERATE_TEST(add, +)
GENERATE_TEST(sub, -)
GENERATE_TEST(and, &)
GENERATE_TEST(or, |)
GENERATE_TEST(xor, ^)

#undef GENERATE_TEST
#undef TEST_BINARY

int
main(void)
{
	run_test_add();
	run_test_sub();
	run_test_and();
	run_test_or();
	run_test_xor();

	return (0);
}


