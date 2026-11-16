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

#ifndef PAS_BITFIELD_VECTOR_H
#define PAS_BITFIELD_VECTOR_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#define PAS_BITFIELD_VECTOR_BITS_PER_WORD 32

/* A bitfield vector can be used to store bitfields that have a power-of-2 number of bits
   ranging from 1 bit to 32 bits. */

#define PAS_BITFIELD_VECTOR_NUM_WORDS(num_fields, num_bits_per_field) \
    (((num_fields) * (num_bits_per_field) + 31) >> 5)
#define PAS_BITFIELD_VECTOR_NUM_BYTES(num_fields, num_bits_per_field) \
    (PAS_BITFIELD_VECTOR_NUM_WORDS((num_fields), (num_bits_per_field)) * sizeof(unsigned))
#define PAS_BITFIELD_VECTOR_NUM_FIELDS(num_words, num_bits_per_field) \
    (((num_words) << 5) / (num_bits_per_field))

#define PAS_BITFIELD_VECTOR_WORD_INDEX(field_index, num_bits_per_field) \
    (((field_index) * (num_bits_per_field)) >> 5)
#define PAS_BITFIELD_VECTOR_FIELD_INDEX(word_index, num_bits_per_field) \
    (((word_index) << 5) / (num_bits_per_field))
#define PAS_BITFIELD_VECTOR_FIELD_SHIFT(field_index, num_bits_per_field) \
    (((field_index) * (num_bits_per_field)) & 31)
#define PAS_BITFIELD_VECTOR_FIELD_MASK(num_bits_per_field) \
    ((unsigned)((((uint64_t)1) << (num_bits_per_field)) - 1))

static inline unsigned pas_bitfield_vector_get(const unsigned* bits,
                                               unsigned num_bits_per_field,
                                               size_t index)
{
    return (bits[PAS_BITFIELD_VECTOR_WORD_INDEX(index, num_bits_per_field)] >>
            PAS_BITFIELD_VECTOR_FIELD_SHIFT(index, num_bits_per_field)) &
        PAS_BITFIELD_VECTOR_FIELD_MASK(num_bits_per_field);
}

static inline void pas_bitfield_vector_set(unsigned* bits, unsigned num_bits_per_field,
                                           size_t index, unsigned value)
{
    unsigned* ptr;
    unsigned word;
    unsigned mask;
    unsigned shift;
    
    ptr = bits + PAS_BITFIELD_VECTOR_WORD_INDEX(index, num_bits_per_field);
    
    mask = PAS_BITFIELD_VECTOR_FIELD_MASK(num_bits_per_field);
    shift = PAS_BITFIELD_VECTOR_FIELD_SHIFT(index, num_bits_per_field);
    
    PAS_ASSERT(value <= mask);
    
    word = *ptr;

    word &= ~(mask << shift);
    word |= value << shift;
    
    *ptr = word;
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIELD_VECTOR_H */

