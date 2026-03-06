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

#include "CSSNumericFactory.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StyleLengthWrapper+DeprecatedCSSValueConversion.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

SingleAnimationRangeLength SingleAnimationRangeLength::defaultValue(SingleAnimationRangeType type)
{
    return type == SingleAnimationRangeType::Start ? 0_css_percentage : 100_css_percentage;
}

bool SingleAnimationRangeLength::isDefault(SingleAnimationRangeType type) const
{
    return *this == defaultValue(type);
}

static RefPtr<CSSNumericValue> toCSSNumericValue(const SingleAnimationRangeLength& offset)
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
    return {
        .start = { 0_css_percentage },
        .end = { 100_css_percentage },
    };
}

SingleAnimationRange SingleAnimationRange::defaultForViewTimeline()
{
    return {
        .start = { CSS::Keyword::Cover { }, 0_css_percentage },
        .end = { CSS::Keyword::Cover { }, 100_css_percentage },
    };
}

// MARK: - Conversion

template<typename Edge> static Edge convertSingleAnimationRangeEdge(BuilderState& state, const CSSValue& value)
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
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

        return toStyleFromCSSValue<SingleAnimationRangeLength>(state, *primitiveValue);
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return CSS::Keyword::Normal { };

    auto offset = toStyleFromCSSValue<SingleAnimationRangeLength>(state, pair->second.get());

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

auto CSSValueConversion<SingleAnimationRangeStart>::operator()(BuilderState& state, const CSSValue& value) -> SingleAnimationRangeStart
{
    return convertSingleAnimationRangeEdge<SingleAnimationRangeStart>(state, value);
}

auto CSSValueConversion<SingleAnimationRangeEnd>::operator()(BuilderState& state, const CSSValue& value) -> SingleAnimationRangeEnd
{
    return convertSingleAnimationRangeEdge<SingleAnimationRangeEnd>(state, value);
}

// MARK: - Deprecated Conversions

template<typename Edge> static std::optional<Edge> deprecatedConvertSingleAnimationRangeEdge(const RefPtr<Element>& element, const CSSValue& value)
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
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

        auto offset = deprecatedToStyleFromCSSValue<SingleAnimationRangeLength>(element, *primitiveValue);
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

    auto offset = deprecatedToStyleFromCSSValue<SingleAnimationRangeLength>(element, *primitiveValue);
    if (!offset)
        return { };

    switch (pair->first().valueID()) {
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

auto DeprecatedCSSValueConversion<SingleAnimationRangeStart>::operator()(const RefPtr<Element>& element, const CSSValue& value) -> std::optional<SingleAnimationRangeStart>
{
    return deprecatedConvertSingleAnimationRangeEdge<SingleAnimationRangeStart>(element, value);
}

auto DeprecatedCSSValueConversion<SingleAnimationRangeEnd>::operator()(const RefPtr<Element>& element, const CSSValue& value) -> std::optional<SingleAnimationRangeEnd>
{
    return deprecatedConvertSingleAnimationRangeEdge<SingleAnimationRangeEnd>(element, value);
}

} // namespace Style
} // namespace WebCore
