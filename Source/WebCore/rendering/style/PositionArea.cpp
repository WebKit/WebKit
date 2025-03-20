/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "PositionArea.h"

#include <wtf/EnumTraits.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

PositionAreaSpan::PositionAreaSpan(PositionAreaAxis axis, PositionAreaTrack track, PositionAreaSelf self)
    : m_axis(enumToUnderlyingType(axis))
    , m_track(enumToUnderlyingType(track))
    , m_self(enumToUnderlyingType(self))
{
}

static bool UNUSED_FUNCTION axisIsBlockOrX(PositionAreaAxis axis)
{
    switch (axis) {
    case PositionAreaAxis::Horizontal:
    case PositionAreaAxis::X:
    case PositionAreaAxis::Block:
        return true;

    default:
        return false;
    }
}

static bool UNUSED_FUNCTION axisIsInlineOrY(PositionAreaAxis axis)
{
    switch (axis) {
    case PositionAreaAxis::Vertical:
    case PositionAreaAxis::Y:
    case PositionAreaAxis::Inline:
        return true;

    default:
        return false;
    }
}

PositionArea::PositionArea(PositionAreaSpan blockOrXAxis, PositionAreaSpan inlineOrYAxis)
    : m_blockOrXAxis(blockOrXAxis)
    , m_inlineOrYAxis(inlineOrYAxis)
{
    ASSERT(axisIsBlockOrX(m_blockOrXAxis.axis()));
    ASSERT(axisIsInlineOrY(m_inlineOrYAxis.axis()));
}

PositionAreaSpan PositionArea::spanForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    bool useSelfWritingMode = m_blockOrXAxis.self() == PositionAreaSelf::Yes;
    auto writingMode = useSelfWritingMode ? selfWritingMode : containerWritingMode;
    return physicalAxis == mapPositionAreaAxisToPhysicalAxis(m_blockOrXAxis.axis(), writingMode)
        ? m_blockOrXAxis : m_inlineOrYAxis;
}

PositionAreaSpan PositionArea::spanForAxis(LogicalBoxAxis logicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    bool useSelfWritingMode = m_blockOrXAxis.self() == PositionAreaSelf::Yes;
    auto writingMode = useSelfWritingMode ? selfWritingMode : containerWritingMode;
    return logicalAxis == mapPositionAreaAxisToLogicalAxis(m_blockOrXAxis.axis(), writingMode)
        ? m_blockOrXAxis : m_inlineOrYAxis;
}

PositionAreaTrack PositionArea::coordMatchedTrackForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    auto relevantSpan = spanForAxis(physicalAxis, containerWritingMode, selfWritingMode);
    auto positionAxis = relevantSpan.axis();
    auto track = relevantSpan.track();

    bool shouldFlip = false;
    if (LogicalBoxAxis::Inline == mapAxisPhysicalToLogical(containerWritingMode, physicalAxis)) {
        if (isPositionAreaDirectionLogical(positionAxis)) {
            shouldFlip = containerWritingMode.isInlineFlipped();
            if (relevantSpan.self() == PositionAreaSelf::Yes
                && !containerWritingMode.isInlineMatchingAny(selfWritingMode))
                shouldFlip = !shouldFlip;
        }
    } else {
        shouldFlip = !isPositionAreaDirectionLogical(positionAxis)
            && containerWritingMode.isBlockFlipped();
        if (relevantSpan.self() == PositionAreaSelf::Yes
            && !containerWritingMode.isBlockMatchingAny(selfWritingMode))
            shouldFlip = !shouldFlip;
    }

    return shouldFlip ? flipPositionAreaTrack(track) : track;
}

static ItemPosition flip(ItemPosition alignment)
{
    return ItemPosition::Start == alignment ? ItemPosition::End : ItemPosition::Start;
};

ItemPosition PositionArea::defaultAlignmentForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const
{
    auto relevantSpan = spanForAxis(physicalAxis, containerWritingMode, selfWritingMode);

    ItemPosition alignment;
    switch (relevantSpan.track()) {
    case PositionAreaTrack::Start:
    case PositionAreaTrack::SpanStart:
        alignment = ItemPosition::End;
        break;
    case PositionAreaTrack::End:
    case PositionAreaTrack::SpanEnd:
        alignment = ItemPosition::Start;
        break;
    case PositionAreaTrack::Center:
    case PositionAreaTrack::SpanAll:
        return ItemPosition::AnchorCenter;
    }

    // Remap for self alignment.
    auto axis = relevantSpan.axis();
    bool shouldFlip = false;
    if (relevantSpan.self() == PositionAreaSelf::Yes && containerWritingMode != selfWritingMode) {
        auto logicalAxis = mapPositionAreaAxisToLogicalAxis(axis, selfWritingMode);
        if (containerWritingMode.isOrthogonal(selfWritingMode)) {
            if (LogicalBoxAxis::Inline == logicalAxis)
                shouldFlip = !selfWritingMode.isInlineMatchingAny(containerWritingMode);
            else
                shouldFlip = !selfWritingMode.isBlockMatchingAny(containerWritingMode);
        } else if (LogicalBoxAxis::Inline == logicalAxis)
            shouldFlip = selfWritingMode.isInlineOpposing(containerWritingMode);
        else
            shouldFlip = selfWritingMode.isBlockOpposing(containerWritingMode);
    }

    if (isPositionAreaDirectionLogical(axis))
        return shouldFlip ? flip(alignment) : alignment;

    ASSERT(PositionAreaAxis::Horizontal == axis || PositionAreaAxis::Vertical == axis);

    if ((PositionAreaAxis::Horizontal == axis) == containerWritingMode.isHorizontal())
        return containerWritingMode.isInlineFlipped() ? flip(alignment) : alignment;
    return containerWritingMode.isBlockFlipped() ? flip(alignment) : alignment;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PositionAreaSpan& span)
{
    ts << "{ axis: "_s;
    switch (span.axis()) {
    case PositionAreaAxis::Horizontal: ts << "horizontal"_s; break;
    case PositionAreaAxis::Vertical:   ts << "vertical"_s; break;
    case PositionAreaAxis::X:          ts << 'x'; break;
    case PositionAreaAxis::Y:          ts << 'y'; break;
    case PositionAreaAxis::Block:      ts << "block"_s; break;
    case PositionAreaAxis::Inline:     ts << "inline"_s; break;
    }

    ts << ", track: "_s;
    switch (span.track()) {
    case PositionAreaTrack::Start:     ts << "start"_s; break;
    case PositionAreaTrack::SpanStart: ts << "span-start"_s; break;
    case PositionAreaTrack::End:       ts << "end"_s; break;
    case PositionAreaTrack::SpanEnd:   ts << "span-end"_s; break;
    case PositionAreaTrack::Center:    ts << "center"_s; break;
    case PositionAreaTrack::SpanAll:   ts << "span-all"_s; break;
    }

    ts << ", self: "_s;
    switch (span.self()) {
    case PositionAreaSelf::No:  ts << "no"_s; break;
    case PositionAreaSelf::Yes: ts << "yes"_s; break;
    }

    ts << " }"_s;

    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PositionArea& positionArea)
{
    ts << "{ span1: "_s << positionArea.blockOrXAxis() << ", span2: "_s << positionArea.inlineOrYAxis() << " }"_s;
    return ts;
}

} // namespace WebCore
