/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_FULL_ALLOC_BITS_H
#define PAS_FULL_ALLOC_BITS_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_full_alloc_bits;
typedef struct pas_full_alloc_bits pas_full_alloc_bits;

struct pas_full_alloc_bits {
    unsigned* bits;
    unsigned word_index_begin;
    unsigned word_index_end;
};

static inline pas_full_alloc_bits pas_full_alloc_bits_create_empty(void)
{
    pas_full_alloc_bits result;
    result.bits = NULL;
    result.word_index_begin = 0;
    result.word_index_end = 0;
    return result;
}

static inline pas_full_alloc_bits pas_full_alloc_bits_create(unsigned* bits,
                                                             unsigned word_index_begin,
                                                             unsigned word_index_end)
{
    pas_full_alloc_bits result;
    result.bits = bits;
    result.word_index_begin = word_index_begin;
    result.word_index_end = word_index_end;
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_FULL_ALLOC_BITS_H */

