/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include <limits.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>

namespace WTF {

// Note that the 'begin' is inclusive, while the 'end' is exclusive. These two ranges are non-
// overlapping:
//
//     rangeA = 0...8
//     rangeB = 8...16

template<typename PassedType>
class Range {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef PassedType Type;
    
    Range()
        : m_begin(0)
        , m_end(0)
    {
    }

    explicit Range(Type value)
        : m_begin(value)
        , m_end(value + 1)
    {
        ASSERT(m_end >= m_begin);
    }

    Range(Type begin, Type end)
        : m_begin(begin)
        , m_end(end)
    {
        ASSERT(m_end >= m_begin);
        if (m_begin == m_end) {
            // Canonicalize empty ranges.
            m_begin = 0;
            m_end = 0;
        }
    }

    static Range top()
    {
        return Range(std::numeric_limits<Type>::min(), std::numeric_limits<Type>::max());
    }

    bool operator==(const Range& other) const
    {
        return m_begin == other.m_begin
            && m_end == other.m_end;
    }

    bool operator!=(const Range& other) const
    {
        return !(*this == other);
    }
    
    explicit operator bool() const { return m_begin != m_end; }

    Range operator|(const Range& other) const
    {
        if (!*this)
            return other;
        if (!other)
            return *this;
        return Range(
            std::min(m_begin, other.m_begin),
            std::max(m_end, other.m_end));
    }
    
    Range& operator|=(const Range& other)
    {
        return *this = *this | other;
    }
    
    Type begin() const { return m_begin; }
    Type end() const { return m_end; }

    bool overlaps(const Range& other) const
    {
        return WTF::rangesOverlap(m_begin, m_end, other.m_begin, other.m_end);
    }

    bool contains(Type point) const
    {
        return m_begin <= point && point < m_end;
    }

    void dump(PrintStream& out) const
    {
        if (*this == Range()) {
            out.print("Bottom");
            return;
        }
        if (*this == top()) {
            out.print("Top");
            return;
        }
        if (m_begin + 1 == m_end) {
            out.print(m_begin);
            return;
        }
        out.print(m_begin, "...", m_end);
    }

private:
    Type m_begin;
    Type m_end;
};

} // namespace WTF
