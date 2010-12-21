/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef PageAllocation_h
#define PageAllocation_h

#include <wtf/Assertions.h>
#include <wtf/OSAllocator.h>
#include <wtf/PageBlock.h>
#include <wtf/UnusedParam.h>
#include <wtf/VMTags.h>
#include <algorithm>

#if OS(DARWIN)
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#endif

#if OS(HAIKU)
#include <OS.h>
#endif

#if OS(WINDOWS)
#include <malloc.h>
#include <windows.h>
#endif

#if OS(SYMBIAN)
#include <e32hal.h>
#include <e32std.h>
#endif

#if HAVE(ERRNO_H)
#include <errno.h>
#endif

#if HAVE(MMAP)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace WTF {

/*
    PageAllocation

    The PageAllocation class provides a cross-platform memory allocation interface
    with similar capabilities to posix mmap/munmap.  Memory is allocated by calling
    PageAllocation::allocate, and deallocated by calling deallocate on the
    PageAllocation object.  The PageAllocation holds the allocation's base pointer
    and size.

    The allocate method is passed the size required (which must be a multiple of
    the system page size, which can be accessed using PageAllocation::pageSize).
    Callers may also optinally provide a flag indicating the usage (for use by
    system memory usage tracking tools, where implemented), and boolean values
    specifying the required protection (defaulting to writable, non-executable).

    Where HAVE(PAGE_ALLOCATE_ALIGNED) is available memory may also be allocated
    with a specified alignment.  PageAllocation::allocateAligned requires that the
    size is a power of two that is >= system page size.
*/

class PageAllocation : private PageBlock {
public:
    static PageAllocation allocate(size_t size, OSAllocator::Usage usage = OSAllocator::UnknownUsage, bool writable = true, bool executable = false)
    {
        ASSERT(isPageAligned(size));
        return PageAllocation(OSAllocator::reserveAndCommit(size, usage, writable, executable), size);
    }

#if HAVE(PAGE_ALLOCATE_ALIGNED)
    static PageAllocation allocateAligned(size_t size, OSAllocator::Usage usage = OSAllocator::UnknownUsage)
    {
        ASSERT(isPageAligned(size));
        ASSERT(isPowerOfTwo(size));
        return systemAllocateAligned(size, usage);
    }
#endif

    PageAllocation();

    using PageBlock::operator bool;
    using PageBlock::base;
    using PageBlock::size;

    void deallocate()
    {
        ASSERT(*this);

        // Zero these before calling release; if this is *inside* allocation,
        // we won't be able to clear then after the call to OSAllocator::release.
        void* base = m_base;
        size_t size = m_size;
        m_base = 0;
        m_size = 0;

        OSAllocator::release(base, size);
    }

private:
    PageAllocation(void* base, size_t size)
        : PageBlock(base, size)
    {
    }

#if HAVE(PAGE_ALLOCATE_ALIGNED)
    static PageAllocation systemAllocateAligned(size_t, OSAllocator::Usage);
#endif
};

inline PageAllocation::PageAllocation()
    : PageBlock()
{
}


#if HAVE(MMAP)

inline PageAllocation PageAllocation::systemAllocateAligned(size_t size, OSAllocator::Usage usage)
{
#if OS(DARWIN)
    vm_address_t address = 0;
    int flags = VM_FLAGS_ANYWHERE;
    if (usage != -1)
        flags |= usage;
    vm_map(current_task(), &address, size, (size - 1), flags, MEMORY_OBJECT_NULL, 0, FALSE, PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE | PROT_EXEC, VM_INHERIT_DEFAULT);
    return PageAllocation(reinterpret_cast<void*>(address), size);
#elif HAVE(POSIX_MEMALIGN)
    void* address;
    posix_memalign(&address, size, size);
    return PageAllocation(address, size);
#else
    size_t extra = size - pageSize();

    // Check for overflow.
    if ((size + extra) < size)
        return PageAllocation(0, size);

#if OS(DARWIN) && !defined(BUILDING_ON_TIGER)
    int fd = usage;
#else
    int fd = -1;
#endif
    void* mmapResult = mmap(0, size + extra, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, fd, 0);
    if (mmapResult == MAP_FAILED)
        return PageAllocation(0, size);
    uintptr_t address = reinterpret_cast<uintptr_t>(mmapResult);

    size_t adjust = 0;
    if ((address & (size - 1)))
        adjust = size - (address & (size - 1));
    if (adjust > 0)
        munmap(reinterpret_cast<char*>(address), adjust);
    if (adjust < extra)
        munmap(reinterpret_cast<char*>(address + adjust + size), extra - adjust);
    address += adjust;

    return PageAllocation(reinterpret_cast<void*>(address), size);
#endif
}

#elif HAVE(VIRTUALALLOC)

#if HAVE(ALIGNED_MALLOC)
inline PageAllocation PageAllocation::systemAllocateAligned(size_t size, OSAllocator::Usage usage)
{
#if COMPILER(MINGW) && !COMPILER(MINGW64)
    void* address = __mingw_aligned_malloc(size, size);
#else
    void* address = _aligned_malloc(size, size);
#endif
    memset(address, 0, size);
    return PageAllocation(address, size);
}
#endif

#endif

}

using WTF::PageAllocation;

#endif // PageAllocation_h
