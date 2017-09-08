/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

#if ENABLE(MASM_PROBE)

namespace JSC {

struct ProbeContext;

namespace Probe {

class Page {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Page(void* baseAddress);

    static void* baseAddressFor(void* p)
    {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p) & ~s_pageMask);
    }
    static void* chunkAddressFor(void* p)
    {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p) & ~s_chunkMask);
    }

    void* baseAddress() { return m_baseLogicalAddress; }

    template<typename T>
    T get(void* logicalAddress)
    {
        void* from = physicalAddressFor(logicalAddress);
        typename std::remove_const<T>::type to { };
        std::memcpy(&to, from, sizeof(to)); // Use std::memcpy to avoid strict aliasing issues.
        return to;
    }
    template<typename T>
    T get(void* logicalBaseAddress, ptrdiff_t offset)
    {
        return get<T>(reinterpret_cast<uint8_t*>(logicalBaseAddress) + offset);
    }

    template<typename T>
    void set(void* logicalAddress, T value)
    {
        m_dirtyBits |= dirtyBitFor(logicalAddress);
        void* to = physicalAddressFor(logicalAddress);
        std::memcpy(to, &value, sizeof(T)); // Use std::memcpy to avoid strict aliasing issues.
    }
    template<typename T>
    void set(void* logicalBaseAddress, ptrdiff_t offset, T value)
    {
        set<T>(reinterpret_cast<uint8_t*>(logicalBaseAddress) + offset, value);
    }

    bool hasWritesToFlush() const { return !!m_dirtyBits; }
    void flushWritesIfNeeded()
    {
        if (m_dirtyBits)
            flushWrites();
    }

private:
    uintptr_t dirtyBitFor(void* logicalAddress)
    {
        uintptr_t offset = reinterpret_cast<uintptr_t>(logicalAddress) & s_pageMask;
        return static_cast<uintptr_t>(1) << (offset >> s_chunkSizeShift);
    }

    void* physicalAddressFor(void* logicalAddress)
    {
        return reinterpret_cast<uint8_t*>(logicalAddress) + m_physicalAddressOffset;
    }

    void flushWrites();

    void* m_baseLogicalAddress { nullptr };
    uintptr_t m_dirtyBits { 0 };
    ptrdiff_t m_physicalAddressOffset;

    static constexpr size_t s_pageSize = 1024;
    static constexpr uintptr_t s_pageMask = s_pageSize - 1;
    static constexpr size_t s_chunksPerPage = sizeof(uintptr_t) * 8; // sizeof(m_dirtyBits) in bits.
    static constexpr size_t s_chunkSize = s_pageSize / s_chunksPerPage;
    static constexpr uintptr_t s_chunkMask = s_chunkSize - 1;
#if USE(JSVALUE64)
    static constexpr size_t s_chunkSizeShift = 4;
#else
    static constexpr size_t s_chunkSizeShift = 5;
#endif
    static_assert(s_pageSize > s_chunkSize, "bad pageSize or chunkSize");
    static_assert(s_chunkSize == (1 << s_chunkSizeShift), "bad chunkSizeShift");


    typedef typename std::aligned_storage<s_pageSize, std::alignment_of<uintptr_t>::value>::type Buffer;
    Buffer m_buffer;
};

class Stack {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Stack()
        : m_lowWatermark(reinterpret_cast<void*>(-1))
        , m_stackBounds(Thread::current().stack())
    { }
    Stack(Stack&& other);

    void* lowWatermark()
    {
        // We use the chunkAddress for the low watermark because we'll be doing write backs
        // to the stack in increments of chunks. Hence, we'll treat the lowest address of
        // the chunk as the low watermark of any given set address.
        return Page::chunkAddressFor(m_lowWatermark);
    }

    template<typename T>
    T get(void* address)
    {
        Page* page = pageFor(address);
        return page->get<T>(address);
    }
    template<typename T>
    T get(void* logicalBaseAddress, ptrdiff_t offset)
    {
        return get<T>(reinterpret_cast<uint8_t*>(logicalBaseAddress) + offset);
    }

    template<typename T>
    void set(void* address, T value)
    {
        Page* page = pageFor(address);
        page->set<T>(address, value);

        if (address < m_lowWatermark)
            m_lowWatermark = address;
    }
    template<typename T>
    void set(void* logicalBaseAddress, ptrdiff_t offset, T value)
    {
        set<T>(reinterpret_cast<uint8_t*>(logicalBaseAddress) + offset, value);
    }

    JS_EXPORT_PRIVATE Page* ensurePageFor(void* address);

    void* newStackPointer() const { return m_newStackPointer; };
    void setNewStackPointer(void* sp) { m_newStackPointer = sp; };

    bool hasWritesToFlush();
    void flushWrites();

#if !ASSERT_DISABLED
    bool isValid() { return m_isValid; }
#endif

private:
    Page* pageFor(void* address)
    {
        if (LIKELY(Page::baseAddressFor(address) == m_lastAccessedPageBaseAddress))
            return m_lastAccessedPage;
        return ensurePageFor(address);
    }

    void* m_newStackPointer { nullptr };
    void* m_lowWatermark;

    // A cache of the last accessed page details for quick access.
    void* m_lastAccessedPageBaseAddress { nullptr };
    Page* m_lastAccessedPage { nullptr };

    StackBounds m_stackBounds;
    HashMap<void*, std::unique_ptr<Page>> m_pages;

#if !ASSERT_DISABLED
    bool m_isValid { true };
#endif
};

} // namespace Probe
} // namespace JSC

#endif // ENABLE(MASM_PROBE)
