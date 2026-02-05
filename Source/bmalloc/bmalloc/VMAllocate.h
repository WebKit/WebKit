/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#ifdef __cplusplus

#include "AllocationCounts.h"
#include "BAssert.h"
#include "BPlatform.h"
#include "BSyscall.h"
#include "BVMTags.h"
#include "Logging.h"
#include "Sizes.h"
#include <algorithm>
#include <cstddef>

#if BOS(WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if BOS(DARWIN)
#include <mach/mach.h>
#include <mach/vm_page_size.h>
#endif

#if defined(MADV_ZERO) && BOS(DARWIN)
#define BMALLOC_USE_MADV_ZERO 0
#else
#define BMALLOC_USE_MADV_ZERO 0
#endif

BALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace bmalloc {

#ifndef BMALLOC_VM_TAG
#define BMALLOC_VM_TAG VM_TAG_FOR_TCMALLOC_MEMORY
#endif

#if BOS(LINUX)
#define BMALLOC_NORESERVE MAP_NORESERVE
#else
#define BMALLOC_NORESERVE 0
#endif

#if BMALLOC_USE_MADV_ZERO
bool isMadvZeroSupported();
#endif

// The following require platform-specific implementations

inline size_t vmPageSize();

inline size_t vmPageSizePhysical();

inline void* tryVMAllocate(size_t vmSize, VMTag usage = VMTag::Malloc);

inline void vmDeallocate(void* p, size_t vmSize);

inline void vmRevokePermissions(void* p, size_t vmSize);

inline void vmZeroAndPurge(void* p, size_t vmSize, VMTag usage = VMTag::Malloc);

inline void vmDeallocatePhysicalPages(void* p, size_t vmSize);

inline void vmAllocatePhysicalPages(void* p, size_t vmSize);

// The following are platform-agnostic, implemented in terms of the above functions

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

inline void* vmAllocate(size_t vmSize, VMTag usage = VMTag::Malloc)
{
    void* result = tryVMAllocate(vmSize, usage);
    RELEASE_BASSERT(result);
    return result;
}

// Allocates vmSize bytes at a specified power-of-two alignment.
// Use this function to create maskable memory regions.
inline void* tryVMAllocate(size_t vmAlignment, size_t vmSize, VMTag usage = VMTag::Malloc)
{
    vmValidate(vmSize);
    vmValidate(vmAlignment);

    size_t mappedSize = vmAlignment + vmSize;
    if (mappedSize < vmAlignment || mappedSize < vmSize) // Check for overflow
        return nullptr;

    char* mapped = static_cast<char*>(tryVMAllocate(mappedSize, usage));
    if (!mapped)
        return nullptr;
    char* mappedEnd = mapped + mappedSize;

    char* aligned = roundUpToMultipleOf(vmAlignment, mapped);
    char* alignedEnd = aligned + vmSize;
    
    RELEASE_BASSERT(alignedEnd <= mappedEnd);
    
    if (size_t leftExtra = aligned - mapped)
        vmDeallocate(mapped, leftExtra);
    
    if (size_t rightExtra = mappedEnd - alignedEnd)
        vmDeallocate(alignedEnd, rightExtra);

    return aligned;
}

inline void* vmAllocate(size_t vmAlignment, size_t vmSize, VMTag usage = VMTag::Malloc)
{
    void* result = tryVMAllocate(vmAlignment, vmSize, usage);
    RELEASE_BASSERT(result);
    return result;
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


#if !BOS(WINDOWS)
// POSIX
inline size_t vmPageSize()
{
    static size_t cached;
    if (!cached) {
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize < 0)
            BCRASH();
        cached = pageSize;
    }
    return cached;
}

inline size_t vmPageSizePhysical()
{
#if BOS(DARWIN) && (BCPU(ARM64) || BCPU(ARM))
    return vm_kernel_page_size;
#else
    static size_t cached;
    if (!cached)
        cached = sysconf(_SC_PAGESIZE);
    return cached;
#endif
}

inline void* tryVMAllocate(size_t vmSize, VMTag usage)
{
    vmValidate(vmSize);
    void* result = mmap(0, vmSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | BMALLOC_NORESERVE, static_cast<int>(usage), 0);
    if (result == MAP_FAILED)
        return nullptr;
    return result;
}

inline void vmDeallocate(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    munmap(p, vmSize);
}

inline void vmRevokePermissions(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    mprotect(p, vmSize, PROT_NONE);
}

#if BENABLE(MTE) && BOS(DARWIN)
bool tryVmZeroAndPurgeMTECase(void* p, size_t vmSize, VMTag usage);
#endif // BENABLE(MTE) && BOS(DARWIN)

inline void vmZeroAndPurge(void* p, size_t vmSize, VMTag usage)
{
    vmValidate(p, vmSize);
    int flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED | BMALLOC_NORESERVE;
    int tag = static_cast<int>(usage);
#if BMALLOC_USE_MADV_ZERO
    if (isMadvZeroSupported()) {
        int rc = madvise(p, vmSize, MADV_ZERO);
        if (rc != -1)
            return;
    }
#endif
    BPROFILE_ZERO_FILL_PAGE(p, vmSize, flags, tag);
#if BENABLE(MTE) && BOS(DARWIN)
    if (tryVmZeroAndPurgeMTECase(p, vmSize, usage))
        return;
#endif // BENABLE(MTE) && BOS(DARWIN)
    // MAP_ANON guarantees the memory is zeroed. This will also cause
    // page faults on accesses to this range following this call.
    void* result = mmap(p, vmSize, PROT_READ | PROT_WRITE, flags, tag, 0);
    RELEASE_BASSERT(result == p);
}

inline void vmDeallocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidatePhysical(p, vmSize);
#if BOS(DARWIN)
    SYSCALL(madvise(p, vmSize, MADV_FREE_REUSABLE));
#elif BOS(FREEBSD)
    SYSCALL(madvise(p, vmSize, MADV_FREE));
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
#else
    SYSCALL(madvise(p, vmSize, MADV_NORMAL));
#if BOS(LINUX)
    SYSCALL(madvise(p, vmSize, MADV_DODUMP));
#endif
#endif
}
#else
// Windows
inline size_t vmPageSize()
{
    static size_t cached;
    if (!cached) {
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        cached = systemInfo.dwPageSize;
    }
    return cached;
}

inline size_t vmPageSizePhysical()
{
    static size_t cached;
    if (!cached) {
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        // Should this be dwAllocationGranularity?
        // It's the virtual address space granularity...
        // but choosing it makes Windows the only 64KB platform
        cached = systemInfo.dwPageSize;
    }
    return cached;
}

static inline DWORD protection(bool writable, bool executable)
{
    return executable ?
        (writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ) :
        (writable ? PAGE_READWRITE : PAGE_READONLY);
}

inline void* tryVMAllocate(size_t vmSize, VMTag usage)
{
    BUNUSED_PARAM(usage);
    const bool writable = true;
    const bool executable = true;
    return VirtualAlloc(nullptr, vmSize, MEM_RESERVE, protection(writable, executable));
}

inline void vmDeallocate(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    VirtualFree(p, vmSize, MEM_RELEASE);
}

inline void vmRevokePermissions(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    bool result = VirtualAlloc(p, vmSize, MEM_COMMIT, protection(false, false));
    if (!result)
        BCRASH();
}

inline void vmZeroAndPurge(void* p, size_t vmSize, VMTag usage)
{
    // Guarantees the memory is zeroed. This will also cause
    // page faults on accesses to this range following this
    BUNUSED_PARAM(usage);

    vmValidate(p, vmSize);

    size_t totalSeen = 0;
    void* currentPtr = p;
    while (totalSeen < vmSize) {
        MEMORY_BASIC_INFORMATION memInfo;
        VirtualQuery(currentPtr, &memInfo, sizeof(memInfo));
        RELEASE_BASSERT(memInfo.RegionSize > 0);
        size_t chunkSize = std::min(memInfo.RegionSize, vmSize - totalSeen);
        BOOL freeResult = VirtualFree(currentPtr, chunkSize, MEM_DECOMMIT);
        RELEASE_BASSERT(freeResult);
        void* allocResult = VirtualAlloc(currentPtr, chunkSize, MEM_COMMIT, PAGE_READWRITE);
        RELEASE_BASSERT(allocResult == currentPtr);
        currentPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(currentPtr) + memInfo.RegionSize);
        totalSeen += memInfo.RegionSize;
    }
}

inline void vmDeallocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidatePhysical(p, vmSize);
    bool writable = true;
    bool executable = true;
    VirtualAlloc(p, vmSize, MEM_RESET, protection(writable, executable));
}

inline void vmAllocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidatePhysical(p, vmSize);
    bool writable = true;
    bool executable = true;
    VirtualAlloc(p, vmSize, MEM_COMMIT, protection(writable, executable));
}
#endif

} // namespace bmalloc

BALLOW_UNSAFE_BUFFER_USAGE_END

#endif // __cplusplus
