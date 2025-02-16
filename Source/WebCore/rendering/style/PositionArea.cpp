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

WTF::TextStream& operator<<(WTF::TextStream& ts, const PositionAreaSpan& span)
{
    ts << "{ axis: ";
    switch (span.axis()) {
    case PositionAreaAxis::Horizontal: ts << "horizontal"; break;
    case PositionAreaAxis::Vertical:   ts << "vertical"; break;
    case PositionAreaAxis::X:          ts << "x"; break;
    case PositionAreaAxis::Y:          ts << "y"; break;
    case PositionAreaAxis::Block:      ts << "block"; break;
    case PositionAreaAxis::Inline:     ts << "inline"; break;
    }

    ts << ", track: ";
    switch (span.track()) {
    case PositionAreaTrack::Start:     ts << "start"; break;
    case PositionAreaTrack::SpanStart: ts << "span-start"; break;
    case PositionAreaTrack::End:       ts << "end"; break;
    case PositionAreaTrack::SpanEnd:   ts << "span-end"; break;
    case PositionAreaTrack::Center:    ts << "center"; break;
    case PositionAreaTrack::SpanAll:   ts << "span-all"; break;
    }

    ts << ", self: ";
    switch (span.self()) {
    case PositionAreaSelf::No:  ts << "no"; break;
    case PositionAreaSelf::Yes: ts << "yes"; break;
    }

    ts << " }";

    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PositionArea& positionArea)
{
    ts << "{ span1: " << positionArea.blockOrXAxis() << ", span2: " << positionArea.inlineOrYAxis() << " }";
    return ts;
}

} // namespace WebCore
