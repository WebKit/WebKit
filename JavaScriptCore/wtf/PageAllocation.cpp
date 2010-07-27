/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "PageAllocation.h"

#if HAVE(ERRNO_H)
#include <errno.h>
#endif

#if HAVE(MMAP)
#include <sys/mman.h>
#endif

#if OS(WINDOWS)
#include "windows.h"
#endif

#if OS(SYMBIAN)
#include <e32hal.h>
#endif

namespace WTF {

#if HAVE(MMAP)

#if HAVE(MADV_FREE_REUSE)
bool PageAllocation::commit(void* start, size_t size, bool writable, bool executable) const
{
    UNUSED_PARAM(writable);
    UNUSED_PARAM(executable);
    while (madvise(start, size, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
    return true;
}

void PageAllocation::decommit(void* start, size_t size) const
{
    while (madvise(start, size, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
}
#elif HAVE(MADV_DONTNEED)
bool PageAllocation::commit(void* start, size_t size, bool writable, bool executable) const
{
    UNUSED_PARAM(writable);
    UNUSED_PARAM(executable);
    return true;
}

void PageAllocation::decommit(void*, size_t) const
{
    while (madvise(start, size, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
}
#else
bool PageAllocation::commit(void*, size_t, bool writable, bool executable) const
{
    UNUSED_PARAM(writable);
    UNUSED_PARAM(executable);
    return true;
}

void PageAllocation::decommit(void*, size_t) const
{
}
#endif

PageAllocation PageAllocation::allocate(size_t size, Usage usage, bool writable, bool executable)
{
    return allocateAt(0, false, size, usage, writable, executable);
}

PageAllocation PageAllocation::reserve(size_t size, Usage usage, bool writable, bool executable)
{
    return reserveAt(0, false, size, usage, writable, executable);
}

PageAllocation PageAllocation::allocateAt(void* address, bool fixed, size_t size, Usage usage, bool writable, bool executable)
{
    int flags = MAP_PRIVATE | MAP_ANON;
    if (fixed)
        flags |= MAP_FIXED;

    int protection = PROT_READ;
    if (writable)
        protection |= PROT_WRITE;
    if (executable)
        protection |= PROT_EXEC;

    void* base = mmap(address, size, protection, flags, usage, 0);
    if (base == MAP_FAILED)
        base = 0;

    return PageAllocation(base, size);
}

PageAllocation PageAllocation::reserveAt(void* address, bool fixed, size_t size, Usage usage, bool writable, bool executable)
{
    PageAllocation result = allocateAt(address, fixed, size, usage, writable, executable);
    if (!!result)
        result.decommit(result.base(), size);
    return result;
}

void PageAllocation::deallocate()
{
    int result = munmap(m_base, m_size);
    ASSERT_UNUSED(result, !result);
    m_base = 0;
}

size_t PageAllocation::pagesize()
{
    static size_t size = 0;
    if (!size)
        size = getpagesize();
    return size;
}

#elif HAVE(VIRTUALALLOC)

static DWORD protection(bool writable, bool executable)
{
    if (executable)
        return writable ?PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    return writable ?PAGE_READWRITE : PAGE_READONLY;
}

bool PageAllocation::commit(void* start, size_t size, bool writable, bool executable) const
{
    return VirtualAlloc(start, size, MEM_COMMIT, protection(writable, executable)) == start;
}

void PageAllocation::decommit(void* start, size_t size) const
{
    VirtualFree(start, size, MEM_DECOMMIT);
}

PageAllocation PageAllocation::allocate(size_t size, Usage, bool writable, bool executable)
{
    return PageAllocation(VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, protection(writable, executable)), size);
}

PageAllocation PageAllocation::reserve(size_t size, Usage usage, bool writable, bool executable)
{
    return PageAllocation(VirtualAlloc(0, size, MEM_RESERVE, protection(writable, executable)), size);
}

void PageAllocation::deallocate()
{
    VirtualFree(m_base, 0, MEM_RELEASE); 
    m_base = 0;
}

size_t PageAllocation::pagesize()
{
    static size_t size = 0;
    if (!size) {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        size = system_info.dwPageSize;
    }
    return size;
}

#elif OS(SYMBIAN)

bool PageAllocation::commit(void* start, size_t size, bool writable, bool executable) const
{
    if (m_chunk) {
        intptr_t offset = static_cast<intptr_t>(base()) - static_cast<intptr_t>(start);
        m_chunk->Commit(offset, size);
    }
    return true;
}

void PageAllocation::decommit(void* start, size_t size) const
{
    if (m_chunk) {
        intptr_t offset = static_cast<intptr_t>(base()) - static_cast<intptr_t>(start);
        m_chunk->Decommit(offset, size);
    }
}

PageAllocation PageAllocation::allocate(size_t size, Usage usage, bool writable, bool executable)
{
    if (!executable)
        return PageAllocation(fastMalloc(size), size, 0);
    RChunk* rchunk = new RChunk();
    TInt errorCode = rchunk->CreateLocalCode(size, size);
    return PageAllocation(rchunk->Base(), size, rchunk);
}

PageAllocation PageAllocation::reserve(size_t size, Usage usage, bool writable, bool executable)
{
    if (!executable)
        return PageAllocation(fastMalloc(size), size, 0);
    RChunk* rchunk = new RChunk();
    TInt errorCode = rchunk->CreateLocalCode(0, size);
    return PageAllocation(rchunk, rchunk->Base(), size);
}

void PageAllocation::deallocate()
{
    if (m_chunk) {
        m_chunk->Close();
        delete m_chunk;
    } else
        fastFree(m_base);
    m_base = 0;
}

size_t PageAllocation::pagesize()
{
    static TInt page_size = 0;
    if (!page_size)
        UserHal::PageSizeInBytes(page_size);
    return page_size;
}

#endif

}
