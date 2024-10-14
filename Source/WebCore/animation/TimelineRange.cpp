/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "TimelineRange.h"

namespace WebCore {

static bool percentIsDefault(float percent, SingleTimelineRange::Type type)
{
    return percent == (type == SingleTimelineRange::Type::Start ? 0.f : 100.f);
}

bool SingleTimelineRange::isDefault(const Length& offset, Type type)
{
    return offset.isAuto() || offset.isNormal() || (offset.isPercent() && percentIsDefault(offset.percent(), type));
}

bool SingleTimelineRange::isDefault(const CSSPrimitiveValue& value, Type type)
{
    return value.valueID() == CSSValueNormal || (value.isPercentage() && !value.isCalculated() && percentIsDefault(value.resolveAsPercentageNoConversionDataRequired(), type));
}

bool SingleTimelineRange::isOffsetValue(const CSSPrimitiveValue& value)
{
    return value.isLength() || value.isPercentage() || value.isCalculatedPercentageWithLength();
}

SingleTimelineRange::Name SingleTimelineRange::timelineName(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueNormal:
        return SingleTimelineRange::Name::Normal;
    case CSSValueCover:
        return SingleTimelineRange::Name::Cover;
    case CSSValueContain:
        return SingleTimelineRange::Name::Contain;
    case CSSValueEntry:
        return SingleTimelineRange::Name::Entry;
    case CSSValueExit:
        return SingleTimelineRange::Name::Exit;
    case CSSValueEntryCrossing:
        return SingleTimelineRange::Name::EntryCrossing;
    case CSSValueExitCrossing:
        return SingleTimelineRange::Name::ExitCrossing;
    default:
        ASSERT_NOT_REACHED();
        return SingleTimelineRange::Name::Normal;
    }
}

CSSValueID SingleTimelineRange::valueID(SingleTimelineRange::Name range)
{
    switch (range) {
    case SingleTimelineRange::Name::Normal:
        return CSSValueNormal;
    case SingleTimelineRange::Name::Cover:
        return CSSValueCover;
    case SingleTimelineRange::Name::Contain:
        return CSSValueContain;
    case SingleTimelineRange::Name::Entry:
        return CSSValueEntry;
    case SingleTimelineRange::Name::Exit:
        return CSSValueExit;
    case SingleTimelineRange::Name::EntryCrossing:
        return CSSValueEntryCrossing;
    case SingleTimelineRange::Name::ExitCrossing:
        return CSSValueExitCrossing;
    case SingleTimelineRange::Name::Omitted:
        return CSSValueInvalid;
    }
    ASSERT_NOT_REACHED();
    return CSSValueNormal;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const SingleTimelineRange& range)
{
    switch (range.name) {
    case SingleTimelineRange::Name::Normal: ts << "Normal "; break;
    case SingleTimelineRange::Name::Omitted: ts << "Omitted "; break;
    case SingleTimelineRange::Name::Cover: ts << "Cover "; break;
    case SingleTimelineRange::Name::Contain: ts << "Contain "; break;
    case SingleTimelineRange::Name::Entry: ts << "Entry "; break;
    case SingleTimelineRange::Name::Exit: ts << "Exit "; break;
    case SingleTimelineRange::Name::EntryCrossing: ts << "EntryCrossing "; break;
    case SingleTimelineRange::Name::ExitCrossing: ts << "ExitCrossing "; break;
    }
    ts << range.offset;
    return ts;
}

} // namespace WebCore
