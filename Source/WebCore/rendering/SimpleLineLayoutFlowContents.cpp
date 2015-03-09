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
#include "RenderText.h"

namespace WebCore {
namespace SimpleLineLayout {

static Vector<FlowContents::Segment> initializeSegments(const RenderBlockFlow& flow)
{
    Vector<FlowContents::Segment, 8> segments;
    unsigned startPosition = 0;
    for (auto& textChild : childrenOfType<RenderText>(flow)) {
        unsigned textLength = textChild.text()->length();
        segments.append(FlowContents::Segment { startPosition, startPosition + textLength, textChild.text(), textChild });
        startPosition += textLength;
    }
    return segments;
}

FlowContents::FlowContents(const RenderBlockFlow& flow)
    : m_segments(initializeSegments(flow))
    , m_lastSegmentIndex(0)
{
}

unsigned FlowContents::segmentIndexForPositionSlow(unsigned position) const
{
    auto it = std::lower_bound(m_segments.begin(), m_segments.end(), position, [](const Segment& segment, unsigned position) {
        return segment.end <= position;
    });
    ASSERT(it != m_segments.end());
    auto index = it - m_segments.begin();
    m_lastSegmentIndex = index;
    return index;
}

const FlowContents::Segment& FlowContents::segmentForRenderer(const RenderObject& renderer) const
{
    for (auto& segment : m_segments) {
        if (&segment.renderer == &renderer)
            return segment;
    }
    ASSERT_NOT_REACHED();
    return m_segments.last();
}

}
}
