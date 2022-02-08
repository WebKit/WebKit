/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_EXPENDABLE_MEMORY_H
#define PAS_EXPENDABLE_MEMORY_H

#include "pas_heap_kind.h"

PAS_BEGIN_EXTERN_C;

struct pas_expendable_memory;
typedef struct pas_expendable_memory pas_expendable_memory;

typedef uint64_t pas_expendable_memory_state;
typedef unsigned pas_expendable_memory_state_kind;
typedef uint64_t pas_expendable_memory_state_version;

#define PAS_EXPENDABLE_MEMORY_STATE_NUM_KIND_BITS       ((uint64_t)3)
#define PAS_EXPENDABLE_MEMORY_STATE_KIND_MASK \
    (((uint64_t)1 << PAS_EXPENDABLE_MEMORY_STATE_NUM_KIND_BITS) - (uint64_t)1)
#define PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED    0u
#define PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR       1u
#define PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED      2u
#define PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED  5u

struct pas_expendable_memory {
    unsigned bump;
    unsigned size;
    pas_expendable_memory_state states[1];
};

#define PAS_EXPENDABLE_MEMORY_PAGE_SIZE 16384lu

PAS_API extern pas_expendable_memory_state_version pas_expendable_memory_version_counter;

enum pas_expendable_memory_touch_kind {
    pas_expendable_memory_touch_to_note_use,
    pas_expendable_memory_touch_to_commit_if_necessary
};

typedef enum pas_expendable_memory_touch_kind pas_expendable_memory_touch_kind;

enum pas_expendable_memory_scavenge_kind {
    /* Decommits only things that haven't been used recently, and does the count increment that allows us
       to tell that something hasn't been used recently. */
    pas_expendable_memory_scavenge_periodic,

    /* Decommits everything that it can. */
    pas_expendable_memory_scavenge_forced,

    /* Pretends to decommit everything that it can without making any syscalls (useful for testing). */
    pas_expendable_memory_scavenge_forced_fake,
};

typedef enum pas_expendable_memory_scavenge_kind pas_expendable_memory_scavenge_kind;

static inline pas_expendable_memory_state_kind
pas_expendable_memory_state_get_kind(pas_expendable_memory_state state)
{
    return (pas_expendable_memory_state_kind)(state & PAS_EXPENDABLE_MEMORY_STATE_KIND_MASK);
}

static inline pas_expendable_memory_state_version
pas_expendable_memory_state_get_version(pas_expendable_memory_state state)
{
    return state >> PAS_EXPENDABLE_MEMORY_STATE_NUM_KIND_BITS;
}

PAS_API pas_expendable_memory_state_version pas_expendable_memory_state_version_next(void);

static inline pas_expendable_memory_state pas_expendable_memory_state_create(
    pas_expendable_memory_state_kind kind,
    pas_expendable_memory_state_version version)
{
    pas_expendable_memory_state result;

    result = kind | (version << PAS_EXPENDABLE_MEMORY_STATE_NUM_KIND_BITS);

    PAS_TESTING_ASSERT(pas_expendable_memory_state_get_kind(result) == kind);
    PAS_TESTING_ASSERT(pas_expendable_memory_state_get_version(result) == version);

    return result;
}

static inline pas_expendable_memory_state pas_expendable_memory_state_with_kind(
    pas_expendable_memory_state state,
    pas_expendable_memory_state_kind kind)
{
    return pas_expendable_memory_state_create(
        kind, pas_expendable_memory_state_get_version(state));
}

static inline pas_expendable_memory_state pas_expendable_memory_state_with_version(
    pas_expendable_memory_state state,
    pas_expendable_memory_state_version version)
{
    return pas_expendable_memory_state_create(
        pas_expendable_memory_state_get_kind(state), version);
}

PAS_API void pas_expendable_memory_construct(pas_expendable_memory* memory,
                                             size_t size);

PAS_API void* pas_expendable_memory_try_allocate(pas_expendable_memory* header,
                                                 void* payload,
                                                 size_t size,
                                                 size_t alignment,
                                                 pas_heap_kind heap_kind,
                                                 const char* name);

PAS_API void* pas_expendable_memory_allocate(pas_expendable_memory* header,
                                             void* payload,
                                             size_t size,
                                             size_t alignment,
                                             pas_heap_kind heap_kind,
                                             const char* name);

/* Call this while holding the heap_lock, not just because it modifies state that is protected by that lock,
   but because anytime you release the heap_lock, the memory may be decommitted.
   
   Like most bool-returning functions in libpas, this returns true if something happened.
   
   So:
   
   If this returns false: it means that the memory was already committed.
   If this returns true: it means that the memory was previously decommitted and was now newly committed. */
PAS_API bool pas_expendable_memory_commit_if_necessary(pas_expendable_memory* header,
                                                       void* payload,
                                                       void* object,
                                                       size_t size);

enum pas_expendable_memoy_note_use_impl_kind {
    pas_expendable_memoy_note_use_impl_first,
    pas_expendable_memoy_note_use_impl_last,
};

typedef enum pas_expendable_memoy_note_use_impl_kind pas_expendable_memoy_note_use_impl_kind;

static PAS_ALWAYS_INLINE void pas_expendable_memory_note_use_impl(
    pas_expendable_memory_state* state,
    pas_expendable_memoy_note_use_impl_kind impl_kind)
{
    for (;;) {
        pas_expendable_memory_state old_state;
        pas_expendable_memory_state_kind old_kind;

        old_state = *state;
        old_kind = pas_expendable_memory_state_get_kind(old_state);
        
        if (PAS_LIKELY(old_kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED))
            return;

        if (old_kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR) {
            /* If we hit this for the last page, then this is a funny case. It means:
               
               -> We allocated an object of size X.
               
               -> We note_use'd it as if it had size Y, where Y < X.
               
               And the last page according to size Y ended up being interior to the object.
               
               Luckily, this is totally fine! Interior page decommit is governed by the first page. So, in
               this case, if we just note_use the first page, it's all good.
            
               On the other hand, if we hit this for the first page, then something is not OK. */
            PAS_ASSERT(impl_kind == pas_expendable_memoy_note_use_impl_last);
            return;
        }

        if (old_kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED)
            return;

        if (pas_compare_and_swap_uint64_weak(
                state,
                old_state,
                pas_expendable_memory_state_with_kind(
                    old_state, PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED)))
            return;
    }
}

static PAS_ALWAYS_INLINE void pas_expendable_memory_note_use(pas_expendable_memory* header,
                                                             void* payload,
                                                             void* object,
                                                             size_t size)
{
    size_t first;
    size_t last;
    uintptr_t offset;

    PAS_TESTING_ASSERT(object);
    PAS_TESTING_ASSERT(size);

    /* If we're testing, make sure that any reads from the header happen *after* we have allocated the
       object. If we're not testing, then it doesn't matter, because in the worst case we'll just see
       decommited page states and do nothing. It's OK for us to sometimes do nothing. */
    if (PAS_ENABLE_TESTING)
        header += pas_depend((uintptr_t)object);

    offset = (uintptr_t)object - (uintptr_t)payload;
    PAS_TESTING_ASSERT(offset >= sizeof(pas_expendable_memory_state_version));
    PAS_TESTING_ASSERT(offset < header->size);
    PAS_TESTING_ASSERT(offset + size <= header->size);
    PAS_TESTING_ASSERT(offset < header->bump);
    PAS_TESTING_ASSERT(offset + size <= header->bump);

    first = (offset - sizeof(pas_expendable_memory_state_version)) / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;
    last = (offset + size - 1) / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;

    pas_expendable_memory_note_use_impl(header->states + first, pas_expendable_memoy_note_use_impl_first);
    if (first != last)
        pas_expendable_memory_note_use_impl(header->states + last, pas_expendable_memoy_note_use_impl_last);
}

/* Returns true if there is possibly more scavenging to do later. */
PAS_API bool pas_expendable_memory_scavenge(pas_expendable_memory* header,
                                            void* payload,
                                            pas_expendable_memory_scavenge_kind kind);

static inline unsigned pas_expendable_memory_num_pages(pas_expendable_memory* header)
{
    PAS_ASSERT(pas_is_aligned(header->size, PAS_EXPENDABLE_MEMORY_PAGE_SIZE));
    return header->size / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;
}

static inline unsigned pas_expendable_memory_num_pages_in_use(pas_expendable_memory* header)
{
    uintptr_t result;

    result = pas_round_up_to_power_of_2(header->bump, PAS_EXPENDABLE_MEMORY_PAGE_SIZE)
        / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;

    PAS_ASSERT((unsigned)result == result);
    return (unsigned)result;
}

PAS_END_EXTERN_C;

#endif /* PAS_EXPENDABLE_MEMORY_H */

