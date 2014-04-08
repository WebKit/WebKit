/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef VMAllocate_h
#define VMAllocate_h

#include "BAssert.h"
#include "Range.h"
#include "Sizes.h"
#include "Syscall.h"
#include <algorithm>
#include <mach/vm_statistics.h>
#include <sys/mman.h>
#include <unistd.h>

namespace bmalloc {

#define BMALLOC_VM_TAG VM_MAKE_TAG(VM_MEMORY_TCMALLOC)
    
static const size_t vmPageSize = 16 * kB; // Least upper bound of the OS's we support.
static const size_t vmPageMask = ~(vmPageSize - 1);
    
inline size_t vmSize(size_t size)
{
    return roundUpToMultipleOf<vmPageSize>(size);
}
    
inline void vmValidate(size_t vmSize)
{
    UNUSED(vmSize);
    ASSERT(vmSize);
    ASSERT(vmSize == bmalloc::vmSize(vmSize));
}

inline void vmValidate(void* p, size_t vmSize)
{
    vmValidate(vmSize);
    
    // We use getpagesize() here instead of vmPageSize because vmPageSize is
    // allowed to be larger than the OS's true page size.
    UNUSED(p);
    ASSERT(p);
    ASSERT(p == mask(p, ~(getpagesize() - 1)));
}

inline void* vmAllocate(size_t vmSize)
{
    vmValidate(vmSize);
    return mmap(0, vmSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, BMALLOC_VM_TAG, 0);
}

inline void vmDeallocate(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    munmap(p, vmSize);
}

// Allocates vmSize bytes at a specified offset from a power-of-two alignment.
// Use this function to create pointer masks that aren't simple powers of two.

inline std::pair<void*, Range> vmAllocate(size_t vmSize, size_t alignment, size_t offset)
{
    vmValidate(vmSize);
    ASSERT(isPowerOfTwo(alignment));

    size_t mappedSize = std::max(vmSize, alignment) + alignment;
    char* mapped = static_cast<char*>(vmAllocate(mappedSize));
    
    uintptr_t alignmentMask = alignment - 1;
    if (!test(mapped, alignmentMask) && offset + vmSize <= alignment) {
        // We got two perfectly aligned regions. Give one back to avoid wasting
        // VM unnecessarily. This isn't costly because we aren't making holes.
        vmDeallocate(mapped + alignment, alignment);
        return std::make_pair(mapped + offset, Range(mapped, alignment));
    }

    // We got an unaligned region. Keep the whole thing to avoid creating holes,
    // and hopefully realign the VM allocator for future allocations. On Darwin,
    // VM holes trigger O(N^2) behavior in mmap, so we want to minimize them.
    char* mappedAligned = mask(mapped, ~alignmentMask) + alignment;
    return std::make_pair(mappedAligned + offset, Range(mapped, mappedSize));
}

inline void vmDeallocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    SYSCALL(madvise(p, vmSize, MADV_FREE_REUSABLE));
}

inline void vmAllocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    SYSCALL(madvise(p, vmSize, MADV_FREE_REUSE));
}

// Trims requests that are un-page-aligned. NOTE: size must be at least a page.
inline void vmDeallocatePhysicalPagesSloppy(void* p, size_t size)
{
    ASSERT(size >= vmPageSize);

    char* begin = roundUpToMultipleOf<vmPageSize>(static_cast<char*>(p));
    char* end = roundDownToMultipleOf<vmPageSize>(static_cast<char*>(p) + size);

    Range range(begin, end - begin);
    if (!range)
        return;
    vmDeallocatePhysicalPages(range.begin(), range.size());
}

// Expands requests that are un-page-aligned. NOTE: Allocation must proceed left-to-right.
inline void vmAllocatePhysicalPagesSloppy(void* p, size_t size)
{
    char* begin = roundUpToMultipleOf<vmPageSize>(static_cast<char*>(p));
    char* end = roundUpToMultipleOf<vmPageSize>(static_cast<char*>(p) + size);

    Range range(begin, end - begin);
    if (!range)
        return;
    vmAllocatePhysicalPages(range.begin(), range.size());
}

} // namespace bmalloc

#endif // VMAllocate_h
