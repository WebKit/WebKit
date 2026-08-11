/*
 * alloc.h
 *
 * interface to memory allocation and deallocation, with optional debugging
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *
 * Copyright (c) 2001-2017 Cisco Systems, Inc.
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

#ifndef CRYPTO_ALLOC_H
#define CRYPTO_ALLOC_H

#include "datatypes.h"

#if defined(WEBRTC_WEBKIT_BUILD)
#include <stdlib.h>  // For _MALLOC_TYPED and malloc_type_id_t.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * srtp_crypto_alloc
 *
 * Allocates a block of memory  of given size. The memory will be
 * initialized to zero's. Free the memory with a call to srtp_crypto_free.
 *
 * returns pointer to memory on success or else NULL
 */
#if defined(WEBRTC_WEBKIT_BUILD) && defined(_MALLOC_TYPE_ENABLED) && _MALLOC_TYPE_ENABLED
void *srtp_crypto_alloc_typed(size_t size, malloc_type_id_t type_id);
void *srtp_crypto_alloc(size_t size) _MALLOC_TYPED(srtp_crypto_alloc_typed, 1);
#else
void *srtp_crypto_alloc(size_t size);
#endif

/*
 * srtp_crypto_free
 *
 * Frees the block of memory  ptr previously  allocated with
 * srtp_crypto_alloc
 */
void srtp_crypto_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_ALLOC_H */
