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

#include "config.h"
#include "SimpleLineLayoutFlowContents.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderLineBreak.h"
#include "RenderText.h"

namespace WebCore {
namespace SimpleLineLayout {

static Vector<FlowContents::Segment> initializeSegments(const RenderBlockFlow& flow)
{
    unsigned numberOfChildren = 0;
    auto children = childrenOfType<RenderObject>(flow);
    for (auto it = children.begin(), end = children.end(); it != end; ++it)
        ++numberOfChildren;
    Vector<FlowContents::Segment> segments;
    segments.reserveCapacity(numberOfChildren);
    unsigned startPosition = 0;
    for (auto& child : childrenOfType<RenderObject>(flow)) {
        if (is<RenderText>(child)) {
            auto& textChild = downcast<RenderText>(child);
            unsigned textLength = textChild.text().length();
            segments.append(FlowContents::Segment { startPosition, startPosition + textLength, textChild.text(),
                textChild, textChild.canUseSimplifiedTextMeasuring() });
            startPosition += textLength;
            continue;
        }
        if (is<RenderLineBreak>(child)) {
            segments.append(FlowContents::Segment { startPosition, startPosition, String(), child, true });
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    return segments;
}

FlowContents::FlowContents(const RenderBlockFlow& flow)
    : m_segments(initializeSegments(flow))
{
}

unsigned FlowContents::segmentIndexForRunSlow(unsigned start, unsigned end) const
{
    auto isEmptyRange = start == end;
    auto it = std::lower_bound(m_segments.begin(), m_segments.end(), start, [isEmptyRange](const Segment& segment, unsigned start) {
        // FIXME: This always find the first empty run (.vs subsequent <br> elements)
        return (isEmptyRange && segment.start == segment.end) ? segment.start < start : segment.end <= start;
    });
    ASSERT(it != m_segments.end());
    ASSERT(end <= it->end);
    m_lastSegmentIndex = it - m_segments.begin();
    return m_lastSegmentIndex;
}

}
}
