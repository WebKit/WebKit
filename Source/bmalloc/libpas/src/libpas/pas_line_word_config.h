/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LINE_WORD_CONFIG_H
#define PAS_LINE_WORD_CONFIG_H

#include "pas_log.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_line_word_config;
typedef struct pas_line_word_config pas_line_word_config;

struct pas_line_word_config {
    uint64_t (*get_line_word)(unsigned* alloc_bits, size_t line_index);
    void (*set_line_word)(unsigned* alloc_bits, size_t line_index, uint64_t value);
    uint64_t (*count_low_zeroes)(uint64_t value);
    uint64_t (*count_high_zeroes)(uint64_t value);
};

#define PAS_LINE_WORD_CONFIG_DEFINE_FUNCTIONS(bits_per_line_word) \
    static PAS_ALWAYS_INLINE uint64_t \
    pas_line_word_config_get_line_word_ ## bits_per_line_word ## _bit( \
        unsigned* alloc_bits, size_t line_index) \
    { \
        return ((uint ## bits_per_line_word ## _t*)alloc_bits)[line_index]; \
    } \
    \
    static PAS_ALWAYS_INLINE void \
    pas_line_word_config_set_line_word_ ## bits_per_line_word ## _bit( \
        unsigned* alloc_bits, size_t line_index, uint64_t value) \
    { \
        static const bool verbose = false; \
        if (verbose) \
            pas_log("Setting %p[%zu] := %llx\n", alloc_bits, line_index, value); \
        ((uint ## bits_per_line_word ## _t*)alloc_bits)[line_index] = \
            (uint ## bits_per_line_word ## _t)value; \
    }

PAS_LINE_WORD_CONFIG_DEFINE_FUNCTIONS(8);
PAS_LINE_WORD_CONFIG_DEFINE_FUNCTIONS(16);
PAS_LINE_WORD_CONFIG_DEFINE_FUNCTIONS(32);
PAS_LINE_WORD_CONFIG_DEFINE_FUNCTIONS(64);

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_low_zeroes_8_bit(uint64_t value)
{
    return __builtin_ctz((unsigned)value);
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_high_zeroes_8_bit(uint64_t value)
{
    unsigned leading_zeroes;
    leading_zeroes = __builtin_clz((unsigned)(value & 0xff));
    PAS_TESTING_ASSERT(leading_zeroes >= 24);
    return leading_zeroes - 24;
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_low_zeroes_16_bit(uint64_t value)
{
    return __builtin_ctz((unsigned)value);
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_high_zeroes_16_bit(uint64_t value)
{
    unsigned leading_zeroes;
    leading_zeroes = __builtin_clz((unsigned)(value & 0xffff));
    PAS_TESTING_ASSERT(leading_zeroes >= 16);
    return leading_zeroes - 16;
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_low_zeroes_32_bit(uint64_t value)
{
    return __builtin_ctz((unsigned)value);
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_high_zeroes_32_bit(uint64_t value)
{
    return __builtin_clz((unsigned)value);
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_low_zeroes_64_bit(uint64_t value)
{
    return __builtin_ctzll(value);
}

static PAS_ALWAYS_INLINE uint64_t
pas_line_word_config_count_high_zeroes_64_bit(uint64_t value)
{
    return __builtin_clzll(value);
}

static PAS_ALWAYS_INLINE void pas_line_word_config_construct(pas_line_word_config* config,
                                                             size_t bits_per_line_word)
{

#define PAS_LINE_WORD_CONFIG_CONSTRUCT_CASE(bits_per_line_word) \
    case bits_per_line_word: \
        config->get_line_word = pas_line_word_config_get_line_word_ ## bits_per_line_word ## _bit; \
        config->set_line_word = pas_line_word_config_set_line_word_ ## bits_per_line_word ## _bit; \
        config->count_low_zeroes = \
            pas_line_word_config_count_low_zeroes_ ## bits_per_line_word ## _bit; \
        config->count_high_zeroes = \
            pas_line_word_config_count_high_zeroes_ ## bits_per_line_word ## _bit; \
        break;

    switch (bits_per_line_word) {
    PAS_LINE_WORD_CONFIG_CONSTRUCT_CASE(8);
    PAS_LINE_WORD_CONFIG_CONSTRUCT_CASE(16);
    PAS_LINE_WORD_CONFIG_CONSTRUCT_CASE(32);
    PAS_LINE_WORD_CONFIG_CONSTRUCT_CASE(64);
    default:
        PAS_ASSERT(!"Bad value for bits_per_line_word");
        break;
    }
}

PAS_END_EXTERN_C;

#endif /* PAS_LINE_WORD_CONFIG_H */

