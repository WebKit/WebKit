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
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSScrollValue.h"
#include "CSSValuePair.h"
#include "CSSViewValue.h"
#include "StyleSingleAnimationRange.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

bool isAnimationRangeKeyword(CSSValueID id)
{
    return identMatches<CSSValueNormal, CSSValueCover, CSSValueContain, CSSValueEntry, CSSValueExit, CSSValueEntryCrossing, CSSValueExitCrossing, CSSValueScroll>(id);
}

RefPtr<CSSValue> consumeAnimationTimelineScroll(CSSParserTokenRange& range, CSS::PropertyParserState&)
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

    return CSSScrollValue::create(WTF::move(scroller), WTF::move(axis));
}

RefPtr<CSSValue> consumeAnimationTimelineView(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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
    auto startInset = CSSPropertyParsing::consumeSingleViewTimelineInset(args, state);
    auto endInset = CSSPropertyParsing::consumeSingleViewTimelineInset(args, state);

    // Try <axis> again since the order of <axis> and <'view-timeline-inset'> is not guaranteed.
    if (!axis)
        axis = CSSPropertyParsing::consumeAxis(args);

    // If there are values left to consume, these are not valid <axis> or <'view-timeline-inset'> and the function is invalid.
    if (args.size())
        return nullptr;

    return CSSViewValue::create(WTF::move(axis), WTF::move(startInset), WTF::move(endInset));
}

RefPtr<CSSValue> consumeSingleViewTimelineInsetItem(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <single-view-timeline-inset-item-item> = <single-view-timeline-inset>{1,2}
    // https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset

    auto startInset = CSSPropertyParsing::consumeSingleViewTimelineInset(range, state);
    if (!startInset)
        return nullptr;

    if (auto endInset = CSSPropertyParsing::consumeSingleViewTimelineInset(range, state)) {
        if (endInset != startInset)
            return CSSValuePair::createNoncoalescing(startInset.releaseNonNull(), endInset.releaseNonNull());
    }

    return startInset;
}

RefPtr<CSSValue> parseSingleViewTimelineInsetItem(const String& string, const CSSParserContext& context)
{
    auto tokenizer = CSSTokenizer(string);
    auto range = tokenizer.tokenRange();

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = context };
    auto result = consumeSingleViewTimelineInsetItem(range, state);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

RefPtr<CSSValue> consumeSingleAnimationRange(CSSParserTokenRange& range, CSS::PropertyParserState& state, Style::SingleAnimationRangeType type)
{
    // <'animation-range-{start|end}'> = normal | <length-percentage> | <timeline-range-name> <length-percentage>?
    // https://drafts.csswg.org/scroll-animations-1/#propdef-animation-range-start

    auto isDefault = [&](auto& value) {
        if (!value.isPercentage() || value.isCalculated())
            return false;
        auto percentageValue = value.resolveAsPercentageNoConversionDataRequired();
        if (type == Style::SingleAnimationRangeType::Start)
            return percentageValue == 0;
        return percentageValue == 100;
    };

    if (auto name = consumeIdent(range)) {
        if (name->valueID() == CSSValueNormal)
            return name;
        if (!isAnimationRangeKeyword(name->valueID()))
            return nullptr;
        if (auto offset = CSSPrimitiveValueResolver<CSS::LengthPercentage<>>::consumeAndResolve(range, state)) {
            if (isDefault(*offset))
                return name;
            return CSSValuePair::createNoncoalescing(name.releaseNonNull(), offset.releaseNonNull());
        }
        return name;
    }
    return CSSPrimitiveValueResolver<CSS::LengthPercentage<>>::consumeAndResolve(range, state);
}

RefPtr<CSSValue> consumeSingleAnimationRangeStart(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumeSingleAnimationRange(range, state, Style::SingleAnimationRangeType::Start);
}

RefPtr<CSSValue> consumeSingleAnimationRangeEnd(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    return consumeSingleAnimationRange(range, state, Style::SingleAnimationRangeType::End);
}

RefPtr<CSSValue> parseSingleAnimationRange(const String& string, const CSSParserContext& context, Style::SingleAnimationRangeType type)
{
    auto tokenizer = CSSTokenizer(string);
    auto range = tokenizer.tokenRange();

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = context };
    auto result = consumeSingleAnimationRange(range, state, type);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
