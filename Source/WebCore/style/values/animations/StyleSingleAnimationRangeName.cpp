/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleSingleAnimationRangeName.h"

#include "CSSValueKeywords.h"
#include <wtf/text/WTFString.h>

namespace WebCore {
namespace Style {

SingleAnimationRangeName convertCSSValueIDToSingleAnimationRangeName(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueNormal:
        return SingleAnimationRangeName::Normal;
    case CSSValueCover:
        return SingleAnimationRangeName::Cover;
    case CSSValueContain:
        return SingleAnimationRangeName::Contain;
    case CSSValueEntry:
        return SingleAnimationRangeName::Entry;
    case CSSValueExit:
        return SingleAnimationRangeName::Exit;
    case CSSValueEntryCrossing:
        return SingleAnimationRangeName::EntryCrossing;
    case CSSValueExitCrossing:
        return SingleAnimationRangeName::ExitCrossing;
    case CSSValueScroll:
        return SingleAnimationRangeName::Scroll;
    default:
        ASSERT_NOT_REACHED();
        return SingleAnimationRangeName::Normal;
    }
}

CSSValueID convertSingleAnimationRangeNameToCSSValueID(SingleAnimationRangeName range)
{
    switch (range) {
    case SingleAnimationRangeName::Normal:
        return CSSValueNormal;
    case SingleAnimationRangeName::Cover:
        return CSSValueCover;
    case SingleAnimationRangeName::Contain:
        return CSSValueContain;
    case SingleAnimationRangeName::Entry:
        return CSSValueEntry;
    case SingleAnimationRangeName::Exit:
        return CSSValueExit;
    case SingleAnimationRangeName::EntryCrossing:
        return CSSValueEntryCrossing;
    case SingleAnimationRangeName::ExitCrossing:
        return CSSValueExitCrossing;
    case SingleAnimationRangeName::Scroll:
        return CSSValueScroll;
    case SingleAnimationRangeName::Omitted:
        return CSSValueInvalid;
    }
    ASSERT_NOT_REACHED();
    return CSSValueNormal;
}

String convertSingleAnimationRangeNameToRangeString(SingleAnimationRangeName rangeName)
{
    switch (rangeName) {
    case SingleAnimationRangeName::Normal:
        return "normal"_s;
    case SingleAnimationRangeName::Omitted:
        return "omitted"_s;
    case SingleAnimationRangeName::Cover:
        return "cover"_s;
    case SingleAnimationRangeName::Contain:
        return "contain"_s;
    case SingleAnimationRangeName::Entry:
        return "entry"_s;
    case SingleAnimationRangeName::Exit:
        return "exit"_s;
    case SingleAnimationRangeName::EntryCrossing:
        return "entry-crossing"_s;
    case SingleAnimationRangeName::ExitCrossing:
        return "exit-crossing"_s;
    case SingleAnimationRangeName::Scroll:
        return "scroll"_s;
    }
    ASSERT_NOT_REACHED();
    return "normal"_s;
}


SingleAnimationRangeName convertRangeStringToSingleTimelineRangeName(const String& rangeString)
{
    if (rangeString == "cover"_s)
        return Style::SingleAnimationRangeName::Cover;
    if (rangeString == "contain"_s)
        return Style::SingleAnimationRangeName::Contain;
    if (rangeString == "entry"_s)
        return Style::SingleAnimationRangeName::Entry;
    if (rangeString == "exit"_s)
        return Style::SingleAnimationRangeName::Exit;
    if (rangeString == "entry-crossing"_s)
        return Style::SingleAnimationRangeName::EntryCrossing;
    if (rangeString == "exit-crossing"_s)
        return Style::SingleAnimationRangeName::ExitCrossing;
    if (rangeString == "scroll"_s)
        return Style::SingleAnimationRangeName::Scroll;
    return Style::SingleAnimationRangeName::Normal;
}

} // namespace Style
} // namespace WebCore
