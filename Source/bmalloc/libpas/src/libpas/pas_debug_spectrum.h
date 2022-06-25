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

#ifndef PAS_DEBUG_SPECTRUM_H
#define PAS_DEBUG_SPECTRUM_H

#include "pas_ptr_hash_map.h"

PAS_BEGIN_EXTERN_C;

struct pas_debug_spectrum_entry;
struct pas_stream;
typedef struct pas_debug_spectrum_entry pas_debug_spectrum_entry;
typedef struct pas_stream pas_stream;

typedef void (*pas_debug_spectrum_dump_key)(pas_stream* stream, void* key);

struct pas_debug_spectrum_entry {
    pas_debug_spectrum_dump_key dump;
    uint64_t count;
};

#define PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT 0

PAS_API extern pas_ptr_hash_map pas_debug_spectrum;

PAS_API void pas_debug_spectrum_add(
    void* key, pas_debug_spectrum_dump_key dump, uint64_t count);

PAS_API void pas_debug_spectrum_dump(pas_stream* stream);

/* This resets all counts to zero. However, it doesn't forget about the things that are already in
   the spectrum, so you can't use this to change your mind about dump methods. */
PAS_API void pas_debug_spectrum_reset(void);

PAS_END_EXTERN_C;

#endif /* PAS_DEBUG_SPECTRUM_H */

