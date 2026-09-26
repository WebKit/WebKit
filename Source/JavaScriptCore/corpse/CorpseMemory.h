/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <JavaScriptCore/CorpseAddress.h>
#include <mach/mach.h>
#include <memory>
#include <stdint.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {
namespace Corpse {

// Memory represents the memory map of the Snapshot, and is the manager through which
// the client can requests views into the remote memory. The views are requested as
// a Memory::Ptr to a type, or a Memory::Span of a range of memory. Memory will map
// the remote memory into this process' address space as Regions (in multiples of
// pages), and manage the life cycle of these Regions.
//
// When mapping a Region, Memory aims to map the smallest range possible. Hence, it is
// possible that subsequest requests lands in the same address range as an existing
// Region but spans more pages beyond the end of the existing Region. In such a
// scenario, we'll map a new Region that may overlap an existing Region. We do expect
// this scenario to be rare. Regardless, as a consequence of these multiple mappings,
// when comparing addresses for uniqueness, the client should always use the remote
// Address.
//
// Ptr and Span will retain and release the Region that serves as their underlying
// backing.

class Memory {
    WTF_FORBID_HEAP_ALLOCATION; // Because Memory is always embedded in its owner Snapshot.
public:
    enum class Error : uint8_t {
        None,
        InvalidRequest,    // Zero size, or the range runs off the end of the address space.
        UnmappedHole,      // The corpse does not have the range mapped over its whole length.
        NotReadable,       // Some page of the range denies read access.
        OutOfAddressSpace, // No room to place the mapping in this process.
        Unknown,           // kernResult() says what the kernel reported.
    };

    template<typename T> class Ptr; // See CorpseMemoryPtr.h.
    template<typename T> class Span; // See CorpseMemorySpan.h.

    explicit Memory(mach_port_t corpsePort);
    ~Memory();

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    template<typename T> Ptr<T> ptr(Address);
    template<typename T> Span<T> span(Address, size_t count);

    size_t regionCount() const;
    size_t mappedPageCount() const;

    void dump(const char* indent = "    ") const;

    static size_t pageSize();

private:
    class Region {
        WTF_MAKE_TZONE_ALLOCATED(Region);
    public:
        Region(Memory&, Address remoteBase, size_t pageCount, const uint8_t* localBase);

        Address remoteBase() const { return m_remoteBase; }
        size_t pageCount() const { return m_pageCount; }
        const uint8_t* localBase() const { return m_localBase; }
        unsigned refCount() const { return m_refCount; }

        void retain() { ++m_refCount; }
        void release();

    private:
        Memory& m_memory;
        Address m_remoteBase;
        size_t m_pageCount { 0 };
        const uint8_t* m_localBase { nullptr };
        unsigned m_refCount { 0 };
    };

    struct MapResult {
        Region* region { nullptr };
        const uint8_t* data { nullptr };
        Error error { Error::None };
        kern_return_t kernResult { KERN_SUCCESS };
    };

    MapResult map(Address, size_t bytes);
    void unmap(Region*);

    mach_port_t m_corpsePort { MACH_PORT_NULL };

    // The Address key must be page aligned. The value is a Vector because there
    // can be more than one Region of different sizes which start at the sane page
    // Address. The Vector is sorted in ascending size (i.e. pageCount).
    HashMap<Address, Vector<std::unique_ptr<Region>>> m_regions;
};

} // namespace Corpse
} // namespace JSC

#include <JavaScriptCore/CorpseMemoryPtr.h>
#include <JavaScriptCore/CorpseMemorySpan.h>

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
