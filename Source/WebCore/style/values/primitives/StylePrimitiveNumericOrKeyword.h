/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {
namespace Style {

// `PrimitiveNumericOrKeyword` provides an efficient representation of `Variant<N, Ks...>`.
// It should be preferred over `ValueOrKeyword<>` when the value is a numeric type.

// FIXME: Add support for non-LengthPercentage<> and non-float ResolveValueType numeric types.

template<Numeric N, CSS::SpecificKeyword... Ks>
struct PrimitiveNumericOrKeyword;

template<auto R, CSS::SpecificKeyword... Ks>
struct PrimitiveNumericOrKeyword<LengthPercentage<R, float>, Ks...> {
public:
    using Base = PrimitiveNumericOrKeyword<LengthPercentage<R, float>, Ks...>;

    using Numeric = LengthPercentage<R, float>;
    using Keywords = WebCore::CSS::KeywordList<Ks...>;

    using Calc = typename Numeric::Calc;
    using Dimension = typename Numeric::Dimension;
    using Percentage = typename Numeric::Percentage;

    template<typename U>
        requires std::same_as<std::remove_cvref_t<U>, Numeric>
    PrimitiveNumericOrKeyword(U&& value) : m_value { std::forward<U>(value) }
    {
    }

    template<WebCore::CSS::ValidKeywordForList<Keywords> Keyword>
    PrimitiveNumericOrKeyword(Keyword)
        : m_value(indexForType<Keyword>())
    {
    }

    PrimitiveNumericOrKeyword(Dimension dimension)
        : m_value(indexForType<Dimension>(), dimension.unresolvedValue())
    {
    }

    PrimitiveNumericOrKeyword(Dimension dimension, bool hasQuirk)
        : m_value(indexForType<Dimension>(), dimension.unresolvedValue(), hasQuirk)
    {
    }

    PrimitiveNumericOrKeyword(Percentage percentage)
        : m_value(indexForType<Percentage>(), percentage.unresolvedValue())
    {
    }

    PrimitiveNumericOrKeyword(Calc&& calc)
        : m_value(indexForType<Calc>(), WTF::move(calc))
    {
    }

    PrimitiveNumericOrKeyword(Numeric&& numeric)
        : m_value(toRepresentation(WTF::move(numeric)))
    {
    }

    PrimitiveNumericOrKeyword(const Numeric& numeric)
        : m_value(toRepresentation(numeric))
    {
    }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValueLiteral<WebCore::CSS::LengthUnit::Px> literal)
        : PrimitiveNumericOrKeyword(Dimension { literal })
    {
    }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValueLiteral<WebCore::CSS::PercentageUnit::Percentage> literal) : PrimitiveNumericOrKeyword(Percentage { literal })
    {
    }

    explicit PrimitiveNumericOrKeyword(WTF::HashTableEmptyValueType token) : m_value(token) { }

    bool hasQuirk() const { return m_value.hasQuirk(); }

    ALWAYS_INLINE bool isDimension() const { return holdsAlternative<Dimension>(); }
    ALWAYS_INLINE bool isPercentage() const { return holdsAlternative<Percentage>(); }
    ALWAYS_INLINE bool isCalc() const { return holdsAlternative<Calc>();}
    ALWAYS_INLINE bool isNumeric() const { return holdsAlternative<Numeric>(); }

    // `isKnownZero` returns whether the value can be guaranteed to be `0`. Keywords and calc() return `false`.
    ALWAYS_INLINE bool isKnownZero() const requires (Numeric::range.min <= 0 && Numeric::range.max >= 0) { return m_value.isKnownZero(evaluationKind()); }
    // `isKnownPositive` returns whether the value can be guaranteed to be more than `0`. Keywords and calc() return `false`.
    ALWAYS_INLINE bool isKnownPositive() const requires (Numeric::range.max > 0) { return m_value.isKnownPositive(evaluationKind()); }
    // `isKnownNegative` returns whether the value can be guaranteed to be less than `0`. Keywords and calc() return `false`.
    ALWAYS_INLINE bool isKnownNegative() const requires (Numeric::range.min < 0) { return m_value.isKnownNegative(evaluationKind()); }

    // `isPossiblyZero` returns whether the value can possibly be `0`. Keywords and calc() return `true`.
    ALWAYS_INLINE bool isPossiblyZero() const requires (Numeric::range.min <= 0 && Numeric::range.max >= 0) { return m_value.isPossiblyZero(evaluationKind()); }
    // `isPossiblyPositive` returns whether the value can possibly be more than `0`. Keywords and calc() return `true.
    ALWAYS_INLINE bool isPossiblyPositive() const requires (Numeric::range.max > 0) { return m_value.isPossiblyPositive(evaluationKind()); }
    // `isPossiblyNegative` returns whether the value can possibly be less than `0`. Keywords and calc() return `true.
    ALWAYS_INLINE bool isPossiblyNegative() const requires (Numeric::range.min < 0) { return m_value.isPossiblyNegative(evaluationKind()); }

    std::optional<Dimension> tryDimension() const { return isDimension() ? std::make_optional(Dimension { m_value.value() }) : std::nullopt; }
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }
    std::optional<Calc> tryCalc() const { return isCalculated() ? std::make_optional(Calc { m_value.calculationValue() }) : std::nullopt; }

    std::optional<Numeric> tryNumeric() const
    {
        // Due to following static assertion, the underlying Representation can be
        // directly copied for conversion to Numeric.
        static_assert(hasSameIndicesAsNumeric());

        if (holdsAlternative<Numeric>())
            return Numeric(m_value);
        return { };
    }

    template<typename T> bool holdsAlternative() const
    {
        if constexpr (std::same_as<T, Numeric>)
            return m_value.type() == indexForType<Dimension>() || m_value.type() == indexForType<Percentage>() || m_value.type() == indexForType<Calc>();
        else
            return m_value.type() == indexForType<T>();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        auto opaqueType = m_value.type();

        if constexpr (hasKeywords) {
             if (isKeyword(opaqueType))
                return Keywords::visitKeywordAtOffset(toKeywordListOffset(opaqueType), visitor);
        }

        if (opaqueType == indexForType<Dimension>())
            return visitor(Dimension { m_value.value() });
        else if (opaqueType == indexForType<Percentage>())
            return visitor(Percentage { m_value.value() });
        else if (opaqueType == indexForType<Calc>())
            SUPPRESS_FORWARD_DECL_ARG return visitor(Calc { m_value.calculationValue() });

        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename... F> decltype(auto) switchOnUsingNumeric(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        auto opaqueType = m_value.type();

        if constexpr (hasKeywords) {
             if (isKeyword(opaqueType))
                return Keywords::visitKeywordAtOffset(toKeywordListOffset(opaqueType), visitor);
        }

        // Due to following static assertion, the underlying Representation can be
        // directly copied for conversion to Numeric.
        static_assert(hasSameIndicesAsNumeric());
        return visitor(Numeric { m_value });
    }

    template<typename T> T get() const
    {
        RELEASE_ASSERT(holdsAlternative<T>());

        if constexpr (WebCore::CSS::ValidKeywordForList<T, Keywords>)
            return T { };
        else if constexpr (std::same_as<T, Dimension>)
            return T { m_value.value() };
        else if constexpr (std::same_as<T, Percentage>)
            return T { m_value.value() };
        else if constexpr (std::same_as<T, Calc>)
            SUPPRESS_FORWARD_DECL_ARG return T { m_value.calculationValue() };
        else if constexpr (std::same_as<T, Numeric>) {
            // Due to following static assertion, the underlying Representation can be directly copied for conversion to Numeric.
            static_assert(hasSameIndicesAsNumeric());
            return T { m_value };
        }
    }

    template<typename T> std::optional<T> tryGet() const
    {
        if (!holdsAlternative<T>())
            return std::nullopt;

        if constexpr (WebCore::CSS::ValidKeywordForList<T, Keywords>)
            return std::optional<T>(std::in_place);
        else if constexpr (std::same_as<T, Dimension>)
            return std::optional<T>(std::in_place, m_value.value());
        else if constexpr (std::same_as<T, Percentage>)
            return std::optional<T>(std::in_place, m_value.value());
        else if constexpr (std::same_as<T, Calc>)
            SUPPRESS_FORWARD_DECL_ARG return std::optional<T>(std::in_place, m_value.calculationValue());
        else if constexpr (std::same_as<T, Numeric>) {
            // Due to following static assertion, the underlying Representation can be directly copied for conversion to Numeric.
            static_assert(hasSameIndicesAsNumeric());
            return std::optional<T>(std::in_place, m_value);
        }
    }

    bool hasSameType(const PrimitiveNumericOrKeyword& other) const
    {
        return m_value.type() == other.m_value.type();
    }

    bool operator==(const PrimitiveNumericOrKeyword&) const = default;

    // Legacy names.
    using Fixed = Dimension;
    using Specified = Numeric;
    ALWAYS_INLINE bool isFixed() const { return holdsAlternative<Dimension>(); }
    ALWAYS_INLINE bool isPercent() const { return holdsAlternative<Percentage>(); }
    ALWAYS_INLINE bool isCalculated() const { return holdsAlternative<Calc>();}
    ALWAYS_INLINE bool isPercentOrCalculated() const { return holdsAlternative<Percentage>() || holdsAlternative<Calc>(); }
    ALWAYS_INLINE bool isSpecified() const { return holdsAlternative<Numeric>(); }
    std::optional<Dimension> tryFixed() const { return tryDimension(); }
    std::optional<Numeric> trySpecified() const { return tryNumeric(); }

private:
    template<typename> friend struct Blending;
    template<typename, typename> friend struct Evaluation;
    template<typename, typename> friend struct EvaluationMinimum;

    using Representation = PrimitiveData;

    static constexpr bool hasKeywords = Keywords::count > 0;

    static constexpr uint8_t indexForDimension          = 0;
    static constexpr uint8_t indexForPercentage         = 1;
    static constexpr uint8_t indexForCalc               = 2;
    static constexpr uint8_t indexForFirstKeyword       = hasKeywords ? 3 : 0;
    static constexpr uint8_t indexForLastKeyword        = hasKeywords ? indexForFirstKeyword + Keywords::count - 1 : 0;

    static constexpr uint8_t maxIndex                   = hasKeywords ? indexForLastKeyword : indexForCalc;

    static consteval bool hasSameIndicesAsNumeric()
    {
        return indexForDimension == Numeric::indexForDimension
            && indexForPercentage == Numeric::indexForPercentage
            && indexForCalc == Numeric::indexForCalc;
    }

    static constexpr bool isKeyword(uint8_t opaqueType) requires (hasKeywords)
    {
        return opaqueType >= indexForFirstKeyword;
    }
    static constexpr uint8_t toKeywordListOffset(uint8_t keywordIndex) requires (hasKeywords)
    {
        return keywordIndex - indexForFirstKeyword;
    }
    static constexpr uint8_t toKeywordIndex(uint8_t keywordListOffset) requires (hasKeywords)
    {
        return keywordListOffset + indexForFirstKeyword;
    }

    template<typename T> static consteval uint8_t indexForType()
    {
        if constexpr (WebCore::CSS::ValidKeywordForList<T, Keywords>)
            return toKeywordIndex(Keywords::offsetForKeyword(T { }));
        else if constexpr (std::same_as<T, Dimension>)
            return indexForDimension;
        else if constexpr (std::same_as<T, Percentage>)
            return indexForPercentage;
        else if constexpr (std::same_as<T, Calc>)
            return indexForCalc;
    }

    static Representation toRepresentation(const Numeric& numeric)
    {
        // Due to following static assertion, the underlying Representation can be directly copied for conversion from Numeric.
        static_assert(hasSameIndicesAsNumeric());
        return Representation { numeric.m_value };
    }

    PrimitiveDataEvaluationKind evaluationKind() const
    {
        auto opaqueType = m_value.type();

        if constexpr (hasKeywords) {
             if (isKeyword(opaqueType))
                return PrimitiveDataEvaluationKind::Flag;
        }

        if (opaqueType == indexForType<Dimension>())
            return PrimitiveDataEvaluationKind::Fixed;
        else if (opaqueType == indexForType<Percentage>())
            return PrimitiveDataEvaluationKind::Percentage;
        else if (opaqueType == indexForType<Calc>())
            return PrimitiveDataEvaluationKind::Calculation;

        RELEASE_ASSERT_NOT_REACHED();
    }

    Representation m_value;
};

template<typename T> concept PrimitiveNumericOrKeywordDerived = WTF::IsBaseOfTemplate<PrimitiveNumericOrKeyword, T>::value && VariantLike<T>;

template<typename T> concept LengthPercentageOrKeywordDerived = PrimitiveNumericOrKeywordDerived<T> && T::Numeric::category == CSS::Category::LengthPercentage;

template<typename T> T get(PrimitiveNumericOrKeywordDerived auto const& value)
{
    return value.template get<T>();
}

} // namespace Style
} // namespace WebCore

template<typename N, typename... Ks> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::PrimitiveNumericOrKeyword<N, Ks...>> = true;
