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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_expendable_memory.h"

#include "pas_allocation_callbacks.h"
#include "pas_heap_lock.h"
#include "pas_page_malloc.h"

pas_expendable_memory_state_version pas_expendable_memory_version_counter = 1;

pas_expendable_memory_state_version pas_expendable_memory_state_version_next(void)
{
    pas_expendable_memory_state_version result;
    pas_heap_lock_assert_held();
    result = ++pas_expendable_memory_version_counter;
    PAS_ASSERT(result > 1);
    return result;
}

void pas_expendable_memory_construct(pas_expendable_memory* memory,
                                     size_t size)
{
    size_t index;

    memory->bump = 0;
    PAS_ASSERT((unsigned)size == size);
    memory->size = (unsigned)size;

    PAS_ASSERT(pas_is_aligned(size, PAS_EXPENDABLE_MEMORY_PAGE_SIZE));

    for (index = size / PAS_EXPENDABLE_MEMORY_PAGE_SIZE; index--;) {
        memory->states[index] =
            pas_expendable_memory_state_create(PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED, 1);
    }
}

void* pas_expendable_memory_try_allocate(pas_expendable_memory* header,
                                         void* payload,
                                         size_t size,
                                         size_t alignment,
                                         pas_heap_kind heap_kind,
                                         const char* name)
{
    size_t aligned_bump;
    size_t new_aligned_bump;
    size_t index;
    size_t first;
    size_t last;
    void* result;
    pas_expendable_memory_state_version version;

    pas_heap_lock_assert_held();

    PAS_ASSERT(pas_is_aligned(header->size, PAS_EXPENDABLE_MEMORY_PAGE_SIZE));
    PAS_ASSERT(heap_kind == pas_compact_expendable_heap_kind
               || heap_kind == pas_large_expendable_heap_kind);

    if (!size)
        return NULL;

    aligned_bump = header->bump;
    if (aligned_bump >= header->size
        || header->size - aligned_bump < sizeof(pas_expendable_memory_state_version))
        return NULL;

    aligned_bump += sizeof(pas_expendable_memory_state_version);

    aligned_bump = pas_round_up_to_power_of_2(aligned_bump, alignment);
    if (aligned_bump >= header->size
        || header->size - aligned_bump < size)
        return NULL;

    first = (aligned_bump - sizeof(pas_expendable_memory_state_version)) / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;
    last = (aligned_bump + size - 1) / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;

    version = PAS_MAX(pas_expendable_memory_state_get_version(header->states[first]),
                      pas_expendable_memory_state_get_version(header->states[last]));

    header->states[first] =
        pas_expendable_memory_state_create(PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED, version);
    header->states[last] =
        pas_expendable_memory_state_create(PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED, version);
    for (index = first + 1; index < last; ++index) {
        header->states[index] =
            pas_expendable_memory_state_create(PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR, version);
    }

    new_aligned_bump = aligned_bump + size;
    PAS_ASSERT((unsigned)new_aligned_bump == new_aligned_bump);
    header->bump = (unsigned)new_aligned_bump;

    result = (char*)payload + aligned_bump;

    ((pas_expendable_memory_state_version*)result)[-1] = version;

    pas_did_allocate(result, size, heap_kind, name, pas_object_allocation);

    pas_store_store_fence();
    
    return result;
}

void* pas_expendable_memory_allocate(pas_expendable_memory* header,
                                     void* payload,
                                     size_t size,
                                     size_t alignment,
                                     pas_heap_kind heap_kind,
                                     const char* name)
{
    void* result;
    result = pas_expendable_memory_try_allocate(header, payload, size, alignment, heap_kind, name);
    PAS_ASSERT(result || !size);
    return result;
}

bool pas_expendable_memory_commit_if_necessary(pas_expendable_memory* header,
                                               void* payload,
                                               void* object,
                                               size_t size)
{
    size_t first;
    size_t last;
    uintptr_t offset;
    pas_expendable_memory_state_version header_version;
    pas_expendable_memory_state first_state;
    pas_expendable_memory_state_kind first_kind;
    pas_expendable_memory_state_version first_version;
    pas_expendable_memory_state last_state;
    pas_expendable_memory_state_kind last_kind;
    pas_expendable_memory_state_version last_version;
    pas_expendable_memory_state_version new_version;
    pas_expendable_memory_state new_state;

    pas_heap_lock_assert_held();

    PAS_TESTING_ASSERT(pas_is_aligned(header->size, PAS_EXPENDABLE_MEMORY_PAGE_SIZE));

    offset = (uintptr_t)object - (uintptr_t)payload;
    PAS_TESTING_ASSERT(offset >= sizeof(pas_expendable_memory_state_version));
    PAS_TESTING_ASSERT(offset < header->size);
    PAS_TESTING_ASSERT(offset + size <= header->size);
    PAS_TESTING_ASSERT(offset < header->bump);
    PAS_TESTING_ASSERT(offset + size <= header->bump);

    first = (offset - sizeof(pas_expendable_memory_state_version)) / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;
    last = (offset + size - 1) / PAS_EXPENDABLE_MEMORY_PAGE_SIZE;

    header_version = ((pas_expendable_memory_state_version*)object)[-1];
    first_state = header->states[first];
    first_kind = pas_expendable_memory_state_get_kind(first_state);
    first_version = pas_expendable_memory_state_get_version(first_state);

    PAS_TESTING_ASSERT(first_kind != PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR);

    if (PAS_LIKELY(first == last))
        goto ignore_last_page;
    
    last_state = header->states[last];
    last_kind = pas_expendable_memory_state_get_kind(last_state);

    if (last_kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR) {
        /* This is a funny case. It means:
           
           -> We allocated an object of size X.
           
           -> We note_use'd it as if it had size Y, where Y < X.
           
           And the last page according to size Y ended up being interior to the object.
           
           Luckily, this is totally fine! Interior page decommit is governed by the first page. So, in
           this case, if we just ignore the last page, and it's all good. */
        goto ignore_last_page;
    }
    
    last_version = pas_expendable_memory_state_get_version(last_state);

    if (first_version == header_version && last_version == header_version) {
        /* The versions matching is proof that we had never decommitted any page that this object straddles.
           It also proves that we had never decommitted the pages and then recommitted then with a prior call
           to commit_if_necessary on some other object that shares any of those pages.
        
           Note that there is a partial redundancy between the state_kind and version matching. For the purpose
           of reasoning about the state, let's imagine a sequence of operations that can happend to this memory
           that comprises these actions: allocate, commit_if_necessary, decommit-request, and decommit-actual.
           We don't care about the ancient history, but only the recent things that happened.
        
           -> versions match and state is not decommitted: the last thing that happened to this object was that
              it had been allocated or committed. It's not possible for there to have been a decommit (request
              or actual) that happened more recently than allocate or commit_if_necessary.
        
           -> versions don't match and state is not decommitted: the last thing that happened to this object
              was decommit-request and maybe decommit-actual. Some *other* object in the same pages may have had
              an even more recent commit_if_necessary or allocate.
        
           -> versions don't match and state is decommitted: the last thing that happened to this object and to
              the pages it occupies was decommit-request and maybe decommit-actual. No other objects in the same
              pages could have had a more recent commit_if_necessary or allocate.
        
           -> versions match and state is decommitted: impossible. */
        
        PAS_TESTING_ASSERT(first_kind != PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED);
        PAS_TESTING_ASSERT(last_kind != PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED);
        return false;
    }

    PAS_ASSERT(first_version >= header_version);

    /* We'd like to assert that last_version >= header_version, except that it's possible for someone to
       do a commit_if_necessary on the prefix of this object, and then not update last_version. So,
       last_version could be stuck arbitrarily in the past. */

    new_version = pas_expendable_memory_state_version_next();
    new_state = pas_expendable_memory_state_create(PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED, new_version);

    header->states[first] = new_state;
    header->states[last] = new_state;

    goto done;

ignore_last_page:
    if (first_version == header_version) {
        PAS_TESTING_ASSERT(first_kind != PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED);
        return false;
    }

    PAS_ASSERT(first_version > header_version);

    new_version = pas_expendable_memory_state_version_next();
    new_state = pas_expendable_memory_state_create(PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED, new_version);

    header->states[first] = new_state;

done:
    ((pas_expendable_memory_state_version*)object)[-1] = new_version;
    return true;
}

static PAS_ALWAYS_INLINE bool scavenge_impl(pas_expendable_memory* header,
                                            void* payload,
                                            pas_expendable_memory_scavenge_kind scavenge_kind)
{
    size_t index;
    size_t index_end;
    bool result;
    pas_expendable_memory_state_version decommit_version;
    
    pas_heap_lock_assert_held();

    decommit_version = pas_expendable_memory_state_version_next();

    PAS_ASSERT(header->size);
    PAS_ASSERT(pas_is_aligned(header->size, PAS_EXPENDABLE_MEMORY_PAGE_SIZE));
    PAS_ASSERT(header->bump < header->size);

    index_end = pas_expendable_memory_num_pages_in_use(header);
    result = false;

    /* This loop needs to be super efficient in the case where bytes are in the range
       [JUST_USED, MAX_JUST_USED). In all other cases, we're going to call decommit, so then we just want to
       minimize the number of times we call decommit. */
    for (index = 0; index < index_end; ++index) {
        pas_expendable_memory_state state;
        pas_expendable_memory_state_kind kind;
        size_t other_index;

        state = header->states[index];
        kind = pas_expendable_memory_state_get_kind(state);

        if (kind <= PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR) {
            PAS_TESTING_ASSERT(kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED
                               || kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR);
            continue;
        }

        PAS_TESTING_ASSERT(kind >= PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED
                           && kind <= PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED);

        if (scavenge_kind == pas_expendable_memory_scavenge_periodic) {
            if (PAS_LIKELY(kind < PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED)) {
                header->states[index] = pas_expendable_memory_state_with_kind(state, kind + 1);
                result = true;
                continue;
            }
            
            /* This part of the loop doesn't have to be as fast, since we're going to do a decommit anyway. We
               just want to make sure we only do one decommit. To that end, we look for a span of MAX_JUST_USED
               and INTERIOR. If we see nothing but those two values, then we're in a decommit span. */
            PAS_ASSERT(kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED);
        }

        for (other_index = index;
             other_index < index_end;
             ++other_index) {
            state = header->states[other_index];
            kind = pas_expendable_memory_state_get_kind(state);
            if (kind == PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR)
                continue;
            if (scavenge_kind == pas_expendable_memory_scavenge_periodic) {
                if (kind != PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED) {
                    PAS_TESTING_ASSERT(kind < PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED);
                    break;
                }
            } else {
                PAS_ASSERT(scavenge_kind == pas_expendable_memory_scavenge_forced
                           || scavenge_kind == pas_expendable_memory_scavenge_forced_fake);
                if (kind < PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED)
                    break;
                PAS_TESTING_ASSERT(kind <= PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED);
            }
            header->states[other_index] = pas_expendable_memory_state_create(
                PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED,
                decommit_version);
        }

        /* Make sure that by the time we decommit, nobody can lie about using the stuff we are decommitting.
           The tricky thing is that note_use may occur at some time other than when we actually used the
           memory. So, it might happen after we have already decommitted, or decided to decommit. */
        pas_store_store_fence();

        /* This currently assumes that it's legal to do decommit without later committing. It's not obvious
           that this is a hard requirement of the algorithm; like perhaps it would be easy to add the necessary
           commit calls. */
        if (scavenge_kind != pas_expendable_memory_scavenge_forced_fake) {
            pas_page_malloc_decommit_without_mprotect(
                (char*)payload + index * PAS_EXPENDABLE_MEMORY_PAGE_SIZE,
                (other_index - index) * PAS_EXPENDABLE_MEMORY_PAGE_SIZE,
                pas_may_mmap);
        }

        /* At this point, any of the pages in this range could get decommitted, but it won't necessarily
           happen immediately. Any write to these pages will cancel the decommit, or undo it if it's already
           happened.
           
           The use of versions protects us because:
           
           -> All loads from expendable memory are engineered to go down a slow path that does
              commit_if_necessary if the they encounter zeroes. So, we don't have to worry about what happens
              there.
           
           -> If commit_if_necessary is called and the memory hadn't ever been decommitted, then the versions
              will match up and it'll do nothing.
           
           -> If commit_if_necessary is called and the decommit call had happened, then the versions won't
              match. This will be true regardless of whether or not the OS actually decommitted the memory.
              So, commit_if_necessary will return true and the memory will get rematerialized. This will be
              true even for multiple commit_if_necessary calls on different objects within the same page, or
              in cases where one commit_if_necessary call happened for an object within a page and another for
              an object that partly straddles that page and other pages.
              
           Note that we could have almost gotten this to work by not having version numbers, but instead, having
           the commit_if_necessary call test if any of the parts of the object had become zero. But that would
           be weird:
           
           -> It's a lot harder to reason about that version of the algorithm.
           
           -> It would mean requiring that zero is never used as a legitimate value for any parts of objects
              stored in expendable memory. */
        
        index = other_index - 1;
    }

    return result;
}

bool pas_expendable_memory_scavenge(pas_expendable_memory* header,
                                    void* payload,
                                    pas_expendable_memory_scavenge_kind kind)
{
    bool result;
    
    if (kind == pas_expendable_memory_scavenge_periodic) {
        /* Specialize for this case. We want the scavenger to be fast. */
        return scavenge_impl(header, payload, pas_expendable_memory_scavenge_periodic);
    }

    PAS_ASSERT(kind == pas_expendable_memory_scavenge_forced
               || kind == pas_expendable_memory_scavenge_forced_fake);

    result = scavenge_impl(header, payload, kind);
    PAS_ASSERT(!result);
    return false;
}

#endif /* LIBPAS_ENABLED */

