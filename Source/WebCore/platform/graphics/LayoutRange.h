/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutUnit.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// This is basically a 1-dimentional LayoutRect.
class LayoutRange {
public:
    LayoutRange() = default;
    LayoutRange(LayoutUnit location, LayoutUnit size)
        : m_location(location)
        , m_size(size)
    { }
    LayoutUnit min() const { return m_location; }
    LayoutUnit max() const { return m_location + m_size; }
    LayoutUnit size() const { return m_size; }

    inline void set(LayoutUnit location, LayoutUnit size);
    inline void reset(LayoutUnit size = 0_lu)
    {
        m_location = 0_lu;
        m_size = size;
    }

    void moveBy(LayoutUnit shift) { m_location += shift; }
    void moveTo(LayoutUnit target) { m_location = target; }

    inline void shiftMinEdgeBy(LayoutUnit shift);
    inline void shiftMaxEdgeBy(LayoutUnit shift);
    inline void shiftMinEdgeTo(LayoutUnit target);
    inline void shiftMaxEdgeTo(LayoutUnit target);

    inline void sizeFromMinEdge(LayoutUnit size = 0_lu);
    inline void sizeFromMaxEdge(LayoutUnit size = 0_lu);

    inline void floorMinEdgeTo(LayoutUnit target);
    inline void floorMaxEdgeTo(LayoutUnit target);
    inline void capMinEdgeTo(LayoutUnit target);
    inline void capMaxEdgeTo(LayoutUnit target);

    inline void floorSizeFromMinEdge(LayoutUnit size = 0_lu);
    inline void floorSizeFromMaxEdge(LayoutUnit size = 0_lu);

private:
    LayoutUnit m_location;
    LayoutUnit m_size;
};

// MARK: - Implementation

inline void LayoutRange::set(LayoutUnit location, LayoutUnit size)
{
    m_location = location;
    m_size = size;
}

inline void LayoutRange::shiftMinEdgeBy(LayoutUnit shift)
{
    m_location += shift;
    m_size -= shift;
}

inline void LayoutRange::shiftMaxEdgeBy(LayoutUnit shift)
{
    m_size += shift;
}

inline void LayoutRange::shiftMinEdgeTo(LayoutUnit target) { shiftMinEdgeBy(target - min()); }
inline void LayoutRange::shiftMaxEdgeTo(LayoutUnit target) { shiftMaxEdgeBy(target - max()); }

inline void LayoutRange::floorMinEdgeTo(LayoutUnit target)
{
    if (target > max())
        shiftMaxEdgeTo(target);
}

inline void LayoutRange::floorMaxEdgeTo(LayoutUnit target)
{
    if (target > max())
        shiftMaxEdgeTo(target);
}

inline void LayoutRange::capMinEdgeTo(LayoutUnit target)
{
    if (target < min())
        shiftMinEdgeTo(target);
}

inline void LayoutRange::capMaxEdgeTo(LayoutUnit target)
{
    if (target < max())
        shiftMaxEdgeTo(target);
}

inline void LayoutRange::sizeFromMinEdge(LayoutUnit size)
{
    m_size = size;
}

inline void LayoutRange::sizeFromMaxEdge(LayoutUnit size)
{
    m_location -= size - m_size;
    m_size = size;
}

inline void LayoutRange::floorSizeFromMinEdge(LayoutUnit size)
{
    m_size = std::max(m_size, size);
}

inline void LayoutRange::floorSizeFromMaxEdge(LayoutUnit size)
{
    if (size > m_size)
        sizeFromMaxEdge(size);
}

inline TextStream& operator<<(TextStream& stream, const LayoutRange& range)
{
    if (stream.hasFormattingFlag(TextStream::Formatting::LayoutUnitsAsIntegers))
        return stream << '[' << range.min().toInt() << ',' << range.max().toInt() << ']';

    return stream << '[' << range.min().toFloat() << ',' << range.max().toFloat() << ']';
}

} // namespace WebCore
