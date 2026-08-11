/*
 * Copyright (C) 2003, 2004, 2005 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef FP_TSF_FORMAT_H
#define FP_TSF_FORMAT_H

#include "tsf_config.h"

#include <inttypes.h>

#define fi8 "%" PRId8
#define fui8 "%" PRIu8
#define fi16 "%" PRId16
#define fui16 "%" PRIu16
#define fi32 "%" PRId32
#define fui32 "%" PRIu32
#define fi64 "%" PRId64
#define fui64 "%" PRIu64

#if TSF_SIZEOF_SIZE_T == 2
#define fsz fui16
#else
#if TSF_SIZEOF_SIZE_T == 4
#define fsz fui32
#else
#if TSF_SIZEOF_SIZE_T == 8
#define fsz fui64
#else
#error "Don't know how to print size_t"
#endif
#endif
#endif

#if TSF_SIZEOF_VOID_P == 2
#define fptr fi16
#define fuptr fui16
#else
#if TSF_SIZEOF_VOID_P == 4
#define fptr fi32
#define fuptr fui32
#else
#if TSF_SIZEOF_VOID_P == 8
#define fptr fi64
#define fuptr fui64
#else
#error "Don't know how to print void *"
#endif
#endif
#endif

#endif


