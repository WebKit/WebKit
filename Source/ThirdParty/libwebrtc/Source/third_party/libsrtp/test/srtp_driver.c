/*
 * srtp_driver.c
 *
 * a test driver for libSRTP
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


#include <string.h>   /* for memcpy()          */
#include <time.h>     /* for clock()           */
#include <stdlib.h>   /* for malloc(), free()  */
#include <stdio.h>    /* for print(), fflush() */
#include "getopt_s.h" /* for local getopt()    */
#include "util.h"

#include "srtp_priv.h"

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#elif defined HAVE_WINSOCK2_H
# include <winsock2.h>
#endif

#define PRINT_REFERENCE_PACKET 1

srtp_err_status_t
srtp_validate(void);

#ifdef OPENSSL
srtp_err_status_t
srtp_validate_gcm(void);
#endif

srtp_err_status_t
srtp_validate_encrypted_extensions_headers(void);

#ifdef OPENSSL
srtp_err_status_t
srtp_validate_encrypted_extensions_headers_gcm(void);
#endif

srtp_err_status_t
srtp_validate_aes_256(void);

srtp_err_status_t
srtp_create_big_policy(srtp_policy_t **list);

srtp_err_status_t
srtp_dealloc_big_policy(srtp_policy_t *list);

srtp_err_status_t
srtp_test_empty_payload(void);

#ifdef OPENSSL
srtp_err_status_t
srtp_test_empty_payload_gcm(void);
#endif

srtp_err_status_t
srtp_test_remove_stream(void);

srtp_err_status_t
srtp_test_update(void);

double
srtp_bits_per_second(int msg_len_octets, const srtp_policy_t *policy);

double
srtp_rejections_per_second(int msg_len_octets, const srtp_policy_t *policy);

void
srtp_do_timing(const srtp_policy_t *policy);

void
srtp_do_rejection_timing(const srtp_policy_t *policy);

srtp_err_status_t
srtp_test(const srtp_policy_t *policy, int extension_header);

srtp_err_status_t
srtcp_test(const srtp_policy_t *policy);

srtp_err_status_t
srtp_session_print_policy(srtp_t srtp);

srtp_err_status_t
srtp_print_policy(const srtp_policy_t *policy);

char *
srtp_packet_to_string(srtp_hdr_t *hdr, int packet_len);

double
mips_estimate(int num_trials, int *ignore);

extern uint8_t test_key[46];

void
usage (char *prog_name)
{
    printf("usage: %s [ -t ][ -c ][ -v ][-d <debug_module> ]* [ -l ]\n"
           "  -t         run timing test\n"
           "  -r         run rejection timing test\n"
           "  -c         run codec timing test\n"
           "  -v         run validation tests\n"
           "  -d <mod>   turn on debugging module <mod>\n"
           "  -l         list debugging modules\n", prog_name);
    exit(1);
}

/*
 * The policy_array is a null-terminated array of policy structs. it
 * is declared at the end of this file
 */

extern const srtp_policy_t *policy_array[];


/* the wildcard_policy is declared below; it has a wildcard ssrc */

extern const srtp_policy_t wildcard_policy;

/*
 * mod_driver debug module - debugging module for this test driver
 *
 * we use the crypto_kernel debugging system in this driver, which
 * makes the interface uniform and increases portability
 */

srtp_debug_module_t mod_driver = {
    0,                /* debugging is off by default */
    "driver"          /* printable name for module   */
};

int
main (int argc, char *argv[])
{
    int q;
    unsigned do_timing_test    = 0;
    unsigned do_rejection_test = 0;
    unsigned do_codec_timing   = 0;
    unsigned do_validation     = 0;
    unsigned do_list_mods      = 0;
    srtp_err_status_t status;

    /*
     * verify that the compiler has interpreted the header data
     * structure srtp_hdr_t correctly
     */
    if (sizeof(srtp_hdr_t) != 12) {
        printf("error: srtp_hdr_t has incorrect size"
               "(size is %ld bytes, expected 12)\n",
               (long)sizeof(srtp_hdr_t));
        exit(1);
    }

    /* initialize srtp library */
    status = srtp_init();
    if (status) {
        printf("error: srtp init failed with error code %d\n", status);
        exit(1);
    }

    /*  load srtp_driver debug module */
    status = srtp_crypto_kernel_load_debug_module(&mod_driver);
    if (status) {
        printf("error: load of srtp_driver debug module failed "
               "with error code %d\n", status);
        exit(1);
    }

    /* process input arguments */
    while (1) {
        q = getopt_s(argc, argv, "trcvld:");
        if (q == -1) {
            break;
        }
        switch (q) {
        case 't':
            do_timing_test = 1;
            break;
        case 'r':
            do_rejection_test = 1;
            break;
        case 'c':
            do_codec_timing = 1;
            break;
        case 'v':
            do_validation = 1;
            break;
        case 'l':
            do_list_mods = 1;
            break;
        case 'd':
            status = srtp_crypto_kernel_set_debug_module(optarg_s, 1);
            if (status) {
                printf("error: set debug module (%s) failed\n", optarg_s);
                exit(1);
            }
            break;
        default:
            usage(argv[0]);
        }
    }

    if (!do_validation && !do_timing_test && !do_codec_timing
        && !do_list_mods && !do_rejection_test) {
        usage(argv[0]);
    }

    if (do_list_mods) {
        status = srtp_crypto_kernel_list_debug_modules();
        if (status) {
            printf("error: list of debug modules failed\n");
            exit(1);
        }
    }

    if (do_validation) {
        const srtp_policy_t **policy = policy_array;
        srtp_policy_t *big_policy;

        /* loop over policy array, testing srtp and srtcp for each policy */
        while (*policy != NULL) {
            printf("testing srtp_protect and srtp_unprotect\n");
            if (srtp_test(*policy, 0) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else{
                printf("failed\n");
                exit(1);
            }
            printf("testing srtp_protect and srtp_unprotect with encrypted extensions headers\n");
            if (srtp_test(*policy, 1) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else{
                printf("failed\n");
                exit(1);
            }
            printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp\n");
            if (srtcp_test(*policy) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else{
                printf("failed\n");
                exit(1);
            }
            policy++;
        }

        /* create a big policy list and run tests on it */
        status = srtp_create_big_policy(&big_policy);
        if (status) {
            printf("unexpected failure with error code %d\n", status);
            exit(1);
        }
        printf("testing srtp_protect and srtp_unprotect with big policy\n");
        if (srtp_test(big_policy, 0) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }
        printf("testing srtp_protect and srtp_unprotect with big policy and encrypted extensions headers\n");
        if (srtp_test(big_policy, 1) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }
        status = srtp_dealloc_big_policy(big_policy);
        if (status) {
            printf("unexpected failure with error code %d\n", status);
            exit(1);
        }

        /* run test on wildcard policy */
        printf("testing srtp_protect and srtp_unprotect on "
               "wildcard ssrc policy\n");
        if (srtp_test(&wildcard_policy, 0) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }
        printf("testing srtp_protect and srtp_unprotect on "
               "wildcard ssrc policy and encrypted extensions headers\n");
        if (srtp_test(&wildcard_policy, 1) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }

        /*
         * run validation test against the reference packets - note
         * that this test only covers the default policy
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet\n");
        if (srtp_validate() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }

#ifdef OPENSSL
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet using GCM\n");
        if (srtp_validate_gcm() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }
#endif

        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet with encrypted extensions headers\n");
        if (srtp_validate_encrypted_extensions_headers() == srtp_err_status_ok)
            printf("passed\n\n");
        else {
            printf("failed\n");
            exit(1);
        }

#ifdef OPENSSL
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet with encrypted extension headers (GCM)\n");
        if (srtp_validate_encrypted_extensions_headers_gcm() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }
#endif

        /*
         * run validation test against the reference packets for
         * AES-256
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet (AES-256)\n");
        if (srtp_validate_aes_256() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else{
            printf("failed\n");
            exit(1);
        }

        /*
         * test packets with empty payload
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "packet with empty payload\n");
        if (srtp_test_empty_payload() == srtp_err_status_ok) {
            printf("passed\n");
        } else{
            printf("failed\n");
            exit(1);
        }
#ifdef OPENSSL
        printf("testing srtp_protect and srtp_unprotect against "
               "packet with empty payload (GCM)\n");
        if (srtp_test_empty_payload_gcm() == srtp_err_status_ok) {
            printf("passed\n");
        } else{
            printf("failed\n");
            exit(1);
        }
#endif

        /*
         * test the function srtp_remove_stream()
         */
        printf("testing srtp_remove_stream()...");
        if (srtp_test_remove_stream() == srtp_err_status_ok) {
            printf("passed\n");
        } else{
            printf("failed\n");
            exit(1);
        }

        /*
         * test the function srtp_update()
         */
        printf("testing srtp_update()...");
        if (srtp_test_update() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }
    }

    if (do_timing_test) {
        const srtp_policy_t **policy = policy_array;

        /* loop over policies, run timing test for each */
        while (*policy != NULL) {
            srtp_print_policy(*policy);
            srtp_do_timing(*policy);
            policy++;
        }
    }

    if (do_rejection_test) {
        const srtp_policy_t **policy = policy_array;

        /* loop over policies, run rejection timing test for each */
        while (*policy != NULL) {
            srtp_print_policy(*policy);
            srtp_do_rejection_timing(*policy);
            policy++;
        }
    }

    if (do_codec_timing) {
        srtp_policy_t policy;
        int ignore;
        double mips_value = mips_estimate(1000000000, &ignore);

        memset(&policy, 0, sizeof(policy));
        srtp_crypto_policy_set_rtp_default(&policy.rtp);
        srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
        policy.ssrc.type  = ssrc_specific;
        policy.ssrc.value = 0xdecafbad;
        policy.key  = test_key;
        policy.ekt = NULL;
        policy.window_size = 128;
        policy.allow_repeat_tx = 0;
        policy.next = NULL;

        printf("mips estimate: %e\n", mips_value);

        printf("testing srtp processing time for voice codecs:\n");
        printf("codec\t\tlength (octets)\t\tsrtp instructions/second\n");
        printf("G.711\t\t%d\t\t\t%e\n", 80,
               (double)mips_value * (80 * 8) /
               srtp_bits_per_second(80, &policy) / .01 );
        printf("G.711\t\t%d\t\t\t%e\n", 160,
               (double)mips_value * (160 * 8) /
               srtp_bits_per_second(160, &policy) / .02);
        printf("G.726-32\t%d\t\t\t%e\n", 40,
               (double)mips_value * (40 * 8) /
               srtp_bits_per_second(40, &policy) / .01 );
        printf("G.726-32\t%d\t\t\t%e\n", 80,
               (double)mips_value * (80 * 8) /
               srtp_bits_per_second(80, &policy) / .02);
        printf("G.729\t\t%d\t\t\t%e\n", 10,
               (double)mips_value * (10 * 8) /
               srtp_bits_per_second(10, &policy) / .01 );
        printf("G.729\t\t%d\t\t\t%e\n", 20,
               (double)mips_value * (20 * 8) /
               srtp_bits_per_second(20, &policy) / .02 );
        printf("Wideband\t%d\t\t\t%e\n", 320,
               (double)mips_value * (320 * 8) /
               srtp_bits_per_second(320, &policy) / .01 );
        printf("Wideband\t%d\t\t\t%e\n", 640,
               (double)mips_value * (640 * 8) /
               srtp_bits_per_second(640, &policy) / .02 );
    }

    status = srtp_shutdown();
    if (status) {
        printf("error: srtp shutdown failed with error code %d\n", status);
        exit(1);
    }

    return 0;
}



/*
 * srtp_create_test_packet(len, ssrc) returns a pointer to a
 * (malloced) example RTP packet whose data field has the length given
 * by pkt_octet_len and the SSRC value ssrc.  The total length of the
 * packet is twelve octets longer, since the header is at the
 * beginning.  There is room at the end of the packet for a trailer,
 * and the four octets following the packet are filled with 0xff
 * values to enable testing for overwrites.
 *
 * note that the location of the test packet can (and should) be
 * deallocated with the free() call once it is no longer needed.
 */

srtp_hdr_t *
srtp_create_test_packet (int pkt_octet_len, uint32_t ssrc)
{
    int i;
    uint8_t *buffer;
    srtp_hdr_t *hdr;
    int bytes_in_hdr = 12;

    /* allocate memory for test packet */
    hdr = (srtp_hdr_t*)malloc(pkt_octet_len + bytes_in_hdr
                              + SRTP_MAX_TRAILER_LEN + 4);
    if (!hdr) {
        return NULL;
    }

    hdr->version = 2;              /* RTP version two     */
    hdr->p    = 0;                 /* no padding needed   */
    hdr->x    = 0;                 /* no header extension */
    hdr->cc   = 0;                 /* no CSRCs            */
    hdr->m    = 0;                 /* marker bit          */
    hdr->pt   = 0xf;               /* payload type        */
    hdr->seq  = htons(0x1234);     /* sequence number     */
    hdr->ts   = htonl(0xdecafbad); /* timestamp           */
    hdr->ssrc = htonl(ssrc);       /* synch. source       */

    buffer = (uint8_t*)hdr;
    buffer += bytes_in_hdr;

    /* set RTP data to 0xab */
    for (i = 0; i < pkt_octet_len; i++) {
        *buffer++ = 0xab;
    }

    /* set post-data value to 0xffff to enable overrun checking */
    for (i = 0; i < SRTP_MAX_TRAILER_LEN + 4; i++) {
        *buffer++ = 0xff;
    }

    return hdr;
}

srtp_hdr_t *
srtp_create_test_packet_ext_hdr(int pkt_octet_len, uint32_t ssrc) {
  int i;
  uint8_t *buffer;
  srtp_hdr_t *hdr;
  int bytes_in_hdr = 12;
  uint8_t extension_header[12] = {
    /* one-byte header */
    0xbe, 0xde,
    /* size */
    0x00, 0x02,
    /* id 1, length 1 (i.e. 2 bytes) */
    0x11,
    /* payload */
    0xca,
    0xfe,
    /* padding */
    0x00,
    /* id 2, length 0 (i.e. 1 byte) */
    0x20,
    /* payload */
    0xba,
    /* padding */
    0x00,
    0x00
  };

  /* allocate memory for test packet */
  hdr = (srtp_hdr_t*) malloc(pkt_octet_len + bytes_in_hdr
           + sizeof(extension_header) + SRTP_MAX_TRAILER_LEN + 4);
  if (!hdr)
    return NULL;

  hdr->version = 2;              /* RTP version two     */
  hdr->p    = 0;                 /* no padding needed   */
  hdr->x    = 1;                 /* no header extension */
  hdr->cc   = 0;                 /* no CSRCs            */
  hdr->m    = 0;                 /* marker bit          */
  hdr->pt   = 0xf;               /* payload type        */
  hdr->seq  = htons(0x1234);     /* sequence number     */
  hdr->ts   = htonl(0xdecafbad); /* timestamp           */
  hdr->ssrc = htonl(ssrc);       /* synch. source       */

  buffer = (uint8_t *)hdr;
  buffer += bytes_in_hdr;

  memcpy(buffer, extension_header, sizeof(extension_header));
  buffer += sizeof(extension_header);

  /* set RTP data to 0xab */
  for (i=0; i < pkt_octet_len; i++)
    *buffer++ = 0xab;

  /* set post-data value to 0xffff to enable overrun checking */
  for (i=0; i < SRTP_MAX_TRAILER_LEN+4; i++)
    *buffer++ = 0xff;

  return hdr;
}

void
srtp_do_timing (const srtp_policy_t *policy)
{
    int len;

    /*
     * note: the output of this function is formatted so that it
     * can be used in gnuplot.  '#' indicates a comment, and "\r\n"
     * terminates a record
     */

    printf("# testing srtp throughput:\r\n");
    printf("# mesg length (octets)\tthroughput (megabits per second)\r\n");

    for (len = 16; len <= 2048; len *= 2) {
        printf("%d\t\t\t%f\r\n", len,
               srtp_bits_per_second(len, policy) / 1.0E6);
    }

    /* these extra linefeeds let gnuplot know that a dataset is done */
    printf("\r\n\r\n");

}

void
srtp_do_rejection_timing (const srtp_policy_t *policy)
{
    int len;

    /*
     * note: the output of this function is formatted so that it
     * can be used in gnuplot.  '#' indicates a comment, and "\r\n"
     * terminates a record
     */

    printf("# testing srtp rejection throughput:\r\n");
    printf("# mesg length (octets)\trejections per second\r\n");

    for (len = 8; len <= 2048; len *= 2) {
        printf("%d\t\t\t%e\r\n", len, srtp_rejections_per_second(len, policy));
    }

    /* these extra linefeeds let gnuplot know that a dataset is done */
    printf("\r\n\r\n");

}


#define MAX_MSG_LEN 1024

double
srtp_bits_per_second (int msg_len_octets, const srtp_policy_t *policy)
{
    srtp_t srtp;
    srtp_hdr_t *mesg;
    int i;
    clock_t timer;
    int num_trials = 100000;
    int len;
    uint32_t ssrc;
    srtp_err_status_t status;

    /*
     * allocate and initialize an srtp session
     */
    status = srtp_create(&srtp, policy);
    if (status) {
        printf("error: srtp_create() failed with error code %d\n", status);
        exit(1);
    }

    /*
     * if the ssrc is unspecified, use a predetermined one
     */
    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdeadbeef;
    } else {
        ssrc = policy->ssrc.value;
    }

    /*
     * create a test packet
     */
    mesg = srtp_create_test_packet(msg_len_octets, ssrc);
    if (mesg == NULL) {
        return 0.0; /* indicate failure by returning zero */

    }
    timer = clock();
    for (i = 0; i < num_trials; i++) {
        len = msg_len_octets + 12; /* add in rtp header length */

        /* srtp protect message */
        status = srtp_protect(srtp, mesg, &len);
        if (status) {
            printf("error: srtp_protect() failed with error code %d\n", status);
            exit(1);
        }

        /* increment message number */
        {
            /* hack sequence to avoid problems with macros for htons/ntohs on some systems */
            short new_seq = ntohs(mesg->seq) + 1;
            mesg->seq = htons(new_seq);
        }
    }
    timer = clock() - timer;

    free(mesg);

    status = srtp_dealloc(srtp);
    if (status) {
        printf("error: srtp_dealloc() failed with error code %d\n", status);
        exit(1);
    }

    return (double)(msg_len_octets) * 8 *
           num_trials * CLOCKS_PER_SEC / timer;
}

double
srtp_rejections_per_second (int msg_len_octets, const srtp_policy_t *policy)
{
    srtp_ctx_t *srtp;
    srtp_hdr_t *mesg;
    int i;
    int len;
    clock_t timer;
    int num_trials = 1000000;
    uint32_t ssrc = policy->ssrc.value;
    srtp_err_status_t status;

    /*
     * allocate and initialize an srtp session
     */
    status = srtp_create(&srtp, policy);
    if (status) {
        printf("error: srtp_create() failed with error code %d\n", status);
        exit(1);
    }

    mesg = srtp_create_test_packet(msg_len_octets, ssrc);
    if (mesg == NULL) {
        return 0.0; /* indicate failure by returning zero */

    }
    len = msg_len_octets;
    srtp_protect(srtp, (srtp_hdr_t*)mesg, &len);

    timer = clock();
    for (i = 0; i < num_trials; i++) {
        len = msg_len_octets;
        srtp_unprotect(srtp, (srtp_hdr_t*)mesg, &len);
    }
    timer = clock() - timer;

    free(mesg);

    status = srtp_dealloc(srtp);
    if (status) {
        printf("error: srtp_dealloc() failed with error code %d\n", status);
        exit(1);
    }

    return (double)num_trials * CLOCKS_PER_SEC / timer;
}


void
err_check (srtp_err_status_t s)
{
    if (s == srtp_err_status_ok) {
        return;
    } else{
        fprintf(stderr, "error: unexpected srtp failure (code %d)\n", s);
    }
    exit(1);
}

srtp_err_status_t
srtp_test (const srtp_policy_t *policy, int extension_header)
{
    int i;
    srtp_t srtp_sender;
    srtp_t srtp_rcvr;
    srtp_err_status_t status = srtp_err_status_ok;
    srtp_hdr_t *hdr, *hdr2;
    uint8_t hdr_enc[64];
    uint8_t *pkt_end;
    int msg_len_octets, msg_len_enc;
    int len;
    int tag_length = policy->rtp.auth_tag_len;
    uint32_t ssrc;
    srtp_policy_t *rcvr_policy;
    srtp_policy_t tmp_policy;
    int header = 1;

    if (extension_header) {
        memcpy(&tmp_policy, policy, sizeof(srtp_policy_t));
        tmp_policy.enc_xtn_hdr = &header;
        tmp_policy.enc_xtn_hdr_count = 1;
        err_check(srtp_create(&srtp_sender, &tmp_policy));
    } else {
        err_check(srtp_create(&srtp_sender, policy));
    }

    /* print out policy */
    err_check(srtp_session_print_policy(srtp_sender));

    /*
     * initialize data buffer, using the ssrc in the policy unless that
     * value is a wildcard, in which case we'll just use an arbitrary
     * one
     */
    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdecafbad;
    } else{
        ssrc = policy->ssrc.value;
    }
    msg_len_octets = 28;
    if (extension_header) {
        hdr = srtp_create_test_packet_ext_hdr(msg_len_octets, ssrc);
        hdr2 = srtp_create_test_packet_ext_hdr(msg_len_octets, ssrc);
    } else {
        hdr = srtp_create_test_packet(msg_len_octets, ssrc);
        hdr2 = srtp_create_test_packet(msg_len_octets, ssrc);
    }

    if (hdr == NULL) {
        free(hdr2);
        return srtp_err_status_alloc_fail;
    }
    if (hdr2 == NULL) {
        free(hdr);
        return srtp_err_status_alloc_fail;
    }

    /* set message length */
    len = msg_len_octets;
    if (extension_header) {
        len += 12;
    }

    debug_print(mod_driver, "before protection:\n%s",
                srtp_packet_to_string(hdr, len));

#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "reference packet before protection:\n%s",
                octet_string_hex_string((uint8_t*)hdr, len));
#endif
    err_check(srtp_protect(srtp_sender, hdr, &len));

    debug_print(mod_driver, "after protection:\n%s",
                srtp_packet_to_string(hdr, len));
#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "after protection:\n%s",
                octet_string_hex_string((uint8_t*)hdr, len));
#endif

    /* save protected message and length */
    memcpy(hdr_enc, hdr, len);
    msg_len_enc = len;

    /*
     * check for overrun of the srtp_protect() function
     *
     * The packet is followed by a value of 0xfffff; if the value of the
     * data following the packet is different, then we know that the
     * protect function is overwriting the end of the packet.
     */
    pkt_end = (uint8_t*)hdr + sizeof(srtp_hdr_t)
              + msg_len_octets + tag_length;
    if (extension_header) {
        pkt_end += 12;
    }
    for (i = 0; i < 4; i++) {
        if (pkt_end[i] != 0xff) {
            fprintf(stdout, "overwrite in srtp_protect() function "
                    "(expected %x, found %x in trailing octet %d)\n",
                    0xff, ((uint8_t*)hdr)[i], i);
            free(hdr);
            free(hdr2);
            return srtp_err_status_algo_fail;
        }
    }

    /*
     * if the policy includes confidentiality, check that ciphertext is
     * different than plaintext
     *
     * Note that this check will give false negatives, with some small
     * probability, especially if the packets are short.  For that
     * reason, we skip this check if the plaintext is less than four
     * octets long.
     */
    if ((policy->rtp.sec_serv & sec_serv_conf) && (msg_len_octets >= 4)) {
        printf("testing that ciphertext is distinct from plaintext...");
        status = srtp_err_status_algo_fail;
        for (i = 12; i < msg_len_octets + 12; i++) {
            if (((uint8_t*)hdr)[i] != ((uint8_t*)hdr2)[i]) {
                status = srtp_err_status_ok;
            }
        }
        if (status) {
            printf("failed\n");
            free(hdr);
            free(hdr2);
            return status;
        }
        printf("passed\n");
    }

    /*
     * if the policy uses a 'wildcard' ssrc, then we need to make a copy
     * of the policy that changes the direction to inbound
     *
     * we always copy the policy into the rcvr_policy, since otherwise
     * the compiler would fret about the constness of the policy
     */
    rcvr_policy = (srtp_policy_t*)malloc(sizeof(srtp_policy_t));
    if (rcvr_policy == NULL) {
        free(hdr);
        free(hdr2);
        return srtp_err_status_alloc_fail;
    }
    if (extension_header) {
        memcpy(rcvr_policy, &tmp_policy, sizeof(srtp_policy_t));
        if (tmp_policy.ssrc.type == ssrc_any_outbound) {
            rcvr_policy->ssrc.type = ssrc_any_inbound;
        }
    } else {
        memcpy(rcvr_policy, policy, sizeof(srtp_policy_t));
        if (policy->ssrc.type == ssrc_any_outbound) {
            rcvr_policy->ssrc.type = ssrc_any_inbound;
        }
    }

    err_check(srtp_create(&srtp_rcvr, rcvr_policy));

    err_check(srtp_unprotect(srtp_rcvr, hdr, &len));

    debug_print(mod_driver, "after unprotection:\n%s",
                srtp_packet_to_string(hdr, len));

    /* verify that the unprotected packet matches the origial one */
    for (i = 0; i < msg_len_octets; i++) {
        if (((uint8_t*)hdr)[i] != ((uint8_t*)hdr2)[i]) {
            fprintf(stdout, "mismatch at octet %d\n", i);
            status = srtp_err_status_algo_fail;
        }
    }
    if (status) {
        free(hdr);
        free(hdr2);
        free(rcvr_policy);
        return status;
    }

    /*
     * if the policy includes authentication, then test for false positives
     */
    if (policy->rtp.sec_serv & sec_serv_auth) {
        char *data = ((char*)hdr) + 12;

        printf("testing for false positives in replay check...");

        /* set message length */
        len = msg_len_enc;

        /* unprotect a second time - should fail with a replay error */
        status = srtp_unprotect(srtp_rcvr, hdr_enc, &len);
        if (status != srtp_err_status_replay_fail) {
            printf("failed with error code %d\n", status);
            free(hdr);
            free(hdr2);
            free(rcvr_policy);
            return status;
        } else {
            printf("passed\n");
        }

        printf("testing for false positives in auth check...");

        /* increment sequence number in header */
        hdr->seq++;

        /* set message length */
        len = msg_len_octets;
        if (extension_header) {
            len += 12;
        }

        /* apply protection */
        err_check(srtp_protect(srtp_sender, hdr, &len));

        /* flip bits in packet */
        data[extension_header ? 12 : 0] ^= 0xff;

        /* unprotect, and check for authentication failure */
        status = srtp_unprotect(srtp_rcvr, hdr, &len);
        if (status != srtp_err_status_auth_fail) {
            printf("failed\n");
            free(hdr);
            free(hdr2);
            free(rcvr_policy);
            return status;
        } else {
            printf("passed\n");
        }

    }

    err_check(srtp_dealloc(srtp_sender));
    err_check(srtp_dealloc(srtp_rcvr));

    free(hdr);
    free(hdr2);
    free(rcvr_policy);
    return srtp_err_status_ok;
}


srtp_err_status_t
srtcp_test (const srtp_policy_t *policy)
{
    int i;
    srtp_t srtcp_sender;
    srtp_t srtcp_rcvr;
    srtp_err_status_t status = srtp_err_status_ok;
    srtp_hdr_t *hdr, *hdr2;
    uint8_t hdr_enc[64];
    uint8_t *pkt_end;
    int msg_len_octets, msg_len_enc;
    int len;
    int tag_length = policy->rtp.auth_tag_len;
    uint32_t ssrc;
    srtp_policy_t *rcvr_policy;

    err_check(srtp_create(&srtcp_sender, policy));

    /* print out policy */
    err_check(srtp_session_print_policy(srtcp_sender));

    /*
     * initialize data buffer, using the ssrc in the policy unless that
     * value is a wildcard, in which case we'll just use an arbitrary
     * one
     */
    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdecafbad;
    } else{
        ssrc = policy->ssrc.value;
    }
    msg_len_octets = 28;
    hdr = srtp_create_test_packet(msg_len_octets, ssrc);

    if (hdr == NULL) {
        return srtp_err_status_alloc_fail;
    }
    hdr2 = srtp_create_test_packet(msg_len_octets, ssrc);
    if (hdr2 == NULL) {
        free(hdr);
        return srtp_err_status_alloc_fail;
    }

    /* set message length */
    len = msg_len_octets;

    debug_print(mod_driver, "before protection:\n%s",
                srtp_packet_to_string(hdr, len));

#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "reference packet before protection:\n%s",
                octet_string_hex_string((uint8_t*)hdr, len));
#endif
    err_check(srtp_protect_rtcp(srtcp_sender, hdr, &len));

    debug_print(mod_driver, "after protection:\n%s",
                srtp_packet_to_string(hdr, len));
#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "after protection:\n%s",
                octet_string_hex_string((uint8_t*)hdr, len));
#endif

    /* save protected message and length */
    memcpy(hdr_enc, hdr, len);
    msg_len_enc = len;

    /*
     * check for overrun of the srtp_protect() function
     *
     * The packet is followed by a value of 0xfffff; if the value of the
     * data following the packet is different, then we know that the
     * protect function is overwriting the end of the packet.
     */
    pkt_end = (uint8_t*)hdr + sizeof(srtp_hdr_t)
              + msg_len_octets + tag_length;
    for (i = 0; i < 4; i++) {
        if (pkt_end[i] != 0xff) {
            fprintf(stdout, "overwrite in srtp_protect_rtcp() function "
                    "(expected %x, found %x in trailing octet %d)\n",
                    0xff, ((uint8_t*)hdr)[i], i);
            free(hdr);
            free(hdr2);
            return srtp_err_status_algo_fail;
        }
    }

    /*
     * if the policy includes confidentiality, check that ciphertext is
     * different than plaintext
     *
     * Note that this check will give false negatives, with some small
     * probability, especially if the packets are short.  For that
     * reason, we skip this check if the plaintext is less than four
     * octets long.
     */
    if ((policy->rtp.sec_serv & sec_serv_conf) && (msg_len_octets >= 4)) {
        printf("testing that ciphertext is distinct from plaintext...");
        status = srtp_err_status_algo_fail;
        for (i = 12; i < msg_len_octets + 12; i++) {
            if (((uint8_t*)hdr)[i] != ((uint8_t*)hdr2)[i]) {
                status = srtp_err_status_ok;
            }
        }
        if (status) {
            printf("failed\n");
            free(hdr);
            free(hdr2);
            return status;
        }
        printf("passed\n");
    }

    /*
     * if the policy uses a 'wildcard' ssrc, then we need to make a copy
     * of the policy that changes the direction to inbound
     *
     * we always copy the policy into the rcvr_policy, since otherwise
     * the compiler would fret about the constness of the policy
     */
    rcvr_policy = (srtp_policy_t*)malloc(sizeof(srtp_policy_t));
    if (rcvr_policy == NULL) {
        return srtp_err_status_alloc_fail;
    }
    memcpy(rcvr_policy, policy, sizeof(srtp_policy_t));
    if (policy->ssrc.type == ssrc_any_outbound) {
        rcvr_policy->ssrc.type = ssrc_any_inbound;
    }

    err_check(srtp_create(&srtcp_rcvr, rcvr_policy));

    err_check(srtp_unprotect_rtcp(srtcp_rcvr, hdr, &len));

    debug_print(mod_driver, "after unprotection:\n%s",
                srtp_packet_to_string(hdr, len));

    /* verify that the unprotected packet matches the origial one */
    for (i = 0; i < msg_len_octets; i++) {
        if (((uint8_t*)hdr)[i] != ((uint8_t*)hdr2)[i]) {
            fprintf(stdout, "mismatch at octet %d\n", i);
            status = srtp_err_status_algo_fail;
        }
    }
    if (status) {
        free(hdr);
        free(hdr2);
        free(rcvr_policy);
        return status;
    }

    /*
     * if the policy includes authentication, then test for false positives
     */
    if (policy->rtp.sec_serv & sec_serv_auth) {
        char *data = ((char*)hdr) + 12;

        printf("testing for false positives in replay check...");

        /* set message length */
        len = msg_len_enc;

        /* unprotect a second time - should fail with a replay error */
        status = srtp_unprotect_rtcp(srtcp_rcvr, hdr_enc, &len);
        if (status != srtp_err_status_replay_fail) {
            printf("failed with error code %d\n", status);
            free(hdr);
            free(hdr2);
            free(rcvr_policy);
            return status;
        } else {
            printf("passed\n");
        }

        printf("testing for false positives in auth check...");

        /* increment sequence number in header */
        hdr->seq++;

        /* set message length */
        len = msg_len_octets;

        /* apply protection */
        err_check(srtp_protect_rtcp(srtcp_sender, hdr, &len));

        /* flip bits in packet */
        data[0] ^= 0xff;

        /* unprotect, and check for authentication failure */
        status = srtp_unprotect_rtcp(srtcp_rcvr, hdr, &len);
        if (status != srtp_err_status_auth_fail) {
            printf("failed\n");
            free(hdr);
            free(hdr2);
            free(rcvr_policy);
            return status;
        } else {
            printf("passed\n");
        }

    }

    err_check(srtp_dealloc(srtcp_sender));
    err_check(srtp_dealloc(srtcp_rcvr));

    free(hdr);
    free(hdr2);
    free(rcvr_policy);
    return srtp_err_status_ok;
}


srtp_err_status_t
srtp_session_print_policy (srtp_t srtp)
{
    char *serv_descr[4] = {
        "none",
        "confidentiality",
        "authentication",
        "confidentiality and authentication"
    };
    char *direction[3] = {
        "unknown",
        "outbound",
        "inbound"
    };
    srtp_stream_t stream;

    /* sanity checking */
    if (srtp == NULL) {
        return srtp_err_status_fail;
    }

    /* if there's a template stream, print it out */
    if (srtp->stream_template != NULL) {
        stream = srtp->stream_template;
        printf("# SSRC:          any %s\r\n"
               "# rtp cipher:    %s\r\n"
               "# rtp auth:      %s\r\n"
               "# rtp services:  %s\r\n"
               "# rtcp cipher:   %s\r\n"
               "# rtcp auth:     %s\r\n"
               "# rtcp services: %s\r\n"
               "# window size:   %lu\r\n"
               "# tx rtx allowed:%s\r\n",
               direction[stream->direction],
               stream->rtp_cipher->type->description,
               stream->rtp_auth->type->description,
               serv_descr[stream->rtp_services],
               stream->rtcp_cipher->type->description,
               stream->rtcp_auth->type->description,
               serv_descr[stream->rtcp_services],
               srtp_rdbx_get_window_size(&stream->rtp_rdbx),
               stream->allow_repeat_tx ? "true" : "false");

        printf("# Encrypted extension headers: ");
        if (stream->enc_xtn_hdr && stream->enc_xtn_hdr_count > 0) {
            int* enc_xtn_hdr = stream->enc_xtn_hdr;
            int count = stream->enc_xtn_hdr_count;
            while (count > 0) {
                printf("%d ", *enc_xtn_hdr);
                enc_xtn_hdr++;
                count--;
            }
            printf("\n");
        } else {
            printf("none\n");
        }
    }

    /* loop over streams in session, printing the policy of each */
    stream = srtp->stream_list;
    while (stream != NULL) {
        if (stream->rtp_services > sec_serv_conf_and_auth) {
            return srtp_err_status_bad_param;
        }

        printf("# SSRC:          0x%08x\r\n"
               "# rtp cipher:    %s\r\n"
               "# rtp auth:      %s\r\n"
               "# rtp services:  %s\r\n"
               "# rtcp cipher:   %s\r\n"
               "# rtcp auth:     %s\r\n"
               "# rtcp services: %s\r\n"
               "# window size:   %lu\r\n"
               "# tx rtx allowed:%s\r\n",
               stream->ssrc,
               stream->rtp_cipher->type->description,
               stream->rtp_auth->type->description,
               serv_descr[stream->rtp_services],
               stream->rtcp_cipher->type->description,
               stream->rtcp_auth->type->description,
               serv_descr[stream->rtcp_services],
               srtp_rdbx_get_window_size(&stream->rtp_rdbx),
               stream->allow_repeat_tx ? "true" : "false");

        printf("# Encrypted extension headers: ");
        if (stream->enc_xtn_hdr && stream->enc_xtn_hdr_count > 0) {
            int* enc_xtn_hdr = stream->enc_xtn_hdr;
            int count = stream->enc_xtn_hdr_count;
            while (count > 0) {
                printf("%d ", *enc_xtn_hdr);
                enc_xtn_hdr++;
                count--;
            }
            printf("\n");
        } else {
            printf("none\n");
        }

        /* advance to next stream in the list */
        stream = stream->next;
    }
    return srtp_err_status_ok;
}

srtp_err_status_t
srtp_print_policy (const srtp_policy_t *policy)
{
    srtp_err_status_t status;
    srtp_t session;

    status = srtp_create(&session, policy);
    if (status) {
        return status;
    }
    status = srtp_session_print_policy(session);
    if (status) {
        return status;
    }
    status = srtp_dealloc(session);
    if (status) {
        return status;
    }
    return srtp_err_status_ok;
}

/*
 * srtp_print_packet(...) is for debugging only
 * it prints an RTP packet to the stdout
 *
 * note that this function is *not* threadsafe
 */

#include <stdio.h>

#define MTU 2048

char packet_string[MTU];

char *
srtp_packet_to_string (srtp_hdr_t *hdr, int pkt_octet_len)
{
    int octets_in_rtp_header = 12;
    uint8_t *data = ((uint8_t*)hdr) + octets_in_rtp_header;
    int hex_len = pkt_octet_len - octets_in_rtp_header;

    /* sanity checking */
    if ((hdr == NULL) || (pkt_octet_len > MTU)) {
        return NULL;
    }

    /* write packet into string */
    sprintf(packet_string,
            "(s)rtp packet: {\n"
            "   version:\t%d\n"
            "   p:\t\t%d\n"
            "   x:\t\t%d\n"
            "   cc:\t\t%d\n"
            "   m:\t\t%d\n"
            "   pt:\t\t%x\n"
            "   seq:\t\t%x\n"
            "   ts:\t\t%x\n"
            "   ssrc:\t%x\n"
            "   data:\t%s\n"
            "} (%d octets in total)\n",
            hdr->version,
            hdr->p,
            hdr->x,
            hdr->cc,
            hdr->m,
            hdr->pt,
            hdr->seq,
            hdr->ts,
            hdr->ssrc,
            octet_string_hex_string(data, hex_len),
            pkt_octet_len);

    return packet_string;
}

/*
 * mips_estimate() is a simple function to estimate the number of
 * instructions per second that the host can perform.  note that this
 * function can be grossly wrong; you may want to have a manual sanity
 * check of its output!
 *
 * the 'ignore' pointer is there to convince the compiler to not just
 * optimize away the function
 */

double
mips_estimate (int num_trials, int *ignore)
{
    clock_t t;
    volatile int i, sum;

    sum = 0;
    t = clock();
    for (i = 0; i < num_trials; i++) {
        sum += i;
    }
    t = clock() - t;

/*   printf("%d\n", sum); */
    *ignore = sum;

    return (double)num_trials * CLOCKS_PER_SEC / t;
}


/*
 * srtp_validate() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the default SRTP policy.
 */


srtp_err_status_t
srtp_validate ()
{
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0x4e, 0x55, 0xdc, 0x4c,
        0xe7, 0x99, 0x78, 0xd8, 0x8c, 0xa4, 0xd2, 0x15,
        0x94, 0x9d, 0x24, 0x02, 0xb7, 0x8d, 0x6a, 0xcc,
        0x99, 0xea, 0x17, 0x9b, 0x8d, 0xbb
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[38] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtcp_ciphertext[38] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0x71, 0x28, 0x03, 0x5b, 0xe4, 0x87, 0xb9, 0xbd,
        0xbe, 0xf8, 0x90, 0x41, 0xf9, 0x77, 0xa5, 0xa8,
        0x80, 0x00, 0x00, 0x01, 0x99, 0x3e, 0x08, 0xcd,
        0x54, 0xd6, 0xc1, 0x23, 0x07, 0x98
    };
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = srtp_protect(srtp_snd, srtp_plaintext, &len);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (octet_string_is_eq(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(mod_driver, "srtcp ciphertext reference:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));

    if (octet_string_is_eq(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (octet_string_is_eq(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 38;
    status = srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (status || (len != 24)) {
        return status;
    }

    if (octet_string_is_eq(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

#ifdef OPENSSL
/*
 * srtp_validate_gcm() verifies the correctness of libsrtp by comparing
 * an computed packet against the known ciphertext for the plaintext.
 */
srtp_err_status_t
srtp_validate_gcm ()
{
    unsigned char test_key_gcm[28] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xab
    };
    uint8_t rtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t rtp_plaintext[44] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[44] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xc5, 0x00, 0x2e, 0xde,
        0x04, 0xcf, 0xdd, 0x2e, 0xb9, 0x11, 0x59, 0xe0,
        0x88, 0x0a, 0xa0, 0x6e, 0xd2, 0x97, 0x68, 0x26,
        0xf7, 0x96, 0xb2, 0x01, 0xdf, 0x31, 0x31, 0xa1,
        0x27, 0xe8, 0xa3, 0x92
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[44] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtcp_ciphertext[44] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xc9, 0x8b, 0x8b, 0x5d, 0xf0, 0x39, 0x2a, 0x55,
        0x85, 0x2b, 0x6c, 0x21, 0xac, 0x8e, 0x70, 0x25,
        0xc5, 0x2c, 0x6f, 0xbe, 0xa2, 0xb3, 0xb4, 0x46,
        0xea, 0x31, 0x12, 0x3b, 0xa8, 0x8c, 0xe6, 0x1e,
        0x80, 0x00, 0x00, 0x01
    };

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key_gcm;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext rtp, then compare with srtp ciphertext
     */
    len = 28;
    status = srtp_protect(srtp_snd, rtp_plaintext, &len);
    if (status || (len != 44)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtp ciphertext:\n  %s",
                octet_string_hex_string(rtp_plaintext, len));
    debug_print(mod_driver, "srtp ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (octet_string_is_eq(rtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len);
    if (status || (len != 44)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(mod_driver, "srtcp ciphertext reference:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));

    if (octet_string_is_eq(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect srtp ciphertext, then compare with rtp plaintext
     */
    len = 44;
    status = srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (octet_string_is_eq(srtp_ciphertext, rtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 44;
    status = srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (status || (len != 24)) {
        return status;
    }

    if (octet_string_is_eq(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}
#endif

/*
 * Test vectors taken from RFC 6904, Appendix A
 */
srtp_err_status_t
srtp_validate_encrypted_extensions_headers() {
    unsigned char test_key_ext_headers[30] = {
        0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
        0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
        0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
        0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
    };
    uint8_t srtp_plaintext_ref[56] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[66] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    uint8_t srtp_ciphertext[66] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x58, 0x8A, 0x92, 0x70, 0xF4, 0xE1, 0x5E,
        0x1C, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x95, 0x46,
        0xA9, 0x94, 0xF0, 0xBC, 0x54, 0x78, 0x97, 0x00,
        0x4e, 0x55, 0xdc, 0x4c, 0xe7, 0x99, 0x78, 0xd8,
        0x8c, 0xa4, 0xd2, 0x15, 0x94, 0x9d, 0x24, 0x02,
        0x5a, 0x46, 0xb3, 0xca, 0x35, 0xc5, 0x35, 0xa8,
        0x91, 0xc7
    };
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;
    int headers[3] = {1, 3, 4};

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key_ext_headers;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.enc_xtn_hdr = headers;
    policy.enc_xtn_hdr_count = sizeof(headers) / sizeof(headers[0]);
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status)
        return status;

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = sizeof(srtp_plaintext_ref);
    status = srtp_protect(srtp_snd, srtp_plaintext, &len);
    if (status || (len != sizeof(srtp_plaintext)))
        return srtp_err_status_fail;

    debug_print(mod_driver, "ciphertext:\n  %s",
                srtp_octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                srtp_octet_string_hex_string(srtp_ciphertext, len));

    if (octet_string_is_eq(srtp_plaintext, srtp_ciphertext, len))
        return srtp_err_status_fail;

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status)
        return status;

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status) {
        return status;
    } else if (len != sizeof(srtp_plaintext_ref)) {
        return srtp_err_status_fail;
    }

    if (octet_string_is_eq(srtp_ciphertext, srtp_plaintext_ref, len))
        return srtp_err_status_fail;

    status = srtp_dealloc(srtp_snd);
    if (status)
        return status;

    status = srtp_dealloc(srtp_recv);
    if (status)
        return status;

    return srtp_err_status_ok;
}


#ifdef OPENSSL

/*
 * Headers of test vectors taken from RFC 6904, Appendix A
 */
srtp_err_status_t
srtp_validate_encrypted_extensions_headers_gcm() {
    unsigned char test_key_ext_headers[30] = {
        0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
        0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
        0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
        0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
    };
    uint8_t srtp_plaintext_ref[56] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[64] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[64] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x12, 0xe0, 0x20, 0x5b, 0xfa, 0x94, 0x9b,
        0x1C, 0x22, 0x00, 0x00, 0xC8, 0x30, 0xbb, 0x46,
        0x73, 0x27, 0x78, 0xd9, 0x92, 0x9a, 0xab, 0x00,
        0x0e, 0xca, 0x0c, 0xf9, 0x5e, 0xe9, 0x55, 0xb2,
        0x6c, 0xd3, 0xd2, 0x88, 0xb4, 0x9f, 0x6c, 0xa9,
        0xf4, 0xb1, 0xb7, 0x59, 0x71, 0x9e, 0xb5, 0xbc
    };
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;
    int headers[3] = {1, 3, 4};

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key_ext_headers;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.enc_xtn_hdr = headers;
    policy.enc_xtn_hdr_count = sizeof(headers) / sizeof(headers[0]);
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status)
        return status;

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = sizeof(srtp_plaintext_ref);
    status = srtp_protect(srtp_snd, srtp_plaintext, &len);
    if (status || (len != sizeof(srtp_plaintext)))
        return srtp_err_status_fail;

    debug_print(mod_driver, "ciphertext:\n  %s",
                srtp_octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                srtp_octet_string_hex_string(srtp_ciphertext, len));

    if (octet_string_is_eq(srtp_plaintext, srtp_ciphertext, len))
        return srtp_err_status_fail;

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status)
        return status;

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status) {
        return status;
    } else if (len != sizeof(srtp_plaintext_ref)) {
        return srtp_err_status_fail;
    }

    if (octet_string_is_eq(srtp_ciphertext, srtp_plaintext_ref, len))
        return srtp_err_status_fail;

    status = srtp_dealloc(srtp_snd);
    if (status)
        return status;

    status = srtp_dealloc(srtp_recv);
    if (status)
        return status;

    return srtp_err_status_ok;
}
#endif

/*
 * srtp_validate_aes_256() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the AES-CM-256/HMAC-SHA-1-80 policy.
 */


srtp_err_status_t
srtp_validate_aes_256 ()
{
    unsigned char aes_256_test_key[46] = {
        0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
        0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
        0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
        0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6,

        0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7, 0xc9,
        0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2
    };
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xf1, 0xd9, 0xde, 0x17,
        0xff, 0x25, 0x1f, 0xf1, 0xaa, 0x00, 0x77, 0x74,
        0xb0, 0xb4, 0xb4, 0x0d, 0xa0, 0x8d, 0x9d, 0x9a,
        0x5b, 0x3a, 0x55, 0xd8, 0x87, 0x3b
    };
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = aes_256_test_key;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = srtp_protect(srtp_snd, srtp_plaintext, &len);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (octet_string_is_eq(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (octet_string_is_eq(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}


srtp_err_status_t
srtp_create_big_policy (srtp_policy_t **list)
{
    extern const srtp_policy_t *policy_array[];
    srtp_policy_t *p, *tmp;
    int i = 0;
    uint32_t ssrc = 0;

    /* sanity checking */
    if ((list == NULL) || (policy_array[0] == NULL)) {
        return srtp_err_status_bad_param;
    }

    /*
     * loop over policy list, mallocing a new list and copying values
     * into it (and incrementing the SSRC value as we go along)
     */
    tmp = NULL;
    while (policy_array[i] != NULL) {
        p  = (srtp_policy_t*)malloc(sizeof(srtp_policy_t));
        if (p == NULL) {
            return srtp_err_status_bad_param;
        }
        memcpy(p, policy_array[i], sizeof(srtp_policy_t));
        p->ssrc.type = ssrc_specific;
        p->ssrc.value = ssrc++;
        p->next = tmp;
        tmp = p;
        i++;
    }
    *list = p;

    return srtp_err_status_ok;
}

srtp_err_status_t
srtp_dealloc_big_policy (srtp_policy_t *list)
{
    srtp_policy_t *p, *next;

    for (p = list; p != NULL; p = next) {
        next = p->next;
        free(p);
    }

    return srtp_err_status_ok;
}

srtp_err_status_t
srtp_test_empty_payload()
{
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;
    srtp_hdr_t *mesg;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    mesg = srtp_create_test_packet(0, policy.ssrc.value);
    if (mesg == NULL) {
        return srtp_err_status_fail;
    }

    len = 12;  /* only the header */
    status = srtp_protect(srtp_snd, mesg, &len);
    if (status) {
        return status;
    } else if (len != 12 + 10) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = srtp_unprotect(srtp_recv, mesg, &len);
    if (status) {
        return status;
    } else if (len != 12) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    free(mesg);

    return srtp_err_status_ok;
}

#ifdef OPENSSL
srtp_err_status_t
srtp_test_empty_payload_gcm()
{
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    int len;
    srtp_policy_t policy;
    srtp_hdr_t *mesg;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    mesg = srtp_create_test_packet(0, policy.ssrc.value);
    if (mesg == NULL) {
        return srtp_err_status_fail;
    }

    len = 12;  /* only the header */
    status = srtp_protect(srtp_snd, mesg, &len);
    if (status) {
        return status;
    } else if (len != 12 + 8) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = srtp_unprotect(srtp_recv, mesg, &len);
    if (status) {
        return status;
    } else if (len != 12) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    free(mesg);

    return srtp_err_status_ok;
}
#endif  // OPENSSL

srtp_err_status_t
srtp_test_remove_stream ()
{
    srtp_err_status_t status;
    srtp_policy_t *policy_list, policy;
    srtp_t session;
    srtp_stream_t stream;

    /*
     * srtp_get_stream() is a libSRTP internal function that we declare
     * here so that we can use it to verify the correct operation of the
     * library
     */
    extern srtp_stream_t srtp_get_stream(srtp_t srtp, uint32_t ssrc);


    status = srtp_create_big_policy(&policy_list);
    if (status) {
        return status;
    }

    status = srtp_create(&session, policy_list);
    if (status) {
        return status;
    }

    /*
     * check for false positives by trying to remove a stream that's not
     * in the session
     */
    status = srtp_remove_stream(session, htonl(0xaaaaaaaa));
    if (status != srtp_err_status_no_ctx) {
        return srtp_err_status_fail;
    }

    /*
     * check for false negatives by removing stream 0x1, then
     * searching for streams 0x0 and 0x2
     */
    status = srtp_remove_stream(session, htonl(0x1));
    if (status != srtp_err_status_ok) {
        return srtp_err_status_fail;
    }
    stream = srtp_get_stream(session, htonl(0x0));
    if (stream == NULL) {
        return srtp_err_status_fail;
    }
    stream = srtp_get_stream(session, htonl(0x2));
    if (stream == NULL) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(session);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_dealloc_big_policy(policy_list);
    if (status != srtp_err_status_ok) {
        return status;
    }

    /* Now test adding and removing a single stream */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key  = test_key;
    policy.ekt = NULL;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.next = NULL;

    status = srtp_create(&session, NULL);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_add_stream(session, &policy);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_remove_stream(session, htonl(0xcafebabe));
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_dealloc(session);
    if (status != srtp_err_status_ok) {
        return status;
    }

    return srtp_err_status_ok;
}


unsigned char test_alt_key[46] = {
  0xe5, 0x19, 0x6f, 0x01, 0x5e, 0xf1, 0x9b, 0xe1,
  0xd7, 0x47, 0xa7, 0x27, 0x07, 0xd7, 0x47, 0x33,
  0x01, 0xc2, 0x35, 0x4d, 0x59, 0x6a, 0xf7, 0x84,
  0x96, 0x98, 0xeb, 0xaa, 0xac, 0xf6, 0xa1, 0x45,
  0xc7, 0x15, 0xe2, 0xea, 0xfe, 0x55, 0x67, 0x96,
  0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
};

/*
 * srtp_test_update() verifies updating/rekeying exsisting streams.
 * As stated in https://tools.ietf.org/html/rfc3711#section-3.3.1
 * the value of the ROC must not be reset after a rekey, this test
 * atempts to prove that srtp_update does not reset the ROC.
 */

srtp_err_status_t
srtp_test_update() {

  srtp_err_status_t status;
  uint32_t ssrc = 0x12121212;
  int msg_len_octets = 32;
  int protected_msg_len_octets;
  srtp_hdr_t * msg;
  srtp_t srtp_snd, srtp_recv;
  srtp_policy_t policy;

  memset(&policy, 0, sizeof(policy));
  srtp_crypto_policy_set_rtp_default(&policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
  policy.ekt = NULL;
  policy.window_size = 128;
  policy.allow_repeat_tx = 0;
  policy.next = NULL;
  policy.ssrc.type  = ssrc_any_outbound;
  policy.key  = test_key;

  /* create a send and recive ctx with defualt profile and test_key */
  status = srtp_create(&srtp_recv, &policy);
  if (status)
    return status;

  policy.ssrc.type  = ssrc_any_inbound;
  status = srtp_create(&srtp_snd, &policy);
  if (status)
    return status;

  /* protect and unprotect two msg's that will cause the ROC to be equal to 1 */
  msg = srtp_create_test_packet(msg_len_octets, ssrc);
  if (msg == NULL)
    return srtp_err_status_alloc_fail;
  msg->seq = htons(65535);

  protected_msg_len_octets = msg_len_octets;
  status = srtp_protect(srtp_snd, msg, &protected_msg_len_octets);
  if (status)
    return srtp_err_status_fail;

  status = srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
  if (status)
    return status;

  free(msg);

  msg = srtp_create_test_packet(msg_len_octets, ssrc);
  if (msg == NULL)
    return srtp_err_status_alloc_fail;
  msg->seq = htons(1);

  protected_msg_len_octets = msg_len_octets;
  status = srtp_protect(srtp_snd, msg, &protected_msg_len_octets);
  if (status)
    return srtp_err_status_fail;

  status = srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
  if (status)
    return status;

  free(msg);

  /* update send ctx with same test_key t verify update works*/
  policy.ssrc.type = ssrc_any_outbound;
  policy.key = test_key;
  status = srtp_update(srtp_snd, &policy);
  if (status)
    return status;

  msg = srtp_create_test_packet(msg_len_octets, ssrc);
  if (msg == NULL)
    return srtp_err_status_alloc_fail;
  msg->seq = htons(2);

  protected_msg_len_octets = msg_len_octets;
  status = srtp_protect(srtp_snd, msg, &protected_msg_len_octets);
  if (status)
    return srtp_err_status_fail;

  status = srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
  if (status)
    return status;

  free(msg);


  /* update send ctx to use test_alt_key */
  policy.ssrc.type = ssrc_any_outbound;
  policy.key = test_alt_key;
  status = srtp_update(srtp_snd, &policy);
  if (status)
    return status;

  /* create and protect msg with new key and ROC still equal to 1 */
  msg = srtp_create_test_packet(msg_len_octets, ssrc);
  if (msg == NULL)
    return srtp_err_status_alloc_fail;
  msg->seq = htons(3);

  protected_msg_len_octets = msg_len_octets;
  status = srtp_protect(srtp_snd, msg, &protected_msg_len_octets);
  if (status)
    return srtp_err_status_fail;

  /* verify that recive ctx will fail to unprotect as it still uses test_key */
  status = srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
  if (status == srtp_err_status_ok)
    return srtp_err_status_fail;

  /* create a new recvieve ctx with test_alt_key but since it is new it will have ROC equal to 1
   * and therefore should fail to unprotected */
  {
    srtp_t srtp_recv_roc_0;

    policy.ssrc.type  = ssrc_any_inbound;
    policy.key = test_alt_key;
    status = srtp_create(&srtp_recv_roc_0, &policy);
    if (status)
      return status;

    status = srtp_unprotect(srtp_recv_roc_0, msg, &protected_msg_len_octets);
    if (status == srtp_err_status_ok)
      return srtp_err_status_fail;

    status = srtp_dealloc(srtp_recv_roc_0);
    if (status)
      return status;
  }

  /* update recive ctx to use test_alt_key */
  policy.ssrc.type = ssrc_any_inbound;
  policy.key = test_alt_key;
  status = srtp_update(srtp_recv, &policy);
  if (status)
    return status;

  /* verify that can still unprotect, therfore key is updated and ROC value is preserved */
  status = srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
  if (status)
    return status;

  free(msg);

  status = srtp_dealloc(srtp_snd);
  if (status)
    return status;

  status = srtp_dealloc(srtp_recv);
  if (status)
    return status;

  return srtp_err_status_ok;
}

/*
 * srtp policy definitions - these definitions are used above
 */

unsigned char test_key[46] = {
    0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
    0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
    0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6, 0xc1, 0x73,
    0xc3, 0x17, 0xf2, 0xda, 0xbe, 0x35, 0x77, 0x93,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
};


const srtp_policy_t default_policy = {
    { ssrc_any_outbound, 0 },  /* SSRC                           */
    {                          /* SRTP policy                    */
        SRTP_AES_128_ICM,           /* cipher type                 */
        30,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,             /* authentication func type    */
        16,                    /* auth key length in octets   */
        10,                    /* auth tag length in octets   */
        sec_serv_conf_and_auth /* security services flag      */
    },
    {                          /* SRTCP policy                   */
        SRTP_AES_128_ICM,           /* cipher type                 */
        30,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,             /* authentication func type    */
        16,                    /* auth key length in octets   */
        10,                    /* auth tag length in octets   */
        sec_serv_conf_and_auth /* security services flag      */
    },
    test_key,
    NULL,      /* indicates that EKT is not in use */
    128,       /* replay window size */
    0,         /* retransmission not allowed */
    NULL,      /* no encrypted extension headers */
    0,         /* list of encrypted extension headers is empty */
    NULL
};

const srtp_policy_t aes_only_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC                        */
    {
        SRTP_AES_128_ICM,          /* cipher type                 */
        30,                   /* cipher key length in octets */
        SRTP_NULL_AUTH,            /* authentication func type    */
        0,                    /* auth key length in octets   */
        0,                    /* auth tag length in octets   */
        sec_serv_conf         /* security services flag      */
    },
    {
        SRTP_AES_128_ICM,        /* cipher type                 */
        30,                 /* cipher key length in octets */
        SRTP_NULL_AUTH,          /* authentication func type    */
        0,                  /* auth key length in octets   */
        0,                  /* auth tag length in octets   */
        sec_serv_conf       /* security services flag      */
    },
    test_key,
    NULL,      /* indicates that EKT is not in use */
    128,       /* replay window size */
    0,         /* retransmission not allowed */
    NULL,      /* no encrypted extension headers */
    0,         /* list of encrypted extension headers is empty */
    NULL
};

const srtp_policy_t hmac_only_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC                        */
    {
        SRTP_NULL_CIPHER,          /* cipher type                 */
        0,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,            /* authentication func type    */
        20,                   /* auth key length in octets   */
        4,                    /* auth tag length in octets   */
        sec_serv_auth         /* security services flag      */
    },
    {
        SRTP_NULL_CIPHER,        /* cipher type                 */
        0,                  /* cipher key length in octets */
        SRTP_HMAC_SHA1,          /* authentication func type    */
        20,                 /* auth key length in octets   */
        4,                  /* auth tag length in octets   */
        sec_serv_auth       /* security services flag      */
    },
    test_key,
    NULL,      /* indicates that EKT is not in use */
    128,       /* replay window size */
    0,         /* retransmission not allowed */
    NULL,      /* no encrypted extension headers */
    0,         /* list of encrypted extension headers is empty */
    NULL
};

#ifdef OPENSSL
const srtp_policy_t aes128_gcm_8_policy = {
    { ssrc_any_outbound, 0 },           /* SSRC                           */
    {                                   /* SRTP policy                    */
        SRTP_AES_128_GCM,                    /* cipher type                 */
        SRTP_AES_128_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {                                   /* SRTCP policy                   */
        SRTP_AES_128_GCM,                    /* cipher type                 */
        SRTP_AES_128_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    test_key,
    NULL,        /* indicates that EKT is not in use */
    128,         /* replay window size */
    0,           /* retransmission not allowed */
    NULL,        /* no encrypted extension headers */
    0,           /* list of encrypted extension headers is empty */
    NULL
};

const srtp_policy_t aes128_gcm_8_cauth_policy = {
    { ssrc_any_outbound, 0 },           /* SSRC                           */
    {                                   /* SRTP policy                    */
        SRTP_AES_128_GCM,                    /* cipher type                 */
        SRTP_AES_128_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {                                   /* SRTCP policy                   */
        SRTP_AES_128_GCM,                    /* cipher type                 */
        SRTP_AES_128_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_auth                   /* security services flag      */
    },
    test_key,
    NULL,        /* indicates that EKT is not in use */
    128,         /* replay window size */
    0,           /* retransmission not allowed */
    NULL,        /* no encrypted extension headers */
    0,           /* list of encrypted extension headers is empty */
    NULL
};

const srtp_policy_t aes256_gcm_8_policy = {
    { ssrc_any_outbound, 0 },           /* SSRC                           */
    {                                   /* SRTP policy                    */
        SRTP_AES_256_GCM,                    /* cipher type                 */
        SRTP_AES_256_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {                                   /* SRTCP policy                   */
        SRTP_AES_256_GCM,                    /* cipher type                 */
        SRTP_AES_256_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    test_key,
    NULL,        /* indicates that EKT is not in use */
    128,         /* replay window size */
    0,           /* retransmission not allowed */
    NULL,        /* no encrypted extension headers */
    0,           /* list of encrypted extension headers is empty */
    NULL
};

const srtp_policy_t aes256_gcm_8_cauth_policy = {
    { ssrc_any_outbound, 0 },           /* SSRC                           */
    {                                   /* SRTP policy                    */
        SRTP_AES_256_GCM,                    /* cipher type                 */
        SRTP_AES_256_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {                                   /* SRTCP policy                   */
        SRTP_AES_256_GCM,                    /* cipher type                 */
        SRTP_AES_256_GCM_KEYSIZE_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                      /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_auth                   /* security services flag      */
    },
    test_key,
    NULL,        /* indicates that EKT is not in use */
    128,         /* replay window size */
    0,           /* retransmission not allowed */
    NULL,        /* no encrypted extension headers */
    0,           /* list of encrypted extension headers is empty */
    NULL
};
#endif

const srtp_policy_t null_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC                        */
    {
        SRTP_NULL_CIPHER,          /* cipher type                 */
        0,                    /* cipher key length in octets */
        SRTP_NULL_AUTH,            /* authentication func type    */
        0,                    /* auth key length in octets   */
        0,                    /* auth tag length in octets   */
        sec_serv_none         /* security services flag      */
    },
    {
        SRTP_NULL_CIPHER,        /* cipher type                 */
        0,                  /* cipher key length in octets */
        SRTP_NULL_AUTH,          /* authentication func type    */
        0,                  /* auth key length in octets   */
        0,                  /* auth tag length in octets   */
        sec_serv_none       /* security services flag      */
    },
    test_key,
    NULL,      /* indicates that EKT is not in use */
    128,       /* replay window size */
    0,         /* retransmission not allowed */
    NULL,      /* no encrypted extension headers */
    0,         /* list of encrypted extension headers is empty */
    NULL
};

unsigned char test_256_key[46] = {
    0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
    0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
    0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
    0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6,

    0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7, 0xc9,
    0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2
};

const srtp_policy_t aes_256_hmac_policy = {
    { ssrc_any_outbound, 0 },  /* SSRC                           */
    {                          /* SRTP policy                    */
        SRTP_AES_ICM,               /* cipher type                 */
        46,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,             /* authentication func type    */
        20,                    /* auth key length in octets   */
        10,                    /* auth tag length in octets   */
        sec_serv_conf_and_auth /* security services flag      */
    },
    {                          /* SRTCP policy                   */
        SRTP_AES_ICM,               /* cipher type                 */
        46,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,             /* authentication func type    */
        20,                    /* auth key length in octets   */
        10,                    /* auth tag length in octets   */
        sec_serv_conf_and_auth /* security services flag      */
    },
    test_256_key,
    NULL,      /* indicates that EKT is not in use */
    128,       /* replay window size */
    0,         /* retransmission not allowed */
    NULL,      /* no encrypted extension headers */
    0,         /* list of encrypted extension headers is empty */
    NULL
};

uint8_t ekt_test_key[16] = {
    0x77, 0x26, 0x9d, 0xac, 0x16, 0xa3, 0x28, 0xca,
    0x8e, 0xc9, 0x68, 0x4b, 0xcc, 0xc4, 0xd2, 0x1b
};

#include "ekt.h"

srtp_ekt_policy_ctx_t ekt_test_policy = {
    0xa5a5,                     /* SPI */
    SRTP_EKT_CIPHER_AES_128_ECB,
    ekt_test_key,
    NULL
};

const srtp_policy_t hmac_only_with_ekt_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC                        */
    {
        SRTP_NULL_CIPHER,          /* cipher type                 */
        0,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,            /* authentication func type    */
        20,                   /* auth key length in octets   */
        4,                    /* auth tag length in octets   */
        sec_serv_auth         /* security services flag      */
    },
    {
        SRTP_NULL_CIPHER,        /* cipher type                 */
        0,                  /* cipher key length in octets */
        SRTP_HMAC_SHA1,          /* authentication func type    */
        20,                 /* auth key length in octets   */
        4,                  /* auth tag length in octets   */
        sec_serv_auth       /* security services flag      */
    },
    test_key,
    &ekt_test_policy,      /* indicates that EKT is not in use */
    128,                   /* replay window size */
    0,                     /* retransmission not allowed */
    NULL,                  /* no encrypted extension headers */
    0,                     /* list of encrypted extension headers is empty */
    NULL
};


/*
 * an array of pointers to the policies listed above
 *
 * This array is used to test various aspects of libSRTP for
 * different cryptographic policies.  The order of the elements
 * matters - the timing test generates output that can be used
 * in a plot (see the gnuplot script file 'timing').  If you
 * add to this list, you should do it at the end.
 */

const srtp_policy_t *
policy_array[] = {
    &hmac_only_policy,
    &aes_only_policy,
    &default_policy,
#ifdef OPENSSL
    &aes128_gcm_8_policy,
    &aes128_gcm_8_cauth_policy,
    &aes256_gcm_8_policy,
    &aes256_gcm_8_cauth_policy,
#endif
    &null_policy,
    &aes_256_hmac_policy,
    &hmac_only_with_ekt_policy,
    NULL
};

const srtp_policy_t wildcard_policy = {
    { ssrc_any_outbound, 0 },  /* SSRC                        */
    {                          /* SRTP policy                    */
        SRTP_AES_128_ICM,           /* cipher type                 */
        30,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,             /* authentication func type    */
        16,                    /* auth key length in octets   */
        10,                    /* auth tag length in octets   */
        sec_serv_conf_and_auth /* security services flag      */
    },
    {                          /* SRTCP policy                   */
        SRTP_AES_128_ICM,           /* cipher type                 */
        30,                    /* cipher key length in octets */
        SRTP_HMAC_SHA1,             /* authentication func type    */
        16,                    /* auth key length in octets   */
        10,                    /* auth tag length in octets   */
        sec_serv_conf_and_auth /* security services flag      */
    },
    test_key,
    NULL,
    128,                 /* replay window size */
    0,                   /* retransmission not allowed */
    NULL,                /* no encrypted extension headers */
    0,                   /* list of encrypted extension headers is empty */
    NULL
};
