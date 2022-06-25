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

#ifndef PAS_FOUND_BIT_INDEX_H
#define PAS_FOUND_BIT_INDEX_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_found_bit_index;
typedef struct pas_found_bit_index pas_found_bit_index;

struct pas_found_bit_index {
    bool did_succeed;
    size_t index;
    size_t word_index;
    unsigned word;
};

static inline pas_found_bit_index pas_found_bit_index_create_empty(void)
{
    pas_found_bit_index result;
    result.did_succeed = false;
    result.index = PAS_SIZE_MAX;
    result.word_index = PAS_SIZE_MAX;
    result.word = 0;
    return result;
}

static inline pas_found_bit_index pas_found_bit_index_create(
    size_t index, size_t word_index, unsigned word)
{
    pas_found_bit_index result;
    result.did_succeed = true;
    result.index = index;
    result.word_index = word_index;
    result.word = word;
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_FOUND_BIT_INDEX_H */

