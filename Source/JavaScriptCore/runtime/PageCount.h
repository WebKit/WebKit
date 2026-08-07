/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include <limits>
#include <wtf/MathExtras.h>

namespace WTF {
class PrintStream;
}

namespace JSC {

#if USE(LARGE_TYPED_ARRAYS)
static_assert(sizeof(size_t) == sizeof(uint64_t));
#define MAX_ARRAY_BUFFER_SIZE (1ull << 34)
#else
static_assert(sizeof(size_t) == sizeof(uint32_t));
// Because we are using a size_t to store the size in bytes of array buffers, we cannot support 4GB on 32-bit platforms.
// So we are sticking with 2GB. It should in theory be possible to support up to (4GB - 1B) if anyone cares.
#define MAX_ARRAY_BUFFER_SIZE 0x7fffffffu
#endif

class PageCount {
public:
    PageCount()
        : m_pageCount(invalidPageCount)
    { }

    PageCount(uint64_t pageCount)
        : m_pageCount(pageCount)
    { }

    void dump(WTF::PrintStream&) const;

    uint64_t bytes() const { return m_pageCount * static_cast<uint64_t>(pageSize); }
    uint64_t pageCount() const { return m_pageCount; }

    static bool isValid(uint64_t pageCount)
    {
        return pageCount <= maxPageCount;
    }
    
    bool isValid() const
    {
        return isValid(m_pageCount);
    }

    static PageCount fromBytes(uint64_t bytes)
    {
        PageCount count = fromBytesUnchecked(bytes);
        RELEASE_ASSERT(count.isValid());
        return count;
    }

    static PageCount fromBytesUnchecked(uint64_t bytes)
    {
        RELEASE_ASSERT(bytes % pageSize == 0);
        return PageCount(bytes / pageSize);
    }

    static PageCount fromBytesWithRoundUp(uint64_t bytes)
    {
        return fromBytes(roundUpToMultipleOf<pageSize>(bytes));
    }

    static PageCount max()
    {
        return PageCount(maxPageCount);
    }

    explicit operator bool() const
    {
        return m_pageCount != invalidPageCount;
    }

    friend auto operator<=>(const PageCount&, const PageCount&) = default;

    PageCount operator+(const PageCount& other) const
    {
        if (sumOverflows<uint64_t>(m_pageCount, other.m_pageCount))
            return PageCount();
        uint64_t newCount = m_pageCount + other.m_pageCount;
        if (!PageCount::isValid(newCount))
            return PageCount();
        return PageCount(newCount);
    }

    static constexpr uint32_t pageSize = 64 * KB;

    // Page counts are declarative: a memory may declare a maximum this process cannot map, and must
    // still parse and instantiate at its initial size. This is the largest count any memory may declare.
    static constexpr uint32_t maxPageCount = 256 * 1024;

    static constexpr uint32_t maxMemory32PageCount = 64 * 1024;
    static constexpr uint64_t maxMemory32Bytes = static_cast<uint64_t>(maxMemory32PageCount) * pageSize;

private:
    static constexpr uint64_t invalidPageCount = std::numeric_limits<uint64_t>::max();
    static_assert(maxPageCount < invalidPageCount);
    // fromBytes() must accept the byte length of any array buffer.
    static_assert(MAX_ARRAY_BUFFER_SIZE <= static_cast<uint64_t>(maxPageCount) * pageSize);

    uint64_t m_pageCount;
};

} // namespace JSC
