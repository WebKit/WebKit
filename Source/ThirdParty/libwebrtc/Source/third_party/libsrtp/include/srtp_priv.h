/*
 * srtp_priv.h
 *
 * private internal data structures and functions for libSRTP
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright (c) 2001-2006 Cisco Systems, Inc.
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

#ifndef SRTP_PRIV_H
#define SRTP_PRIV_H

#include "config.h"
#include "srtp.h"
#include "rdbx.h"
#include "rdb.h"
#include "integers.h"
#include "cipher.h"
#include "auth.h"
#include "aes.h"
#include "key.h"
#include "crypto_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SRTP_VER_STRING	    PACKAGE_STRING
#define SRTP_VERSION        PACKAGE_VERSION

/*
 * the following declarations are libSRTP internal functions 
 */

/*
 * srtp_get_stream(ssrc) returns a pointer to the stream corresponding
 * to ssrc, or NULL if no stream exists for that ssrc
 */
srtp_stream_t srtp_get_stream(srtp_t srtp, uint32_t ssrc);


/*
 * srtp_stream_init_keys(s, k) (re)initializes the srtp_stream_t s by
 * deriving all of the needed keys using the KDF and the key k.
 */
srtp_err_status_t srtp_stream_init_keys(srtp_stream_t srtp, const void *key);

/*
 * srtp_stream_init(s, p) initializes the srtp_stream_t s to 
 * use the policy at the location p
 */
srtp_err_status_t srtp_stream_init(srtp_stream_t srtp, const srtp_policy_t *p);


/*
 * libsrtp internal datatypes 
 */

typedef enum direction_t { 
  dir_unknown       = 0,
  dir_srtp_sender   = 1, 
  dir_srtp_receiver = 2
} direction_t;

/* 
 * an srtp_stream_t has its own SSRC, encryption key, authentication
 * key, sequence number, and replay database
 * 
 * note that the keys might not actually be unique, in which case the
 * srtp_cipher_t and srtp_auth_t pointers will point to the same structures
 */

typedef struct srtp_stream_ctx_t_ {
  uint32_t   ssrc;
  srtp_cipher_t  *rtp_cipher;
  srtp_cipher_t  *rtp_xtn_hdr_cipher;
  srtp_auth_t    *rtp_auth;
  srtp_rdbx_t     rtp_rdbx;
  srtp_sec_serv_t rtp_services;
  srtp_cipher_t  *rtcp_cipher;
  srtp_auth_t    *rtcp_auth;
  srtp_rdb_t      rtcp_rdb;
  srtp_sec_serv_t rtcp_services;
  srtp_key_limit_ctx_t *limit;
  direction_t direction;
  int        allow_repeat_tx;
  srtp_ekt_stream_t ekt; 
  uint8_t    salt[SRTP_AEAD_SALT_LEN];   /* used with GCM mode for SRTP */
  uint8_t    c_salt[SRTP_AEAD_SALT_LEN]; /* used with GCM mode for SRTCP */
  int       *enc_xtn_hdr;
  int        enc_xtn_hdr_count;
  struct srtp_stream_ctx_t_ *next;   /* linked list of streams */
} strp_stream_ctx_t_;


/*
 * an srtp_ctx_t holds a stream list and a service description
 */

typedef struct srtp_ctx_t_ {
  struct srtp_stream_ctx_t_ *stream_list;     /* linked list of streams            */
  struct srtp_stream_ctx_t_ *stream_template; /* act as template for other streams */
  void *user_data;                    /* user custom data */
} srtp_ctx_t_;



/*
 * srtp_handle_event(srtp, srtm, evnt) calls the event handling
 * function, if there is one.
 *
 * This macro is not included in the documentation as it is 
 * an internal-only function.
 */

#define srtp_handle_event(srtp, strm, evnt)         \
   if(srtp_event_handler) {                         \
      srtp_event_data_t data;                       \
      data.session = srtp;                          \
      data.stream  = strm;                          \
      data.event   = evnt;                          \
      srtp_event_handler(&data);                    \
}   

#ifdef __cplusplus
}
#endif

#endif /* SRTP_PRIV_H */
