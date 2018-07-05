/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "RenderObject.h"

namespace WebCore {
class RenderBlockFlow;

namespace SimpleLineLayout {

class FlowContents {
public:
    FlowContents(const RenderBlockFlow&);

    struct Segment {
        unsigned toSegmentPosition(unsigned position) const
        {
            ASSERT(position >= start);
            return position - start;
        }
        unsigned toRenderPosition(unsigned position) const { return start + position; }
        unsigned start;
        unsigned end;
        StringView text;
        const RenderObject& renderer;
        bool canUseSimplifiedTextMeasuring;
    };
    const Segment& segmentForRun(unsigned start, unsigned end) const;
    const Segment& segmentForPosition(unsigned) const;

    typedef Vector<Segment, 8>::const_iterator Iterator;
    Iterator begin() const { return m_segments.begin(); }
    Iterator end() const { return m_segments.end(); }

private:
    unsigned segmentIndexForRunSlow(unsigned start, unsigned end) const;
    const Vector<Segment> m_segments;
    mutable unsigned m_lastSegmentIndex { 0 };
};

inline const FlowContents::Segment& FlowContents::segmentForRun(unsigned start, unsigned end) const
{
    ASSERT(start <= end);
    auto isEmptyRange = start == end;
    auto& lastSegment = m_segments[m_lastSegmentIndex];
    if ((lastSegment.start <= start && end <= lastSegment.end) && (!isEmptyRange || end != lastSegment.end))
        return m_segments[m_lastSegmentIndex];
    return m_segments[segmentIndexForRunSlow(start, end)];
}

inline const FlowContents::Segment& FlowContents::segmentForPosition(unsigned position) const
{
    auto it = std::lower_bound(m_segments.begin(), m_segments.end(), position, [](const Segment& segment, unsigned position) {
        return segment.end <= position;
    });
    ASSERT(it != m_segments.end());
    return m_segments[it - m_segments.begin()];
}

}
}
