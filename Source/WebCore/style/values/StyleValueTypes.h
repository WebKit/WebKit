/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSValueTypes.h"
#include <optional>
#include <tuple>
#include <utility>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSToLengthConversionData;
class RenderStyle;
struct BlendingContext;

namespace Style {

class BuilderState;

// Types can specialize this and set the value to true to be treated as "non-converting"
// for css to style / style to css conversion algorithms. This means the type is identical
// for both CSS and Style systems (e.g. a constant value or an enum).
template<typename> inline constexpr bool TreatAsNonConverting = false;

// The `NonConverting` concept can be used to filter to types that specialize `TreatAsNonConverting`.
template<typename T> concept NonConverting = TreatAsNonConverting<T>;

// Types that are treated as "tuple-like" can have their conversion operations defined
// automatically by just defining their type mapping.
template<typename> struct ToStyleMapping;
template<typename> struct ToCSSMapping;

// Macro to define two-way mapping between a CSS and Style type. This is only needed
// for "tuple-like" types, in lieu of explicit ToCSS/ToStyle specializations.
#define DEFINE_TYPE_MAPPING(css, style) \
    template<> struct ToStyleMapping<css> { using type = style; }; \
    template<> struct ToCSSMapping<style> { using type = css; }; \

// All non-converting and non-tuple-like conforming types must implement the following for conversions:
//
//    template<> struct WebCore::Style::ToCSS<StyleType> {
//        CSSType operator()(const StyleType&, const RenderStyle&);
//    };
//
//    template<> struct WebCore::Style::ToStyle<CSSType> {
//        StyleType operator()(const CSSType&, const CSSToLengthConversionData&);
//        StyleType operator()(const CSSType&, const BuilderState&);
//        StyleType operator()(const CSSType&, NoConversionDataRequiredToken);
//    };

template<typename> struct ToCSS;
template<typename> struct ToStyle;

// MARK: Common Types.

// Specialize `TreatAsNonConverting` for `Constant<C>`, to indicate that its type does not change from the CSS representation.
template<CSSValueID C> inline constexpr bool TreatAsNonConverting<Constant<C>> = true;

// Specialize `TreatAsNonConverting` for `CustomIdentifier`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<CustomIdentifier> = true;

// Specialize `TreatAsNonConverting` for `WTF::AtomString`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<WTF::AtomString> = true;

// Specialize `TreatAsNonConverting` for `WTF::String`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<WTF::String> = true;

// Specialize `TreatAsNonConverting` for `WTF::URL`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<WTF::URL> = true;


// MARK: - Conversion from "Style to "CSS"

// Conversion Invoker
template<typename StyleType, typename... Rest> decltype(auto) toCSS(const StyleType& styleType, const RenderStyle& style, Rest&&... rest)
{
    return ToCSS<StyleType>{}(styleType, style, std::forward<Rest>(rest)...);
}

// Conversion Utility Types
template<typename StyleType> using CSSType = std::decay_t<decltype(toCSS(std::declval<const StyleType&>(), std::declval<const RenderStyle&>()))>;

template<typename To, typename From, typename... Rest> auto toCSSOnTupleLike(const From& tupleLike, Rest&&... rest) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toCSS(x, rest...)... }; }, tupleLike);
}

// Standard NonConverting type mappings (identity mappings):
template<NonConverting T> struct ToCSSMapping<T> { using type = T; };

// Standard Optional-Like type mappings:
template<typename T> struct ToCSSMapping<std::optional<T>> { using type = std::optional<CSSType<T>>; };
template<typename T> struct ToCSSMapping<WTF::Markable<T>> { using type = WTF::Markable<CSSType<T>>; };

// Standard Tuple-Like type mappings:
template<typename... Ts> struct ToCSSMapping<std::tuple<Ts...>> { using type = std::tuple<CSSType<Ts>...>; };
template<CSSValueID C, typename T> struct ToCSSMapping<FunctionNotation<C, T>> { using type = FunctionNotation<C, CSSType<T>>; };
template<typename T, size_t N> struct ToCSSMapping<SpaceSeparatedArray<T, N>> { using type = SpaceSeparatedArray<CSSType<T>, N>; };
template<typename T, size_t N> struct ToCSSMapping<CommaSeparatedArray<T, N>> { using type = CommaSeparatedArray<CSSType<T>, N>; };
template<typename... Ts> struct ToCSSMapping<SpaceSeparatedTuple<Ts...>> { using type = SpaceSeparatedTuple<CSSType<Ts>...>; };
template<typename... Ts> struct ToCSSMapping<CommaSeparatedTuple<Ts...>> { using type = CommaSeparatedTuple<CSSType<Ts>...>; };
template<typename T> struct ToCSSMapping<SpaceSeparatedPoint<T>> { using type = SpaceSeparatedPoint<CSSType<T>>; };
template<typename T> struct ToCSSMapping<SpaceSeparatedSize<T>> { using type = SpaceSeparatedSize<CSSType<T>>; };
template<typename T> struct ToCSSMapping<SpaceSeparatedRectEdges<T>> { using type = SpaceSeparatedRectEdges<CSSType<T>>; };
template<typename T> struct ToCSSMapping<MinimallySerializingSpaceSeparatedSize<T>> { using type = MinimallySerializingSpaceSeparatedSize<CSSType<T>>; };
template<typename T> struct ToCSSMapping<MinimallySerializingSpaceSeparatedRectEdges<T>> { using type = MinimallySerializingSpaceSeparatedRectEdges<CSSType<T>>; };

// Standard Range-Like type mappings:
template<typename T, size_t N> struct ToCSSMapping<SpaceSeparatedVector<T, N>> { using type = SpaceSeparatedVector<CSSType<T>, N>; };
template<typename T, size_t N> struct ToCSSMapping<CommaSeparatedVector<T, N>> { using type = CommaSeparatedVector<CSSType<T>, N>; };

// Standard Variant-Like type mappings:
template<typename... Ts> struct ToCSSMapping<Variant<Ts...>> {
    using type = Variant<CSSType<Ts>...>;
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting StyleType> struct ToCSS<StyleType> {
    constexpr StyleType operator()(const StyleType& value, const RenderStyle&)
    {
        return value;
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike StyleType> struct ToCSS<StyleType> {
    using Result = typename ToCSSMapping<StyleType>::type;

    Result operator()(const StyleType& value, const RenderStyle& style)
    {
        if (value)
            return toCSS(*value, style);
        return std::nullopt;
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> struct ToCSS<StyleType> {
    using Result = typename ToCSSMapping<StyleType>::type;

    Result operator()(const StyleType& value, const RenderStyle& style)
    {
        return toCSSOnTupleLike<Result>(value, style);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct ToCSS<StyleType> {
    using Result = typename ToCSSMapping<StyleType>::type;

    Result operator()(const StyleType& value, const RenderStyle& style)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return Result { toCSS(alternative, style) }; });
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct ToCSS<SpaceSeparatedVector<StyleType, inlineCapacity>> {
    using Result = SpaceSeparatedVector<CSSType<StyleType>, inlineCapacity>;

    Result operator()(const SpaceSeparatedVector<StyleType, inlineCapacity>& value, const RenderStyle& style)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toCSS(x, style); }) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct ToCSS<CommaSeparatedVector<StyleType, inlineCapacity>> {
    using Result = CommaSeparatedVector<CSSType<StyleType>, inlineCapacity>;

    Result operator()(const CommaSeparatedVector<StyleType, inlineCapacity>& value, const RenderStyle& style)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toCSS(x, style); }) };
    }
};

// MARK: - Conversion from "CSS" to "Style"

// Conversion Invokers
template<typename CSSType, typename... Rest> decltype(auto) toStyle(const CSSType& cssType, const CSSToLengthConversionData& conversionData, Rest&&... rest)
{
    return ToStyle<CSSType>{}(cssType, conversionData, std::forward<Rest>(rest)...);
}

template<typename CSSType, typename... Rest> decltype(auto) toStyle(const CSSType& cssType, const BuilderState& builderState, Rest&&... rest)
{
    return ToStyle<CSSType>{}(cssType, builderState, std::forward<Rest>(rest)...);
}

template<typename CSSType, typename... Rest> decltype(auto) toStyle(const CSSType& cssType, NoConversionDataRequiredToken token, Rest&&... rest)
{
    return ToStyle<CSSType>{}(cssType, token, std::forward<Rest>(rest)...);
}

// Convenience invoker that adds a `NoConversionDataRequiredToken` argument.
template<typename CSSType, typename... Rest> decltype(auto) toStyleNoConversionDataRequired(const CSSType& cssType, Rest&&... rest)
{
    return toStyle(cssType, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<typename To, typename From, typename... Rest> auto toStyleOnTupleLike(const From& tupleLike, Rest&&... rest) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toStyle(x, rest...)... }; }, tupleLike);
}

template<typename To, typename From, typename... Rest> auto toStyleNoConversionDataRequiredOnTupleLike(const From& tupleLike, Rest&&... rest) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toStyleNoConversionDataRequired(x, rest...)... }; }, tupleLike);
}

// Conversion Utility Types
template<typename CSSType> using StyleType = std::decay_t<decltype(toStyle(std::declval<const CSSType&>(), std::declval<const BuilderState&>()))>;

// Standard NonConverting type mappings (identity mappings):
template<NonConverting T> struct ToStyleMapping<T> { using type = T; };

// Standard Optional-Like type mappings:
template<typename T> struct ToStyleMapping<std::optional<T>> { using type = std::optional<StyleType<T>>; };
template<typename T> struct ToStyleMapping<WTF::Markable<T>> { using type = WTF::Markable<StyleType<T>>; };

// Standard Tuple-Like type mappings:
template<typename... Ts> struct ToStyleMapping<std::tuple<Ts...>> { using type = std::tuple<StyleType<Ts>...>; };
template<CSSValueID C, typename T> struct ToStyleMapping<FunctionNotation<C, T>> { using type = FunctionNotation<C, StyleType<T>>; };
template<typename T, size_t N> struct ToStyleMapping<SpaceSeparatedArray<T, N>> { using type = SpaceSeparatedArray<StyleType<T>, N>; };
template<typename T, size_t N> struct ToStyleMapping<CommaSeparatedArray<T, N>> { using type = CommaSeparatedArray<StyleType<T>, N>; };
template<typename... Ts> struct ToStyleMapping<SpaceSeparatedTuple<Ts...>> { using type = SpaceSeparatedTuple<StyleType<Ts>...>; };
template<typename... Ts> struct ToStyleMapping<CommaSeparatedTuple<Ts...>> { using type = CommaSeparatedTuple<StyleType<Ts>...>; };
template<typename T> struct ToStyleMapping<SpaceSeparatedPoint<T>> { using type = SpaceSeparatedPoint<StyleType<T>>; };
template<typename T> struct ToStyleMapping<SpaceSeparatedSize<T>> { using type = SpaceSeparatedSize<StyleType<T>>; };
template<typename T> struct ToStyleMapping<SpaceSeparatedRectEdges<T>> { using type = SpaceSeparatedRectEdges<StyleType<T>>; };
template<typename T> struct ToStyleMapping<MinimallySerializingSpaceSeparatedSize<T>> { using type = MinimallySerializingSpaceSeparatedSize<StyleType<T>>; };
template<typename T> struct ToStyleMapping<MinimallySerializingSpaceSeparatedRectEdges<T>> { using type = MinimallySerializingSpaceSeparatedRectEdges<StyleType<T>>; };

// Standard Range-Like type mappings:
template<typename T, size_t N> struct ToStyleMapping<SpaceSeparatedVector<T, N>> { using type = SpaceSeparatedVector<StyleType<T>, N>; };
template<typename T, size_t N> struct ToStyleMapping<CommaSeparatedVector<T, N>> { using type = CommaSeparatedVector<StyleType<T>, N>; };

// Standard Variant-Like type mappings:
template<typename... Ts> struct ToStyleMapping<Variant<Ts...>> {
    using type = Variant<StyleType<Ts>...>;
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting CSSType> struct ToStyle<CSSType> {
    template<typename... Rest> constexpr CSSType operator()(const CSSType& value, Rest&&...)
    {
        return value;
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct ToStyle<CSSType> {
    using Result = typename ToStyleMapping<CSSType>::type;

    template<typename... Rest> Result operator()(const CSSType& value, Rest&&... rest)
    {
        if (value)
            return toStyle(*value, std::forward<Rest>(rest)...);
        return std::nullopt;
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct ToStyle<CSSType> {
    using Result = typename ToStyleMapping<CSSType>::type;

    template<typename... Rest> Result operator()(const CSSType& value, Rest&&... rest)
    {
        return toStyleOnTupleLike<Result>(value, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct ToStyle<CSSType> {
    using Result = typename ToStyleMapping<CSSType>::type;

    template<typename... Rest> Result operator()(const CSSType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return Result { toStyle(alternative, std::forward<Rest>(rest)...) }; });
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ToStyle<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    using Result = SpaceSeparatedVector<StyleType<CSSType>, inlineCapacity>;

    template<typename... Rest> Result operator()(const SpaceSeparatedVector<CSSType, inlineCapacity>& value, Rest&&... rest)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyle(x, rest...); }) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ToStyle<CommaSeparatedVector<CSSType, inlineCapacity>> {
    using Result = CommaSeparatedVector<StyleType<CSSType>, inlineCapacity>;

    template<typename... Rest> Result operator()(const CommaSeparatedVector<CSSType, inlineCapacity>& value, Rest&&... rest)
    {
        return Result { value.value.template map<typename Result::Vector>([&](const auto& x) { return toStyle(x, rest...); }) };
    }
};

// MARK: - Conversion from "Style to "Ref<CSSValue>"

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::CSSValueCreation<StyleType> {
//        Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const StyleType&);
//    };

template<typename StyleType> struct CSSValueCreation;

template<typename StyleType> Ref<CSSValue> createCSSValue(CSSValuePool& pool, const RenderStyle& style, const StyleType& value)
{
    return CSSValueCreation<StyleType>{}(pool, style, value);
}

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct CSSValueCreation<StyleType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) -> Ref<CSSValue> { return createCSSValue(pool, style, alternative); });
    }
};

// Constrained for `TreatAsTupleLike`
template<TupleLike StyleType> struct CSSValueCreation<StyleType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value)
    {
        if constexpr (std::tuple_size_v<StyleType> == 1 && SerializationSeparator<StyleType> == SerializationSeparatorType::None) {
            return createCSSValue(pool, style, get<0>(value));
        } else {
            CSSValueListBuilder list;

            auto caller = WTF::makeVisitor(
                [&]<typename T>(const std::optional<T>& element) {
                    if (!element)
                        return;
                    list.append(createCSSValue(pool, style, *element));
                },
                [&]<typename T>(const Markable<T>& element) {
                    if (!element)
                        return;
                    list.append(createCSSValue(pool, style, *element));
                },
                [&](const auto& element) {
                    list.append(createCSSValue(pool, style, element));
                }
            );
            WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);

            return CSS::makeListCSSValue<StyleType>(WTFMove(list));
        }
    }
};

// Constrained for `TreatAsRangeLike`
template<RangeLike StyleType> struct CSSValueCreation<StyleType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value)
    {
        CSSValueListBuilder list;
        for (const auto& element : value)
            list.append(createCSSValue(pool, style, element));

        return CSS::makeListCSSValue<StyleType>(WTFMove(list));
    }
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting StyleType> struct CSSValueCreation<StyleType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle&, const StyleType& value)
    {
        return CSS::createCSSValue(pool, value);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename StyleType> struct CSSValueCreation<FunctionNotation<Name, StyleType>> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const FunctionNotation<Name, StyleType>& value)
    {
        return CSS::makeFunctionCSSValue(value.name, createCSSValue(pool, style, value.parameters));
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedSize`.
template<typename CSSType> struct CSSValueCreation<MinimallySerializingSpaceSeparatedSize<CSSType>> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const MinimallySerializingSpaceSeparatedSize<CSSType>& value)
    {
        return CSS::makeSpaceSeparatedCoalescingPairCSSValue(createCSSValue(pool, style, get<0>(value)), createCSSValue(pool, style, get<1>(value)));
    }
};

// MARK: - Serialization

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::Serialize<StyleType> {
//        void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const StyleType&);
//    };

template<typename StyleType> struct Serialize;

// Serialization Invokers
template<typename StyleType> void serializationForCSS(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
{
    Serialize<StyleType>{}(builder, context, style, value);
}

template<typename StyleType> [[nodiscard]] String serializationForCSS(const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
{
    StringBuilder builder;
    serializationForCSS(builder, context, style, value);
    return builder.toString();
}

template<typename StyleType> void serializationForCSSOnOptionalLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
{
    if (!value)
        return;
    serializationForCSS(builder, context, style, *value);
}

template<typename StyleType> void serializationForCSSOnTupleLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename T>(const std::optional<T>& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, style, *element);
        },
        [&]<typename T>(const Markable<T>& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, style, *element);
        },
        [&](const auto& element) {
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, style, element);
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename StyleType> void serializationForCSSOnRangeLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        builder.append(std::exchange(swappedSeparator, separator));
        serializationForCSS(builder, context, style, element);
    }
}

template<typename StyleType> void serializationForCSSOnVariantLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
{
    WTF::switchOn(value, [&](const auto& alternative) { serializationForCSS(builder, context, style, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike StyleType> struct Serialize<StyleType> {
    void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const StyleType&)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike StyleType> struct Serialize<StyleType> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
    {
        serializationForCSSOnOptionalLike(builder, context, style, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> struct Serialize<StyleType> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
    {
        serializationForCSSOnTupleLike(builder, context, style, value, SerializationSeparatorString<StyleType>);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike StyleType> struct Serialize<StyleType> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
    {
        serializationForCSSOnRangeLike(builder, context, style, value, SerializationSeparatorString<StyleType>);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct Serialize<StyleType> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value)
    {
        serializationForCSSOnVariantLike(builder, context, style, value);
    }
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting StyleType> struct Serialize<StyleType> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle&, const StyleType& value)
    {
        CSS::serializationForCSS(builder, context, value);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename StyleType> struct Serialize<FunctionNotation<Name, StyleType>> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const FunctionNotation<Name, StyleType>& value)
    {
        builder.append(nameLiteralForSerialization(value.name), '(');
        serializationForCSS(builder, context, style, value.parameters);
        builder.append(')');
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedSize`.
template<typename CSSType> struct Serialize<MinimallySerializingSpaceSeparatedSize<CSSType>> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const MinimallySerializingSpaceSeparatedSize<CSSType>& value)
    {
        constexpr auto separator = SerializationSeparatorString<MinimallySerializingSpaceSeparatedSize<CSSType>>;

        if (get<0>(value) != get<1>(value)) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { get<0>(value), get<1>(value) }, separator);
            return;
        }
        serializationForCSS(builder, context, style, get<0>(value));
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedRectEdges`.
template<typename StyleType> struct Serialize<MinimallySerializingSpaceSeparatedRectEdges<StyleType>> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const MinimallySerializingSpaceSeparatedRectEdges<StyleType>& value)
    {
        constexpr auto separator = SerializationSeparatorString<MinimallySerializingSpaceSeparatedRectEdges<StyleType>>;

        if (value.left() != value.right()) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { value.top(), value.right(), value.bottom(), value.left() }, separator);
            return;
        }
        if (value.bottom() != value.top()) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { value.top(), value.right(), value.bottom() }, separator);
            return;
        }
        if (value.right() != value.top()) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { value.top(), value.right() }, separator);
            return;
        }
        serializationForCSS(builder, context, style, value.top());
    }
};

// MARK: - Evaluation

// Types that want to participate in evaluation overloading must specialize the following interface:
//
//    template<> struct WebCore::Style::Evaluation<StyleType> {
//        decltype(auto) operator()(const StyleType&, ...);
//    };

template<typename> struct Evaluation;

// `Evaluation` Invokers
template<typename StyleType> decltype(auto) evaluate(const StyleType& value)
{
    return Evaluation<StyleType>{}(value);
}

template<typename StyleType, typename Reference> decltype(auto) evaluate(const StyleType& value, Reference&& reference)
{
    return Evaluation<StyleType>{}(value, std::forward<Reference>(reference));
}

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct Evaluation<StyleType> {
    template<typename... Rest> decltype(auto) operator()(const StyleType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return evaluate(alternative, std::forward<Rest>(rest)...); });
    }
};

// Specialization for `TupleLike` (wrapper).
template<TupleLike StyleType> requires (std::tuple_size_v<StyleType> == 1) struct Evaluation<StyleType> {
    template<typename... Rest> decltype(auto) operator()(const StyleType& value, Rest&&... rest)
    {
        return evaluate(get<0>(value), std::forward<Rest>(rest)...);
    }
};

// MARK: - Blending

// All non-tuple-like leaf types must specialize `Blending` with the following member functions:
//
//    template<> struct WebCore::Style::Blending<StyleType> {
//        bool canBlend(const StyleType&, const StyleType&);
//        StyleType blend(const StyleType&, const StyleType&, const BlendingContext&);
//    };
//
// or, if a RenderStyle is needed for blending:
//
//    template<> struct WebCore::Style::Blending<StyleType> {
//        bool canBlend(const StyleType&, const StyleType&, const RenderStyle&, const RenderStyle&);
//        StyleType blend(const StyleType&, const StyleType&, const RenderStyle&, const RenderStyle&, const BlendingContext&);
//    };

template<typename> struct Blending;

// `CanBlend` Invoker
template<typename StyleType> auto canBlend(const StyleType& a, const StyleType& b) -> bool
{
    return Blending<StyleType>{}.canBlend(a, b);
}

template<typename StyleType> auto canBlend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    return Blending<StyleType>{}.canBlend(a, b, aStyle, bStyle);
}

// `Blend` Invoker
template<typename StyleType> auto blend(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
{
    return Blending<StyleType>{}.blend(a, b, context);
}

template<typename StyleType> auto blend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> StyleType
{
    return Blending<StyleType>{}.blend(a, b, aStyle, bStyle, context);
}

// Utilities for blending

template<typename StyleType> auto canBlendOnOptionalLike(const StyleType& a, const StyleType& b) -> bool
{
    if (a && b)
        return WebCore::Style::canBlend(*a, *b);
    return !a && !b;
}

template<typename StyleType> auto canBlendOnOptionalLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (a && b)
        return WebCore::Style::canBlend(*a, *b, aStyle, bStyle);
    return !a && !b;
}

template<typename StyleType> auto blendOnOptionalLike(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
{
    if (a && b)
        return WebCore::Style::blend(*a, *b, context);
    return std::nullopt;
}

template<typename StyleType> auto blendOnOptionalLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> StyleType
{
    if (a && b)
        return WebCore::Style::blend(*a, *b, aStyle, bStyle, context);
    return std::nullopt;
}

template<typename StyleType> auto canBlendOnTupleLike(const StyleType& a, const StyleType& b) -> bool
{
    return WTF::apply([&](const auto& ...pair) {
        return (WebCore::Style::canBlend(std::get<0>(pair), std::get<1>(pair)) && ...);
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto canBlendOnTupleLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    return WTF::apply([&](const auto& ...pair) {
        return (WebCore::Style::canBlend(std::get<0>(pair), std::get<1>(pair), aStyle, bStyle) && ...);
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto blendOnTupleLike(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
{
    return WTF::apply([&](const auto& ...pair) {
        return StyleType { WebCore::Style::blend(std::get<0>(pair), std::get<1>(pair), context)... };
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto blendOnTupleLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> StyleType
{
    return WTF::apply([&](const auto& ...pair) {
        return StyleType { WebCore::Style::blend(std::get<0>(pair), std::get<1>(pair), aStyle, bStyle, context)... };
    }, WTF::tuple_zip(a, b));
}

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike StyleType> struct Blending<StyleType> {
    constexpr auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return canBlendOnOptionalLike(a, b);
    }
    constexpr auto canBlend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return canBlendOnOptionalLike(a, b, aStyle, bStyle);
    }
    auto blend(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
    {
        return blendOnOptionalLike(a, b, context);
    }
    auto blend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> StyleType
    {
        return blendOnOptionalLike(a, b, aStyle, bStyle, context);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> struct Blending<StyleType> {
    constexpr auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return canBlendOnTupleLike(a, b);
    }
    constexpr auto canBlend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return canBlendOnTupleLike(a, b, aStyle, bStyle);
    }
    auto blend(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
    {
        return blendOnTupleLike(a, b, context);
    }
    auto blend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> StyleType
    {
        return blendOnTupleLike(a, b, aStyle, bStyle, context);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Blending<Constant<C>> {
    constexpr auto canBlend(const Constant<C>&, const Constant<C>&) -> bool
    {
        return true;
    }
    constexpr auto canBlend(const Constant<C>&, const Constant<C>&, const RenderStyle&, const RenderStyle&) -> bool
    {
        return true;
    }
    auto blend(const Constant<C>&, const Constant<C>&, const BlendingContext&) -> Constant<C>
    {
        return { };
    }
    auto blend(const Constant<C>&, const Constant<C>&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> Constant<C>
    {
        return { };
    }
};

// Specialization for `Variant`.
template<typename... StyleTypes> struct Blending<Variant<StyleTypes...>> {
    auto canBlend(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            []<typename T>(const T& a, const T& b) -> bool {
                return WebCore::Style::canBlend(a, b);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
    auto canBlend(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> bool {
                return WebCore::Style::canBlend(a, b, aStyle, bStyle);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
    auto blend(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const BlendingContext& context) -> Variant<StyleTypes...>
    {
        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> Variant<StyleTypes...> {
                return WebCore::Style::blend(a, b, context);
            },
            [](const auto&, const auto&) -> Variant<StyleTypes...> {
                RELEASE_ASSERT_NOT_REACHED();
            }
        ), a, b);
    }
    auto blend(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> Variant<StyleTypes...>
    {
        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> Variant<StyleTypes...> {
                return WebCore::Style::blend(a, b, aStyle, bStyle, context);
            },
            [](const auto&, const auto&) -> Variant<StyleTypes...> {
                RELEASE_ASSERT_NOT_REACHED();
            }
        ), a, b);
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct Blending<SpaceSeparatedVector<StyleType, inlineCapacity>> {
    auto canBlend(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::canBlend(a[i], b[i]))
                return false;
        }
        return true;
    }
    auto canBlend(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::canBlend(a[i], b[i], aStyle, bStyle))
                return false;
        }
        return true;
    }
    auto blend(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const BlendingContext& context) -> SpaceSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename SpaceSeparatedVector<StyleType, inlineCapacity>::Vector result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], context));
        return { WTFMove(result) };
    }
    auto blend(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> SpaceSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename SpaceSeparatedVector<StyleType, inlineCapacity>::Vector result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], aStyle, bStyle, context));
        return { WTFMove(result) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct Blending<CommaSeparatedVector<StyleType, inlineCapacity>> {
    auto canBlend(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::canBlend(a[i], b[i]))
                return false;
        }
        return true;
    }
    auto canBlend(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::canBlend(a[i], b[i], aStyle, bStyle))
                return false;
        }
        return true;
    }
    auto blend(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const BlendingContext& context) -> CommaSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename CommaSeparatedVector<StyleType, inlineCapacity>::Vector result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], context));
        return { WTFMove(result) };
    }
    auto blend(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> CommaSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename CommaSeparatedVector<StyleType, inlineCapacity>::Vector result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], aStyle, bStyle, context));
        return { WTFMove(result) };
    }
};

// MARK: - IsZero

// All leaf types that want to conform to IsZero must implement
// the following:
//
//    template<> struct WebCore::Style::IsZero<StyleType> {
//        bool operator()(const StyleType&);
//    };
//
// or have a member function such that the type matches the
// `HasIsZero` concept.

template<typename> struct IsZero;

// IsZero Invoker
template<typename T> bool isZero(const T& value)
{
    if constexpr (HasIsZero<T>)
        return value.isZero();
    else
        return IsZero<T>{}(value);
}

// Constrained for `TreatAsTupleLike`.
template<TupleLike T> struct IsZero<T> {
    bool operator()(const T& value)
    {
        return WTF::apply([&](const auto& ...x) { return (isZero(x) && ...); }, value);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike T> struct IsZero<T> {
    bool operator()(const T& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isZero(alternative); });
    }
};

// MARK: - IsEmpty

// All leaf types that want to conform to IsEmpty must implement
// the following:
//
//    template<> struct WebCore::Style::IsEmpty<StyleType> {
//        bool operator()(const StyleType&);
//    };
//
// or have a member function such that the type matches the
// `HasIsEmpty` concept.

template<typename> struct IsEmpty;

// IsEmpty Invoker
template<typename T> bool isEmpty(const T& value)
{
    if constexpr (HasIsEmpty<T>)
        return value.isEmpty();
    else
        return IsEmpty<T>{}(value);
}

// Specialization for `SpaceSeparatedSize`.
template<typename T> struct IsEmpty<SpaceSeparatedSize<T>> {
    bool operator()(const auto& value)
    {
        return isZero(value.width()) || isZero(value.height());
    }
};

// MARK: - Logging

template<typename StyleType> void logForCSSOnTupleLike(TextStream& ts, const StyleType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename T>(const std::optional<T>& element) {
            if (!element)
                return;
            ts << std::exchange(swappedSeparator, separator);
            ts << *element;
        },
        [&]<typename T>(const Markable<T>& element) {
            if (!element)
                return;
            ts << std::exchange(swappedSeparator, separator);
            ts << *element;
        },
        [&](const auto& element) {
            ts << std::exchange(swappedSeparator, separator);
            ts << element;
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename StyleType> void logForCSSOnRangeLike(TextStream& ts, const StyleType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        ts << std::exchange(swappedSeparator, separator);
        ts << element;
    }
}

template<typename StyleType> void logForCSSOnVariantLike(TextStream& ts, const StyleType& value)
{
    WTF::switchOn(value, [&](const auto& value) { ts << value; });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike StyleType> TextStream& operator<<(TextStream& ts, const StyleType&)
{
    return ts;
}

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> TextStream& operator<<(TextStream& ts, const StyleType& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<StyleType>);
    return ts;
}

// Constrained for `TreatAsRangeLike`.
template<RangeLike StyleType> TextStream& operator<<(TextStream& ts, const StyleType& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<StyleType>);
    return ts;
}

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> TextStream& operator<<(TextStream& ts, const StyleType& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

} // namespace Style
} // namespace WebCore
