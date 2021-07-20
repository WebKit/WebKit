/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_enumerator_region.h"

#include "pas_internal_config.h"
#include "pas_page_malloc.h"

void* pas_enumerator_region_allocate(pas_enumerator_region** region_ptr,
                                     size_t size)
{
    pas_enumerator_region* region;
    void* result;

    size = pas_round_up_to_power_of_2(size, PAS_INTERNAL_MIN_ALIGN);

    region = *region_ptr;

    if (!region || region->size - region->offset < size) {
        pas_enumerator_region* new_region;
        size_t allocation_size;
        pas_aligned_allocation_result allocation_result;

        allocation_size = PAS_OFFSETOF(pas_enumerator_region, payload) + size;

        PAS_ASSERT(pas_is_aligned(allocation_size, PAS_INTERNAL_MIN_ALIGN));

        allocation_result = pas_page_malloc_try_allocate_without_deallocating_padding(
            allocation_size, pas_alignment_create_trivial());

        PAS_ASSERT(allocation_result.result);
        PAS_ASSERT(allocation_result.result == allocation_result.left_padding);
        PAS_ASSERT(!allocation_result.left_padding_size);

        new_region = allocation_result.result;
        new_region->previous = region;
        new_region->size =
            allocation_result.result_size + allocation_result.right_padding_size -
            PAS_OFFSETOF(pas_enumerator_region, payload);
        new_region->offset = 0;

        *region_ptr = new_region;
        region = new_region;
    }

    PAS_ASSERT(region);
    PAS_ASSERT(region->size - region->offset >= size);

    result = (char*)region->payload + region->offset;
    region->offset += size;

    return result;
}

void pas_enumerator_region_destroy(pas_enumerator_region* region)
{
    while (region) {
        pas_enumerator_region* previous;

        previous = region->previous;

        pas_page_malloc_deallocate(region, region->size + PAS_OFFSETOF(pas_enumerator_region, payload));

        region = previous;
    }
}

#endif /* LIBPAS_ENABLED */
