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

#ifndef PAS_TRY_SHRINK_H
#define PAS_TRY_SHRINK_H

/* Returns true if the object was actually shrunk. You'll get false if the heap that the object is
   in doesn't support shrinking. However, if you try to shrink something that isn't even allocated
   then you'll get a trap.

   Shrinking is supported by the bitfit and large heaps. It's not supported by the segregated heap. */
static PAS_ALWAYS_INLINE bool pas_try_shrink(void* ptr,
                                             size_t new_size,
                                             pas_heap_config config)
{
    uintptr_t begin;

    begin = (uintptr_t)ptr;

    switch (config.fast_megapage_kind_func(begin)) {
    case pas_small_exclusive_segregated_fast_megapage_kind:
        return false;
    case pas_small_other_fast_megapage_kind: {
        pas_page_base_and_kind page_and_kind;
        page_and_kind = pas_get_page_base_and_kind_for_small_other_in_fast_megapage(begin, config);
        switch (page_and_kind.page_kind) {
        case pas_small_shared_segregated_page_kind:
            return false;
        case pas_small_bitfit_page_kind:
            config.small_bitfit_config.specialized_page_shrink_with_page(
                pas_page_base_get_bitfit(page_and_kind.page_base),
                begin, new_size);
            return true;
        default:
            PAS_ASSERT(!"Should not be reached");
            return 0;
        }
    }
    case pas_not_a_fast_megapage_kind: {
        pas_page_base* page_base;
        bool shrink_result;

        page_base = config.page_header_func(begin);
        if (page_base) {
            switch (pas_page_base_get_kind(page_base)) {
            case pas_small_exclusive_segregated_page_kind:
            case pas_small_shared_segregated_page_kind:
                PAS_ASSERT(!config.small_segregated_is_in_megapage);
                return false;
            case pas_small_bitfit_page_kind:
                PAS_ASSERT(!config.small_bitfit_is_in_megapage);
                config.small_bitfit_config.specialized_page_shrink_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin, new_size);
                return true;
            case pas_medium_exclusive_segregated_page_kind:
            case pas_medium_shared_segregated_page_kind:
                return false;
            case pas_medium_bitfit_page_kind:
                config.medium_bitfit_config.specialized_page_shrink_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin, new_size);
                return true;
            case pas_marge_bitfit_page_kind:
                config.marge_bitfit_config.specialized_page_shrink_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin, new_size);
                return true;
            }
            PAS_ASSERT(!"Bad page kind");
            return false;
        }

        pas_heap_lock_lock();
        shrink_result = pas_large_heap_try_shrink(begin, new_size, config.config_ptr);
        pas_heap_lock_unlock();
        if (!shrink_result)
            pas_deallocation_did_fail("Object not allocated", begin);
        return true;
    } }

    PAS_ASSERT(!"Should not be reached");
    return false;
}

#endif /* PAS_TRY_SHRINK_H */

