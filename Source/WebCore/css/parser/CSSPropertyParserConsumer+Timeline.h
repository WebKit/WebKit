/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSValue;
class Document;
enum CSSValueID : uint16_t;
struct CSSParserContext;

namespace CSS {
struct PropertyParserState;
}

namespace Style {
enum class SingleAnimationRangeName : uint8_t;
enum class SingleAnimationRangeType : bool;
struct SingleAnimationRangeEnd;
struct SingleAnimationRangeStart;
struct ViewTimelineInsetItem;
}

namespace CSSPropertyParserHelpers {

bool NODELETE isTimelineRangeName(CSSValueID);
std::optional<Style::SingleAnimationRangeName> parseTimelineRangeNameRaw(const String&);
std::optional<Style::SingleAnimationRangeName> parseTimelineRangeNameOrNormalRaw(const String&);

// MARK: - Consumer functions

// <scroll()> = scroll( [ <scroller> || <axis> ]? )
// https://drafts.csswg.org/scroll-animations-1/#scroll-notation
RefPtr<CSSValue> consumeAnimationTimelineScroll(CSSParserTokenRange&, CSS::PropertyParserState&);

// <view()> = view( [ <axis> || <'view-timeline-inset'> ]? )
// https://drafts.csswg.org/scroll-animations-1/#view-notation
RefPtr<CSSValue> consumeAnimationTimelineView(CSSParserTokenRange&, CSS::PropertyParserState&);

// <single-view-timeline-inset-item> = <single-view-timeline-inset>{1,2}
// https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset
RefPtr<CSSValue> consumeSingleViewTimelineInsetItem(CSSParserTokenRange&, CSS::PropertyParserState&);

std::optional<Style::ViewTimelineInsetItem> parseAbsoluteSingleViewTimelineInsetItemRaw(const String&, const CSSParserContext&, const Document&);

// <single-animation-range> = normal | <length-percentage> | <timeline-range-name> <length-percentage>?
// https://drafts.csswg.org/scroll-animations-1/#propdef-animation-range-start
RefPtr<CSSValue> consumeSingleAnimationRange(CSSParserTokenRange&, CSS::PropertyParserState&, Style::SingleAnimationRangeType);
RefPtr<CSSValue> consumeSingleAnimationRangeStart(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeSingleAnimationRangeEnd(CSSParserTokenRange&, CSS::PropertyParserState&);

std::optional<Style::SingleAnimationRangeStart> parseAbsoluteSingleAnimationRangeStartRaw(const String&, const CSSParserContext&, const Document&);
std::optional<Style::SingleAnimationRangeEnd> parseAbsoluteSingleAnimationRangeEndRaw(const String&, const CSSParserContext&, const Document&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
