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
#include <wtf/UnusedParam.h>
#include <wtf/VMTags.h>

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

    Where HAVE(PAGE_ALLOCATE_AT) and HAVE(PAGE_ALLOCATE_ALIGNED) are available
    memory may also be allocated at a specified address, or with a specified
    alignment respectively.  PageAllocation::allocateAt take an address to try
    to allocate at, and a boolean indicating whether this behaviour is strictly
    required (if this address is unavailable, should memory at another address
    be allocated instead).  PageAllocation::allocateAligned requires that the
    size is a power of two that is >= system page size.
*/
class PageAllocation {
public:
    enum Usage {
        UnknownUsage = -1,
        FastMallocPages = VM_TAG_FOR_TCMALLOC_MEMORY,
        JSGCHeapPages = VM_TAG_FOR_COLLECTOR_MEMORY,
        JSVMStackPages = VM_TAG_FOR_REGISTERFILE_MEMORY,
        JSJITCodePages = VM_TAG_FOR_EXECUTABLEALLOCATOR_MEMORY,
    };

    PageAllocation()
        : m_base(0)
        , m_size(0)
#if OS(SYMBIAN)
        , m_chunk(0)
#endif
    {
    }

    bool operator!() const { return !m_base; }
    void* base() const { return m_base; }
    size_t size() const { return m_size; }

    static PageAllocation allocate(size_t size, Usage usage = UnknownUsage, bool writable = true, bool executable = false)
    {
        ASSERT(isPageAligned(size));
        return systemAllocate(size, usage, writable, executable);
    }

#if HAVE(PAGE_ALLOCATE_AT)
    static PageAllocation allocateAt(void* address, bool fixed, size_t size, Usage usage = UnknownUsage, bool writable = true, bool executable = false)
    {
        ASSERT(isPageAligned(address));
        ASSERT(isPageAligned(size));
        return systemAllocateAt(address, fixed, size, usage, writable, executable);
    }
#endif

#if HAVE(PAGE_ALLOCATE_ALIGNED)
    static PageAllocation allocateAligned(size_t size, Usage usage = UnknownUsage)
    {
        ASSERT(isPageAligned(size));
        ASSERT(isPowerOfTwo(size));
        return systemAllocateAligned(size, usage);
    }
#endif

    void deallocate()
    {
        ASSERT(m_base);
        systemDeallocate(true);
    }

    static size_t pageSize()
    {
        if (!s_pageSize)
            s_pageSize = systemPageSize();
        ASSERT(isPowerOfTwo(s_pageSize));
        return s_pageSize;
    }

#ifndef NDEBUG
    static bool isPageAligned(void* address) { return !(reinterpret_cast<intptr_t>(address) & (pageSize() - 1)); }
    static bool isPageAligned(size_t size) { return !(size & (pageSize() - 1)); }
    static bool isPowerOfTwo(size_t size) { return !(size & (size - 1)); }
    static int lastError();
#endif

protected:
#if OS(SYMBIAN)
    PageAllocation(void* base, size_t size, RChunk* chunk)
        : m_base(base)
        , m_size(size)
        , m_chunk(chunk)
    {
    }
#else
    PageAllocation(void* base, size_t size)
        : m_base(base)
        , m_size(size)
    {
    }
#endif

    static PageAllocation systemAllocate(size_t, Usage, bool, bool);
#if HAVE(PAGE_ALLOCATE_AT)
    static PageAllocation systemAllocateAt(void*, bool, size_t, Usage, bool, bool);
#endif
#if HAVE(PAGE_ALLOCATE_ALIGNED)
    static PageAllocation systemAllocateAligned(size_t, Usage);
#endif
    // systemDeallocate takes a parameter indicating whether memory is currently committed
    // (this should always be true for PageAllocation, false for PageReservation).
    void systemDeallocate(bool committed);
    static size_t systemPageSize();

    void* m_base;
    size_t m_size;
#if OS(SYMBIAN)
    RChunk* m_chunk;
#endif

    static JS_EXPORTDATA size_t s_pageSize;
};


#if HAVE(MMAP)


inline PageAllocation PageAllocation::systemAllocate(size_t size, Usage usage, bool writable, bool executable)
{
    return systemAllocateAt(0, false, size, usage, writable, executable);
}

inline PageAllocation PageAllocation::systemAllocateAt(void* address, bool fixed, size_t size, Usage usage, bool writable, bool executable)
{
    int protection = PROT_READ;
    if (writable)
        protection |= PROT_WRITE;
    if (executable)
        protection |= PROT_EXEC;

    int flags = MAP_PRIVATE | MAP_ANON;
    if (fixed)
        flags |= MAP_FIXED;

#if OS(DARWIN) && !defined(BUILDING_ON_TIGER)
    int fd = usage;
#else
    int fd = -1;
#endif

    void* base = mmap(address, size, protection, flags, fd, 0);
    if (base == MAP_FAILED)
        base = 0;
    return PageAllocation(base, size);
}

inline PageAllocation PageAllocation::systemAllocateAligned(size_t size, Usage usage)
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

inline void PageAllocation::systemDeallocate(bool)
{
    int result = munmap(m_base, m_size);
    ASSERT_UNUSED(result, !result);
    m_base = 0;
}

inline size_t PageAllocation::systemPageSize()
{
    return getpagesize();
}


#elif HAVE(VIRTUALALLOC)


inline PageAllocation PageAllocation::systemAllocate(size_t size, Usage, bool writable, bool executable)
{
    DWORD protection = executable ?
        (writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ) :
        (writable ? PAGE_READWRITE : PAGE_READONLY);
    return PageAllocation(VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, protection), size);
}

#if HAVE(ALIGNED_MALLOC)
inline PageAllocation PageAllocation::systemAllocateAligned(size_t size, Usage usage)
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

inline void PageAllocation::systemDeallocate(bool committed)
{
#if OS(WINCE)
    if (committed)
        VirtualFree(m_base, m_size, MEM_DECOMMIT);
#else
    UNUSED_PARAM(committed);
#endif
    VirtualFree(m_base, 0, MEM_RELEASE); 
    m_base = 0;
}

inline size_t PageAllocation::systemPageSize()
{
    static size_t size = 0;
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    size = system_info.dwPageSize;
    return size;
}


#elif OS(SYMBIAN)


inline PageAllocation PageAllocation::systemAllocate(size_t size, Usage usage, bool writable, bool executable)
{
    RChunk* rchunk = new RChunk();
    if (executable)
        rchunk->CreateLocalCode(size, size);
    else
        rchunk->CreateLocal(size, size);
    return PageAllocation(rchunk->Base(), size, rchunk);
}

inline void PageAllocation::systemDeallocate(bool)
{
    m_chunk->Close();
    delete m_chunk;
    m_base = 0;
}

inline size_t PageAllocation::systemPageSize()
{
    static TInt page_size = 0;
    UserHal::PageSizeInBytes(page_size);
    return page_size;
}


#endif


}

using WTF::PageAllocation;

#endif // PageAllocation_h
