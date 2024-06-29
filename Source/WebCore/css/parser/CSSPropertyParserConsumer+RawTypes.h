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

#include <optional>
#include <variant>
#include <wtf/Brigand.h>
#include <wtf/Forward.h>
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

namespace CSSPropertyParserHelpers {
enum class IntegerValueRange : uint8_t { All, Positive, NonNegative };
}

enum class CSSUnitType : uint8_t;
enum CSSValueID : uint16_t;

class CSSCalcSymbolTable;

struct NoneRaw {
    bool operator==(const NoneRaw&) const = default;
};

void serializationForCSS(StringBuilder&, const NoneRaw&);

struct NumberRaw {
    double value;

    bool operator==(const NumberRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const NumberRaw&);

struct PercentRaw {
    double value;

    bool operator==(const PercentRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const PercentRaw&);

struct AngleRaw {
    CSSUnitType type;
    double value;

    bool operator==(const AngleRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const AngleRaw&);

struct LengthRaw {
    CSSUnitType type;
    double value;

    bool operator==(const LengthRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const LengthRaw&);

struct ResolutionRaw {
    CSSUnitType type;
    double value;

    bool operator==(const ResolutionRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const ResolutionRaw&);

struct TimeRaw {
    CSSUnitType type;
    double value;

    bool operator==(const TimeRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const TimeRaw&);

struct SymbolRaw {
    CSSValueID value;

    bool operator==(const SymbolRaw&) const = default;
};
void serializationForCSS(StringBuilder&, const SymbolRaw&);

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

constexpr double computeMinimumValue(CSSPropertyParserHelpers::IntegerValueRange range)
{
    switch (range) {
    case CSSPropertyParserHelpers::IntegerValueRange::All:
        return -std::numeric_limits<double>::infinity();
    case CSSPropertyParserHelpers::IntegerValueRange::NonNegative:
        return 0.0;
    case CSSPropertyParserHelpers::IntegerValueRange::Positive:
        return 1.0;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();

    return 0.0;
}

template<typename IntType, CSSPropertyParserHelpers::IntegerValueRange Range>
struct IntegerRaw {
    IntType value;

    bool operator==(const IntegerRaw<IntType, Range>&) const = default;
};

using LengthOrPercentRaw = std::variant<LengthRaw, PercentRaw>;
using PercentOrNumberRaw = std::variant<PercentRaw, NumberRaw>;

template<typename... Ts>
void serializationForCSS(StringBuilder& builder, const std::variant<Ts...>& variant)
{
    WTF::switchOn(variant, [&](auto& value) { serializationForCSS(builder, value); });
}

} // namespace WebCore
