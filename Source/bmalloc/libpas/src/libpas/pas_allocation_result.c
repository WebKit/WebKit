/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "pas_allocation_result.h"
#include "pas_mte.h"
#include "pas_page_malloc.h"
#include "pas_zero_memory.h"

pas_allocation_result pas_allocation_result_zero_large_slow(pas_allocation_result result, size_t size)
{
    size_t page_size;
    uintptr_t begin;
    uintptr_t end;
    uintptr_t page_aligned_begin;
    uintptr_t page_aligned_end;

    PAS_PROFILE(ZERO_ALLOCATION_RESULT, result.begin);
    PAS_MTE_HANDLE(ZERO_ALLOCATION_RESULT, result.begin);

    page_size = pas_page_malloc_alignment();
    begin = result.begin;
    end = begin + size;

    page_aligned_begin = pas_round_up_to_power_of_2(begin, page_size);
    page_aligned_end = pas_round_down_to_power_of_2(end, page_size);

    if (page_aligned_end > page_aligned_begin) {
        if (begin != page_aligned_begin)
            pas_zero_memory((void*)begin, page_aligned_begin - begin);
        pas_page_malloc_zero_fill((void*)page_aligned_begin, page_aligned_end - page_aligned_begin);
        if (end != page_aligned_end)
            pas_zero_memory((void*)page_aligned_end, end - page_aligned_end);
    } else
        pas_zero_memory((void*)begin, size);

    return pas_allocation_result_create_success_with_zero_mode(result.begin, pas_zero_mode_is_all_zero);
}

#endif /* LIBPAS_ENABLED */
