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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveKeywordList.h"
#include "Length.h"
#include "LengthFunctions.h"
#include "StylePrimitiveNumericTypes+Platform.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

template<typename Numeric, CSS::PrimitiveKeyword... Ks> struct LengthWrapperBase;

// Transitionary type acting as a `Style::PrimitiveNumericOrKeyword<...>` but implemented by wrapping a `WebCore::Length`.
template<typename Numeric, CSS::PrimitiveKeyword... Ks> struct LengthWrapperBase {
    using Base = LengthWrapperBase<Numeric, Ks...>;
    using Keywords = CSS::PrimitiveKeywordList<Ks...>;

    using Specified = Numeric;
    using Fixed = typename Specified::Dimension;
    using Percentage = typename Specified::Percentage;
    using Calc = typename Specified::Calc;

    static constexpr bool SupportsAuto = Keywords::isValidKeyword(CSS::Keyword::Auto { });
    static constexpr bool SupportsNormal = Keywords::isValidKeyword(CSS::Keyword::Normal { });
    static constexpr bool SupportsIntrinsic = Keywords::isValidKeyword(CSS::Keyword::Intrinsic { });
    static constexpr bool SupportsMinIntrinsic = Keywords::isValidKeyword(CSS::Keyword::MinIntrinsic { });
    static constexpr bool SupportsMinContent = Keywords::isValidKeyword(CSS::Keyword::MinContent { });
    static constexpr bool SupportsMaxContent = Keywords::isValidKeyword(CSS::Keyword::MaxContent { });
    static constexpr bool SupportsWebkitFillAvailable = Keywords::isValidKeyword(CSS::Keyword::WebkitFillAvailable { });
    static constexpr bool SupportsFitContent = Keywords::isValidKeyword(CSS::Keyword::FitContent { });
    static constexpr bool SupportsContent = Keywords::isValidKeyword(CSS::Keyword::Content { });
    static constexpr bool SupportsNone = Keywords::isValidKeyword(CSS::Keyword::None { });

    LengthWrapperBase(CSS::Keyword::Auto) requires (SupportsAuto) : m_value(WebCore::LengthType::Auto) { }
    LengthWrapperBase(CSS::Keyword::Normal) requires (SupportsNormal) : m_value(WebCore::LengthType::Normal) { }
    LengthWrapperBase(CSS::Keyword::Intrinsic) requires (SupportsIntrinsic) : m_value(WebCore::LengthType::Intrinsic) { }
    LengthWrapperBase(CSS::Keyword::MinIntrinsic) requires (SupportsMinIntrinsic) : m_value(WebCore::LengthType::MinIntrinsic) { }
    LengthWrapperBase(CSS::Keyword::MinContent) requires (SupportsMinContent) : m_value(WebCore::LengthType::MinContent) { }
    LengthWrapperBase(CSS::Keyword::MaxContent) requires (SupportsMaxContent) : m_value(WebCore::LengthType::MaxContent) { }
    LengthWrapperBase(CSS::Keyword::WebkitFillAvailable) requires (SupportsWebkitFillAvailable) : m_value(WebCore::LengthType::FillAvailable) { }
    LengthWrapperBase(CSS::Keyword::FitContent) requires (SupportsFitContent) : m_value(WebCore::LengthType::FitContent) { }
    LengthWrapperBase(CSS::Keyword::Content) requires (SupportsContent) : m_value(WebCore::LengthType::Content) { }
    LengthWrapperBase(CSS::Keyword::None) requires (SupportsNone) : m_value(WebCore::LengthType::Undefined) { }

    LengthWrapperBase(Fixed fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    LengthWrapperBase(Percentage percent) : m_value(percent.value, WebCore::LengthType::Percent) { }
    LengthWrapperBase(Specified&& specified) : m_value(toPlatform(WTFMove(specified))) { }
    LengthWrapperBase(const Specified& specified) : m_value(toPlatform(specified)) { }

    LengthWrapperBase(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Fixed) { }
    LengthWrapperBase(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Percent) { }

    explicit LengthWrapperBase(WebCore::Length&& other) : m_value(WTFMove(other)) { validate(m_value); }
    explicit LengthWrapperBase(const WebCore::Length& other) : m_value(other) { validate(m_value); }

    explicit LengthWrapperBase(WTF::HashTableEmptyValueType token) : m_value(token) { }

    // IPC Support
    explicit LengthWrapperBase(WebCore::Length::IPCData&& data) : m_value { WTFMove(data) } { validate(m_value); }
    WebCore::Length::IPCData ipcData() const { return m_value.ipcData(); }

    ALWAYS_INLINE bool isFixed() const { return m_value.type() == WebCore::LengthType::Fixed; }
    ALWAYS_INLINE bool isPercent() const { return m_value.type() == WebCore::LengthType::Percent; }
    ALWAYS_INLINE bool isCalculated() const { return m_value.type() == WebCore::LengthType::Calculated; }
    ALWAYS_INLINE bool isPercentOrCalculated() const { return isPercent() || isCalculated(); }
    ALWAYS_INLINE bool isSpecified() const { return isFixed() || isPercent() || isCalculated(); }

    ALWAYS_INLINE bool isAuto() const requires (SupportsAuto) { return m_value.type() == WebCore::LengthType::Auto; }
    ALWAYS_INLINE bool isNormal() const requires (SupportsNormal) { return m_value.type() == WebCore::LengthType::Normal; }
    ALWAYS_INLINE bool isIntrinsicKeyword() const requires (SupportsIntrinsic) { return m_value.type() == WebCore::LengthType::Intrinsic; }
    ALWAYS_INLINE bool isMinIntrinsic() const requires (SupportsMinIntrinsic) { return m_value.type() == WebCore::LengthType::MinIntrinsic; }
    ALWAYS_INLINE bool isMinContent() const requires (SupportsMinContent) { return m_value.type() == WebCore::LengthType::MinContent; }
    ALWAYS_INLINE bool isMaxContent() const requires (SupportsMaxContent) { return m_value.type() == WebCore::LengthType::MaxContent; }
    ALWAYS_INLINE bool isFillAvailable() const requires (SupportsWebkitFillAvailable) { return m_value.type() == WebCore::LengthType::FillAvailable; }
    ALWAYS_INLINE bool isFitContent() const requires (SupportsFitContent) { return m_value.type() == WebCore::LengthType::FitContent; }
    ALWAYS_INLINE bool isContent() const requires (SupportsContent) { return m_value.type() == WebCore::LengthType::Content; }
    ALWAYS_INLINE bool isNone() const requires (SupportsNone) { return m_value.type() == WebCore::LengthType::Undefined; }

    // FIXME: This is misleadingly named. One would expect this function checks `type == LengthType::Intrinsic` but instead it checks `type = LengthType::MinContent || type == LengthType::MaxContent || type == LengthType::FillAvailable || type == LengthType::FitContent`.

    static constexpr bool SupportsIsIntrinsic = SupportsMinContent || SupportsMaxContent || SupportsWebkitFillAvailable || SupportsFitContent;
    static constexpr bool SupportsIsLegacyIntrinsic = SupportsIntrinsic || SupportsMinIntrinsic;

    ALWAYS_INLINE bool isIntrinsic() const
        requires (SupportsIsIntrinsic)
    {
        return (SupportsMinContent && m_value.type() == WebCore::LengthType::MinContent)
            || (SupportsMaxContent && m_value.type() == WebCore::LengthType::MaxContent)
            || (SupportsWebkitFillAvailable && m_value.type() == WebCore::LengthType::FillAvailable)
            || (SupportsFitContent && m_value.type() == WebCore::LengthType::FitContent);
    }
    ALWAYS_INLINE bool isLegacyIntrinsic() const
        requires (SupportsIsLegacyIntrinsic)
    {
        return (SupportsIntrinsic && m_value.type() == WebCore::LengthType::Intrinsic)
            || (SupportsMinIntrinsic && m_value.type() == WebCore::LengthType::MinIntrinsic);
    }
    ALWAYS_INLINE bool isIntrinsicOrLegacyIntrinsicOrAuto() const
        requires (SupportsIsIntrinsic || SupportsIsLegacyIntrinsic || SupportsAuto)
    {
        return (SupportsMinContent && m_value.type() == WebCore::LengthType::MinContent)
            || (SupportsMinContent && m_value.type() == WebCore::LengthType::MaxContent)
            || (SupportsWebkitFillAvailable && m_value.type() == WebCore::LengthType::FillAvailable)
            || (SupportsFitContent && m_value.type() == WebCore::LengthType::FitContent)
            || (SupportsIntrinsic && m_value.type() == WebCore::LengthType::Intrinsic)
            || (SupportsMinIntrinsic && m_value.type() == WebCore::LengthType::MinIntrinsic)
            || (SupportsAuto && m_value.type() == WebCore::LengthType::Auto);
    }

    ALWAYS_INLINE bool isZero() const { return m_value.isZero(); }
    ALWAYS_INLINE bool isPositive() const { return m_value.isPositive(); }
    ALWAYS_INLINE bool isNegative() const { return m_value.isNegative(); }

    std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { m_value.value() }) : std::nullopt; }
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }
    std::optional<Calc> tryCalc() const { return isCalculated() ? std::make_optional(Calc { m_value.calculationValue() }) : std::nullopt; }

    template<typename T> bool holdsAlternative() const
    {
             if constexpr (std::same_as<T, Fixed>)                                                              return isFixed();
        else if constexpr (std::same_as<T, Percentage>)                                                         return isPercent();
        else if constexpr (std::same_as<T, Calc>)                                                               return isCalculated();
        else if constexpr (std::same_as<T, CSS::Keyword::Auto> && SupportsAuto)                                 return isAuto();
        else if constexpr (std::same_as<T, CSS::Keyword::Normal> && SupportsNormal)                             return isNormal();
        else if constexpr (std::same_as<T, CSS::Keyword::Intrinsic> && SupportsIntrinsic)                       return isIntrinsicKeyword();
        else if constexpr (std::same_as<T, CSS::Keyword::MinIntrinsic> && SupportsMinIntrinsic)                 return isMinIntrinsic();
        else if constexpr (std::same_as<T, CSS::Keyword::MinContent> && SupportsMinContent)                     return isMinContent();
        else if constexpr (std::same_as<T, CSS::Keyword::MaxContent> && SupportsMaxContent)                     return isMaxContent();
        else if constexpr (std::same_as<T, CSS::Keyword::WebkitFillAvailable> && SupportsWebkitFillAvailable)   return isFillAvailable();
        else if constexpr (std::same_as<T, CSS::Keyword::FitContent> && SupportsFitContent)                     return isFitContent();
        else if constexpr (std::same_as<T, CSS::Keyword::Content> && SupportsContent)                           return isContent();
        else if constexpr (std::same_as<T, CSS::Keyword::None> && SupportsNone)                                 return isNone();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_value.type()) {
        case WebCore::LengthType::Fixed:            return visitor(Fixed { m_value.value() });
        case WebCore::LengthType::Percent:          return visitor(Percentage { m_value.value() });
        case WebCore::LengthType::Calculated:       return visitor(Calc { m_value.calculationValue() });
        case WebCore::LengthType::Auto:             if constexpr (SupportsAuto) { return visitor(CSS::Keyword::Auto { }); } else { break; }
        case WebCore::LengthType::Intrinsic:        if constexpr (SupportsIntrinsic) { return visitor(CSS::Keyword::Intrinsic { }); } else { break; }
        case WebCore::LengthType::MinIntrinsic:     if constexpr (SupportsMinIntrinsic) { return visitor(CSS::Keyword::MinIntrinsic { }); } else { break; }
        case WebCore::LengthType::MinContent:       if constexpr (SupportsMinContent) { return visitor(CSS::Keyword::MinContent { }); } else { break; }
        case WebCore::LengthType::MaxContent:       if constexpr (SupportsMaxContent) { return visitor(CSS::Keyword::MaxContent { }); } else { break; }
        case WebCore::LengthType::FillAvailable:    if constexpr (SupportsWebkitFillAvailable) { return visitor(CSS::Keyword::WebkitFillAvailable { }); } else { break; }
        case WebCore::LengthType::FitContent:       if constexpr (SupportsFitContent) { return visitor(CSS::Keyword::FitContent { }); } else { break; }
        case WebCore::LengthType::Content:          if constexpr (SupportsContent) { return visitor(CSS::Keyword::Content { }); } else { break; }
        case WebCore::LengthType::Normal:           if constexpr (SupportsNormal) { return visitor(CSS::Keyword::Normal { }); } else { break; }
        case WebCore::LengthType::Undefined:        if constexpr (SupportsNone) { return visitor(CSS::Keyword::None { }); } else { break; }
        case WebCore::LengthType::Relative:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool hasSameType(const LengthWrapperBase& other) const { return m_value.type() == other.m_value.type(); }

    bool operator==(const LengthWrapperBase&) const = default;

protected:
    template<typename> friend struct ToPlatform;

    static void validate(const WebCore::Length& length)
    {
        switch (length.type()) {
        case WebCore::LengthType::Fixed:            RELEASE_ASSERT(CSS::isWithinRange<Fixed::range>(length.value())); return;
        case WebCore::LengthType::Percent:          RELEASE_ASSERT(CSS::isWithinRange<Percentage::range>(length.value())); return;
        case WebCore::LengthType::Calculated:       return;
        case WebCore::LengthType::Auto:             if constexpr (SupportsAuto) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::Intrinsic:        if constexpr (SupportsIntrinsic) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::MinIntrinsic:     if constexpr (SupportsMinIntrinsic) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::MinContent:       if constexpr (SupportsMinContent) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::MaxContent:       if constexpr (SupportsMaxContent) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::FillAvailable:    if constexpr (SupportsWebkitFillAvailable) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::FitContent:       if constexpr (SupportsFitContent) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::Content:          if constexpr (SupportsContent) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::Normal:           if constexpr (SupportsNormal) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::Undefined:        if constexpr (SupportsNone) { return; } else { RELEASE_ASSERT_NOT_REACHED(); }
        case WebCore::LengthType::Relative:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    WebCore::Length m_value;
};

// MARK: - Concepts

template<typename T> concept LengthWrapperBaseDerived = WTF::IsBaseOfTemplate<LengthWrapperBase, T>::value && VariantLike<T>;

// MARK: - Platform

template<LengthWrapperBaseDerived T> struct ToPlatform<T> {
    auto operator()(const T& value) -> WebCore::Length
    {
        return value.m_value;
    }
};

// MARK: - Evaluation

template<LengthWrapperBaseDerived T> struct Evaluation<T> {
    auto operator()(const T& value, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor) -> LayoutUnit
    {
        return valueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(toPlatform(value), lazyMaximumValueFunctor);
    }
    auto operator()(const T& value, LayoutUnit referenceLength) -> LayoutUnit
    {
        return valueForLength(toPlatform(value), referenceLength);
    }
    auto operator()(const T& value, float referenceLength) -> float
    {
        return floatValueForLength(toPlatform(value), referenceLength);
    }
};

template<LengthWrapperBaseDerived T> inline LayoutUnit evaluateMinimum(const T& value, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor)
{
    return minimumValueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(toPlatform(value), lazyMaximumValueFunctor);
}

template<LengthWrapperBaseDerived T> inline LayoutUnit evaluateMinimum(const T& value, LayoutUnit maximumValue)
{
    return minimumValueForLength(toPlatform(value), maximumValue);
}

template<LengthWrapperBaseDerived T> inline float evaluateMinimum(const T& value, float maximumValue)
{
    return minimumValueForLength(toPlatform(value), maximumValue);
}

// MARK: - Blending

template<LengthWrapperBaseDerived T> struct Blending<T> {
    auto canBlend(const T& a, const T& b) -> bool
    {
        return WebCore::canInterpolateLengths(toPlatform(a), toPlatform(b), true);
    }
    auto requiresInterpolationForAccumulativeIteration(const T& a, const T& b) -> bool
    {
        return WebCore::lengthsRequireInterpolationForAccumulativeIteration(toPlatform(a), toPlatform(b));
    }
    auto blend(const T& a, const T& b, const BlendingContext& context) -> T
    {
        return T { WebCore::blend(toPlatform(a), toPlatform(b), context, T::Fixed::range == CSS::Nonnegative ? ValueRange::NonNegative : ValueRange::All) };
    }
};

// MARK: - Logging

template<LengthWrapperBaseDerived T> WTF::TextStream& operator<<(WTF::TextStream& ts, const T& value)
{
    return ts << toPlatform(value);
}

} // namespace Style
} // namespace WebCore
