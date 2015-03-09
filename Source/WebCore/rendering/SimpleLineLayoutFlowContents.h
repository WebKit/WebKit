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

#ifndef SimpleLineLayoutFlowContents_h
#define SimpleLineLayoutFlowContents_h

#include "RenderText.h"

namespace WebCore {
class RenderBlockFlow;

namespace SimpleLineLayout {

class FlowContents {
public:
    FlowContents(const RenderBlockFlow&);

    struct Segment {
        unsigned start;
        unsigned end;
        String text;
        const RenderObject& renderer;
    };
    const Segment& segmentForPosition(unsigned) const;
    const Segment& segmentForRenderer(const RenderObject&) const;

    class Iterator {
    public:
        Iterator(const FlowContents& flowContents, unsigned segmentIndex)
            : m_flowContents(flowContents)
            , m_segmentIndex(segmentIndex)
        {
        }

        Iterator& operator++();
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
        const Segment& operator*() const { return m_flowContents.m_segments[m_segmentIndex]; }

    private:
        const FlowContents& m_flowContents;
        unsigned m_segmentIndex;
    };

    Iterator begin() const { return Iterator(*this, 0); }
    Iterator end() const { return Iterator(*this, m_segments.size()); }

    unsigned length() const { return m_segments.last().end; };

    unsigned segmentIndexForPosition(unsigned position) const;

private:
    unsigned segmentIndexForPositionSlow(unsigned position) const;

    const Vector<Segment, 8> m_segments;

    mutable unsigned m_lastSegmentIndex;
};

inline FlowContents::Iterator& FlowContents::Iterator::operator++()
{
    ++m_segmentIndex;
    return *this;
}

inline bool FlowContents::Iterator::operator==(const FlowContents::Iterator& other) const
{
    return m_segmentIndex == other.m_segmentIndex;
}

inline bool FlowContents::Iterator::operator!=(const FlowContents::Iterator& other) const
{
    return !(*this == other);
}

inline unsigned FlowContents::segmentIndexForPosition(unsigned position) const
{
    auto& lastSegment = m_segments[m_lastSegmentIndex];
    if (lastSegment.start <= position && position < lastSegment.end)
        return m_lastSegmentIndex;
    return segmentIndexForPositionSlow(position);
}

inline const FlowContents::Segment& FlowContents::segmentForPosition(unsigned position) const
{
    return m_segments[segmentIndexForPosition(position)];
}

}
}

#endif
