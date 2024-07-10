/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSUnits.h"
#include "CSSUnits.h"
#include <optional>
#include <variant>
#include <wtf/Brigand.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

template<typename... Ts>
using VariantWrapper = typename std::variant<Ts...>;

template<typename TypeList>
using VariantOrSingle = std::conditional_t<
    brigand::size<TypeList>::value == 1,
    brigand::front<TypeList>,
    brigand::wrap<TypeList, VariantWrapper>
>;

enum CSSValueID : uint16_t;

class CSSCalcSymbolTable;
class CSSPrimitiveValue;
class CSSToLengthConversionData;

// MARK: Integer

enum class IntegerRawValueRange : uint8_t { All, NonNegative, Positive };

template<IntegerRawValueRange range> constexpr double computeMinimumValue()
{
    if constexpr (range == IntegerRawValueRange::All)
        return -std::numeric_limits<double>::infinity();
    else if constexpr (range == IntegerRawValueRange::NonNegative)
        return 0;
    else if constexpr (range == IntegerRawValueRange::Positive)
        return 1.0;
}

template<IntegerRawValueRange> struct IntegerRawTypeMap;
template<> struct IntegerRawTypeMap<IntegerRawValueRange::All> { using type = int; };
template<> struct IntegerRawTypeMap<IntegerRawValueRange::NonNegative> { using type = int; };
template<> struct IntegerRawTypeMap<IntegerRawValueRange::Positive> { using type = unsigned; };

template<IntegerRawValueRange R>
struct IntegerRaw {
    using IntType = typename IntegerRawTypeMap<R>::type;

    static constexpr CSSUnitType type = CSSUnitType::CSS_INTEGER;
    IntType value;

    bool operator==(const IntegerRaw<R>&) const = default;
};
template<IntegerRawValueRange R> using CanonicalIntegerRaw = IntegerRaw<R>; // IntegerRaw is already canonical, so it can be its own canonical type.
template<IntegerRawValueRange R> constexpr CanonicalIntegerRaw<R> canonicalize(const IntegerRaw<R>& value) { return value; };

// Out-of-line implementations for concrete integer values.
void serializationIntegerForCSS(StringBuilder&, int);
void serializationIntegerForCSS(StringBuilder&, unsigned);
Ref<CSSPrimitiveValue> createCSSPrimitiveValueForInteger(int);
Ref<CSSPrimitiveValue> createCSSPrimitiveValueForInteger(unsigned);

template<IntegerRawValueRange R> void serializationForCSS(StringBuilder& builder, const IntegerRaw<R>& value)
{
    serializationIntegerForCSS(builder, value.value);
}
template<IntegerRawValueRange R> Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const IntegerRaw<R>& value)
{
    return createCSSPrimitiveValueForInteger(value.value);
}

// MARK: Number

struct NumberRaw {
    static constexpr CSSUnitType type = CSSUnitType::CSS_NUMBER;
    double value;

    bool operator==(const NumberRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const NumberRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const NumberRaw&);

using CanonicalNumberRaw = NumberRaw; // NumberRaw is already canonical, so it can be its own canonical type.
constexpr CanonicalNumberRaw canonicalize(const NumberRaw& value) { return value; };

// MARK: Percent

struct PercentRaw {
    static constexpr CSSUnitType type = CSSUnitType::CSS_PERCENTAGE;
    double value;

    bool operator==(const PercentRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const PercentRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const PercentRaw&);

using CanonicalPercentRaw = PercentRaw; // PercentRaw is already canonical, so it can be its own canonical type.
constexpr CanonicalPercentRaw canonicalize(const PercentRaw& value) { return value; };

// MARK: Angle

struct AngleRaw {
    CSSUnitType type;
    double value;

    bool operator==(const AngleRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const AngleRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const AngleRaw&);

struct CanonicalAngleRaw {
    static constexpr CSSUnitType type = CSSUnitType::CSS_DEG;
    double value;

    bool operator==(const CanonicalAngleRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const CanonicalAngleRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalAngleRaw&);
CanonicalAngleRaw canonicalize(const AngleRaw&);

// MARK: Length

struct LengthRaw {
    CSSUnitType type;
    double value;

    bool operator==(const LengthRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const LengthRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const LengthRaw&);

struct CanonicalLengthRaw {
    static constexpr CSSUnitType type = CSSUnitType::CSS_PX;
    double value;

    bool operator==(const CanonicalLengthRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const CanonicalLengthRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalLengthRaw&);
CanonicalLengthRaw canonicalize(const LengthRaw&, const CSSToLengthConversionData&);

// MARK: Resolution

struct ResolutionRaw {
    CSSUnitType type;
    double value;

    bool operator==(const ResolutionRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const ResolutionRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const ResolutionRaw&);

struct CanonicalResolutionRaw {
    static constexpr CSSUnitType type = CSSUnitType::CSS_DPPX;
    double value;

    bool operator==(const CanonicalResolutionRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const CanonicalResolutionRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalResolutionRaw&);
CanonicalResolutionRaw canonicalize(const ResolutionRaw&);

// MARK: Time

struct TimeRaw {
    CSSUnitType type;
    double value;

    bool operator==(const TimeRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const TimeRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const TimeRaw&);

struct CanonicalTimeRaw {
    static constexpr CSSUnitType type = CSSUnitType::CSS_S;
    double value;

    bool operator==(const CanonicalTimeRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const CanonicalTimeRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalTimeRaw&);
CanonicalTimeRaw canonicalize(const TimeRaw&);


// MARK: Symbol

struct SymbolRaw {
    CSSValueID value;

    bool operator==(const SymbolRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const SymbolRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const SymbolRaw&);

// MARK: None

struct NoneRaw {
    bool operator==(const NoneRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const NoneRaw&);
Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const NoneRaw&);

// MARK: - Utility templates

template<typename T>
struct IsSymbolRaw : public std::integral_constant<bool, std::is_same_v<T, SymbolRaw>> { };

template<typename TypeList>
struct TypesMinusSymbolRaw {
    using ResultTypeList = brigand::remove_if<TypeList, IsSymbolRaw<brigand::_1>>;
    using type = VariantOrSingle<ResultTypeList>;
};
template<typename... Ts> using TypesMinusSymbolRawType = typename TypesMinusSymbolRaw<brigand::list<Ts...>>::type;

template<typename TypeList>
struct TypesPlusSymbolRaw {
    using ResultTypeList = brigand::append<TypeList, brigand::list<SymbolRaw>>;
    using type = VariantOrSingle<ResultTypeList>;
};
template<typename... Ts> using TypesPlusSymbolRawType = typename TypesPlusSymbolRaw<brigand::list<Ts...>>::type;

// MARK: - Symbol replacement

// Replaces the symbol with a value from the symbol table. This is only relevant
// for SymbolRaw, so a catchall overload that implements the identity function is
// provided to allow generic replacement.
NumberRaw replaceSymbol(SymbolRaw, const CSSCalcSymbolTable&);
template<typename T> T replaceSymbol(T value, const CSSCalcSymbolTable&) { return value; }

// `resolve` helper to replace any SymbolRaw values with their symbol table value.
template<typename... Ts>
auto replaceSymbol(const std::variant<Ts...>& component, const CSSCalcSymbolTable& symbolTable) -> TypesMinusSymbolRawType<Ts...>
{
    return WTF::switchOn(component, [&](auto part) -> TypesMinusSymbolRawType<Ts...> {
        return replaceSymbol(part, symbolTable);
    });
}

template<typename... Ts>
auto replaceSymbol(const std::optional<std::variant<Ts...>>& component, const CSSCalcSymbolTable& symbolTable) -> std::optional<TypesMinusSymbolRawType<Ts...>>
{
    if (component)
        return replaceSymbol(*component, symbolTable);
    return std::nullopt;
}

using LengthOrPercentRaw = std::variant<LengthRaw, PercentRaw>;
using PercentOrNumberRaw = std::variant<PercentRaw, NumberRaw>;

template<typename... Ts>
void serializationForCSS(StringBuilder& builder, const std::variant<Ts...>& variant)
{
    WTF::switchOn(variant, [&](auto& value) { serializationForCSS(builder, value); });
}

} // namespace WebCore
