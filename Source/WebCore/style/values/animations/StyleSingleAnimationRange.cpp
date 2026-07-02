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
#include "StyleSingleAnimationRange.h"

#include "CSSKeywordValue.h"
#include "CSSNumericFactory.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+DeprecatedCSSValueConversion.h"

namespace WebCore {
namespace Style {

static RefPtr<CSSNumericValue> toCSSNumericValue(const LengthPercentage<>& offset)
{
    // FIXME: This will fail for calc().
    return offset.isPercentOrCalculated()
        ? CSSNumericFactory::percent(offset.tryPercentage()->value)
        : CSSNumericFactory::px(offset.tryFixed()->resolveZoom(Style::ZoomNeeded { }));
}

TimelineRangeValue SingleAnimationRangeStart::toTimelineRangeValue() const
{
    if (m_name == SingleAnimationRangeName::Normal)
        return convertSingleAnimationRangeNameToRangeString(m_name);

    return TimelineRangeOffset {
        convertSingleAnimationRangeNameToRangeString(m_name),
        toCSSNumericValue(m_offset),
    };
}

TimelineRangeValue SingleAnimationRangeEnd::toTimelineRangeValue() const
{
    if (m_name == SingleAnimationRangeName::Normal)
        return convertSingleAnimationRangeNameToRangeString(m_name);

    return TimelineRangeOffset {
        convertSingleAnimationRangeNameToRangeString(m_name),
        toCSSNumericValue(m_offset),
    };
}

SingleAnimationRange SingleAnimationRange::defaultForScrollTimeline()
{
    using namespace CSS::Literals;

    return {
        .start = { 0_css_percentage },
        .end = { 100_css_percentage },
    };
}

SingleAnimationRange SingleAnimationRange::defaultForViewTimeline()
{
    using namespace CSS::Literals;

    return {
        .start = { CSS::Keyword::Cover { }, 0_css_percentage },
        .end = { CSS::Keyword::Cover { }, 100_css_percentage },
    };
}

// MARK: - Conversion

template<typename Edge> static Edge convertSingleAnimationRangeEdge(BuilderState& state, const CSSValue& value)
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueCover:
            return CSS::Keyword::Cover { };
        case CSSValueContain:
            return CSS::Keyword::Contain { };
        case CSSValueEntry:
            return CSS::Keyword::Entry { };
        case CSSValueExit:
            return CSS::Keyword::Exit { };
        case CSSValueEntryCrossing:
            return CSS::Keyword::EntryCrossing { };
        case CSSValueExitCrossing:
            return CSS::Keyword::ExitCrossing { };
        case CSSValueScroll:
            return CSS::Keyword::Scroll { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return toStyleFromCSSValue<typename Edge::Offset>(state, *primitiveValue);

    auto pair = requiredPairDowncast<CSSKeywordValue, CSSPrimitiveValue>(state, value);
    if (!pair)
        return CSS::Keyword::Normal { };

    auto offset = toStyleFromCSSValue<typename Edge::Offset>(state, pair->second.get());

    switch (pair->first->valueID()) {
    case CSSValueCover:
        return { CSS::Keyword::Cover { }, WTF::move(offset) };
    case CSSValueContain:
        return { CSS::Keyword::Contain { }, WTF::move(offset) };
    case CSSValueEntry:
        return { CSS::Keyword::Entry { }, WTF::move(offset) };
    case CSSValueExit:
        return { CSS::Keyword::Exit { }, WTF::move(offset) };
    case CSSValueEntryCrossing:
        return { CSS::Keyword::EntryCrossing { }, WTF::move(offset) };
    case CSSValueExitCrossing:
        return { CSS::Keyword::ExitCrossing { }, WTF::move(offset) };
    case CSSValueScroll:
        return { CSS::Keyword::Scroll { }, WTF::move(offset) };
    default:
        break;
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Normal { };
}

template<typename Edge> static Edge convertSingleAnimationRangeEdge(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueCover:
            return CSS::Keyword::Cover { };
        case CSSValueContain:
            return CSS::Keyword::Contain { };
        case CSSValueEntry:
            return CSS::Keyword::Entry { };
        case CSSValueExit:
            return CSS::Keyword::Exit { };
        case CSSValueEntryCrossing:
            return CSS::Keyword::EntryCrossing { };
        case CSSValueExitCrossing:
            return CSS::Keyword::ExitCrossing { };
        case CSSValueScroll:
            return CSS::Keyword::Scroll { };
        default:
            break;
        }

        return CSS::Keyword::Normal { };
    }

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return toStyleFromCSSValue<typename Edge::Offset>(conversionData, *primitiveValue);

    RefPtr pair = dynamicDowncast<CSSValuePair>(value);
    if (!pair)
        return CSS::Keyword::Normal { };

    RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(pair->second());
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    auto offset = toStyleFromCSSValue<typename Edge::Offset>(conversionData, *primitiveValue);

    RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(pair->first());
    if (!keywordValue)
        return CSS::Keyword::Normal { };

    switch (keywordValue->valueID()) {
    case CSSValueCover:
        return { CSS::Keyword::Cover { }, WTF::move(offset) };
    case CSSValueContain:
        return { CSS::Keyword::Contain { }, WTF::move(offset) };
    case CSSValueEntry:
        return { CSS::Keyword::Entry { }, WTF::move(offset) };
    case CSSValueExit:
        return { CSS::Keyword::Exit { }, WTF::move(offset) };
    case CSSValueEntryCrossing:
        return { CSS::Keyword::EntryCrossing { }, WTF::move(offset) };
    case CSSValueExitCrossing:
        return { CSS::Keyword::ExitCrossing { }, WTF::move(offset) };
    case CSSValueScroll:
        return { CSS::Keyword::Scroll { }, WTF::move(offset) };
    default:
        break;
    }

    return CSS::Keyword::Normal { };
}

auto CSSValueConversion<SingleAnimationRangeStart>::operator()(BuilderState& state, const CSSValue& value) -> SingleAnimationRangeStart
{
    return convertSingleAnimationRangeEdge<SingleAnimationRangeStart>(state, value);
}

auto CSSValueConversion<SingleAnimationRangeStart>::operator()(const CSSToLengthConversionData& conversionData, const CSSValue& value) -> SingleAnimationRangeStart
{
    return convertSingleAnimationRangeEdge<SingleAnimationRangeStart>(conversionData, value);
}

auto CSSValueConversion<SingleAnimationRangeEnd>::operator()(BuilderState& state, const CSSValue& value) -> SingleAnimationRangeEnd
{
    return convertSingleAnimationRangeEdge<SingleAnimationRangeEnd>(state, value);
}

auto CSSValueConversion<SingleAnimationRangeEnd>::operator()(const CSSToLengthConversionData& conversionData, const CSSValue& value) -> SingleAnimationRangeEnd
{
    return convertSingleAnimationRangeEdge<SingleAnimationRangeEnd>(conversionData, value);
}

// MARK: - Deprecated Conversions

template<typename Edge> static std::optional<Edge> deprecatedConvertSingleAnimationRangeEdge(const CSSValue& value)
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueCover:
            return { CSS::Keyword::Cover { } };
        case CSSValueContain:
            return { CSS::Keyword::Contain { } };
        case CSSValueEntry:
            return { CSS::Keyword::Entry { } };
        case CSSValueExit:
            return { CSS::Keyword::Exit { } };
        case CSSValueEntryCrossing:
            return { CSS::Keyword::EntryCrossing { } };
        case CSSValueExitCrossing:
            return { CSS::Keyword::ExitCrossing { } };
        case CSSValueScroll:
            return { CSS::Keyword::Scroll { } };
        default:
            break;
        }

        return { };
    }

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        auto offset = deprecatedToStyleFromCSSValue<typename Edge::Offset>(*primitiveValue);
        if (!offset)
            return { };

        return { WTF::move(*offset) };
    }

    RefPtr pair = dynamicDowncast<CSSValuePair>(value);
    if (!pair)
        return { };

    RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(pair->second());
    if (!primitiveValue)
        return { };

    auto offset = deprecatedToStyleFromCSSValue<typename Edge::Offset>(*primitiveValue);
    if (!offset)
        return { };

    RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(pair->first());
    if (!keywordValue)
        return { };

    switch (keywordValue->valueID()) {
    case CSSValueCover:
        return { { CSS::Keyword::Cover { }, WTF::move(offset) } };
    case CSSValueContain:
        return { { CSS::Keyword::Contain { }, WTF::move(offset) } };
    case CSSValueEntry:
        return { { CSS::Keyword::Entry { }, WTF::move(offset) } };
    case CSSValueExit:
        return { { CSS::Keyword::Exit { }, WTF::move(offset) } };
    case CSSValueEntryCrossing:
        return { { CSS::Keyword::EntryCrossing { }, WTF::move(offset) } };
    case CSSValueExitCrossing:
        return { { CSS::Keyword::ExitCrossing { }, WTF::move(offset) } };
    case CSSValueScroll:
        return { { CSS::Keyword::Scroll { }, WTF::move(offset) } };
    default:
        break;
    }

    return { };
}

auto DeprecatedCSSValueConversion<SingleAnimationRangeStart>::operator()(const CSSValue& value) -> std::optional<SingleAnimationRangeStart>
{
    return deprecatedConvertSingleAnimationRangeEdge<SingleAnimationRangeStart>(value);
}

auto DeprecatedCSSValueConversion<SingleAnimationRangeEnd>::operator()(const CSSValue& value) -> std::optional<SingleAnimationRangeEnd>
{
    return deprecatedConvertSingleAnimationRangeEdge<SingleAnimationRangeEnd>(value);
}

} // namespace Style
} // namespace WebCore
