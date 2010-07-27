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

#include <wtf/UnusedParam.h>
#include <wtf/VMTags.h>

#if OS(SYMBIAN)
#include <e32std.h>
#endif

#if HAVE(MMAP)
#define PAGE_ALLOCATION_ALLOCATE_AT 1
#else
#define PAGE_ALLOCATION_ALLOCATE_AT 0
#endif

namespace WTF {

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

    // Create a PageAllocation object representing a sub-region of an existing allocation;
    // deallocate should never be called on an object represnting a subregion, only on the
    // initial allocation.
    PageAllocation(void* base, size_t size, const PageAllocation& parent)
        : m_base(base)
        , m_size(size)
#if OS(SYMBIAN)
        , m_chunk(parent.chunk)
#endif
    {
#if defined(NDEBUG) && !OS(SYMBIAN)
        UNUSED_PARAM(parent);
#endif
        ASSERT(base >= parent.m_base);
        ASSERT(size <= parent.m_size);
        ASSERT(static_cast<char*>(base) + size <= static_cast<char*>(parent.m_base) + parent.m_size);
    }

    void* base() const { return m_base; }
    size_t size() const { return m_size; }

    bool operator!() const { return !m_base; }

    bool commit(void*, size_t, bool writable = true, bool executable = false) const;
    void decommit(void*, size_t) const;
    void deallocate();

    static PageAllocation allocate(size_t, Usage = UnknownUsage, bool writable = true, bool executable = false);
    static PageAllocation reserve(size_t, Usage = UnknownUsage, bool writable = true, bool executable = false);
#if PAGE_ALLOCATION_ALLOCATE_AT
    static PageAllocation allocateAt(void* address, bool fixed, size_t, Usage = UnknownUsage, bool writable = true, bool executable = false);
    static PageAllocation reserveAt(void* address, bool fixed, size_t, Usage = UnknownUsage, bool writable = true, bool executable = false);
#endif
    static size_t pagesize();

private:
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

    void* m_base;
    size_t m_size;
#if OS(SYMBIAN)
    RChunk* m_chunk;
#endif
};

}

using WTF::PageAllocation;

#endif // PageAllocation_h
