/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_MALLOC_H
#define PAS_PAGE_MALLOC_H

#include "pas_aligned_allocation_result.h"
#include "pas_alignment.h"
#include "pas_mmap_capability.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern size_t pas_page_malloc_num_allocated_bytes;
PAS_API extern size_t pas_page_malloc_cached_alignment;
PAS_API extern size_t pas_page_malloc_cached_alignment_shift;
#if PAS_OS(DARWIN)
PAS_API extern bool pas_page_malloc_decommit_zero_fill;
#endif /* PAS_OS(DARWIN) */

PAS_API PAS_NEVER_INLINE size_t pas_page_malloc_alignment_slow(void);

static inline size_t pas_page_malloc_alignment(void)
{
    if (!pas_page_malloc_cached_alignment)
        pas_page_malloc_cached_alignment = pas_page_malloc_alignment_slow();
    return pas_page_malloc_cached_alignment;
}

PAS_API PAS_NEVER_INLINE size_t pas_page_malloc_alignment_shift_slow(void);

static inline size_t pas_page_malloc_alignment_shift(void)
{
    if (!pas_page_malloc_cached_alignment_shift)
        pas_page_malloc_cached_alignment_shift = pas_page_malloc_alignment_shift_slow();
    return pas_page_malloc_cached_alignment_shift;
}

PAS_API pas_aligned_allocation_result
pas_page_malloc_try_allocate_without_deallocating_padding(
    size_t size, pas_alignment alignment);

PAS_API void pas_page_malloc_deallocate(void* base, size_t size);

PAS_API void pas_page_malloc_zero_fill(void* base, size_t size);

/* This even works if size < pas_page_malloc_alignment so long as the range [base, base+size) is
   entirely within a page according to pas_page_malloc_alignment. */
PAS_API void pas_page_malloc_commit(void* base, size_t size, pas_mmap_capability mmap_capability);
PAS_API void pas_page_malloc_decommit(void* base, size_t size, pas_mmap_capability mmap_capability);

/* In testing mode, we have commit/decommit mprotect the memory as a way of helping us see if we are
   accidentally reading or writing that memory. But sometimes we use commit/decommit in a way that prevents
   us from making such assertions, like if we allow some reads and writes to decommitted memory in rare
   cases. */
PAS_API void pas_page_malloc_commit_without_mprotect(
    void* base, size_t size, pas_mmap_capability mmap_capability);
PAS_API void pas_page_malloc_decommit_without_mprotect(
    void* base, size_t size, pas_mmap_capability mmap_capability);

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_MALLOC_H */

