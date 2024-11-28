/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef PAS_ALLOCATION_MODE_H
#define PAS_ALLOCATION_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_allocation_mode {
    /* We are allocating an object from ordinary memory and don't plan on
       compacting its address. */
    pas_non_compact_allocation_mode,

    /* We are allocating an object from ordinary memory and expect to
       be able to compact its address, but don't expect all addresses in
       that memory to be trivially compactible. */
    pas_maybe_compact_allocation_mode,

    /* We are allocating an object from memory where all addresses within
       that memory are trivially compactible, like in the immortal heap. */
    pas_always_compact_allocation_mode,
};

typedef enum pas_allocation_mode pas_allocation_mode;
typedef enum pas_allocation_mode __pas_allocation_mode;

static inline const char* pas_allocation_mode_get_string(pas_allocation_mode allocation_mode)
{
    switch (allocation_mode) {
    case pas_non_compact_allocation_mode:
        return "non-compact";
    case pas_maybe_compact_allocation_mode:
        return "maybe compact";
    case pas_always_compact_allocation_mode:
        return "always compact";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_ALLOCATION_MODE_H */
