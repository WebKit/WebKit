/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>

#if ENABLE(JIT)

namespace JSC { namespace DOMJIT {

class HeapRange {
public:
    constexpr HeapRange()
        : m_begin(UINT16_MAX)
        , m_end(UINT16_MAX)
    {
    }

    HeapRange(uint16_t begin, uint16_t end)
        : m_begin(begin)
        , m_end(end)
    {
        ASSERT_WITH_MESSAGE(begin <= end, "begin <= end is the invariant of this HeapRange.");
    }

private:
    enum ConstExprTag { ConstExpr };
    constexpr HeapRange(ConstExprTag, uint16_t begin, uint16_t end)
        : m_begin(begin)
        , m_end(end)
    {
    }

    template<uint16_t begin, uint16_t end>
    static constexpr HeapRange fromConstant()
    {
        static_assert(begin <= end, "begin <= end is the invariant of this HeapRange.");
        return HeapRange(ConstExpr, begin, end);
    }

public:
    enum RawRepresentationTag { RawRepresentation };
    explicit constexpr HeapRange(RawRepresentationTag, uint32_t value)
        : m_raw(value)
    {
    }

    static HeapRange fromRaw(uint32_t value)
    {
        return HeapRange(RawRepresentation, value);
    }

    uint16_t begin() const { return m_begin; }
    uint16_t end() const { return m_end; }
    uint32_t rawRepresentation() { return m_raw; }

    explicit operator bool() const
    {
        return m_begin != m_end;
    }

    bool operator==(const HeapRange& other) const
    {
        return m_begin == other.m_begin && m_end == other.m_end;
    }

    static constexpr HeapRange top() { return fromConstant<0, UINT16_MAX>(); }
    static constexpr HeapRange none() { return fromConstant<UINT16_MAX, UINT16_MAX>(); } // Empty range.

    bool isStrictSubtypeOf(const HeapRange& other) const
    {
        if (!*this || !other)
            return false;
        if (*this == other)
            return false;
        return other.m_begin <= m_begin && m_end <= other.m_end;
    }

    bool isSubtypeOf(const HeapRange& other) const
    {
        if (!*this || !other)
            return false;
        if (*this == other)
            return true;
        return isStrictSubtypeOf(other);
    }

    bool overlaps(const HeapRange& other) const
    {
        return WTF::rangesOverlap(m_begin, m_end, other.m_begin, other.m_end);
    }

    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

private:
    union {
        struct {
            uint16_t m_begin;
            uint16_t m_end;
        };
        uint32_t m_raw;
    };
};

} }

#endif
