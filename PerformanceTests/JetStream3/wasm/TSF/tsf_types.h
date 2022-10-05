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

#ifndef FP_TSF_TYPES_H
#define FP_TSF_TYPES_H

#include "tsf_config.h"

/* need to use this hack because NetBSD sucks. */
#include "tsf_inttypes.h"

/* our own definition of UINT32_MAX, which we need to use in all external
 * header files because C++ is dumb. */
#define TSF_UINT32_MAX ((((uint32_t)1)<<31)+\
                        ((((uint32_t)1)<<31)-1))

/* something that inttypes doesn't seem to define */
#if TSF_SIZEOF_VOID_P == 2
#define TSF_UINTPTR_C(arg) UINT16_C(arg)
#else
#if TSF_SIZEOF_VOID_P == 4
#define TSF_UINTPTR_C(arg) UINT32_C(arg)
#else
#if TSF_SIZEOF_VOID_P == 8
#define TSF_UINTPTR_C(arg) UINT64_C(arg)
#else
#error "Don't know how to express void * literals"
#endif
#endif
#endif

/* every system hacker needs sys/types! */
#include <sys/types.h>

/* our own offset type.  meant to be entirely compatible with the
 * system's offset type. */
#if TSF_SIZEOF_OFF_T == 2   /* who knows? */
typedef int16_t tsf_off_t;
#else
#if TSF_SIZEOF_OFF_T == 4
typedef int32_t tsf_off_t;
#else
#if TSF_SIZEOF_OFF_T == 8
typedef int64_t tsf_off_t;
#else
#error "Don't know how to represent file offsets"
#endif
#endif
#endif

/* why not use std_bool.h?  becuase it will fuck us if this header is
 * used from C++!  there seems to be no good way of making sure that
 * the C99 _Bool type and the C++ bool type are the same size, and,
 * what is more unnerving is that there is no good way, if one is
 * writing ANSI C to conjure up a type that is magically the same size
 * as the C++ bool type.  So, the only solution is to not use _Bool
 * or bool, and just roll our own thing.  that way, you know what size
 * it is from either C or C++. */
typedef uint8_t tsf_bool_t;

/* use this for va_arg */
typedef unsigned tsf_arg_bool_t;

/* you don't actually have to use these macros.  you can use 1
 * and 0 directly.  or if you're in C++, you should just be able
 * to use C++'s true and false keywords. */
#define tsf_true 1
#define tsf_false 0

/* a tristate type, which might come in handy */
typedef enum {
    tsf_no,
    tsf_maybe,
    tsf_yes
} tsf_tristate_t;

/* TSF has a notion of integers that work just like 32-bit or 64-bit
 * integers, but are encoded with some very basic compaction. It's
 * acceptable to use int32_t or int64_t directly for these, though
 * it's slightly better to use the typedefs. */
typedef int32_t tsf_integer_t;
typedef uint32_t tsf_unsigned_t; /* This type is not exposed to the user but it is
                                    used internally for various things, like choice
                                    type formatting. */
typedef int64_t tsf_long_t;

/* Constants for the TSF_TK_INTEGER and TSF_TK_LONG compaction protocols. */
#define TSF_INTEGER_SMALL_LIM   64
#define TSF_INTEGER_MEDIUM_LIM  16192
#define TSF_INTEGER_LARGE_LIM   8404800
#define TSF_INTEGER_MEDIUM_BIT  128
#define TSF_INTEGER_MEDIUM_DIFF 1
#define TSF_INTEGER_LARGE_TAG   ((int8_t)(62 | TSF_INTEGER_MEDIUM_BIT))
#define TSF_INTEGER_HUGE_TAG    ((int8_t)(63 | TSF_INTEGER_MEDIUM_BIT))
#define TSF_INTEGER_MAX_SIZE    5
#define TSF_KONG_MAX_SIZE       9

/* some constants for TSF unsigned integers. */
#define TSF_UNSIGNED_SMALL_LIM   128
#define TSF_UNSIGNED_MEDIUM_LIM  32384
#define TSF_UNSIGNED_LARGE_LIM   16809600
#define TSF_UNSIGNED_MEDIUM_BIT  128
#define TSF_UNSIGNED_LARGE_TAG   254
#define TSF_UNSIGNED_HUGE_TAG    255
#define TSF_UNSIGNED_MAX_SIZE    5

/* what follows are some types that must be pre-declared. */

struct gpc_proto;
typedef struct gpc_proto gpc_proto_t;

struct gpc_program;
typedef struct gpc_program gpc_program_t;

enum tsf_zip_mode {
    TSF_ZIP_NONE,
    TSF_ZIP_ZLIB,
    TSF_ZIP_BZIP2,
    TSF_ZIP_UNKNOWN
};

typedef enum tsf_zip_mode tsf_zip_mode_t;

#endif

