/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "BAssert.h"
#include "BSyscall.h"
#include "BVMTags.h"
#include "Logging.h"
#include "Range.h"
#include "Sizes.h"
#include <algorithm>
#include <optional>

#if BPLATFORM(WIN)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if BOS(DARWIN)
#include <mach/vm_page_size.h>
#endif

namespace bmalloc {

#ifndef BMALLOC_VM_TAG
#define BMALLOC_VM_TAG VM_TAG_FOR_TCMALLOC_MEMORY
#endif

#if BOS(LINUX)
#define BMALLOC_NORESERVE MAP_NORESERVE
#else
#define BMALLOC_NORESERVE 0
#endif

struct VMAllocation {
    void* address;
    size_t size;
};

struct VMAlignedAllocation {
    void* aligned;
    VMAllocation allocation;
};

#if BPLATFORM(WIN)

inline size_t vmGranularity()
{
    static size_t cached;
    if (!cached) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        long granularity = info.dwAllocationGranularity;
        if (granularity < 0)
            BCRASH();
        cached = granularity;
    }
    return cached;
}

#endif

inline size_t vmPageSize()
{
    static size_t cached;
    if (!cached) {
#if BPLATFORM(WIN)
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        long pageSize = info.dwPageSize;
#else
        long pageSize = sysconf(_SC_PAGESIZE);
#endif
        if (pageSize < 0)
            BCRASH();
        cached = pageSize;
    }
    return cached;
}

inline size_t vmPageShift()
{
    static size_t cached;
    if (!cached)
        cached = log2(vmPageSize());
    return cached;
}

inline size_t vmSize(size_t size)
{
    return roundUpToMultipleOf(vmPageSize(), size);
}

inline void vmValidate(size_t vmSize)
{
    BUNUSED(vmSize);
    BASSERT(vmSize);
    BASSERT(vmSize == roundUpToMultipleOf(vmPageSize(), vmSize));
}

inline void vmValidate(void* p, size_t vmSize)
{
    vmValidate(vmSize);
    
    BUNUSED(p);
    BASSERT(p);
    BASSERT(p == mask(p, ~(vmPageSize() - 1)));
}

inline size_t vmPageSizePhysical()
{
#if BOS(DARWIN) && (BCPU(ARM64) || BCPU(ARM))
    return vm_kernel_page_size;
#else
    return vmPageSize();
#endif
}

inline void vmValidatePhysical(size_t vmSize)
{
    BUNUSED(vmSize);
    BASSERT(vmSize);
    BASSERT(vmSize == roundUpToMultipleOf(vmPageSizePhysical(), vmSize));
}

inline void vmValidatePhysical(void* p, size_t vmSize)
{
    vmValidatePhysical(vmSize);
    
    BUNUSED(p);
    BASSERT(p);
    BASSERT(p == mask(p, ~(vmPageSizePhysical() - 1)));
}

inline void* tryVMAllocate(size_t vmSize, VMTag usage = VMTag::Malloc)
{
    vmValidate(vmSize);
#if BPLATFORM(WIN)
    void* result = VirtualAlloc(nullptr, vmSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* result = mmap(0, vmSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | BMALLOC_NORESERVE, static_cast<int>(usage), 0);
    if (result == MAP_FAILED)
        return nullptr;
#endif
    return result;
}

inline void* vmAllocate(size_t vmSize, VMTag usage = VMTag::Malloc)
{
    void* result = tryVMAllocate(vmSize, usage);
    RELEASE_BASSERT(result);
    return result;
}

inline void vmDeallocate(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
#if BPLATFORM(WIN)
    auto success = VirtualFree(p, vmSize, MEM_DECOMMIT);
    RELEASE_BASSERT(success);
#else
    munmap(p, vmSize);
#endif
}

inline void vmRevokePermissions(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
#if BPLATFORM(WIN)
    auto success = VirtualProtect(p, vmSize, PAGE_NOACCESS, nullptr);
    RELEASE_BASSERT(success);
#else
    mprotect(p, vmSize, PROT_NONE);
#endif
}

inline void vmZeroAndPurge(void* p, size_t vmSize, VMTag usage = VMTag::Malloc)
{
    vmValidate(p, vmSize);
#if BPLATFORM(WIN)
    auto success = VirtualFree(p, vmSize, MEM_DECOMMIT);
    RELEASE_BASSERT(success);
    void* result = VirtualAlloc(p, vmSize, MEM_COMMIT, PAGE_READWRITE);
    RELEASE_BASSERT(result);
#else
    // MAP_ANON guarantees the memory is zeroed. This will also cause
    // page faults on accesses to this range following this call.
    void* result = mmap(p, vmSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED | BMALLOC_NORESERVE, static_cast<int>(usage), 0);
    RELEASE_BASSERT(result == p);
#endif
}

inline void vmDeallocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidatePhysical(p, vmSize);
#if BOS(DARWIN)
    SYSCALL(madvise(p, vmSize, MADV_FREE_REUSABLE));
#elif BOS(FREEBSD)
    SYSCALL(madvise(p, vmSize, MADV_FREE));
#elif BPLATFORM(WIN)
    void* result = VirtualAlloc(p, vmSize, MEM_RESET, PAGE_NOACCESS);
    RELEASE_BASSERT(result);
#else
    SYSCALL(madvise(p, vmSize, MADV_DONTNEED));
#if BOS(LINUX)
    SYSCALL(madvise(p, vmSize, MADV_DONTDUMP));
#endif
#endif
}

inline void vmAllocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidatePhysical(p, vmSize);
#if BOS(DARWIN)
    BUNUSED_PARAM(p);
    BUNUSED_PARAM(vmSize);
    // For the Darwin platform, we don't need to call madvise(..., MADV_FREE_REUSE)
    // to commit physical memory to back a range of allocated virtual memory.
    // Instead the kernel will commit pages as they are touched.
#elif BPLATFORM(WIN)
    void* result = VirtualAlloc(p, vmSize, MEM_COMMIT, PAGE_READWRITE);
    RELEASE_BASSERT(result);
#else
    SYSCALL(madvise(p, vmSize, MADV_NORMAL));
#if BOS(LINUX)
    SYSCALL(madvise(p, vmSize, MADV_DODUMP));
#endif
#endif
}

// Returns how much memory you would commit/decommit had you called
// vmDeallocate/AllocatePhysicalPagesSloppy with p and size.
inline size_t physicalPageSizeSloppy(void* p, size_t size)
{
    char* begin = roundUpToMultipleOf(vmPageSizePhysical(), static_cast<char*>(p));
    char* end = roundDownToMultipleOf(vmPageSizePhysical(), static_cast<char*>(p) + size);

    if (begin >= end)
        return 0;
    return end - begin;
}

// Trims requests that are un-page-aligned.
inline void vmDeallocatePhysicalPagesSloppy(void* p, size_t size)
{
    char* begin = roundUpToMultipleOf(vmPageSizePhysical(), static_cast<char*>(p));
    char* end = roundDownToMultipleOf(vmPageSizePhysical(), static_cast<char*>(p) + size);

    if (begin >= end)
        return;

    vmDeallocatePhysicalPages(begin, end - begin);
}

// Expands requests that are un-page-aligned.
inline void vmAllocatePhysicalPagesSloppy(void* p, size_t size)
{
    char* begin = roundDownToMultipleOf(vmPageSizePhysical(), static_cast<char*>(p));
    char* end = roundUpToMultipleOf(vmPageSizePhysical(), static_cast<char*>(p) + size);

    if (begin >= end)
        return;

    vmAllocatePhysicalPages(begin, end - begin);
}

// Allocates vmSize bytes at a specified power-of-two alignment.
// Use this function to create maskable memory regions.

inline std::optional<VMAlignedAllocation> tryVMAllocateAligned(size_t vmAlignment, size_t vmSize, VMTag usage = VMTag::Malloc)
{
    vmValidate(vmSize);
    vmValidate(vmAlignment);

    size_t mappedSize = vmAlignment + vmSize;
    if (mappedSize < vmAlignment || mappedSize < vmSize) // Check for overflow
        return std::nullopt;

    char* mapped = static_cast<char*>(tryVMAllocate(mappedSize, usage));
    if (!mapped)
        return std::nullopt;
    char* mappedEnd = mapped + mappedSize;

    char* aligned = roundUpToMultipleOf(vmAlignment, mapped);
    char* alignedEnd = aligned + vmSize;

    RELEASE_BASSERT(alignedEnd <= mappedEnd);

    size_t leftExtra = aligned - mapped;
    size_t rightExtra = mappedEnd - alignedEnd;

#if BPLATFORM(WIN)
    if (leftExtra)
        vmDeallocatePhysicalPagesSloppy(mapped, leftExtra);
    if (rightExtra)
        vmDeallocatePhysicalPagesSloppy(alignedEnd, rightExtra);

    return VMAlignedAllocation { aligned, VMAllocation { mapped, mappedSize } };
#else
    if (leftExtra)
        vmDeallocate(mapped, leftExtra);
    if (rightExtra)
        vmDeallocate(alignedEnd, rightExtra);

    return VMAlignedAllocation { aligned, VMAllocation { aligned, vmSize } };
#endif
}

} // namespace bmalloc
