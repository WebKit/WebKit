/*
 * Copyright (c) 2018-2025 Apple Inc. All rights reserved.
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
#include <stdio.h>
#include <string.h>
#if !PAS_OS(WINDOWS)
#include <sys/mman.h>
#include <unistd.h>
#endif
#if PAS_OS(DARWIN)
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#endif
#if PAS_OS(LINUX)
#include <sys/prctl.h>
#endif

#include "pas_internal_config.h"
#include "pas_log.h"
#include "pas_mte.h"
#include "pas_stats.h"
#include "pas_utils.h"
#include "pas_zero_memory.h"

size_t pas_page_malloc_num_allocated_bytes;
size_t pas_page_malloc_cached_alignment;
size_t pas_page_malloc_cached_alignment_shift;

#if defined(MADV_ZERO) && PAS_OS(DARWIN)
#define PAS_USE_MADV_ZERO 1
#else
#define PAS_USE_MADV_ZERO 0
#endif

#if PAS_OS(DARWIN)
bool pas_page_malloc_decommit_zero_fill = false;
#endif /* PAS_OS(DARWIN) */

#if PAS_USE_MADV_ZERO
/* It is possible that MADV_ZERO is defined but still not supported by the
 * running OS. In this case, we check once to see if we get ENOSUP, and if
 * we thereafter short-circuit to the fallback (mmap), thus avoiding the
 * extra overhead of calling into madvise(MADV_ZERO) every time. */
static pthread_once_t madv_zero_once_control = PTHREAD_ONCE_INIT;
static bool madv_zero_supported = false;
#endif

#if PAS_OS(DARWIN)
#define PAS_VM_TAG VM_MAKE_TAG(VM_MEMORY_TCMALLOC)
#elif PAS_PLATFORM(PLAYSTATION) && defined(VM_MAKE_TAG)
#define PAS_VM_TAG VM_MAKE_TAG(VM_TYPE_USER1)
#else
#define PAS_VM_TAG -1
#endif
#define PAS_VM_TAG_NAME "WKFastMalloc"

#if PAS_OS(LINUX)
#define PAS_NORESERVE MAP_NORESERVE

#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#endif

#ifndef PR_SET_VMA_ANON_NAME
#define PR_SET_VMA_ANON_NAME 0
#endif

#else
#define PAS_NORESERVE 0
#endif

#if PAS_OS(WINDOWS)
static void virtual_query_checked(void* ptr, MEMORY_BASIC_INFORMATION* memInfo)
{
    SIZE_T bytes = VirtualQuery(ptr, memInfo, sizeof(*memInfo));
    PAS_ASSERT(bytes == sizeof(*memInfo));
    PAS_ASSERT(memInfo->RegionSize > 0);
}

static void* virtual_alloc_with_retry(LPVOID ptr, SIZE_T size, DWORD allocation_type, DWORD protection)
{
    void* result = VirtualAlloc(ptr, size, allocation_type, protection);
    if (PAS_LIKELY(result))
        return result;

    DWORD error = GetLastError();
    if (error != ERROR_COMMITMENT_LIMIT && error != ERROR_NOT_ENOUGH_MEMORY)
        return result;

    /* Only retry commits */
    if (!(allocation_type & MEM_COMMIT))
        return result;

    const size_t max_attempts = 10;
    const unsigned long delay_ms = 50;
    for (size_t i = 0; i < max_attempts; ++i) {
        Sleep(delay_ms);
        result = VirtualAlloc(ptr, size, allocation_type, protection);

        if (result)
            return result;
        DWORD error = GetLastError();
        if (error != ERROR_COMMITMENT_LIMIT && error != ERROR_NOT_ENOUGH_MEMORY)
            return result;
    }

    return result;
}
#endif

PAS_NEVER_INLINE size_t pas_page_malloc_alignment_slow(void)
{
#if PAS_OS(WINDOWS)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    long result = sysInfo.dwPageSize;
#else
    long result = sysconf(_SC_PAGESIZE);
#endif
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

static void*
pas_page_malloc_try_map_pages(size_t size, bool may_contain_small_or_medium)
{

#if PAS_OS(WINDOWS)
    PAS_PROFILE(PAGE_ALLOCATION, size, may_contain_small_or_medium, PAS_VM_TAG);
    PAS_MTE_HANDLE(PAGE_ALLOCATION, size, may_contain_small_or_medium, PAS_VM_TAG);

    /*
     * PAS_STATS is not currently supported on Windows, so we do not currently
     * increment pas_stats_page_alloc_counts counters here. If that ever
     * changes, we should call PAS_RECORD_STAT here as well.
     */
    PAS_TESTING_ASSERT(!PAS_ENABLE_STATS);

    return virtual_alloc_with_retry(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* mmap_result = NULL;

    PAS_PROFILE(PAGE_ALLOCATION, size, may_contain_small_or_medium, PAS_VM_TAG);
    PAS_MTE_HANDLE(PAGE_ALLOCATION, size, may_contain_small_or_medium, PAS_VM_TAG);

    mmap_result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | PAS_NORESERVE, PAS_VM_TAG, 0);
    if (mmap_result == MAP_FAILED) {
        errno = 0; /* Clear the error so that we don't leak errno in those
                      cases where we handle the allocation failure
                      internally. If we want to set errno for clients then we
                      do that explicitly. */
        return NULL;
    } else
        PAS_RECORD_STAT(page_alloc_counts, size, may_contain_small_or_medium, false);

#if PAS_OS(LINUX)
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, mmap_result, size, PAS_VM_TAG_NAME);
#endif

    return mmap_result;
#endif
}

pas_aligned_allocation_result
pas_page_malloc_try_allocate_without_deallocating_padding(
    size_t size, pas_alignment alignment, bool may_contain_small_or_medium)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_OTHER);
    
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

    mmap_result = pas_page_malloc_try_map_pages(mapped_size, may_contain_small_or_medium);
    if (!mmap_result)
        return result;

    uintptr_t pages_begin = (uintptr_t)mmap_result;
    mmap_result = (void*)pages_begin;
    
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

#if PAS_USE_MADV_ZERO
static void pas_page_malloc_zero_fill_latch_if_madv_zero_is_supported(void)
{
    /* It is possible that the MADV_ZERO macro is defined but that the kernel
     * does not actually support it. In this case we want to avoid calling madvise
     * since it will just return -1 every time, and so just short-circuit
     * to the mmap fallback instead.
     * However, we could also get unlucky and have the madvise fail for another
     * reason (e.g. CoW memory) so we need to make sure we're getting ENOTSUP
     * and not another error before we latch off madvise. */
    size_t size;
    void* base;

    size = PAS_SMALL_PAGE_DEFAULT_SIZE;
    base = mmap(NULL, PAS_SMALL_PAGE_DEFAULT_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON | PAS_NORESERVE, PAS_VM_TAG, 0);
    PAS_ASSERT(base, (uintptr_t)base);

    int rc = madvise(base, size, MADV_ZERO);
    if (rc)
        madv_zero_supported = (errno != ENOTSUP);
    else
        madv_zero_supported = true;
    munmap(base, size);
}
#endif

void pas_page_malloc_zero_fill(void* base, size_t size)
{
#if PAS_OS(WINDOWS)
    size_t page_size;

    page_size = pas_page_malloc_alignment();

    PAS_ASSERT(pas_is_aligned((uintptr_t)base, page_size));
    PAS_ASSERT(pas_is_aligned(size, page_size));

    PAS_ASSERT(SecureZeroMemory(base, size));
#else
    size_t page_size;
    void* result_ptr;
    int result;

    page_size = pas_page_malloc_alignment();
    
    PAS_ASSERT(pas_is_aligned((uintptr_t)base, page_size));
    PAS_ASSERT(pas_is_aligned(size, page_size));

    int flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED | PAS_NORESERVE;
    int tag = PAS_VM_TAG;

#if PAS_USE_MADV_ZERO
    pthread_once(&madv_zero_once_control, pas_page_malloc_zero_fill_latch_if_madv_zero_is_supported);
    if (madv_zero_supported) {
        int rc = madvise(base, size, MADV_ZERO);
        if (rc != -1)
            return;
    }
#endif /* PAS_USE_MADV_ZERO */

    PAS_UNUSED_PARAM(result_ptr);
    PAS_UNUSED_PARAM(result);

    PAS_PROFILE(ZERO_FILL_PAGE, base, size, flags, tag);
    PAS_MTE_HANDLE(ZERO_FILL_PAGE, base, size, flags, tag);
#if PAS_OS(LINUX)
    result = madvise(base, size, MADV_DONTNEED);
    PAS_ASSERT(!result);
#else
    result_ptr = mmap(base, size, PROT_READ | PROT_WRITE, flags, tag, 0);
    PAS_ASSERT(result_ptr == base, (uintptr_t)result_ptr, (uintptr_t)base);
#endif
#endif /* PAS_OS(WINDOWS) */
}

static void commit_impl(void* ptr, size_t size, bool do_mprotect, bool is_symmetric, pas_mmap_capability mmap_capability)
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

    if (PAS_MPROTECT_DECOMMITTED && do_mprotect && mmap_capability) {
#if PAS_OS(WINDOWS)
        PAS_ASSERT(virtual_alloc_with_retry(ptr, size, MEM_COMMIT, PAGE_READWRITE));
#else
        PAS_SYSCALL(mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_READ | PROT_WRITE));
#endif
    }

#if PAS_OS(LINUX)
    PAS_ASSERT(!is_symmetric);
    PAS_SYSCALL(madvise(ptr, size, MADV_DODUMP));
#elif PAS_OS(WINDOWS)
    if (is_symmetric) {
        /*
         * Symmetric: re-commit pages that the matching decommit released via
         * MEM_DECOMMIT. Loop because the range may span multiple reservations.
         */
        size_t totalSeen = 0;
        void *currentPtr = ptr;
        while (totalSeen < size) {
            MEMORY_BASIC_INFORMATION memInfo;
            virtual_query_checked(currentPtr, &memInfo);
            PAS_ASSERT(memInfo.State != 0x10000);
            PAS_ASSERT(virtual_alloc_with_retry(currentPtr, PAS_MIN(memInfo.RegionSize, size - totalSeen), MEM_COMMIT, PAGE_READWRITE));
            currentPtr = (void*) ((uintptr_t) currentPtr + memInfo.RegionSize);
            totalSeen += memInfo.RegionSize;
        }
    } else {
        /*
         * Asymmetric: pages stayed committed through the matching decommit,
         * so there's nothing to re-commit.
         */
    }
#elif PAS_PLATFORM(PLAYSTATION)
    /* We don't need to call madvise to map page. */
    PAS_ASSERT(!is_symmetric);
#elif PAS_OS(FREEBSD)
    PAS_ASSERT(!is_symmetric);
    PAS_SYSCALL(madvise(ptr, size, MADV_NORMAL));
#else
    PAS_ASSERT(!is_symmetric);
#endif
}

void pas_page_malloc_commit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = true;
    static const bool is_symmetric = !!PAS_USE_SYMMETRIC_PAGE_ALLOCATION;
    commit_impl(ptr, size, do_mprotect, is_symmetric, mmap_capability);
}

void pas_page_malloc_commit_without_mprotect(void* ptr, size_t size, bool is_symmetric, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = false;
    commit_impl(ptr, size, do_mprotect, is_symmetric, mmap_capability);
}

static void decommit_impl(void* ptr, size_t size,
                          bool do_mprotect, bool is_symmetric,
                          pas_mmap_capability mmap_capability)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_OTHER);

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
    PAS_ASSERT(!is_symmetric);
    if (pas_page_malloc_decommit_zero_fill && mmap_capability)
        pas_page_malloc_zero_fill(ptr, size);
    else
        PAS_SYSCALL(madvise(ptr, size, MADV_FREE_REUSABLE));
#elif PAS_OS(FREEBSD)
    PAS_ASSERT(!is_symmetric);
    PAS_SYSCALL(madvise(ptr, size, MADV_FREE));
#elif PAS_OS(LINUX)
    PAS_ASSERT(!is_symmetric);
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTNEED));
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTDUMP));
#elif PAS_OS(WINDOWS)
    if (is_symmetric) {
        /*
         * Symmetric: MEM_DECOMMIT releases both the physical backing and
         * the Windows commit charge. The matching commit path is required
         * before re-access; reading decommitted pages faults.
         *
         * This is the cost of preserving commit-charge accounting on Windows,
         * where commit is a hard system-wide limit (RAM + pagefile).
         */
        size_t totalSeen = 0;
        void* currentPtr = ptr;
        while (totalSeen < size) {
            MEMORY_BASIC_INFORMATION memInfo;
            virtual_query_checked(currentPtr, &memInfo);
            PAS_ASSERT(VirtualFree(currentPtr, PAS_MIN(memInfo.RegionSize, size - totalSeen), MEM_DECOMMIT));
            currentPtr = (void*)((uintptr_t)currentPtr + memInfo.RegionSize);
            totalSeen += memInfo.RegionSize;
        }
    } else {
        /*
         * Asymmetric: pages stay committed but contents are discarded.
         * This is equivalent to madvise(MADV_DONTNEED).
         *
         * DiscardVirtualMemory frees physical pages immediately and removes
         * them from the working set. On older Windows 10 builds and on ranges
         * spanning multiple reservations it can fail, so we fall back
         * to a per-reservation MEM_RESET loop + VirtualUnlock, which has the
         * same effect but with lazy reclaim.
         */
        PAS_ASSERT(!do_mprotect);
        if (DiscardVirtualMemory(ptr, size)) {
            size_t totalSeen = 0;
            void* currentPtr = ptr;
            while (totalSeen < size) {
                MEMORY_BASIC_INFORMATION memInfo;
                virtual_query_checked(currentPtr, &memInfo);
                PAS_ASSERT(VirtualAlloc(currentPtr, PAS_MIN(memInfo.RegionSize, size - totalSeen), MEM_RESET, PAGE_READWRITE));
                currentPtr = (void*)((uintptr_t)currentPtr + memInfo.RegionSize);
                totalSeen += memInfo.RegionSize;
            }
            /*
             * MEM_RESET on its own does not remove pages from the process working set.
             * VirtualUnlock of unlocked pages evicts them from the working set.
             */
            VirtualUnlock(ptr, size);
        }
    }
#else
    PAS_ASSERT(!is_symmetric);
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTNEED));
#endif

    if (PAS_MPROTECT_DECOMMITTED && do_mprotect && mmap_capability) {
#if PAS_OS(WINDOWS)
        PAS_ASSERT(virtual_alloc_with_retry(ptr, size, MEM_COMMIT, PAGE_NOACCESS));
#else
        PAS_SYSCALL(mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_NONE));
#endif
    }
}

void pas_page_malloc_decommit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = true;
    static const bool is_symmetric = !!PAS_USE_SYMMETRIC_PAGE_ALLOCATION;
    decommit_impl(ptr, size, do_mprotect, is_symmetric, mmap_capability);
}

void pas_page_malloc_decommit_without_mprotect(void* ptr, size_t size, bool is_symmetric, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = false;
    decommit_impl(ptr, size, do_mprotect, is_symmetric, mmap_capability);
}

void pas_page_malloc_deallocate(void* ptr, size_t size)
{
    uintptr_t ptr_as_int;
    
    ptr_as_int = (uintptr_t)ptr;
    PAS_PROFILE(PAGE_DEALLOCATION, ptr_as_int);
    PAS_ASSERT(pas_is_aligned(ptr_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));
    ptr = (void*)ptr_as_int;
    
    if (!size)
        return;

#if PAS_OS(WINDOWS)
    /* For MEM_RELEASE, the size argument to VirtualFree must be 0 — it releases the
       entire range allocated by the original VirtualAlloc(MEM_RESERVE) call. */
    PAS_ASSERT(VirtualFree(ptr, 0, MEM_RELEASE));
#else
    munmap(ptr, size);
#endif

    pas_page_malloc_num_allocated_bytes -= size;
}

#endif /* LIBPAS_ENABLED */
