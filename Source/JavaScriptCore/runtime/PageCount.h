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

// A count of pages, each 2**log2PageSize bytes. Wasm memories may use a page size other than the
// default, so a count is only meaningful alongside the unit it counts in: mixing units is a factor
// of 65536, and the comparisons below gate memory growth. The operators therefore assert that the
// units agree rather than silently comparing raw counts.
class PageCount {
public:
    static constexpr uint8_t defaultLog2PageSize = 16;
    static constexpr uint32_t pageSize = 1u << defaultLog2PageSize;
    static_assert(pageSize == 64 * KB);

    PageCount() = default;

    explicit PageCount(uint64_t pageCount, uint8_t log2PageSize = defaultLog2PageSize)
        : m_pageCount(pageCount)
        , m_log2PageSize(log2PageSize)
    {
        ASSERT(log2PageSize < 64);
    }

    void dump(WTF::PrintStream&) const;

    // A page count may be larger than any byte count can express, so saturate rather than wrap: every
    // byte-based bound (what can be allocated, what a buffer may advertise) must keep rejecting it.
    uint64_t bytes() const
    {
        if (m_pageCount > std::numeric_limits<uint64_t>::max() >> m_log2PageSize)
            return std::numeric_limits<uint64_t>::max();
        return m_pageCount << m_log2PageSize;
    }
    uint64_t pageCount() const { return m_pageCount; }
    uint8_t log2PageSize() const { return m_log2PageSize; }

    // The largest count expressible in this unit, i.e. the number of pages spanning an i64 memory's
    // entire address space. Page counts are declarative: a memory may declare a maximum this process
    // cannot map, and must still parse and instantiate at its initial size.
    static constexpr uint64_t maxPageCount(uint8_t log2PageSize)
    {
        // 2**(64 - log2PageSize), except that one-byte pages would need 2**64, which does not fit and
        // whose top value is the invalid sentinel besides.
        if (!log2PageSize)
            return std::numeric_limits<uint64_t>::max() - 1;
        return 1ull << (64 - log2PageSize);
    }

    static bool isValid(uint64_t pageCount, uint8_t log2PageSize = defaultLog2PageSize)
    {
        return pageCount <= maxPageCount(log2PageSize);
    }

    bool isValid() const
    {
        return *this && isValid(m_pageCount, m_log2PageSize);
    }

    static PageCount fromBytes(uint64_t bytes, uint8_t log2PageSize = defaultLog2PageSize)
    {
        PageCount count = fromBytesUnchecked(bytes, log2PageSize);
        RELEASE_ASSERT(count.isValid());
        return count;
    }

    static PageCount fromBytesUnchecked(uint64_t bytes, uint8_t log2PageSize = defaultLog2PageSize)
    {
        RELEASE_ASSERT(!(bytes & ((1ull << log2PageSize) - 1)));
        return PageCount(bytes >> log2PageSize, log2PageSize);
    }

    static PageCount fromBytesWithRoundUp(uint64_t bytes, uint8_t log2PageSize = defaultLog2PageSize)
    {
        const uint64_t mask = (1ull << log2PageSize) - 1;
        RELEASE_ASSERT(bytes <= std::numeric_limits<uint64_t>::max() - mask);
        return fromBytes((bytes + mask) & ~mask, log2PageSize);
    }

    explicit operator bool() const
    {
        return m_pageCount != invalidPageCount;
    }

    friend bool operator==(const PageCount& lhs, const PageCount& rhs)
    {
        assertComparable(lhs, rhs);
        return lhs.m_pageCount == rhs.m_pageCount;
    }

    friend std::strong_ordering operator<=>(const PageCount& lhs, const PageCount& rhs)
    {
        assertComparable(lhs, rhs);
        return lhs.m_pageCount <=> rhs.m_pageCount;
    }

    PageCount operator+(const PageCount& other) const
    {
        assertComparable(*this, other);
        if (sumOverflows<uint64_t>(m_pageCount, other.m_pageCount))
            return PageCount();
        uint64_t newCount = m_pageCount + other.m_pageCount;
        if (!PageCount::isValid(newCount, m_log2PageSize))
            return PageCount();
        return PageCount(newCount, m_log2PageSize);
    }

    static constexpr uint32_t maxMemory32PageCount = 64 * 1024;
    static constexpr uint64_t maxMemory32Bytes = static_cast<uint64_t>(maxMemory32PageCount) * pageSize;

private:
    static constexpr uint64_t invalidPageCount = std::numeric_limits<uint64_t>::max();

    // An invalid count is the "no maximum" sentinel and carries no unit, so it compares against
    // anything; two real counts must agree on what they are counting.
    static void assertComparable(const PageCount& lhs, const PageCount& rhs)
    {
        RELEASE_ASSERT(!lhs || !rhs || lhs.m_log2PageSize == rhs.m_log2PageSize);
    }

    uint64_t m_pageCount { invalidPageCount };
    uint8_t m_log2PageSize { defaultLog2PageSize };
};

// fromBytes() must accept the byte length of any array buffer.
static_assert(MAX_ARRAY_BUFFER_SIZE / PageCount::pageSize <= PageCount::maxPageCount(PageCount::defaultLog2PageSize));

} // namespace JSC
