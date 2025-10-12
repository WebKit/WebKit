/*
 *
 * Copyright (c) 2013-2021, Cisco Systems, Inc.
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

#include "cipher_test_cases.h"
#include <stddef.h>

/*
 * KAT values for AES self-test.  These
 * values came from the legacy libsrtp code.
 */
/* clang-format off */
static const uint8_t srtp_aes_icm_128_test_case_0_key[SRTP_AES_ICM_128_KEY_LEN_WSALT] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd
};
/* clang-format on */

/* clang-format off */
static uint8_t srtp_aes_icm_128_test_case_0_nonce[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_icm_128_test_case_0_plaintext[32] =  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_icm_128_test_case_0_ciphertext[32] = {
    0xe0, 0x3e, 0xad, 0x09, 0x35, 0xc9, 0x5e, 0x80,
    0xe1, 0x66, 0xb1, 0x6d, 0xd9, 0x2b, 0x4e, 0xb4,
    0xd2, 0x35, 0x13, 0x16, 0x2b, 0x02, 0xd0, 0xf7,
    0x2a, 0x43, 0xa2, 0xfe, 0x4a, 0x5f, 0x97, 0xab
};
/* clang-format on */

const srtp_cipher_test_case_t srtp_aes_icm_128_test_case_0 = {
    SRTP_AES_ICM_128_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_icm_128_test_case_0_key,        /* key                      */
    srtp_aes_icm_128_test_case_0_nonce,      /* packet index             */
    32,                                      /* octets in plaintext      */
    srtp_aes_icm_128_test_case_0_plaintext,  /* plaintext                */
    32,                                      /* octets in ciphertext     */
    srtp_aes_icm_128_test_case_0_ciphertext, /* ciphertext               */
    0,                                       /* */
    NULL,                                    /* */
    0,                                       /* */
    NULL                                     /* pointer to next testcase */
};

/*
 * KAT values for AES-192-CTR self-test.  These
 * values came from section 7 of RFC 6188.
 */
/* clang-format off */
static const uint8_t srtp_aes_icm_192_test_case_0_key[SRTP_AES_ICM_192_KEY_LEN_WSALT] = {
    0xea, 0xb2, 0x34, 0x76, 0x4e, 0x51, 0x7b, 0x2d,
    0x3d, 0x16, 0x0d, 0x58, 0x7d, 0x8c, 0x86, 0x21,
    0x97, 0x40, 0xf6, 0x5f, 0x99, 0xb6, 0xbc, 0xf7,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd
};
/* clang-format on */

/* clang-format off */
static uint8_t srtp_aes_icm_192_test_case_0_nonce[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_icm_192_test_case_0_plaintext[32] =  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_icm_192_test_case_0_ciphertext[32] = {
    0x35, 0x09, 0x6c, 0xba, 0x46, 0x10, 0x02, 0x8d,
    0xc1, 0xb5, 0x75, 0x03, 0x80, 0x4c, 0xe3, 0x7c,
    0x5d, 0xe9, 0x86, 0x29, 0x1d, 0xcc, 0xe1, 0x61,
    0xd5, 0x16, 0x5e, 0xc4, 0x56, 0x8f, 0x5c, 0x9a
};
/* clang-format on */

const srtp_cipher_test_case_t srtp_aes_icm_192_test_case_0 = {
    SRTP_AES_ICM_192_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_icm_192_test_case_0_key,        /* key                      */
    srtp_aes_icm_192_test_case_0_nonce,      /* packet index             */
    32,                                      /* octets in plaintext      */
    srtp_aes_icm_192_test_case_0_plaintext,  /* plaintext                */
    32,                                      /* octets in ciphertext     */
    srtp_aes_icm_192_test_case_0_ciphertext, /* ciphertext               */
    0,                                       /* */
    NULL,                                    /* */
    0,                                       /* */
    NULL                                     /* pointer to next testcase */
};

/*
 * KAT values for AES-256-CTR self-test.  These
 * values came from section 7 of RFC 6188.
 */
/* clang-format off */
static const uint8_t srtp_aes_icm_256_test_case_0_key[SRTP_AES_ICM_256_KEY_LEN_WSALT] = {
    0x57, 0xf8, 0x2f, 0xe3, 0x61, 0x3f, 0xd1, 0x70,
    0xa8, 0x5e, 0xc9, 0x3c, 0x40, 0xb1, 0xf0, 0x92,
    0x2e, 0xc4, 0xcb, 0x0d, 0xc0, 0x25, 0xb5, 0x82,
    0x72, 0x14, 0x7c, 0xc4, 0x38, 0x94, 0x4a, 0x98,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd
};
/* clang-format on */

/* clang-format off */
static uint8_t srtp_aes_icm_256_test_case_0_nonce[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_icm_256_test_case_0_plaintext[32] =  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_icm_256_test_case_0_ciphertext[32] = {
    0x92, 0xbd, 0xd2, 0x8a, 0x93, 0xc3, 0xf5, 0x25,
    0x11, 0xc6, 0x77, 0xd0, 0x8b, 0x55, 0x15, 0xa4,
    0x9d, 0xa7, 0x1b, 0x23, 0x78, 0xa8, 0x54, 0xf6,
    0x70, 0x50, 0x75, 0x6d, 0xed, 0x16, 0x5b, 0xac
};
/* clang-format on */

const srtp_cipher_test_case_t srtp_aes_icm_256_test_case_0 = {
    SRTP_AES_ICM_256_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_icm_256_test_case_0_key,        /* key                      */
    srtp_aes_icm_256_test_case_0_nonce,      /* packet index             */
    32,                                      /* octets in plaintext      */
    srtp_aes_icm_256_test_case_0_plaintext,  /* plaintext                */
    32,                                      /* octets in ciphertext     */
    srtp_aes_icm_256_test_case_0_ciphertext, /* ciphertext               */
    0,                                       /* */
    NULL,                                    /* */
    0,                                       /* */
    NULL                                     /* pointer to next testcase */
};

/*
 * KAT values for AES self-test.  These
 * values we're derived from independent test code
 * using OpenSSL.
 */
/* clang-format off */
static const uint8_t srtp_aes_gcm_128_test_case_0_key[SRTP_AES_GCM_128_KEY_LEN_WSALT] = {
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
    0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c,
};
/* clang-format on */

/* clang-format off */
static uint8_t srtp_aes_gcm_128_test_case_0_iv[12] = {
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
    0xde, 0xca, 0xf8, 0x88
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_gcm_128_test_case_0_plaintext[60] =  {
    0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
    0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
    0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
    0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
    0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
    0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
    0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
    0xba, 0x63, 0x7b, 0x39
};

/* clang-format off */
static const uint8_t srtp_aes_gcm_128_test_case_0_aad[20] = {
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
    0xab, 0xad, 0xda, 0xd2
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_gcm_128_test_case_0_ciphertext[76] = {
    0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
    0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
    0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
    0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
    0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
    0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
    0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
    0x3d, 0x58, 0xe0, 0x91,
    /* the last 16 bytes are the tag */
    0x5b, 0xc9, 0x4f, 0xbc, 0x32, 0x21, 0xa5, 0xdb,
    0x94, 0xfa, 0xe9, 0x5a, 0xe7, 0x12, 0x1a, 0x47,
};
/* clang-format on */

static const srtp_cipher_test_case_t srtp_aes_gcm_128_test_case_0a = {
    SRTP_AES_GCM_128_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_gcm_128_test_case_0_key,        /* key                      */
    srtp_aes_gcm_128_test_case_0_iv,         /* packet index             */
    60,                                      /* octets in plaintext      */
    srtp_aes_gcm_128_test_case_0_plaintext,  /* plaintext                */
    68,                                      /* octets in ciphertext     */
    srtp_aes_gcm_128_test_case_0_ciphertext, /* ciphertext  + tag        */
    20,                                      /* octets in AAD            */
    srtp_aes_gcm_128_test_case_0_aad,        /* AAD                      */
    8,                                       /* */
    NULL                                     /* pointer to next testcase */
};

const srtp_cipher_test_case_t srtp_aes_gcm_128_test_case_0 = {
    SRTP_AES_GCM_128_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_gcm_128_test_case_0_key,        /* key                      */
    srtp_aes_gcm_128_test_case_0_iv,         /* packet index             */
    60,                                      /* octets in plaintext      */
    srtp_aes_gcm_128_test_case_0_plaintext,  /* plaintext                */
    76,                                      /* octets in ciphertext     */
    srtp_aes_gcm_128_test_case_0_ciphertext, /* ciphertext  + tag        */
    20,                                      /* octets in AAD            */
    srtp_aes_gcm_128_test_case_0_aad,        /* AAD                      */
    16,                                      /* */
    &srtp_aes_gcm_128_test_case_0a           /* pointer to next testcase */
};

/* clang-format off */
static const uint8_t srtp_aes_gcm_256_test_case_0_key[SRTP_AES_GCM_256_KEY_LEN_WSALT] = {
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
    0xa5, 0x59, 0x09, 0xc5, 0x54, 0x66, 0x93, 0x1c,
    0xaf, 0xf5, 0x26, 0x9a, 0x21, 0xd5, 0x14, 0xb2,
    0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c,
};
/* clang-format on */

/* clang-format off */
static uint8_t srtp_aes_gcm_256_test_case_0_iv[12] = {
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
    0xde, 0xca, 0xf8, 0x88
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_gcm_256_test_case_0_plaintext[60] =  {
    0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
    0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
    0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
    0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
    0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
    0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
    0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
    0xba, 0x63, 0x7b, 0x39
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_gcm_256_test_case_0_aad[20] = {
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
    0xab, 0xad, 0xda, 0xd2
};
/* clang-format on */

/* clang-format off */
static const uint8_t srtp_aes_gcm_256_test_case_0_ciphertext[76] = {
    0x0b, 0x11, 0xcf, 0xaf, 0x68, 0x4d, 0xae, 0x46,
    0xc7, 0x90, 0xb8, 0x8e, 0xb7, 0x6a, 0x76, 0x2a,
    0x94, 0x82, 0xca, 0xab, 0x3e, 0x39, 0xd7, 0x86,
    0x1b, 0xc7, 0x93, 0xed, 0x75, 0x7f, 0x23, 0x5a,
    0xda, 0xfd, 0xd3, 0xe2, 0x0e, 0x80, 0x87, 0xa9,
    0x6d, 0xd7, 0xe2, 0x6a, 0x7d, 0x5f, 0xb4, 0x80,
    0xef, 0xef, 0xc5, 0x29, 0x12, 0xd1, 0xaa, 0x10,
    0x09, 0xc9, 0x86, 0xc1,
    /* the last 16 bytes are the tag */
    0x45, 0xbc, 0x03, 0xe6, 0xe1, 0xac, 0x0a, 0x9f,
    0x81, 0xcb, 0x8e, 0x5b, 0x46, 0x65, 0x63, 0x1d,
};
/* clang-format on */

static const srtp_cipher_test_case_t srtp_aes_gcm_256_test_case_0a = {
    SRTP_AES_GCM_256_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_gcm_256_test_case_0_key,        /* key                      */
    srtp_aes_gcm_256_test_case_0_iv,         /* packet index             */
    60,                                      /* octets in plaintext      */
    srtp_aes_gcm_256_test_case_0_plaintext,  /* plaintext                */
    68,                                      /* octets in ciphertext     */
    srtp_aes_gcm_256_test_case_0_ciphertext, /* ciphertext  + tag        */
    20,                                      /* octets in AAD            */
    srtp_aes_gcm_256_test_case_0_aad,        /* AAD                      */
    8,                                       /* */
    NULL                                     /* pointer to next testcase */
};

const srtp_cipher_test_case_t srtp_aes_gcm_256_test_case_0 = {
    SRTP_AES_GCM_256_KEY_LEN_WSALT,          /* octets in key            */
    srtp_aes_gcm_256_test_case_0_key,        /* key                      */
    srtp_aes_gcm_256_test_case_0_iv,         /* packet index             */
    60,                                      /* octets in plaintext      */
    srtp_aes_gcm_256_test_case_0_plaintext,  /* plaintext                */
    76,                                      /* octets in ciphertext     */
    srtp_aes_gcm_256_test_case_0_ciphertext, /* ciphertext  + tag        */
    20,                                      /* octets in AAD            */
    srtp_aes_gcm_256_test_case_0_aad,        /* AAD                      */
    16,                                      /* */
    &srtp_aes_gcm_256_test_case_0a           /* pointer to next testcase */
};
