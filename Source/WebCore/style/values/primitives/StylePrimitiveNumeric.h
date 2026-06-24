/*
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

#pragma once

#include <WebCore/CSSKeyword.h>
#include <WebCore/CSSPrimitiveNumeric.h>
#include <WebCore/StylePrimitiveData.h>
#include <WebCore/StylePrimitiveNumeric+Forward.h>
#include <WebCore/StylePrimitiveNumericConcepts.h>
#include <WebCore/StyleUnevaluatedCalculation.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/StyleZoomPrimitives.h>
#include <algorithm>
#include <wtf/CompactVariant.h>
#include <wtf/Forward.h>

namespace WebCore {
namespace Style {

template<typename> struct DimensionPercentageMapping;
template<Numeric, WebCore::CSS::SpecificKeyword...> struct PrimitiveNumericOrKeyword;
template<typename, typename> struct EvaluationMinimum;

struct PrimitiveNumericEmptyToken { constexpr bool operator==(const PrimitiveNumericEmptyToken&) const = default; };

template<typename T> struct PrimitiveNumericMarkableTraits {
    static bool isEmptyValue(const T& value) { return value.isEmpty(); }
    static T emptyValue() { return T(PrimitiveNumericEmptyToken { }); }
};

// Default implementation of `PrimitiveNumeric` for non-composite numeric types.
template<CSS::Numeric CSSType> struct PrimitiveNumeric {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    static constexpr auto unit = UnitTraits::canonical;
    ResolvedValueType value { 0 };

    constexpr PrimitiveNumeric(ResolvedValueType value)
        : value { value }
    {
    }

    constexpr PrimitiveNumeric(WebCore::CSS::ValueLiteral<UnitTraits::canonical> value)
        : value { clampTo<ResolvedValueType>(value.value) }
    {
    }

    constexpr auto unresolvedValue() const { return value; }

    constexpr bool isZero() const requires (range.min <= 0 && range.max >= 0) { return !value; }
    constexpr bool isKnownZero() const requires (range.min <= 0 && range.max >= 0) { return isZero(); }
    constexpr bool isPositive() const requires (range.max > 0) { return value > 0; }
    constexpr bool isKnownPositive() const requires (range.max > 0) { return isPositive(); }
    constexpr bool isPositiveOrZero() const requires (range.max >= 0) { return value >= 0; }
    constexpr bool isNegative() const requires (range.min < 0) { return value < 0; }
    constexpr bool isKnownNegative() const requires (range.min < 0) { return isNegative(); }
    constexpr bool isNegativeOrZero() const requires (range.min <= 0) { return value <= 0; }

    constexpr bool operator==(const PrimitiveNumeric&) const = default;
    constexpr bool operator==(ResolvedValueType other) const { return value == other; }

    constexpr auto operator<=>(const PrimitiveNumeric&) const = default;

private:
    template<typename> friend struct PrimitiveNumericMarkableTraits;

    // Markable is supported for numeric values that have free bits. These currently include:
    //  - any floating point value (using NaN).
    //  - any numeric value where the minimum allowed value is greater than 0 (using 0).
    //  - any numeric value where the minimum allowed value is equal to 0 and the minimum representable value is not zero (using -1).
    static consteval ResolvedValueType emptyValue()
    {
        if constexpr (std::floating_point<ResolvedValueType>)
            return std::numeric_limits<ResolvedValueType>::quiet_NaN();
        else if constexpr (range.min > 0)
            return 0;
        else if constexpr (range.min == 0 && std::numeric_limits<ResolvedValueType>::min() != 0)
            return -1;
    }

    PrimitiveNumeric(PrimitiveNumericEmptyToken)
        : value { emptyValue() }
    {
    }

    bool isEmpty() const
    {
        if constexpr (std::floating_point<ResolvedValueType>)
            return std::isnan(value);
        else
            return value == emptyValue();
    }
};

// Specialization of `PrimitiveNumeric` for `CSS::Length` types.
template<CSS::Range R, typename V> struct PrimitiveNumeric<CSS::Length<R, V>> {
    using CSS = CSS::Length<R, V>;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    static constexpr auto unit = UnitTraits::canonical;

    constexpr PrimitiveNumeric(ResolvedValueType value)
        : value { value }
    {
    }

    constexpr PrimitiveNumeric(WebCore::CSS::ValueLiteral<UnitTraits::canonical> value)
        : value { clampTo<ResolvedValueType>(value.value) }
    {
    }

    constexpr auto resolveZoom(ZoomNeeded) const
        requires (range.zoomOptions == WebCore::CSS::RangeZoomOptions::Default)
    {
        return value;
    }

    constexpr auto resolveZoom(ZoomFactor zoom) const
        requires (range.zoomOptions == WebCore::CSS::RangeZoomOptions::Unzoomed)
    {
        return value * zoom.value;
    }

    constexpr auto unresolvedValue() const { return value; }

    constexpr bool isZero() const requires (range.min <= 0 && range.max >= 0) { return !value; }
    constexpr bool isKnownZero() const requires (range.min <= 0 && range.max >= 0) { return isZero(); }
    constexpr bool isPositive() const requires (range.max > 0) { return value > 0; }
    constexpr bool isKnownPositive() const requires (range.max > 0) { return isPositive(); }
    constexpr bool isPositiveOrZero() const requires (range.max >= 0) { return value >= 0; }
    constexpr bool isNegative() const requires (range.min < 0) { return value < 0; }
    constexpr bool isKnownNegative() const requires (range.min < 0) { return isNegative(); }
    constexpr bool isNegativeOrZero() const requires (range.min <= 0) { return value <= 0; }

    constexpr bool operator==(const PrimitiveNumeric&) const = default;

    constexpr auto operator<=>(const PrimitiveNumeric&) const = default;

private:
    template<typename> friend struct PrimitiveNumericMarkableTraits;

    // Markable is supported for numeric values that have free bits. These currently include:
    //  - any floating point value (using NaN).
    //  - any numeric value where the minimum allowed value is greater than 0 (using 0).
    //  - any numeric value where the minimum allowed value is equal to 0 and the minimum representable value is not zero (using -1).
    static consteval ResolvedValueType emptyValue()
    {
        if constexpr (std::floating_point<ResolvedValueType>)
            return std::numeric_limits<ResolvedValueType>::quiet_NaN();
        else if constexpr (range.min > 0)
            return 0;
        else if constexpr (range.min == 0 && std::numeric_limits<ResolvedValueType>::min() != 0)
            return -1;
    }

    PrimitiveNumeric(PrimitiveNumericEmptyToken)
        : value { emptyValue() }
    {
    }

    bool isEmpty() const
    {
        if constexpr (std::floating_point<ResolvedValueType>)
            return std::isnan(value);
        else
            return value == emptyValue();
    }

    ResolvedValueType value { 0 };
};

// Specialization of `PrimitiveNumeric` for composite dimension-percentage types.
template<CSS::DimensionPercentageNumeric CSSType> struct PrimitiveNumeric<CSSType> {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    // Composite types only currently support float as the `ResolvedValueType`, allowing unconditional use of `CompactVariant`.
    static_assert(std::same_as<ResolvedValueType, float>);

    using Dimension = typename DimensionPercentageMapping<CSS>::Dimension;
    using Percentage = typename DimensionPercentageMapping<CSS>::Percentage;
    using Calc = UnevaluatedCalculation<CSS>;
    using Representation = CompactVariant<Dimension, Percentage, Calc>;

    PrimitiveNumeric(Dimension dimension)
        : m_value { WTF::move(dimension) }
    {
    }

    PrimitiveNumeric(Percentage percentage)
        : m_value { WTF::move(percentage) }
    {
    }

    PrimitiveNumeric(Calc calc)
        : m_value { WTF::move(calc) }
    {
    }

    PrimitiveNumeric(WebCore::CSS::ValueLiteral<Dimension::UnitTraits::canonical> literal)
        : m_value { Dimension { literal } }
    {
    }

    PrimitiveNumeric(WebCore::CSS::ValueLiteral<Percentage::UnitTraits::canonical> literal)
        : m_value { Percentage { literal } }
    {
    }

    constexpr size_t index() const { return m_value.index(); }

    template<typename T> constexpr bool holdsAlternative() const { return WTF::holdsAlternative<T>(m_value); }
    template<size_t I> constexpr bool holdsAlternative() const { return WTF::holdsAlternative<I>(m_value); }

    template<typename T> T get() const
    {
        return WTF::switchOn(m_value,
            []<std::same_as<T> U>(const U& alternative) -> T { return alternative; },
            [](const auto&) -> T { RELEASE_ASSERT_NOT_REACHED(); }
        );
    }

    template<typename... F> decltype(auto) switchOn(F&&... functors) const
    {
        return WTF::switchOn(m_value, std::forward<F>(functors)...);
    }

    constexpr bool isKnownZero() const requires (range.min <= 0 && range.max >= 0)
    {
        return WTF::switchOn(m_value,
            []<HasIsZero T>(const T& alternative) { return alternative.isZero(); },
            [](const auto&) { return false; }
        );
    }

    bool operator==(const PrimitiveNumeric&) const = default;

private:
    Representation m_value;
};

// Specialization of `PrimitiveNumeric` for `CSS::LengthPercentage` types.
template<CSS::Range R, typename V> struct PrimitiveNumeric<CSS::LengthPercentage<R, V>> {
    using CSS = CSS::LengthPercentage<R, V>;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    // Composite types only currently support float as the `ResolvedValueType`, allowing unconditional use of `CompactVariant`.
    static_assert(std::same_as<ResolvedValueType, float>);

    using Dimension = typename DimensionPercentageMapping<CSS>::Dimension;
    using Percentage = typename DimensionPercentageMapping<CSS>::Percentage;
    using Calc = UnevaluatedCalculation<CSS>;

    PrimitiveNumeric(Dimension dimension)
        : m_value(indexForType<Dimension>(), dimension.unresolvedValue())
    {
    }

    PrimitiveNumeric(Dimension dimension, bool hasQuirk)
        : m_value(indexForType<Dimension>(), dimension.unresolvedValue(), hasQuirk)
    {
    }

    PrimitiveNumeric(Percentage percentage)
        : m_value(indexForType<Percentage>(), percentage.unresolvedValue())
    {
    }

    PrimitiveNumeric(const Calc& calc)
        : m_value(indexForType<Calc>(), calc)
    {
    }

    PrimitiveNumeric(Calc&& calc)
        : m_value(indexForType<Calc>(), WTF::move(calc))
    {
    }

    PrimitiveNumeric(WebCore::CSS::ValueLiteral<Dimension::UnitTraits::canonical> literal)
        : PrimitiveNumeric { Dimension { literal } }
    {
    }

    PrimitiveNumeric(WebCore::CSS::ValueLiteral<Percentage::UnitTraits::canonical> literal)
        : PrimitiveNumeric { Percentage { literal } }
    {
    }

    explicit PrimitiveNumeric(WTF::HashTableEmptyValueType token)
        : m_value(token)
    {
    }

    explicit PrimitiveNumeric(WTF::HashTableDeletedValueType token)
        : m_value(token)
    {
    }

    bool hasQuirk() const { return m_value.hasQuirk(); }

    ALWAYS_INLINE bool isDimension() const { return holdsAlternative<Dimension>(); }
    ALWAYS_INLINE bool isPercentage() const { return holdsAlternative<Percentage>(); }
    ALWAYS_INLINE bool isCalc() const { return holdsAlternative<Calc>();}
    ALWAYS_INLINE bool isPercentageOrCalc() const { return !holdsAlternative<Dimension>(); }

    std::optional<Dimension> tryDimension() const { return tryGet<Dimension>(); }
    std::optional<Percentage> tryPercentage() const { return tryGet<Percentage>(); }
    std::optional<Calc> tryCalc() const { return tryGet<Calc>(); }

    // `isKnownZero` returns whether the value can be guaranteed to be `0`. calc() returns `false`.
    ALWAYS_INLINE bool isKnownZero() const requires (range.min <= 0 && range.max >= 0) { return m_value.isKnownZero(evaluationKind()); }
    // `isKnownPositive` returns whether the value can be guaranteed to be more than `0`. calc() returns `false`.
    ALWAYS_INLINE bool isKnownPositive() const requires (range.max > 0) { return m_value.isKnownPositive(evaluationKind()); }
    // `isKnownNegative` returns whether the value can be guaranteed to be less than `0`. calc() returns `false`.
    ALWAYS_INLINE bool isKnownNegative() const requires (range.min < 0) { return m_value.isKnownNegative(evaluationKind()); }

    // `isPossiblyZero` returns whether the value can possibly be `0`. calc() returns `true`.
    ALWAYS_INLINE bool isPossiblyZero() const requires (range.min <= 0 && range.max >= 0) { return m_value.isPossiblyZero(evaluationKind()); }
    // `isPossiblyPositive` returns whether the value can possibly be more than `0`. calc() returns `true.
    ALWAYS_INLINE bool isPossiblyPositive() const requires (range.max > 0) { return m_value.isPossiblyPositive(evaluationKind()); }
    // `isPossiblyNegative` returns whether the value can possibly be less than `0`. calc() returns `true.
    ALWAYS_INLINE bool isPossiblyNegative() const requires (range.min < 0) { return m_value.isPossiblyNegative(evaluationKind()); }

    template<typename T> bool holdsAlternative() const
    {
        return m_value.type() == indexForType<T>();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        auto opaqueType = m_value.type();

        if (opaqueType == indexForType<Dimension>())
            return visitor(Dimension { m_value.value() });
        else if (opaqueType == indexForType<Percentage>())
            return visitor(Percentage { m_value.value() });
        else if (opaqueType == indexForType<Calc>())
            SUPPRESS_FORWARD_DECL_ARG return visitor(Calc { m_value.calculationValue() });

        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename T> T get() const
    {
        RELEASE_ASSERT(holdsAlternative<T>());

        if constexpr (std::same_as<T, Dimension>)
            return Dimension { m_value.value() };
        else if constexpr (std::same_as<T, Percentage>)
            return Percentage { m_value.value() };
        else if constexpr (std::same_as<T, Calc>)
            SUPPRESS_FORWARD_DECL_ARG return Calc { m_value.calculationValue() };
    }

    template<typename T> std::optional<T> tryGet() const
    {
        if (!holdsAlternative<T>())
            return std::nullopt;

        if constexpr (std::same_as<T, Dimension>)
            return std::optional<T>(std::in_place, m_value.value());
        else if constexpr (std::same_as<T, Percentage>)
            return std::optional<T>(std::in_place, m_value.value());
        else if constexpr (std::same_as<T, Calc>)
            SUPPRESS_FORWARD_DECL_ARG return std::optional<T>(std::in_place, m_value.calculationValue());
    }

    bool hasSameType(const PrimitiveNumeric& other) const
    {
        return m_value.type() == other.m_value.type();
    }

    bool operator==(const PrimitiveNumeric&) const = default;

    // Legacy name support
    using Fixed = Dimension;
    ALWAYS_INLINE bool isFixed() const { return holdsAlternative<Dimension>(); }
    ALWAYS_INLINE bool isPercent() const { return holdsAlternative<Percentage>(); }
    ALWAYS_INLINE bool isCalculated() const { return holdsAlternative<Calc>();}
    ALWAYS_INLINE bool isPercentOrCalculated() const { return !holdsAlternative<Dimension>(); }
    std::optional<Dimension> tryFixed() const { return tryGet<Dimension>(); }

private:
    template<Numeric, WebCore::CSS::SpecificKeyword...> friend struct PrimitiveNumericOrKeyword;
    template<typename> friend struct Blending;
    template<typename, typename> friend struct Evaluation;
    template<typename, typename> friend struct EvaluationMinimum;

    using Representation = PrimitiveData;

    static constexpr uint8_t indexForDimension          = 0;
    static constexpr uint8_t indexForPercentage         = 1;
    static constexpr uint8_t indexForCalc               = 2;
    static constexpr uint8_t maxIndex                   = indexForCalc;

    template<typename T> static consteval uint8_t indexForType()
    {
        if constexpr (std::same_as<T, Dimension>)
            return indexForDimension;
        else if constexpr (std::same_as<T, Percentage>)
            return indexForPercentage;
        else if constexpr (std::same_as<T, Calc>)
            return indexForCalc;
    }

    explicit PrimitiveNumeric(Representation&& representation)
        : m_value(WTF::move(representation))
    {
    }

    explicit PrimitiveNumeric(const Representation& representation)
        : m_value(representation)
    {
    }

    PrimitiveDataEvaluationKind evaluationKind() const
    {
        auto opaqueType = m_value.type();

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

// MARK: Integer Primitive

template<CSS::Range R, typename V> struct Integer : PrimitiveNumeric<CSS::Integer<R, V>> {
    using Base = PrimitiveNumeric<CSS::Integer<R, V>>;
    using Base::Base;

    // Allow <integer> values to be initialized with number literals as well as integer literals.
    constexpr Integer(WebCore::CSS::ValueLiteral<WebCore::CSS::NumberUnit::Number> value)
        : Integer { clampTo<typename Base::ResolvedValueType>(value.value) }
    {
    }
};

// MARK: Number Primitive

template<CSS::Range R, typename V> struct Number : PrimitiveNumeric<CSS::Number<R, V>> {
    using Base = PrimitiveNumeric<CSS::Number<R, V>>;
    using Base::Base;

    // Allow <number> values to be initialized with integer literals as well as number literals.
    constexpr Number(WebCore::CSS::ValueLiteral<WebCore::CSS::IntegerUnit::Integer> value)
        : Number { clampTo<typename Base::ResolvedValueType>(value.value) }
    {
    }
};

// MARK: Percentage Primitive

template<CSS::Range R, typename V> struct Percentage : PrimitiveNumeric<CSS::Percentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::Percentage<R, V>>;
    using Base::Base;
};

// MARK: Dimension Primitives

template<CSS::Range R, typename V> struct Angle : PrimitiveNumeric<CSS::Angle<R, V>> {
    using Base = PrimitiveNumeric<CSS::Angle<R, V>>;
    using Base::Base;

    constexpr Angle() requires (R.min == R.max)
        : Base { static_cast<V>(R.min) }
    {
    }
};
template<CSS::Range R, typename V> struct Length : PrimitiveNumeric<CSS::Length<R, V>> {
    using Base = PrimitiveNumeric<CSS::Length<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Time : PrimitiveNumeric<CSS::Time<R, V>> {
    using Base = PrimitiveNumeric<CSS::Time<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Frequency : PrimitiveNumeric<CSS::Frequency<R, V>> {
    using Base = PrimitiveNumeric<CSS::Frequency<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Resolution : PrimitiveNumeric<CSS::Resolution<R, V>> {
    using Base = PrimitiveNumeric<CSS::Resolution<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Flex : PrimitiveNumeric<CSS::Flex<R, V>> {
    using Base = PrimitiveNumeric<CSS::Flex<R, V>>;
    using Base::Base;
};

// MARK: Dimension + Percentage Primitives

template<CSS::Range R, typename V> struct AnglePercentage : PrimitiveNumeric<CSS::AnglePercentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::AnglePercentage<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct LengthPercentage : PrimitiveNumeric<CSS::LengthPercentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::LengthPercentage<R, V>>;
    using Base::Base;
};

template<auto R, typename V> struct DimensionPercentageMapping<CSS::AnglePercentage<R, V>> {
    using Dimension = Style::Angle<R, V>;
    using Percentage = Style::Percentage<R, V>;
};
template<auto R, typename V> struct DimensionPercentageMapping<CSS::LengthPercentage<R, V>> {
    using Dimension = Style::Length<R, V>;
    using Percentage = Style::Percentage<R, V>;
};

template<typename T> T get(DimensionPercentageNumeric auto const& dimensionPercentage)
{
    return dimensionPercentage.template get<T>();
}

// MARK: CSS -> Style

template<auto R, typename V> struct ToStyleMapping<CSS::Integer<R, V>>          { using type = Integer<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Number<R, V>>           { using type = Number<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Percentage<R, V>>       { using type = Percentage<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Angle<R, V>>            { using type = Angle<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Length<R, V>>           { using type = Length<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Time<R, V>>             { using type = Time<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Frequency<R, V>>        { using type = Frequency<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Resolution<R, V>>       { using type = Resolution<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Flex<R, V>>             { using type = Flex<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::AnglePercentage<R, V>>  { using type = AnglePercentage<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::LengthPercentage<R, V>> { using type = LengthPercentage<R, V>; };

// MARK: Style -> CSS

template<Numeric T> struct ToCSSMapping<T> {
    using type = typename T::CSS;
};

// MARK: Wrapper base types

template<typename T> struct PrimitiveNumericWrapperBase;

template<CSS::Range R, typename V> struct PrimitiveNumericWrapperBase<LengthPercentage<R, V>> {
    using Base = PrimitiveNumericWrapperBase<LengthPercentage<R, V>>;
    using Wrapped = LengthPercentage<R, V>;

    using Raw = typename Wrapped::Raw;
    using UnitType = typename Wrapped::UnitType;
    using UnitTraits = typename Wrapped::UnitTraits;
    using ResolvedValueType = typename Wrapped::ResolvedValueType;
    static constexpr auto range = Wrapped::range;
    static constexpr auto category = Wrapped::category;

    using Dimension = typename Wrapped::Dimension;
    using Percentage = typename Wrapped::Percentage;
    using Calc = typename Wrapped::Calc;

    Wrapped value;

    template<typename... Args>
    ALWAYS_INLINE PrimitiveNumericWrapperBase(Args&&... args) requires (requires { { LengthPercentage<R, V>(args...) }; })
        : value(std::forward<Args>(args)...)
    {
    }

    template<typename T>
    ALWAYS_INLINE bool holdsAlternative() const
    {
        return value.template holdsAlternative<T>();
    }
    template<typename... F>
    ALWAYS_INLINE decltype(auto) switchOn(F&&... f) const
    {
        return value.switchOn(std::forward<F>(f)...);
    }

    ALWAYS_INLINE bool isDimension() const { return value.isDimension(); }
    ALWAYS_INLINE bool isPercentage() const { return value.isPercentage(); }
    ALWAYS_INLINE bool isCalc() const { return value.isCalc(); }
    ALWAYS_INLINE bool isPercentageOrCalc() const { return value.isPercentageOrCalc(); }

    ALWAYS_INLINE std::optional<Dimension> tryDimension() const { return value.tryDimension(); }
    ALWAYS_INLINE std::optional<Percentage> tryPercentage() const { return value.tryPercentage(); }
    ALWAYS_INLINE std::optional<Calc> tryCalc() const { return value.tryCalc(); }

    ALWAYS_INLINE bool isKnownZero() const requires (range.min <= 0 && range.max >= 0) { return value.isKnownZero(); }
    ALWAYS_INLINE bool isKnownPositive() const requires (range.max > 0) { return value.isKnownPositive(); }
    ALWAYS_INLINE bool isKnownNegative() const requires (range.min < 0) { return value.isKnownNegative(); }
    ALWAYS_INLINE bool isPossiblyZero() const requires (range.min <= 0 && range.max >= 0) { return value.isPossiblyZero(); }
    ALWAYS_INLINE bool isPossiblyPositive() const requires (range.max > 0) { return value.isPossiblyPositive(); }
    ALWAYS_INLINE bool isPossiblyNegative() const requires (range.min < 0) { return value.isPossiblyNegative(); }

    bool operator==(const PrimitiveNumericWrapperBase&) const = default;

    template<size_t> friend const auto& get(const PrimitiveNumericWrapperBase& self)
    {
        return self.value;
    }

    // Legacy name support
    using Fixed = Dimension;
    ALWAYS_INLINE bool isFixed() const { return value.isFixed(); }
    ALWAYS_INLINE bool isPercent() const { return value.isPercent(); }
    ALWAYS_INLINE bool isCalculated() const { return value.isCalculated(); }
    ALWAYS_INLINE bool isPercentOrCalculated() const { return value.isPercentOrCalculated(); }
    ALWAYS_INLINE std::optional<Dimension> tryFixed() const { return value.tryFixed(); }
};


// MARK: Utility Macros

#define DEFINE_PRIMITIVE_NUMERIC_TYPE_WRAPPER(wrapper, wrapped)               \
    struct wrapper : PrimitiveNumericWrapperBase<wrapped> {                   \
        using Base::Base;                                                     \
    };

// MARK: Utility Concepts

template<typename T> concept IsPercentage = std::same_as<T, Percentage<T::range, typename T::ResolvedValueType>>;
template<typename T> concept IsCalc = std::same_as<T, UnevaluatedCalculation<typename T::CSS>>;

template<typename T> concept IsPercentageOrCalc = IsPercentage<T> || IsCalc<T>;


template<typename T> concept PrimitiveNumericWrapperBaseDerived = WTF::IsBaseOfTemplate<PrimitiveNumericWrapperBase, T>::value && TupleLike<T>;
template<typename T, auto category> concept SpecificPrimitiveNumericWrapperBaseDerived = WTF::IsBaseOfTemplate<PrimitiveNumericWrapperBase, T>::value && TupleLike<T> && T::Wrapped::category == category;

} // namespace Style
} // namespace WebCore

template<WebCore::Style::DimensionPercentageNumeric T>
struct WTF::FlatteningVariantTraits<T> {
    using TypeList = typename FlatteningVariantTraits<typename T::Representation>::TypeList;
};

namespace WTF {

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Integer<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Integer<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Number<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Number<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Percentage<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Percentage<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Angle<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Angle<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Length<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Length<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Time<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Time<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Frequency<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Frequency<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Resolution<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Resolution<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Flex<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Flex<R, V>> { };

} // namespace WTF

template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::AnglePercentage<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthPercentage<R, V>> = true;
