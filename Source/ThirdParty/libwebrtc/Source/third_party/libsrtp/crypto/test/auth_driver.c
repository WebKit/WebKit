/*
 * auth_driver.c
 *
 * a driver for auth functions
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdio.h>    /* for printf() */
#include <stdlib.h>   /* for xalloc() */
#include <unistd.h>   /* for getopt() */

#include "auth.h"
#include "null_auth.h"

#define PRINT_DEBUG_DATA 0

extern srtp_auth_type_t tmmhv2;

const uint16_t msg0[9] = {
  0x6015, 0xf141, 0x5ba1, 0x29a0, 0xf604, 0xd1c, 0x2d9, 0xaa8a, 0x7931
};

/* key1 is for TAG_WORDS = 2 */

const uint16_t key1[47] = {
  0xe627, 0x6a01, 0x5ea7, 0xf27a, 0xc536, 0x2192, 0x11be, 0xea35,
  0xdb9d, 0x63d6, 0xfa8a, 0xfc45, 0xe08b, 0xd216, 0xced2, 0x7853,
  0x1a82, 0x22f5, 0x90fb, 0x1c29, 0x708e, 0xd06f, 0x82c3, 0xbee6,
  0x4f21, 0x6f33, 0x65c0, 0xd211, 0xc25e, 0x9138, 0x4fa3, 0x7c1f,
  0x61ac, 0x3489, 0x2976, 0x8c19, 0x8252, 0xddbf, 0xcad3, 0xc28f,
  0x68d6, 0x58dd, 0x504f, 0x2bbf, 0x0278, 0x70b7, 0xcfca
};

double
auth_bits_per_second(auth_t *h, int msg_len);


void
usage(char *prog_name) {
  printf("usage: %s [ -t | -v ]\n", prog_name);
  exit(255);
}

#define MAX_MSG_LEN 2048

int
main (int argc, char *argv[]) {
  auth_t *a = NULL;
  err_status_t status;
  int i;
  int c;
  unsigned do_timing_test = 0;
  unsigned do_validation = 0;

  /* process input arguments */
  while (1) {
    c = getopt(argc, argv, "tv");
    if (c == -1) 
      break;
    switch (c) {
    case 't':
      do_timing_test = 1;
      break;
    case 'v':
      do_validation = 1;
      break;
    default:
      usage(argv[0]);
    }    
  }
  
  printf("auth driver\nDavid A. McGrew\nCisco Systems, Inc.\n");

  if (!do_validation && !do_timing_test)
    usage(argv[0]);

  if (do_validation) {
    printf("running self-test for %s...", tmmhv2.description);
    status = tmmhv2_add_big_test();
    if (status) {
      printf("tmmhv2_add_big_test failed with error code %d\n", status);
      exit(status);
    }  
    status = auth_type_self_test(&tmmhv2);
    if (status) {
      printf("failed with error code %d\n", status);
      exit(status);
    }
    printf("passed\n");
  }

  if (do_timing_test) {

    /* tmmhv2 timing test */
    status = auth_type_alloc(&tmmhv2, &a, 94, 4);
    if (status) {
      fprintf(stderr, "can't allocate tmmhv2\n");
      exit(status);
    }
    status = auth_init(a, (uint8_t *)key1);
    if (status) {
      printf("error initializaing auth function\n");
      exit(status);
    }
    
    printf("timing %s (tag length %d)\n", 
	   tmmhv2.description, auth_get_tag_length(a));
    for (i=8; i <= MAX_MSG_LEN; i *= 2)
      printf("msg len: %d\tgigabits per second: %f\n",
	     i, auth_bits_per_second(a, i) / 1E9);

    status = auth_dealloc(a);
    if (status) {
      printf("error deallocating auth function\n");
      exit(status);
    }
    
  }

  return 0;
}

#define NUM_TRIALS 100000

#include <time.h>

double auth_bits_per_second(auth_t *a, int msg_len_octets) {
  int i;
  clock_t timer;
  uint8_t *result;
  int msg_len = (msg_len_octets + 1)/2;
  uint16_t *msg_string; 

  /* create random message */
  msg_string = (uint16_t *) srtp_crypto_alloc(msg_len_octets);
  if (msg_string == NULL)
    return 0.0; /* indicate failure */  
  for (i=0; i < msg_len; i++) 
    msg_string[i] = (uint16_t) random();

  /* allocate temporary storage for authentication tag */
  result = srtp_crypto_alloc(auth_get_tag_length(a));
  if (result == NULL) {
    srtp_crypto_free(msg_string);
    return 0.0; /* indicate failure */  
  }
  
  timer = clock();
  for (i=0; i < NUM_TRIALS; i++) {
    auth_compute(a, (uint8_t *)msg_string, msg_len_octets, (uint8_t *)result);
  }
  timer = clock() - timer;

  srtp_crypto_free(msg_string);
  srtp_crypto_free(result);
  
  return (double) NUM_TRIALS * 8 * msg_len_octets * CLOCKS_PER_SEC / timer;
}


