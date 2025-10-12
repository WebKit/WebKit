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

#include <WebCore/CSSPrimitiveNumericRange.h>
#include <WebCore/CSSValueConcepts.h>
#include <concepts>
#include <optional>
#include <wtf/Brigand.h>

namespace WebCore {

enum CSSValueID : uint16_t;
enum class CSSUnitType : uint8_t;

template<CSSValueID> struct Constant;

namespace Calculation {
enum class Category : uint8_t;
}

namespace CSS {

template<typename> struct UnitTraits;

// Concept covering all unit types.
template<typename T> concept UnitEnum = std::is_enum_v<T> && requires {
    requires std::integral<decltype(UnitTraits<T>::count)>;
    requires std::same_as<decltype(UnitTraits<T>::canonical), const T>;
    requires std::same_as<decltype(UnitTraits<T>::category), const Calculation::Category>;
    { UnitTraits<T>::validate(std::declval<CSSUnitType>()) } -> std::same_as<std::optional<T>>;
};

// Concept covering unit types where the enumeration contains only a single value.
//   e.g. IntegerUnit, NumberUnit, PercentageUnit, FlexUnit
template<typename T> concept SingleValueUnitEnum = UnitEnum<T> &&  requires {
    requires (UnitTraits<T>::count == 1);
};

// Concept covering unit types where the type is a composite of multiple other unit types.
//   e.g. AnglePercentageUnit, LengthPercentageUnit
template<typename T> concept CompositeUnitEnum = UnitEnum<T> && requires {
    typename UnitTraits<T>::Composite;
};

// Concept to check if a type, `T`, is one of the types `CompositeParent` composes over.
//   e.g. `NestedUnitEnumOf<LengthUnit, LengthPercentageUnit> == true`
template<typename T, typename CompositeParent> concept NestedUnitEnumOf = UnitEnum<T>
    && CompositeUnitEnum<CompositeParent>
    && brigand::contains<typename UnitTraits<CompositeParent>::Composite, T>::value;

// Forward declaration of PrimitiveNumericRaw to needed to create a hard constraint for the NumericRaw concept below.
template<Range, UnitEnum, typename> struct PrimitiveNumericRaw;

// Concept for use in generic contexts to filter on raw numeric CSS types.
template<typename T> concept NumericRaw = std::derived_from<T, PrimitiveNumericRaw<T::range, typename T::UnitType, typename T::ResolvedValueType>>;

// Forward declaration of PrimitiveNumeric to needed to create a hard constraint for the Numeric concept below.
template<NumericRaw> struct PrimitiveNumeric;

// Concept for use in generic contexts to filter on all numeric CSS types.
template<typename T> concept Numeric = VariantLike<T> && std::derived_from<T, PrimitiveNumeric<typename T::Raw>>;

// Concept for use in generic contexts to filter on non-composite numeric CSS types.
template<typename T> concept NonCompositeNumeric = Numeric<T> && (!CompositeUnitEnum<typename T::UnitType>);

// Concept for use in generic contexts to filter on dimension-percentage numeric CSS types.
template<typename T> concept DimensionPercentageNumeric = Numeric<T> && CompositeUnitEnum<typename T::UnitType>;

// Forward declaration of UnevaluatedCalc to needed to create a hard constraint for the Calc concept below.
template<NumericRaw> struct UnevaluatedCalc;

// Concept for use in generic contexts to filter on UnevaluatedCalc CSS types.
template<typename T> concept Calc = std::same_as<T, UnevaluatedCalc<typename T::Raw>>;

} // namespace CSS
} // namespace WebCore
