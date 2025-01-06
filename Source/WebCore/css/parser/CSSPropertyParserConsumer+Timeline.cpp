/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "CSSPropertyParserConsumer+Timeline.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParsing.h"
#include "CSSScrollValue.h"
#include "CSSValuePair.h"
#include "CSSViewValue.h"
#include "TimelineRange.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeAnimationTimelineScroll(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <scroll()> = scroll( [ <scroller> || <axis> ]? )
    // <scroller> = root | nearest | self
    // <axis> = block | inline | x | y
    // https://drafts.csswg.org/scroll-animations-1/#scroll-notation

    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueScroll)
        return nullptr;

    auto args = consumeFunction(range);

    if (!args.size())
        return CSSScrollValue::create(nullptr, nullptr);

    auto scroller = CSSPropertyParsing::consumeScroller(args);
    auto axis = CSSPropertyParsing::consumeAxis(args);

    // Try <scroller> again since the order of <scroller> and <axis> is not guaranteed.
    if (!scroller)
        scroller = CSSPropertyParsing::consumeScroller(args);

    // If there are values left to consume, these are not valid <scroller> or <axis> and the function is invalid.
    if (args.size())
        return nullptr;

    return CSSScrollValue::create(WTFMove(scroller), WTFMove(axis));
}

static RefPtr<CSSValue> consumeAnimationTimelineView(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <view()> = view( [ <axis> || <'view-timeline-inset'> ]? )
    // <axis> = block | inline | x | y
    // <'view-timeline-inset'> = [ [ auto | <length-percentage> ]{1,2} ]#
    // https://drafts.csswg.org/scroll-animations-1/#view-notation

    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueView)
        return nullptr;

    auto args = consumeFunction(range);

    if (!args.size())
        return CSSViewValue::create();

    auto axis = CSSPropertyParsing::consumeAxis(args);
    auto startInset = CSSPropertyParsing::consumeSingleViewTimelineInset(args, context);
    auto endInset = CSSPropertyParsing::consumeSingleViewTimelineInset(args, context);

    // Try <axis> again since the order of <axis> and <'view-timeline-inset'> is not guaranteed.
    if (!axis)
        axis = CSSPropertyParsing::consumeAxis(args);

    // If there are values left to consume, these are not valid <axis> or <'view-timeline-inset'> and the function is invalid.
    if (args.size())
        return nullptr;

    return CSSViewValue::create(WTFMove(axis), WTFMove(startInset), WTFMove(endInset));
}

static RefPtr<CSSValue> consumeSingleAnimationTimeline(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-animation-timeline> = auto | none | <dashed-ident> | <scroll()> | <view()>
    // https://drafts.csswg.org/css-animations-2/#typedef-single-animation-timeline

    auto id = range.peek().id();
    if (id == CSSValueAuto || id == CSSValueNone)
        return consumeIdent(range);
    if (auto name = consumeDashedIdent(range))
        return name;
    if (auto scroll = consumeAnimationTimelineScroll(range, context))
        return scroll;
    return consumeAnimationTimelineView(range, context);
}

RefPtr<CSSValue> consumeAnimationTimeline(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <animation-timeline> = <single-animation-timeline>#
    // https://drafts.csswg.org/css-animations-2/#animation-timeline

    return consumeCommaSeparatedListWithSingleValueOptimization(range, [context](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        return consumeSingleAnimationTimeline(range, context);
    });
}

RefPtr<CSSValue> consumeViewTimelineInsetListItem(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <view-timeline-inset-item> = <single-view-timeline-inset>{1,2}
    // <single-view-timeline-inset> = auto | <length-percentage>
    // https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset

    auto startInset = CSSPropertyParsing::consumeSingleViewTimelineInset(range, context);
    if (!startInset)
        return nullptr;

    if (auto endInset = CSSPropertyParsing::consumeSingleViewTimelineInset(range, context)) {
        if (endInset != startInset)
            return CSSValuePair::createNoncoalescing(startInset.releaseNonNull(), endInset.releaseNonNull());
    }

    return startInset;
}

RefPtr<CSSValue> consumeViewTimelineInset(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <view-timeline-inset> = <view-timeline-inset-item>#
    // https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset

    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [context](CSSParserTokenRange& range) {
        return consumeViewTimelineInsetListItem(range, context);
    });
}

static bool isAnimationRangeKeyword(CSSValueID id)
{
    return identMatches<CSSValueNormal, CSSValueCover, CSSValueContain, CSSValueEntry, CSSValueExit, CSSValueEntryCrossing, CSSValueExitCrossing>(id);
}

RefPtr<CSSValue> consumeAnimationRange(CSSParserTokenRange& range, const CSSParserContext& context, SingleTimelineRange::Type type)
{
    // https://drafts.csswg.org/scroll-animations-1/#propdef-animation-range-start
    // normal | <length-percentage> | <timeline-range-name> <length-percentage>?
    // FIXME: Add extra handling for animation range sequences.

    if (auto name = consumeIdent(range)) {
        if (name->valueID() == CSSValueNormal)
            return name;
        if (!isAnimationRangeKeyword(name->valueID()))
            return nullptr;
        if (auto offset = consumeLengthPercentage(range, context, ValueRange::All, UnitlessQuirk::Forbid)) {
            if (SingleTimelineRange::isDefault(*offset, type))
                return name;
            return CSSValuePair::createNoncoalescing(name.releaseNonNull(), offset.releaseNonNull());
        }
        return name;
    }
    return consumeLengthPercentage(range, context, ValueRange::All, UnitlessQuirk::Forbid);
}

RefPtr<CSSValue> consumeAnimationRangeStart(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeCommaSeparatedListWithSingleValueOptimization(range, [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        return consumeAnimationRange(range, context, SingleTimelineRange::Type::Start);
    });
}

RefPtr<CSSValue> consumeAnimationRangeEnd(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeCommaSeparatedListWithSingleValueOptimization(range, [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        return consumeAnimationRange(range, context, SingleTimelineRange::Type::End);
    });
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
