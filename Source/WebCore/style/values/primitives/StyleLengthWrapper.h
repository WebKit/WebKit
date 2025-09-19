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

#include <WebCore/CSSPrimitiveKeywordList.h>
#include <WebCore/CalculationValue.h>
#include <WebCore/StyleLengthWrapperData.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

template<typename, CSS::PrimitiveKeyword...> struct LengthWrapperBase;
template<typename> struct MinimumEvaluation;

// Transitionary type acting as a `Style::PrimitiveNumericOrKeyword<...>` but implemented by wrapping a `LengthWrapperData`.
template<typename Numeric, CSS::PrimitiveKeyword... Ks> struct LengthWrapperBase {
    using Base = LengthWrapperBase<Numeric, Ks...>;
    using Keywords = CSS::PrimitiveKeywordList<Ks...>;

    static constexpr bool hasKeywords = Keywords::count > 0;

    static constexpr uint8_t indexForFirstKeyword       = 0;
    static constexpr uint8_t indexForLastKeyword        = hasKeywords ? Keywords::count - 1: 0;
    static constexpr uint8_t indexForFixed              = hasKeywords ? indexForLastKeyword + 1 : 0;
    static constexpr uint8_t indexForPercentage         = indexForFixed + 1;
    static constexpr uint8_t indexForCalc               = indexForFixed + 2;
    static constexpr uint8_t maxIndex                   = indexForCalc;

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

    LengthWrapperBase(CSS::ValidKeywordForList<Keywords> auto keyword) : m_value(Keywords::offsetForKeyword(keyword)) { }

    LengthWrapperBase(Fixed fixed) : m_value(indexForFixed, fixed.value) { }
    LengthWrapperBase(Fixed fixed, bool hasQuirk) : m_value(indexForFixed, fixed.value, hasQuirk) { }
    LengthWrapperBase(Percentage percent) : m_value(indexForPercentage, percent.value) { }
    LengthWrapperBase(Calc&& calc) : m_value(indexForCalc, calc.protectedCalculation()) { }
    LengthWrapperBase(Specified&& specified) : m_value(toData(specified)) { }
    LengthWrapperBase(const Specified& specified) : m_value(toData(specified)) { }

    LengthWrapperBase(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : LengthWrapperBase(Fixed { literal }) { }
    LengthWrapperBase(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : LengthWrapperBase(Percentage { literal }) { }

    explicit LengthWrapperBase(WebCore::Length&& other) : m_value(toData(other)) { }
    explicit LengthWrapperBase(const WebCore::Length& other) : m_value(toData(other)) { }

    explicit LengthWrapperBase(WTF::HashTableEmptyValueType token) : m_value(token) { }

    // IPC Support
    explicit LengthWrapperBase(LengthWrapperData::IPCData&& data) : m_value { toData(WTFMove(data)) } { }
    LengthWrapperData::IPCData ipcData() const { return m_value.ipcData(); }

    ALWAYS_INLINE bool isFixed() const { return holdsAlternative<Fixed>(); }
    ALWAYS_INLINE bool isPercent() const { return holdsAlternative<Percentage>(); }
    ALWAYS_INLINE bool isCalculated() const { return holdsAlternative<Calc>();}
    ALWAYS_INLINE bool isPercentOrCalculated() const { return isPercent() || isCalculated(); }
    ALWAYS_INLINE bool isSpecified() const { return isFixed() || isPercent() || isCalculated(); }

    ALWAYS_INLINE bool isZero() const { return m_value.isZero(); }
    ALWAYS_INLINE bool isPositive() const { return m_value.isPositive(); }
    ALWAYS_INLINE bool isNegative() const { return m_value.isNegative(); }

    std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { m_value.value() }) : std::nullopt; }
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }
    std::optional<Calc> tryCalc() const { return isCalculated() ? std::make_optional(Calc { m_value.calculationValue() }) : std::nullopt; }

    std::optional<Specified> trySpecified() const
    {
        auto opaqueType = m_value.type();

             if (opaqueType == indexForFixed)       return Specified(Fixed { m_value.value() });
        else if (opaqueType == indexForPercentage)  return Specified(Percentage { m_value.value() });
        else if (opaqueType == indexForCalc)        return Specified(Calc { m_value.calculationValue() });
        else                                        return { };
    }

    template<typename T> bool holdsAlternative() const
    {
             if constexpr (CSS::ValidKeywordForList<T, Keywords>)   return m_value.type() == Keywords::offsetForKeyword(T { });
        else if constexpr (std::same_as<T, Fixed>)                  return m_value.type() == indexForFixed;
        else if constexpr (std::same_as<T, Percentage>)             return m_value.type() == indexForPercentage;
        else if constexpr (std::same_as<T, Calc>)                   return m_value.type() == indexForCalc;
        else if constexpr (std::same_as<T, Specified>)              return m_value.type() == indexForFixed || m_value.type() == indexForPercentage || m_value.type() == indexForCalc;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        auto opaqueType = m_value.type();

        if constexpr (hasKeywords) {
             if (opaqueType <= indexForLastKeyword)
                return Keywords::visitKeywordAtOffset(opaqueType, visitor);
        }

             if (opaqueType == indexForFixed)       return visitor(Fixed { m_value.value() });
        else if (opaqueType == indexForPercentage)  return visitor(Percentage { m_value.value() });
        else if (opaqueType == indexForCalc)        return visitor(Calc { m_value.calculationValue() });

        RELEASE_ASSERT_NOT_REACHED();
    }

    bool hasQuirk() const { return m_value.hasQuirk(); }

    bool hasSameType(const LengthWrapperBase& other) const { return m_value.type() == other.m_value.type(); }

    bool operator==(const LengthWrapperBase&) const = default;

private:
    template<typename> friend struct ToPlatform;
    template<typename> friend struct Evaluation;
    template<typename> friend struct MinimumEvaluation;
    template<typename> friend struct Blending;

    static LengthWrapperData toData(const Specified& specified)
    {
        return WTF::switchOn(specified,
            [](const Fixed& fixed) {
                return LengthWrapperData { indexForFixed, fixed.value };
            },
            [](const Percentage& percentage) {
                return LengthWrapperData { indexForPercentage, percentage.value };
            },
            [](const Calc& calc) {
                return LengthWrapperData { indexForCalc, calc.protectedCalculation() };
            }
        );
    }

    static LengthWrapperData toData(LengthWrapperData::IPCData&& ipcData)
    {
        RELEASE_ASSERT(ipcData.opaqueType <= maxIndex);
        RELEASE_ASSERT(ipcData.opaqueType != indexForCalc);

        if (ipcData.opaqueType == indexForFixed) {
            RELEASE_ASSERT(CSS::isWithinRange<Fixed::range>(ipcData.value));
        }
        if (ipcData.opaqueType == indexForPercentage) {
            RELEASE_ASSERT(CSS::isWithinRange<Percentage::range>(ipcData.value));
        }

        return LengthWrapperData { WTFMove(ipcData) };
    }

    static LengthWrapperData toData(const WebCore::Length& length)
    {
        switch (length.type()) {
        case WebCore::LengthType::Fixed:
            return LengthWrapperData(indexForFixed, CSS::clampToRange<Fixed::range, float>(length.value()), length.hasQuirk());
        case WebCore::LengthType::Percent:
            return LengthWrapperData(indexForPercentage, CSS::clampToRange<Percentage::range, float>(length.value()));
        case WebCore::LengthType::Calculated:
            return LengthWrapperData(indexForCalc, LengthWrapperData::LengthCalculation { length });
        case WebCore::LengthType::Auto:
            if constexpr (SupportsAuto) {
                return { Keywords::offsetForKeyword(CSS::Keyword::Auto { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::Content:
            if constexpr (SupportsContent) {
                return { Keywords::offsetForKeyword(CSS::Keyword::Content { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::FillAvailable:
            if constexpr (SupportsWebkitFillAvailable) {
                return { Keywords::offsetForKeyword(CSS::Keyword::WebkitFillAvailable { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::FitContent:
            if constexpr (SupportsFitContent) {
                return { Keywords::offsetForKeyword(CSS::Keyword::FitContent { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::Intrinsic:
            if constexpr (SupportsIntrinsic) {
                return { Keywords::offsetForKeyword(CSS::Keyword::Intrinsic { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::MinIntrinsic:
            if constexpr (SupportsMinIntrinsic) {
                return { Keywords::offsetForKeyword(CSS::Keyword::MinIntrinsic { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::MinContent:
            if constexpr (SupportsMinContent) {
                return { Keywords::offsetForKeyword(CSS::Keyword::MinContent { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::MaxContent:
            if constexpr (SupportsMaxContent) {
                return { Keywords::offsetForKeyword(CSS::Keyword::MaxContent { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::Normal:
            if constexpr (SupportsNormal) {
                return { Keywords::offsetForKeyword(CSS::Keyword::Normal { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::Undefined:
            if constexpr (SupportsNone) {
                return { Keywords::offsetForKeyword(CSS::Keyword::None { }) };
            } else {
                RELEASE_ASSERT_NOT_REACHED();
            }
        case WebCore::LengthType::Relative:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    LengthWrapperDataEvaluationKind evaluationKind() const
    {
        auto opaqueType = m_value.type();

        if constexpr (hasKeywords) {
             if (opaqueType <= indexForLastKeyword)
                return LengthWrapperDataEvaluationKind::Flag;
        }

             if (opaqueType == indexForFixed)           return LengthWrapperDataEvaluationKind::Fixed;
        else if (opaqueType == indexForPercentage)      return LengthWrapperDataEvaluationKind::Percentage;
        else if (opaqueType == indexForCalc)            return LengthWrapperDataEvaluationKind::Calculation;

        RELEASE_ASSERT_NOT_REACHED();
    }

    LengthWrapperData m_value;
};

// MARK: - Concepts

template<typename T> concept LengthWrapperBaseDerived = WTF::IsBaseOfTemplate<LengthWrapperBase, T>::value && VariantLike<T>;

// MARK: - Evaluation

template<LengthWrapperBaseDerived T> struct Evaluation<T> {
    auto operator()(const T& value, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor, float zoom) -> LayoutUnit
    {
        return value.m_value.template valueForLengthWrapperDataWithLazyMaximum<LayoutUnit, LayoutUnit>(value.evaluationKind(), lazyMaximumValueFunctor, zoom);
    }
    auto operator()(const T& value, NOESCAPE const Invocable<float()> auto& lazyMaximumValueFunctor, float zoom) -> float
    {
        return value.m_value.template valueForLengthWrapperDataWithLazyMaximum<float, float>(value.evaluationKind(), lazyMaximumValueFunctor, zoom);
    }
    auto operator()(const T& value, LayoutUnit maximumValue, float zoom) -> LayoutUnit
    {
        return value.m_value.template valueForLengthWrapperDataWithLazyMaximum<LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return maximumValue; }, zoom);
    }
    auto operator()(const T& value, float maximumValue, float zoom) -> float
    {
        return value.m_value.template valueForLengthWrapperDataWithLazyMaximum<float, float>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return maximumValue; }, zoom);
    }
};

template<typename StyleType, typename Reference> decltype(auto) evaluateMinimum(const StyleType& value, NOESCAPE Reference&& reference, float zoom)
{
    return MinimumEvaluation<StyleType> { }(value, std::forward<Reference>(reference), zoom);
}

template<LengthWrapperBaseDerived T> struct MinimumEvaluation<T> {
    auto operator()(const T& value, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor, float zoom) -> LayoutUnit
    {
        return value.m_value.template minimumValueForLengthWrapperDataWithLazyMaximum<LayoutUnit, LayoutUnit>(value.evaluationKind(), lazyMaximumValueFunctor, zoom);
    }
    auto operator()(const T& value, LayoutUnit maximumValue, float zoom) -> LayoutUnit
    {
        return value.m_value.template minimumValueForLengthWrapperDataWithLazyMaximum<LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return maximumValue; }, zoom);
    }
    auto operator()(const T& value, float maximumValue, float zoom) -> float
    {
        return value.m_value.template minimumValueForLengthWrapperDataWithLazyMaximum<LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return LayoutUnit(maximumValue); }, zoom);
    }
};

// MARK: - Logging

template<LengthWrapperBaseDerived T> WTF::TextStream& operator<<(WTF::TextStream& ts, const T& value)
{
    WTF::switchOn(value, [&](const auto& alternative) { ts << alternative; });
    return ts;
}

} // namespace Style
} // namespace WebCore
