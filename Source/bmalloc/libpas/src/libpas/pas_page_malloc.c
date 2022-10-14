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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_page_malloc.h"

#include <errno.h>
#include <math.h>
#include "pas_config.h"
#include "pas_internal_config.h"
#include "pas_log.h"
#include "pas_utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if PAS_OS(DARWIN)
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#endif

size_t pas_page_malloc_num_allocated_bytes;
size_t pas_page_malloc_cached_alignment;
size_t pas_page_malloc_cached_alignment_shift;

#if PAS_OS(DARWIN)
bool pas_page_malloc_decommit_zero_fill = false;
#endif /* PAS_OS(DARWIN) */

#if PAS_OS(DARWIN)
#define PAS_VM_TAG VM_MAKE_TAG(VM_MEMORY_TCMALLOC)
#elif PAS_PLATFORM(PLAYSTATION) && defined(VM_MAKE_TAG)
#define PAS_VM_TAG VM_MAKE_TAG(VM_TYPE_USER1)
#else
#define PAS_VM_TAG -1
#endif

#if PAS_OS(LINUX)
#define PAS_NORESERVE MAP_NORESERVE
#else
#define PAS_NORESERVE 0
#endif

PAS_NEVER_INLINE size_t pas_page_malloc_alignment_slow(void)
{
    long result = sysconf(_SC_PAGESIZE);
    PAS_ASSERT(result >= 0);
    PAS_ASSERT(result > 0);
    PAS_ASSERT(result >= 4096);
    return (size_t)result;
}

PAS_NEVER_INLINE size_t pas_page_malloc_alignment_shift_slow(void)
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
    result.left_padding_size = (size_t)(aligned - mapped);
    result.right_padding = aligned_end;
    result.right_padding_size = (size_t)(mapped_end - aligned_end);
    result.zero_mode = pas_zero_mode_is_all_zero;

    return result;
}

void pas_page_malloc_zero_fill(void* base, size_t size)
{
    size_t page_size;
    void* result_ptr;

    page_size = pas_page_malloc_alignment();
    
    PAS_ASSERT(pas_is_aligned((uintptr_t)base, page_size));
    PAS_ASSERT(pas_is_aligned(size, page_size));

    result_ptr = mmap(base,
                      size,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON | MAP_FIXED | PAS_NORESERVE,
                      PAS_VM_TAG,
                      0);
    PAS_ASSERT(result_ptr == base);
}

static void commit_impl(void* ptr, size_t size, bool do_mprotect, pas_mmap_capability mmap_capability)
{
    uintptr_t base_as_int;
    uintptr_t end_as_int;

    base_as_int = (uintptr_t)ptr;
    end_as_int = base_as_int + size;

    PAS_ASSERT(
        base_as_int == pas_round_down_to_power_of_2(base_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(
        end_as_int == pas_round_up_to_power_of_2(end_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(end_as_int >= base_as_int);

    if (end_as_int == base_as_int)
        return;

    if (PAS_MPROTECT_DECOMMITTED && do_mprotect && mmap_capability)
        PAS_SYSCALL(mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_READ | PROT_WRITE));

#if PAS_OS(LINUX)
    PAS_SYSCALL(madvise(ptr, size, MADV_DODUMP));
#elif PAS_PLATFORM(PLAYSTATION)
    // We don't need to call madvise to map page.
#elif PAS_OS(FREEBSD)
    PAS_SYSCALL(madvise(ptr, size, MADV_NORMAL));
#endif
}

void pas_page_malloc_commit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = true;
    commit_impl(ptr, size, do_mprotect, mmap_capability);
}

void pas_page_malloc_commit_without_mprotect(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = false;
    commit_impl(ptr, size, do_mprotect, mmap_capability);
}

static void decommit_impl(void* ptr, size_t size,
                          bool do_mprotect,
                          pas_mmap_capability mmap_capability)
{
    static const bool verbose = false;
    
    uintptr_t base_as_int;
    uintptr_t end_as_int;

    if (verbose)
        pas_log("Decommitting %p...%p\n", ptr, (char*)ptr + size);

    base_as_int = (uintptr_t)ptr;
    end_as_int = base_as_int + size;
    PAS_ASSERT(end_as_int >= base_as_int);

    PAS_ASSERT(
        base_as_int == pas_round_up_to_power_of_2(base_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(
        end_as_int == pas_round_down_to_power_of_2(end_as_int, pas_page_malloc_alignment()));
    
#if PAS_OS(DARWIN)
    if (pas_page_malloc_decommit_zero_fill && mmap_capability)
        pas_page_malloc_zero_fill(ptr, size);
    else
        PAS_SYSCALL(madvise(ptr, size, MADV_FREE_REUSABLE));
#elif PAS_OS(FREEBSD)
    PAS_SYSCALL(madvise(ptr, size, MADV_FREE));
#elif PAS_OS(LINUX)
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTNEED));
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTDUMP));
#else
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTNEED));
#endif

    if (PAS_MPROTECT_DECOMMITTED && do_mprotect && mmap_capability)
        PAS_SYSCALL(mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_NONE));
}

void pas_page_malloc_decommit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = true;
    decommit_impl(ptr, size, do_mprotect, mmap_capability);
}

void pas_page_malloc_decommit_without_mprotect(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = false;
    decommit_impl(ptr, size, do_mprotect, mmap_capability);
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
