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

#ifndef PageReservation_h
#define PageReservation_h

#include <wtf/PageAllocation.h>

namespace WTF {

/*
    PageReservation

    Like PageAllocation, the PageReservation class provides a cross-platform memory
    allocation interface, but with a set of capabilities more similar to that of
    VirtualAlloc than posix mmap.  PageReservation can be used to allocate virtual
    memory without committing physical memory pages using PageReservation::reserve.
    Following a call to reserve all memory in the region is in a decommited state,
    in which the memory should not be used (accessing the memory may cause a fault).

    Before using memory it must be committed by calling commit, which is passed start
    and size values (both of which require system page size granularity).  One the
    committed memory is no longer needed 'decommit' may be called to return the
    memory to its devommitted state.  Commit should only be called on memory that is
    currently decommitted, and decommit should only be called on memory regions that
    are currently committed.  All memory should be decommited before the reservation
    is deallocated.  Values in memory may not be retained accross a pair of calls if
    the region of memory is decommitted and then committed again.

    Where HAVE(PAGE_ALLOCATE_AT) is available a PageReservation::reserveAt method
    also exists, with behaviour mirroring PageAllocation::allocateAt.

    Memory protection should not be changed on decommitted memory, and if protection
    is changed on memory while it is committed it should be returned to the orignal
    protection before decommit is called.

    Note: Inherits from PageAllocation privately to prevent clients accidentally
    calling PageAllocation::deallocate on a PageReservation.
*/
class PageReservation : private PageAllocation {
public:
    PageReservation()
    {
    }

    using PageAllocation::operator!;
    using PageAllocation::base;
    using PageAllocation::size;

    bool commit(void* start, size_t size)
    {
        ASSERT(m_base);
        ASSERT(isPageAligned(start));
        ASSERT(isPageAligned(size));

        bool commited = systemCommit(start, size);
#ifndef NDEBUG
        if (commited)
            m_committed += size;
#endif
        return commited;
    }
    void decommit(void* start, size_t size)
    {
        ASSERT(m_base);
        ASSERT(isPageAligned(start));
        ASSERT(isPageAligned(size));

#ifndef NDEBUG
        m_committed -= size;
#endif
        systemDecommit(start, size);
    }

    static PageReservation reserve(size_t size, Usage usage = UnknownUsage, bool writable = true, bool executable = false)
    {
        ASSERT(isPageAligned(size));
        return systemReserve(size, usage, writable, executable);
    }

#if HAVE(PAGE_ALLOCATE_AT)
    static PageReservation reserveAt(void* address, bool fixed, size_t size, Usage usage = UnknownUsage, bool writable = true, bool executable = false)
    {
        ASSERT(isPageAligned(address));
        ASSERT(isPageAligned(size));
        return systemReserveAt(address, fixed, size, usage, writable, executable);
    }
#endif

    void deallocate()
    {
        ASSERT(m_base);
        ASSERT(!m_committed);
        systemDeallocate(false);
    }

#ifndef NDEBUG
    using PageAllocation::lastError;
#endif

private:
#if OS(SYMBIAN)
    PageReservation(void* base, size_t size, RChunk* chunk)
        : PageAllocation(base, size, chunk)
#else
    PageReservation(void* base, size_t size)
        : PageAllocation(base, size)
#endif
#ifndef NDEBUG
        , m_committed(0)
#endif
    {
    }

    bool systemCommit(void*, size_t);
    void systemDecommit(void*, size_t);
    static PageReservation systemReserve(size_t, Usage, bool, bool);
#if HAVE(PAGE_ALLOCATE_AT)
    static PageReservation systemReserveAt(void*, bool, size_t, Usage, bool, bool);
#endif

#if HAVE(VIRTUALALLOC)
    DWORD m_protection;
#endif
#ifndef NDEBUG
    size_t m_committed;
#endif
};


#if HAVE(MMAP)


inline bool PageReservation::systemCommit(void* start, size_t size)
{
#if HAVE(MADV_FREE_REUSE)
    while (madvise(start, size, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
#else
    UNUSED_PARAM(start);
    UNUSED_PARAM(size);
#endif
    return true;
}

inline void PageReservation::systemDecommit(void* start, size_t size)
{
#if HAVE(MADV_FREE_REUSE)
    while (madvise(start, size, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
#elif HAVE(MADV_FREE)
    while (madvise(start, size, MADV_FREE) == -1 && errno == EAGAIN) { }
#elif HAVE(MADV_DONTNEED)
    while (madvise(start, size, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#else
    UNUSED_PARAM(start);
    UNUSED_PARAM(size);
#endif
}

inline PageReservation PageReservation::systemReserve(size_t size, Usage usage, bool writable, bool executable)
{
    return systemReserveAt(0, false, size, usage, writable, executable);
}

inline PageReservation PageReservation::systemReserveAt(void* address, bool fixed, size_t size, Usage usage, bool writable, bool executable)
{
    void* base = systemAllocateAt(address, fixed, size, usage, writable, executable).base();
#if HAVE(MADV_FREE_REUSE)
    // When using MADV_FREE_REUSE we keep all decommitted memory marked as REUSABLE.
    // We call REUSE on commit, and REUSABLE on decommit.
    if (base)
        while (madvise(base, size, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
#endif
    return PageReservation(base, size);
}


#elif HAVE(VIRTUALALLOC)


inline bool PageReservation::systemCommit(void* start, size_t size)
{
    return VirtualAlloc(start, size, MEM_COMMIT, m_protection) == start;
}

inline void PageReservation::systemDecommit(void* start, size_t size)
{
    VirtualFree(start, size, MEM_DECOMMIT);
}

inline PageReservation PageReservation::systemReserve(size_t size, Usage usage, bool writable, bool executable)
{
    // Record the protection for use during commit.
    DWORD protection = executable ?
        (writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ) :
        (writable ? PAGE_READWRITE : PAGE_READONLY);
    PageReservation reservation(VirtualAlloc(0, size, MEM_RESERVE, protection), size);
    reservation.m_protection = protection;
    return reservation;
}


#elif OS(SYMBIAN)


inline bool PageReservation::systemCommit(void* start, size_t size)
{
    intptr_t offset = reinterpret_cast<intptr_t>(start) - reinterpret_cast<intptr_t>(m_base);
    m_chunk->Commit(offset, size);
    return true;
}

inline void PageReservation::systemDecommit(void* start, size_t size)
{
    intptr_t offset = reinterpret_cast<intptr_t>(start) - reinterpret_cast<intptr_t>(m_base);
    m_chunk->Decommit(offset, size);
}

inline PageReservation PageReservation::systemReserve(size_t size, Usage usage, bool writable, bool executable)
{
    RChunk* rchunk = new RChunk();
    if (executable)
        rchunk->CreateLocalCode(0, size);
    else
        rchunk->CreateDisconnectedLocal(0, 0, size);
    return PageReservation(rchunk->Base(), size, rchunk);
}


#endif


}

using WTF::PageReservation;

#endif // PageReservation_h
