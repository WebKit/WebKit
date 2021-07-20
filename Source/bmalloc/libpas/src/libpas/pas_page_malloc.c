/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_page_malloc.h"

#include <errno.h>
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#include <math.h>
#include "pas_config.h"
#include "pas_internal_config.h"
#include "pas_log.h"
#include "pas_utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

size_t pas_page_malloc_num_allocated_bytes;
size_t pas_page_malloc_cached_alignment;
size_t pas_page_malloc_cached_alignment_shift;
bool pas_page_malloc_mprotect_decommitted = PAS_ENABLE_TESTING;

#define PAS_VM_TAG VM_MAKE_TAG(VM_MEMORY_TCMALLOC)
#define PAS_NORESERVE 0

size_t pas_page_malloc_alignment_slow(void)
{
    return sysconf(_SC_PAGESIZE);
}

size_t pas_page_malloc_alignment_shift_slow(void)
{
    size_t result;

    result = pas_log2(pas_page_malloc_alignment());
    PAS_ASSERT(((size_t)1 << result) == pas_page_malloc_alignment());

    return result;
}

pas_aligned_allocation_result
pas_page_malloc_try_allocate_without_deallocating_padding(
    size_t size, pas_alignment alignment)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    size_t mapped_size;
    void* mmap_result;
    char* mapped;
    char* mapped_end;
    char* aligned;
    char* aligned_end;
    pas_aligned_allocation_result result;
    size_t page_allocation_alignment;

    if (verbose)
        pas_log("Allocating pages, size = %zu.\n", size);
    
    pas_alignment_validate(alignment);
    
    pas_zero_memory(&result, sizeof(result));
    
    /* What do we do to the alignment offset here? */
    page_allocation_alignment = pas_round_up_to_power_of_2(alignment.alignment,
                                                           pas_page_malloc_alignment());
    aligned_size = pas_round_up_to_power_of_2(size, page_allocation_alignment);
    
    if (page_allocation_alignment <= pas_page_malloc_alignment() && !alignment.alignment_begin)
        mapped_size = aligned_size;
    else {
        /* If we have any interesting alignment requirements to satisfy, allocate extra memory,
           which the caller may choose to free or keep in reserve. */
        if (__builtin_add_overflow(page_allocation_alignment, aligned_size, &mapped_size))
            return result;
    }
    
    mmap_result = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANON | PAS_NORESERVE, PAS_VM_TAG, 0);
    if (mmap_result == MAP_FAILED) {
        errno = 0; /* Clear the error so that we don't leak errno in those
                      cases where we handle the allocation failure
                      internally. If we want to set errno for clients then we
                      do that explicitly. */
        return result;
    }
    
    mapped = (char*)mmap_result;
    mapped_end = mapped + mapped_size;
    
    aligned = (char*)(
        pas_round_up_to_power_of_2((uintptr_t)mapped, page_allocation_alignment) +
        alignment.alignment_begin);
    aligned_end = aligned + size;
    
    if (aligned_end > mapped_end) {
        PAS_ASSERT(alignment.alignment_begin);

        aligned -= page_allocation_alignment;
        aligned_end -= page_allocation_alignment;
        
        PAS_ASSERT(aligned >= mapped);
        PAS_ASSERT(aligned <= mapped_end);
        PAS_ASSERT(aligned_end >= mapped);
        PAS_ASSERT(aligned_end <= mapped_end);
    }
    
    if (page_allocation_alignment <= pas_page_malloc_alignment()
        && !alignment.alignment_begin)
        PAS_ASSERT(mapped == aligned);
    
    PAS_ASSERT(pas_alignment_is_ptr_aligned(alignment, (uintptr_t)aligned));
    
    pas_page_malloc_num_allocated_bytes += mapped_size;
    
    result.result = aligned;
    result.result_size = size;
    result.left_padding = mapped;
    result.left_padding_size = aligned - mapped;
    result.right_padding = aligned_end;
    result.right_padding_size = mapped_end - aligned_end;
    result.zero_mode = pas_zero_mode_is_all_zero;

    return result;
}

void pas_page_malloc_commit(void* ptr, size_t size)
{
    uintptr_t base_as_int;
    uintptr_t end_as_int;
    int result;

    base_as_int = (uintptr_t)ptr;
    end_as_int = base_as_int + size;

    PAS_ASSERT(
        base_as_int == pas_round_down_to_power_of_2(base_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(
        end_as_int == pas_round_up_to_power_of_2(end_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(end_as_int >= base_as_int);

    if (end_as_int == base_as_int)
        return;

    if (pas_page_malloc_mprotect_decommitted) {
        result = mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_READ | PROT_WRITE);
        if (result) {
            pas_log("Could not mprotect on commit: %s\n", strerror(errno));
            PAS_ASSERT(!result);
        }
    }

    if (!PAS_MADVISE_SYMMETRIC)
        return;

    result = madvise((void*)base_as_int, end_as_int - base_as_int, MADV_FREE_REUSE);
    PAS_ASSERT(!result);
}

void pas_page_malloc_decommit(void* ptr, size_t size)
{
    static const bool verbose = false;
    
    uintptr_t base_as_int;
    uintptr_t end_as_int;
    int result;

    if (verbose)
        pas_log("Decommitting %p...%p\n", ptr, (char*)ptr + size);

    base_as_int = (uintptr_t)ptr;
    end_as_int = base_as_int + size;
    PAS_ASSERT(end_as_int >= base_as_int);

    PAS_ASSERT(
        base_as_int == pas_round_up_to_power_of_2(base_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(
        end_as_int == pas_round_down_to_power_of_2(end_as_int, pas_page_malloc_alignment()));
    
    result = madvise((void*)base_as_int, end_as_int - base_as_int, MADV_FREE_REUSABLE);
    PAS_ASSERT(!result);

    if (pas_page_malloc_mprotect_decommitted) {
        result = mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_NONE);
        PAS_ASSERT(!result);
    }
}

void pas_page_malloc_deallocate(void* ptr, size_t size)
{
    uintptr_t ptr_as_int;
    
    ptr_as_int = (uintptr_t)ptr;
    PAS_ASSERT(pas_is_aligned(ptr_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));
    
    if (!size)
        return;
    
    munmap(ptr, size);

    pas_page_malloc_num_allocated_bytes -= size;
}

#endif /* LIBPAS_ENABLED */
