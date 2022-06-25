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

#ifndef PAS_COMPACT_EXPENDABLE_MEMORY_H
#define PAS_COMPACT_EXPENDABLE_MEMORY_H

#include "pas_expendable_memory.h"

PAS_BEGIN_EXTERN_C;

#define PAS_COMPACT_EXPENDABLE_MEMORY_PAYLOAD_SIZE 20lu * 1024lu * 1024lu
#define PAS_COMPACT_EXPENDABLE_MEMORY_HEADER_SIZE \
    (PAS_OFFSETOF(pas_expendable_memory, states) + \
     PAS_COMPACT_EXPENDABLE_MEMORY_PAYLOAD_SIZE / PAS_EXPENDABLE_MEMORY_PAGE_SIZE \
     * sizeof(pas_expendable_memory_state))

union pas_compact_expendable_memory;
typedef union pas_compact_expendable_memory pas_compact_expendable_memory;

union pas_compact_expendable_memory {
    pas_expendable_memory header;
    char fake_field_to_force_size[PAS_COMPACT_EXPENDABLE_MEMORY_HEADER_SIZE];
};

PAS_API extern pas_compact_expendable_memory pas_compact_expendable_memory_header;
PAS_API extern void* pas_compact_expendable_memory_payload;

PAS_API void* pas_compact_expendable_memory_allocate(size_t size,
                                                     size_t alignment,
                                                     const char* name);

PAS_API bool pas_compact_expendable_memory_commit_if_necessary(void* object, size_t size);

static inline void pas_compact_expendable_memory_note_use(void* object, size_t size)
{
    pas_expendable_memory_note_use(
        &pas_compact_expendable_memory_header.header, pas_compact_expendable_memory_payload, object, size);
}

static PAS_ALWAYS_INLINE bool pas_compact_expendable_memory_touch(
    void* object, size_t size, pas_expendable_memory_touch_kind kind)
{
    switch (kind) {
    case pas_expendable_memory_touch_to_note_use:
        pas_compact_expendable_memory_note_use(object, size);
        return false;
    case pas_expendable_memory_touch_to_commit_if_necessary:
        return pas_compact_expendable_memory_commit_if_necessary(object, size);
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

PAS_API bool pas_compact_expendable_memory_scavenge(pas_expendable_memory_scavenge_kind kind);

PAS_END_EXTERN_C;

#endif /* PAS_COMPACT_EXPENDABLE_MEMORY_H */

