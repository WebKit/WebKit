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

#include "CSSCalcSymbolTable.h"
#include "CSSValueTypes.h"
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class RenderStyle;

namespace Style {

class BuilderState;

// Types can specialize this and set the value to true to be treated as "non-converting"
// for css to style / style to css conversion algorithms. This means the type is identical
// for both CSS and Style systems (e.g. a constant value or an enum).
template<class> inline constexpr bool TreatAsNonConverting = false;

// Types can specialize this and set the value to true to be treated as "tuple-like"
// for css to style / style to css conversion algorithms.
// NOTE: This gets automatically specialized when using the STYLE_TUPLE_LIKE_CONFORMANCE macro.
template<class> inline constexpr bool TreatAsTupleLike = false;

// Types that are treated as "tuple-like" can have their conversion operations defined
// automatically by just defining their type mapping.
template<typename> struct CSSToStyleMapping;
template<typename> struct StyleToCSSMapping;

// Macro to define two-way mapping between a CSS and Style type. This is only needed
// for "tuple-like" types, in lieu of explicit ToCSS/ToStyle specializations.
#define DEFINE_CSS_STYLE_MAPPING(css, style) \
    template<> struct CSSToStyleMapping<css> { using type = style; }; \
    template<> struct StyleToCSSMapping<style> { using type = css; }; \

// All non-converting and non-tuple like conforming types must implement the following for conversions:
//
//    template<> struct WebCore::Style::ToCSS<StyleType> {
//        CSSType operator()(const StyleType&, const RenderStyle&);
//    };
//
//    template<> struct WebCore::Style::ToStyle<CSSType> {
//        StyleType operator()(const CSSType&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&);
//        StyleType operator()(const CSSType&, const BuilderState&, const CSSCalcSymbolTable&);
//        StyleType operator()(const CSSType&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&);
//    };

// Token passed to ToStyle to indicate that the caller has checked that no conversion data is required.
struct NoConversionDataRequiredToken { };

template<typename> struct ToCSS;
template<typename> struct ToStyle;

// Types can specialize `PrimaryCSSType` to provided a "primary" type the specialized type should be converted to before conversion to Style. For
// instance, `CSS::NumberRaw`, would specialize this as `template<> struct ToPrimaryCSSTypeMapping<CSS::NumberRaw> { using type = CSS::Number };` to
// allow callers to use `toStyle(...)` directly on values of type `CSS::NumberRaw`.
template<typename CSSType> struct ToPrimaryCSSTypeMapping { using type = CSSType; };
template<typename CSSType> using PrimaryCSSType = typename ToPrimaryCSSTypeMapping<CSSType>::type;

// MARK: Common Types.

// Helper type used to represent a constant identifier.
template<CSSValueID C> using Constant = CSS::Constant<C>;

// Specialize TreatAsNonConverting for Constant<C>, to indicate that its type does not change from the CSS representation.
template<CSSValueID C> inline constexpr bool TreatAsNonConverting<Constant<C>> = true;

// Helper type used to represent a CSS function.
template<CSSValueID C, typename T> using FunctionNotation = CSS::FunctionNotation<C, T>;

// Wraps a variable number of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T, size_t inlineCapacity = 0> struct SpaceSeparatedVector {
    using Vector = WTF::Vector<T, inlineCapacity>;
    using const_iterator = typename Vector::const_iterator;
    using const_reverse_iterator = typename Vector::const_reverse_iterator;
    using value_type = typename Vector::value_type;

    SpaceSeparatedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    SpaceSeparatedVector(WTF::Vector<T, inlineCapacity>&& value)
        : value { WTFMove(value) }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    bool operator==(const SpaceSeparatedVector<T, inlineCapacity>&) const = default;

    WTF::Vector<T, inlineCapacity> value;
};

// Wraps a variable size list of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T, size_t inlineCapacity = 0> struct CommaSeparatedVector {
    using Vector = WTF::Vector<T, inlineCapacity>;
    using const_iterator = typename Vector::const_iterator;
    using const_reverse_iterator = typename Vector::const_reverse_iterator;
    using value_type = typename Vector::value_type;

    CommaSeparatedVector(std::initializer_list<T> initializerList)
        : value { initializerList }
    {
    }

    CommaSeparatedVector(WTF::Vector<T, inlineCapacity>&& value)
        : value { WTFMove(value) }
    {
    }

    const_iterator begin() const { return value.begin(); }
    const_iterator end() const { return value.end(); }
    const_reverse_iterator rbegin() const { return value.rbegin(); }
    const_reverse_iterator rend() const { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const T& operator[](size_t i) const { return value[i]; }

    bool operator==(const CommaSeparatedVector<T, inlineCapacity>&) const = default;

    WTF::Vector<T, inlineCapacity> value;
};

// Wraps a fixed size list of elements of a single type, semantically marking them as serializing as "space separated".
template<typename T, size_t N> struct SpaceSeparatedArray {
    using Array = std::array<T, N>;

    template<typename...Ts>
        requires (sizeof...(Ts) == N && WTF::all<std::is_same_v<T, Ts>...>)
    constexpr SpaceSeparatedArray(Ts&&... values)
        : value { std::forward<Ts>(values)... }
    {
    }

    constexpr SpaceSeparatedArray(std::array<T, N>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedArray<T, N>&) const = default;

    std::array<T, N> value;
};

template<class T, class... Ts>
    requires (WTF::all<std::is_same_v<T, Ts>...>)
SpaceSeparatedArray(T, Ts...) -> SpaceSeparatedArray<T, 1 + sizeof...(Ts)>;

template<size_t I, typename T, size_t N> decltype(auto) get(const SpaceSeparatedArray<T, N>& array)
{
    return std::get<I>(array.value);
}

// Wraps a fixed size list of elements of a single type, semantically marking them as serializing as "comma separated".
template<typename T, size_t N> struct CommaSeparatedArray {
    using Array = std::array<T, N>;

    template<typename...Ts>
        requires (sizeof...(Ts) == N && WTF::all<std::is_same_v<T, Ts>...>)
    constexpr CommaSeparatedArray(Ts&&... values)
        : value { std::forward<Ts>(values)... }
    {
    }

    constexpr CommaSeparatedArray(std::array<T, N>&& array)
        : value { WTFMove(array) }
    {
    }

    constexpr bool operator==(const CommaSeparatedArray<T, N>&) const = default;

    std::array<T, N> value;
};

template<class T, class... Ts>
    requires (WTF::all<std::is_same_v<T, Ts>...>)
CommaSeparatedArray(T, Ts...) -> CommaSeparatedArray<T, 1 + sizeof...(Ts)>;

template<size_t I, typename T, size_t N> decltype(auto) get(const CommaSeparatedArray<T, N>& array)
{
    return std::get<I>(array.value);
}

// Wraps a variadic list of types, semantically marking them as serializing as "space separated".
template<typename... Ts> struct SpaceSeparatedTuple {
    using Tuple = std::tuple<Ts...>;

    constexpr SpaceSeparatedTuple(Ts&&... values)
        : value { std::make_tuple(std::forward<Ts>(values)...) }
    {
    }

    constexpr SpaceSeparatedTuple(const Ts&... values)
        : value { std::make_tuple(values...) }
    {
    }

    constexpr SpaceSeparatedTuple(std::tuple<Ts...>&& tuple)
        : value { WTFMove(tuple) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedTuple<Ts...>&) const = default;

    std::tuple<Ts...> value;
};

template<size_t I, typename... Ts> decltype(auto) get(const SpaceSeparatedTuple<Ts...>& tuple)
{
    return std::get<I>(tuple.value);
}

// Wraps a variadic list of types, semantically marking them as serializing as "comma separated".
template<typename... Ts> struct CommaSeparatedTuple {
    using Tuple = std::tuple<Ts...>;

    constexpr CommaSeparatedTuple(Ts&&... values)
        : value { std::make_tuple(std::forward<Ts>(values)...) }
    {
    }

    constexpr CommaSeparatedTuple(const Ts&... values)
        : value { std::make_tuple(values...) }
    {
    }

    constexpr CommaSeparatedTuple(std::tuple<Ts...>&& tuple)
        : value { WTFMove(tuple) }
    {
    }

    constexpr bool operator==(const CommaSeparatedTuple<Ts...>&) const = default;

    std::tuple<Ts...> value;
};

template<size_t I, typename... Ts> decltype(auto) get(const CommaSeparatedTuple<Ts...>& tuple)
{
    return std::get<I>(tuple.value);
}

// MARK: - Conversion from "Style to "CSS"

// Conversion Invoker
template<typename StyleType> decltype(auto) toCSS(const StyleType& styleType, const RenderStyle& style)
{
    return ToCSS<StyleType>{}(styleType, style);
}

template<typename To, typename From> auto toCSSOnTupleLike(const From& tupleLike, const RenderStyle& style) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toCSS(x, style)... }; }, tupleLike);
}

// Conversion Utility Types
template<typename StyleType> using CSSType = std::decay_t<decltype(toCSS(std::declval<const StyleType&>(), std::declval<const RenderStyle&>()))>;
template<typename... StyleTypes> using CSSVariant = std::variant<CSSType<StyleTypes>...>;
template<typename... StyleTypes> using CSSTuple = std::tuple<CSSType<StyleTypes>...>;

// Constrained for `TreatAsNonConverting`.
template<typename StyleType> requires (TreatAsNonConverting<StyleType>) struct ToCSS<StyleType> {
    constexpr decltype(auto) operator()(const StyleType& value, const RenderStyle&)
    {
        return value;
    }
};

// Constrained for `TreatAsTupleLike`.
template<typename StyleType> requires (TreatAsTupleLike<StyleType>) struct ToCSS<StyleType> {
    decltype(auto) operator()(const StyleType& value, const RenderStyle& style)
    {
        return toCSSOnTupleLike<typename StyleToCSSMapping<StyleType>::type>(value, style);
    }
};

// Specialization for `std::optional`.
template<typename StyleType> struct ToCSS<std::optional<StyleType>> {
    decltype(auto) operator()(const std::optional<StyleType>& value, const RenderStyle& style)
    {
        return value ? std::make_optional(toCSS(*value, style)) : std::nullopt;
    }
};

// Specialization for `std::variant`.
template<typename... StyleTypes> struct ToCSS<std::variant<StyleTypes...>> {
    decltype(auto) operator()(const std::variant<StyleTypes...>& value, const RenderStyle& style)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return CSSVariant<StyleTypes...> { toCSS(alternative, style) }; });
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename StyleType> struct ToCSS<FunctionNotation<Name, StyleType>> {
    decltype(auto) operator()(const FunctionNotation<Name, StyleType>& value, const RenderStyle& style)
    {
        return FunctionNotation<Name, CSSType<StyleType>> { toCSS(value.parameters, style) };
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct ToCSS<SpaceSeparatedVector<StyleType, inlineCapacity>> {
    using Result = CSS::SpaceSeparatedVector<CSSType<StyleType>, inlineCapacity>;

    decltype(auto) operator()(const SpaceSeparatedVector<StyleType, inlineCapacity>& value, const RenderStyle& style)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toCSS(x, style); }) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct ToCSS<CommaSeparatedVector<StyleType, inlineCapacity>> {
    using Result = CSS::CommaSeparatedVector<CSSType<StyleType>, inlineCapacity>;

    decltype(auto) operator()(const CommaSeparatedVector<StyleType, inlineCapacity>& value, const RenderStyle& style)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toCSS(x, style); }) };
    }
};

// Specialization for `SpaceSeparatedArray`.
template<typename StyleType, size_t N> struct ToCSS<SpaceSeparatedArray<StyleType, N>> {
    using Result = CSS::SpaceSeparatedArray<CSSType<StyleType>, N>;

    decltype(auto) operator()(const SpaceSeparatedArray<StyleType, N>& value, const RenderStyle& style)
    {
        return Result { toCSSOnTupleLike<typename Result::Array>(value.value, style) };
    }
};

// Specialization for `CommaSeparatedArray`.
template<typename StyleType, size_t N> struct ToCSS<CommaSeparatedArray<StyleType, N>> {
    using Result = CSS::CommaSeparatedArray<CSSType<StyleType>, N>;

    decltype(auto) operator()(const CommaSeparatedArray<StyleType, N>& value, const RenderStyle& style)
    {
        return Result { toCSSOnTupleLike<typename Result::Array>(value.value, style) };
    }
};

// Specialization for `SpaceSeparatedTuple`.
template<typename... StyleTypes> struct ToCSS<SpaceSeparatedTuple<StyleTypes...>> {
    using Result = CSS::SpaceSeparatedTuple<CSSType<StyleTypes>...>;

    decltype(auto) operator()(const SpaceSeparatedTuple<StyleTypes...>& value, const RenderStyle& style)
    {
        return Result { toCSSOnTupleLike<typename Result::Tuple>(value.value, style) };
    }
};

// Specialization for `CommaSeparatedTuple`.
template<typename... StyleTypes> struct ToCSS<CommaSeparatedTuple<StyleTypes...>> {
    using Result = CSS::CommaSeparatedTuple<CSSType<StyleTypes>...>;

    decltype(auto) operator()(const CommaSeparatedTuple<StyleTypes...>& value, const RenderStyle& style)
    {
        return Result { toCSSOnTupleLike<typename Result::Tuple>(value.value, style) };
    }
};

// MARK: - Conversion from "CSS" to "Style"

// Conversion Invokers
template<typename CSSType> decltype(auto) toStyle(const CSSType& cssType, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return ToStyle<PrimaryCSSType<CSSType>>{}(cssType, conversionData, symbolTable);
}

template<typename CSSType> decltype(auto) toStyle(const CSSType& cssType, const CSSToLengthConversionData& conversionData)
{
    return ToStyle<PrimaryCSSType<CSSType>>{}(cssType, conversionData, CSSCalcSymbolTable { });
}

template<typename CSSType> decltype(auto) toStyle(const CSSType& cssType, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
{
    return ToStyle<PrimaryCSSType<CSSType>>{}(cssType, builderState, symbolTable);
}

template<typename CSSType> decltype(auto) toStyle(const CSSType& cssType, const BuilderState& builderState)
{
    return ToStyle<PrimaryCSSType<CSSType>>{}(cssType, builderState, CSSCalcSymbolTable { });
}

template<typename CSSType> decltype(auto) toStyleNoConversionDataRequired(const CSSType& cssType, const CSSCalcSymbolTable& symbolTable)
{
    return ToStyle<PrimaryCSSType<CSSType>>{}(cssType, NoConversionDataRequiredToken { }, symbolTable);
}

template<typename CSSType> decltype(auto) toStyleNoConversionDataRequired(const CSSType& cssType)
{
    return ToStyle<PrimaryCSSType<CSSType>>{}(cssType, NoConversionDataRequiredToken { }, CSSCalcSymbolTable { });
}

template<typename To, typename From, typename... Args> auto toStyleOnTupleLike(const From& tupleLike, Args&&... args) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toStyle(x, args...)... }; }, tupleLike);
}

template<typename To, typename From, typename... Args> auto toStyleNoConversionDataRequiredOnTupleLike(const From& tupleLike, Args&&... args) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toStyleNoConversionDataRequired(x, args...)... }; }, tupleLike);
}

// Conversion Utility Types
template<typename CSSType> using StyleType = std::decay_t<decltype(toStyle(std::declval<const CSSType&>(), std::declval<const BuilderState&>(), std::declval<const CSSCalcSymbolTable&>()))>;
template<typename... CSSTypes> using StyleVariant = std::variant<StyleType<CSSTypes>...>;
template<typename... CSSTypes> using StyleTuple = std::tuple<StyleType<CSSTypes>...>;

// Constrained for `TreatAsNonConverting`.
template<typename CSSType> requires (TreatAsNonConverting<CSSType>) struct ToStyle<CSSType> {
    constexpr decltype(auto) operator()(const CSSType& value, const CSSToLengthConversionData&, const CSSCalcSymbolTable&)
    {
        return value;
    }
    constexpr decltype(auto) operator()(const CSSType& value, const BuilderState&, const CSSCalcSymbolTable&)
    {
        return value;
    }
    constexpr decltype(auto) operator()(const CSSType& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&)
    {
        return value;
    }
};

// Constrained for `TreatAsTupleLike`.
template<typename CSSType> requires (CSS::TreatAsTupleLike<CSSType>) struct ToStyle<CSSType> {
    decltype(auto) operator()(const CSSType& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return toStyleOnTupleLike<typename CSSToStyleMapping<CSSType>::type>(value, conversionData, symbolTable);
    }
    decltype(auto) operator()(const CSSType& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return toStyleOnTupleLike<typename CSSToStyleMapping<CSSType>::type>(value, builderState, symbolTable);
    }
    decltype(auto) operator()(const CSSType& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return toStyleNoConversionDataRequiredOnTupleLike<typename CSSToStyleMapping<CSSType>::type>(value, symbolTable);
    }
};

// Specialization for `std::optional`.
template<typename CSSType> struct ToStyle<std::optional<CSSType>> {
    decltype(auto) operator()(const std::optional<CSSType>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return value ? std::make_optional(toStyle(*value, conversionData, symbolTable)) : std::nullopt;
    }
    decltype(auto) operator()(const std::optional<CSSType>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return value ? std::make_optional(toStyle(*value, builderState, symbolTable)) : std::nullopt;
    }
    decltype(auto) operator()(const std::optional<CSSType>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return value ? std::make_optional(toStyleNoConversionDataRequired(*value, symbolTable)) : std::nullopt;
    }
};

// Specialization for `std::variant`.
template<typename... CSSTypes> struct ToStyle<std::variant<CSSTypes...>> {
    decltype(auto) operator()(const std::variant<CSSTypes...>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return StyleVariant<CSSTypes...> { toStyle(alternative, conversionData, symbolTable) }; });
    }
    decltype(auto) operator()(const std::variant<CSSTypes...>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return StyleVariant<CSSTypes...> { toStyle(alternative, builderState, symbolTable) }; });
    }
    decltype(auto) operator()(const std::variant<CSSTypes...>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return StyleVariant<CSSTypes...> { toStyleNoConversionDataRequired(alternative, symbolTable) }; });
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct ToStyle<CSS::FunctionNotation<Name, CSSType>> {
    decltype(auto) operator()(const CSS::FunctionNotation<Name, CSSType>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return FunctionNotation<Name, StyleType<CSSType>> { toStyle(value.parameters, conversionData, symbolTable) };
    }
    decltype(auto) operator()(const CSS::FunctionNotation<Name, CSSType>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return FunctionNotation<Name, StyleType<CSSType>> { toStyle(value.parameters, builderState, symbolTable) };
    }
    decltype(auto) operator()(const CSS::FunctionNotation<Name, CSSType>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return FunctionNotation<Name, StyleType<CSSType>> { toStyleNoConversionDataRequired(value.parameters, symbolTable) };
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ToStyle<CSS::SpaceSeparatedVector<CSSType, inlineCapacity>> {
    using Result = SpaceSeparatedVector<StyleType<CSSType>, inlineCapacity>;

    decltype(auto) operator()(const CSS::SpaceSeparatedVector<CSSType, inlineCapacity>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyle(x, conversionData, symbolTable); }) };
    }
    decltype(auto) operator()(const CSS::SpaceSeparatedVector<CSSType, inlineCapacity>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyle(x, builderState, symbolTable); }) };
    }
    decltype(auto) operator()(const CSS::SpaceSeparatedVector<CSSType, inlineCapacity>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyleNoConversionDataRequired(x, symbolTable); }) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ToStyle<CSS::CommaSeparatedVector<CSSType, inlineCapacity>> {
    using Result = CommaSeparatedVector<StyleType<CSSType>, inlineCapacity>;

    decltype(auto) operator()(const CSS::CommaSeparatedVector<CSSType, inlineCapacity>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyle(x, conversionData, symbolTable); }) };
    }
    decltype(auto) operator()(const CSS::CommaSeparatedVector<CSSType, inlineCapacity>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyle(x, builderState, symbolTable); }) };
    }
    decltype(auto) operator()(const CSS::CommaSeparatedVector<CSSType, inlineCapacity>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyleNoConversionDataRequired(x, symbolTable); }) };
    }
};

// Specialization for `SpaceSeparatedArray`.
template<typename CSSType, size_t N> struct ToStyle<CSS::SpaceSeparatedArray<CSSType, N>> {
    using Result = SpaceSeparatedArray<StyleType<CSSType>, N>;

    decltype(auto) operator()(const CSS::SpaceSeparatedArray<CSSType, N>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Array>(value, conversionData, symbolTable) };
    }
    decltype(auto) operator()(const CSS::SpaceSeparatedArray<CSSType, N>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Array>(value, builderState, symbolTable) };
    }
    decltype(auto) operator()(const CSS::SpaceSeparatedArray<CSSType, N>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleNoConversionDataRequiredOnTupleLike<typename Result::Array>(value, symbolTable) };
    }
};

// Specialization for `CommaSeparatedArray`.
template<typename CSSType, size_t N> struct ToStyle<CSS::CommaSeparatedArray<CSSType, N>> {
    using Result = CommaSeparatedArray<StyleType<CSSType>, N>;

    decltype(auto) operator()(const CSS::CommaSeparatedArray<CSSType, N>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Array>(value, conversionData, symbolTable) };
    }
    decltype(auto) operator()(const CSS::CommaSeparatedArray<CSSType, N>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Array>(value, builderState, symbolTable) };
    }
    decltype(auto) operator()(const CSS::CommaSeparatedArray<CSSType, N>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleNoConversionDataRequiredOnTupleLike<typename Result::Array>(value, symbolTable) };
    }
};

// Specialization for `SpaceSeparatedTuple`.
template<typename... CSSTypes> struct ToStyle<CSS::SpaceSeparatedTuple<CSSTypes...>> {
    using Result = SpaceSeparatedTuple<StyleType<CSSTypes>...>;

    decltype(auto) operator()(const CSS::SpaceSeparatedTuple<CSSTypes...>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Tuple>(value, conversionData, symbolTable) };
    }
    decltype(auto) operator()(const CSS::SpaceSeparatedTuple<CSSTypes...>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Tuple>(value, builderState, symbolTable) };
    }
    decltype(auto) operator()(const CSS::SpaceSeparatedTuple<CSSTypes...>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleNoConversionDataRequiredOnTupleLike<typename Result::Tuple>(value, symbolTable) };
    }
};

// Specialization for `CommaSeparatedTuple`.
template<typename... CSSTypes> struct ToStyle<CSS::CommaSeparatedTuple<CSSTypes...>> {
    using Result = CommaSeparatedTuple<StyleType<CSSTypes>...>;

    decltype(auto) operator()(const CSS::CommaSeparatedTuple<CSSTypes...>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Tuple>(value, conversionData, symbolTable) };
    }
    decltype(auto) operator()(const CSS::CommaSeparatedTuple<CSSTypes...>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleOnTupleLike<typename Result::Tuple>(value, builderState, symbolTable) };
    }
    decltype(auto) operator()(const CSS::CommaSeparatedTuple<CSSTypes...>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result { toStyleNoConversionDataRequiredOnTupleLike<typename Result::Tuple>(value, symbolTable) };
    }
};

} // namespace Style
} // namespace WebCore

namespace std {

template<typename T, size_t N> class tuple_size<WebCore::Style::SpaceSeparatedArray<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, WebCore::Style::SpaceSeparatedArray<T, N>> {
public:
    using type = T;
};

template<typename T, size_t N> class tuple_size<WebCore::Style::CommaSeparatedArray<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, WebCore::Style::CommaSeparatedArray<T, N>> {
public:
    using type = T;
};

template<typename... Ts> class tuple_size<WebCore::Style::SpaceSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::Style::SpaceSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

template<typename... Ts> class tuple_size<WebCore::Style::CommaSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::Style::CommaSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

#define STYLE_TUPLE_LIKE_CONFORMANCE(t, numberOfArguments) \
    namespace std { \
        template<> class tuple_size<WebCore::Style::t> : public std::integral_constant<size_t, numberOfArguments> { }; \
        template<size_t I> class tuple_element<I, WebCore::Style::t> { \
        public: \
            using type = decltype(WebCore::Style::get<I>(std::declval<WebCore::Style::t>())); \
        }; \
    } \
    template<> inline constexpr bool WebCore::Style::TreatAsTupleLike<WebCore::Style::t> = true; \
\

} // namespace std
