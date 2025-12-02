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

#include <WebCore/CSSCalcSymbolTable.h>
#include <WebCore/CSSNoConversionDataRequiredToken.h>
#include <WebCore/CSSValueTypes.h>
#include <optional>
#include <tuple>
#include <utility>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSToLengthConversionData;
class Element;
class RenderStyle;
struct BlendingContext;
enum class CompositeOperation : uint8_t;

namespace Style {

namespace Interpolation {
struct Context;
}

class BuilderState;

// MARK: - ValueRepresentation

// All leaf types that want to conform to ValueRepresentation must implement
// the following:
//
//    template<> struct WebCore::Style::ValueRepresentation<StyleType> {
//        template<typename... F> bool operator()(const StyleType&, F&&... f);
//    };

template<typename> struct ValueRepresentation;

struct ValueRepresentationInvoker {
    template<typename StyleType, typename... F> decltype(auto) operator()(const StyleType& value, F&&... f) const
    {
        return ValueRepresentation<StyleType>{}(value, std::forward<F>(f)...);
    }
};
inline constexpr ValueRepresentationInvoker valueRepresentation{};

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

// Specialize `TreatAsNonConverting` for `PropertyIdentifier`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<PropertyIdentifier> = true;

// Specialize `TreatAsNonConverting` for `WTF::AtomString`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<WTF::AtomString> = true;

// Specialize `TreatAsNonConverting` for `WTF::String`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<WTF::String> = true;

// Specialize `TreatAsNonConverting` for `WTF::URL`, to indicate that its type does not change from the CSS representation.
template<> inline constexpr bool TreatAsNonConverting<WTF::URL> = true;


// MARK: - Conversion from "Style to "CSS"

struct ToCSSInvoker {
    template<typename StyleType, typename... Rest> decltype(auto) operator()(const StyleType& styleType, const RenderStyle& style, Rest&&... rest) const
    {
        return ToCSS<StyleType>{}(styleType, style, std::forward<Rest>(rest)...);
    }
};
inline constexpr ToCSSInvoker toCSS{};

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
template<typename T> struct ToCSSMapping<CommaSeparatedRectEdges<T>> { using type = CommaSeparatedRectEdges<CSSType<T>>; };
template<typename T> struct ToCSSMapping<MinimallySerializingSpaceSeparatedPair<T>> { using type = MinimallySerializingSpaceSeparatedPair<CSSType<T>>; };
template<typename T> struct ToCSSMapping<MinimallySerializingSpaceSeparatedPoint<T>> { using type = MinimallySerializingSpaceSeparatedPoint<CSSType<T>>; };
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
    template<typename... Rest> constexpr StyleType operator()(const StyleType& value, const RenderStyle&, Rest&&...)
    {
        return value;
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike StyleType> struct ToCSS<StyleType> {
    using Result = typename ToCSSMapping<StyleType>::type;

    template<typename... Rest> Result operator()(const StyleType& value, const RenderStyle& style, Rest&&... rest)
    {
        if (value)
            return toCSS(*value, style, std::forward<Rest>(rest)...);
        return std::nullopt;
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> struct ToCSS<StyleType> {
    using Result = typename ToCSSMapping<StyleType>::type;

    template<typename... Rest> Result operator()(const StyleType& value, const RenderStyle& style, Rest&&... rest)
    {
        return toCSSOnTupleLike<Result>(value, style, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct ToCSS<StyleType> {
    using Result = typename ToCSSMapping<StyleType>::type;

    template<typename... Rest> Result operator()(const StyleType& value, const RenderStyle& style, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return Result { toCSS(alternative, style, std::forward<Rest>(rest)...) }; });
    }
};

// Specialization for `SpaceSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct ToCSS<SpaceSeparatedVector<StyleType, inlineCapacity>> {
    using Result = SpaceSeparatedVector<CSSType<StyleType>, inlineCapacity>;

    template<typename... Rest> Result operator()(const SpaceSeparatedVector<StyleType, inlineCapacity>& value, const RenderStyle& style, Rest&&... rest)
    {
        return Result { value.value.template map<typename Result::Container>([&](const auto& x) { return toCSS(x, style, rest...); }) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct ToCSS<CommaSeparatedVector<StyleType, inlineCapacity>> {
    using Result = CommaSeparatedVector<CSSType<StyleType>, inlineCapacity>;

    template<typename... Rest> Result operator()(const CommaSeparatedVector<StyleType, inlineCapacity>& value, const RenderStyle& style, Rest&&... rest)
    {
        return Result { value.value.template map<typename Result::Container>([&](const auto& x) { return toCSS(x, style, rest...); }) };
    }
};

// MARK: - Conversion from "CSS" to "Style"

struct ToStyleInvoker {
    template<typename CSSType, typename... Rest> decltype(auto) operator()(const CSSType& cssType, const CSSToLengthConversionData& conversionData, Rest&&... rest) const
    {
        return ToStyle<CSSType>{}(cssType, conversionData, std::forward<Rest>(rest)...);
    }

    template<typename CSSType, typename... Rest> decltype(auto) operator()(const CSSType& cssType, const BuilderState& builderState, Rest&&... rest) const
    {
        return ToStyle<CSSType>{}(cssType, builderState, std::forward<Rest>(rest)...);
    }

    template<typename CSSType, typename... Rest> decltype(auto) operator()(const CSSType& cssType, NoConversionDataRequiredToken token, Rest&&... rest) const
    {
        return ToStyle<CSSType>{}(cssType, token, std::forward<Rest>(rest)...);
    }
};
inline constexpr ToStyleInvoker toStyle{};

// Convenience invoker that adds a `NoConversionDataRequiredToken` argument.
struct ToStyleNoConversionDataRequiredInvoker {
    template<typename CSSType, typename... Rest> decltype(auto) operator()(const CSSType& cssType, Rest&&... rest) const
    {
        return WebCore::Style::toStyle(cssType, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
    }
};
inline constexpr ToStyleNoConversionDataRequiredInvoker toStyleNoConversionDataRequired{};

// Conversion Utilities
template<typename To, typename From, typename... Rest> auto toStyleOnTupleLike(const From& tupleLike, Rest&&... rest) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toStyle(x, rest...)... }; }, tupleLike);
}

template<typename To, typename From, typename... Rest> auto toStyleNoConversionDataRequiredOnTupleLike(const From& tupleLike, Rest&&... rest) -> To
{
    return WTF::apply([&](const auto& ...x) { return To { toStyleNoConversionDataRequired(x, rest...)... }; }, tupleLike);
}

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
template<typename T> struct ToStyleMapping<CommaSeparatedRectEdges<T>> { using type = CommaSeparatedRectEdges<StyleType<T>>; };
template<typename T> struct ToStyleMapping<MinimallySerializingSpaceSeparatedPair<T>> { using type = MinimallySerializingSpaceSeparatedPair<StyleType<T>>; };
template<typename T> struct ToStyleMapping<MinimallySerializingSpaceSeparatedPoint<T>> { using type = MinimallySerializingSpaceSeparatedPoint<StyleType<T>>; };
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
        return Result { value.value.template map<typename Result::Container>([&](const auto& x) { return toStyle(x, rest...); }) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename CSSType, size_t inlineCapacity> struct ToStyle<CommaSeparatedVector<CSSType, inlineCapacity>> {
    using Result = CommaSeparatedVector<StyleType<CSSType>, inlineCapacity>;

    template<typename... Rest> Result operator()(const CommaSeparatedVector<CSSType, inlineCapacity>& value, Rest&&... rest)
    {
        return Result { value.value.template map<typename Result::Container>([&](const auto& x) { return toStyle(x, rest...); }) };
    }
};

// MARK: - Conversion directly from "Style to "Ref<CSSValue>"

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::CSSValueCreation<StyleType> {
//        Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const StyleType&);
//    };

template<typename StyleType> struct CSSValueCreation;

struct CSSValueCreationInvoker {
    template<typename StyleType, typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value, Rest&&... rest) const
    {
        return CSSValueCreation<StyleType>{}(pool, style, value, std::forward<Rest>(rest)...);
    }
};
inline constexpr CSSValueCreationInvoker createCSSValue{};

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct CSSValueCreation<StyleType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) -> Ref<CSSValue> { return createCSSValue(pool, style, alternative, std::forward<Rest>(rest)...); });
    }
};

// Constrained for `TreatAsTupleLike`
template<TupleLike StyleType> struct CSSValueCreation<StyleType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        if constexpr (std::tuple_size_v<StyleType> == 1 && SerializationSeparator<StyleType> == SerializationSeparatorType::None) {
            return createCSSValue(pool, style, get<0>(value), std::forward<Rest>(rest)...);
        } else if constexpr (std::tuple_size_v<StyleType> == 2 && SerializationCoalescing<StyleType> == SerializationCoalescingType::Minimal) {
            return CSS::makeCoalescingPairCSSValue<SerializationSeparator<StyleType>>(createCSSValue(pool, style, get<0>(value), rest...), createCSSValue(pool, style, get<1>(value), rest...));
        } else if constexpr (std::tuple_size_v<StyleType> == 4 && SerializationCoalescing<StyleType> == SerializationCoalescingType::Minimal) {
            return CSS::makeCoalescingQuadCSSValue<SerializationSeparator<StyleType>>(createCSSValue(pool, style, get<0>(value), rest...), createCSSValue(pool, style, get<1>(value), rest...), createCSSValue(pool, style, get<2>(value), rest...), createCSSValue(pool, style, get<3>(value), rest...));
        } else {
            CSSValueListBuilder list;

            auto caller = WTF::makeVisitor(
                [&]<OptionalLike T>(const T& element) {
                    if (!element)
                        return;
                    list.append(createCSSValue(pool, style, *element, rest...));
                },
                [&](const auto& element) {
                    list.append(createCSSValue(pool, style, element, rest...));
                }
            );
            WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);

            return CSS::makeListCSSValue<SerializationSeparator<StyleType>>(WTFMove(list));
        }
    }
};

// Constrained for `TreatAsRangeLike`
template<RangeLike StyleType> struct CSSValueCreation<StyleType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        CSSValueListBuilder list;
        for (const auto& element : value)
            list.append(createCSSValue(pool, style, element, rest...));

        return CSS::makeListCSSValue<SerializationSeparator<StyleType>>(WTFMove(list));
    }
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting StyleType> struct CSSValueCreation<StyleType> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle&, const StyleType& value, Rest&&... rest)
    {
        return CSS::createCSSValue(pool, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename StyleType> struct CSSValueCreation<FunctionNotation<Name, StyleType>> {
    template<typename... Rest> Ref<CSSValue> operator()(CSSValuePool& pool, const RenderStyle& style, const FunctionNotation<Name, StyleType>& value, Rest&&... rest)
    {
        return CSS::makeFunctionCSSValue(value.name, createCSSValue(pool, style, value.parameters, std::forward<Rest>(rest)...));
    }
};

// MARK: - Conversion directly from "Ref<CSSValue>" to "Style"

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::CSSValueConversion<StyleType> {
//                   StyleType operator()(BuilderState&, const CSSValue&);
//        [optional] StyleType operator()(BuilderState&, [std::derived_from<CSSValue>]);
//        [optional] StyleType operator()(const CSSToLengthConversionData&, [std::derived_from<CSSValue>]);
//    };

template<typename StyleType> struct CSSValueConversion;

template<typename StyleType> struct CSSValueConversionInvoker {
    template<typename... Rest> StyleType operator()(BuilderState& builderState, const CSSValue& value, Rest&&... rest) const
    {
        return CSSValueConversion<StyleType>{}(builderState, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> StyleType operator()(BuilderState& builderState, const CSSPrimitiveValue& value, Rest&&... rest) const
    {
        return CSSValueConversion<StyleType>{}(builderState, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> StyleType operator()(BuilderState& builderState, std::derived_from<CSSValue> auto const& value, Rest&&... rest) const
    {
        return CSSValueConversion<StyleType>{}(builderState, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> StyleType operator()(const CSSToLengthConversionData& conversionData, const CSSValue& value, Rest&&... rest) const
    {
        return CSSValueConversion<StyleType>{}(conversionData, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> StyleType operator()(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value, Rest&&... rest) const
    {
        return CSSValueConversion<StyleType>{}(conversionData, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> StyleType operator()(const CSSToLengthConversionData& conversionData, std::derived_from<CSSValue> auto const& value, Rest&&... rest) const
    {
        return CSSValueConversion<StyleType>{}(conversionData, value, std::forward<Rest>(rest)...);
    }
};
template<typename StyleType> inline constexpr CSSValueConversionInvoker<StyleType> toStyleFromCSSValue{};

// MARK: - Conversion directly from "Ref<CSSValue>" to "Style" when lacking BuilderState or CSSToLengthConversionData. Should not be used for new code and should be phased out.

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::DeprecatedCSSValueConversion<StyleType> {
//                   std::optional<StyleType> operator()(const RefPtr<Element>&&, const CSSValue&);
//                   std::optional<StyleType> operator()(const RefPtr<Element>&&, const CSSPrimitiveValue&);
//        [optional] std::optional<StyleType> operator()(const RefPtr<Element>&&, [std::derived_from<CSSValue>]);
//    };

template<typename StyleType> struct DeprecatedCSSValueConversion;

template<typename StyleType> struct DeprecatedCSSValueConversionInvoker {
    template<typename... Rest> std::optional<StyleType> operator()(const RefPtr<Element>& element, const CSSValue& value, Rest&&... rest) const
    {
        return DeprecatedCSSValueConversion<StyleType>{}(element, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> std::optional<StyleType> operator()(const RefPtr<Element>& element, const CSSPrimitiveValue& value, Rest&&... rest) const
    {
        return DeprecatedCSSValueConversion<StyleType>{}(element, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> std::optional<StyleType> operator()(const RefPtr<Element>& element, std::derived_from<CSSValue> auto const& value, Rest&&... rest) const
    {
        return DeprecatedCSSValueConversion<StyleType>{}(element, value, std::forward<Rest>(rest)...);
    }
};
template<typename StyleType> inline constexpr DeprecatedCSSValueConversionInvoker<StyleType> deprecatedToStyleFromCSSValue{};

// MARK: - Conversion directly from "Style" to "Platform"

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::ToPlatform<StyleType> {
//        PlatformType operator()(const StyleType&);
//    };

template<typename StyleType> struct ToPlatform;

struct ToPlatformInvoker {
    template<typename StyleType, typename... Rest> decltype(auto) operator()(const StyleType& value, Rest&&... rest) const
    {
        return ToPlatform<StyleType>{}(value, std::forward<Rest>(rest)...);
    }
};
inline constexpr ToPlatformInvoker toPlatform{};

// MARK: - Serialization

// All leaf types must implement the following:
//
//    template<> struct WebCore::Style::Serialize<StyleType> {
//        void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const StyleType&);
//    };

template<typename StyleType> struct Serialize;

struct SerializeInvoker {
    template<typename StyleType, typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest) const
    {
        Serialize<StyleType>{}(builder, context, style, value, std::forward<Rest>(rest)...);
    }

    template<typename StyleType, typename... Rest> [[nodiscard]] String operator()(const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest) const
    {
        StringBuilder builder;
        this->operator()(builder, context, style, value, std::forward<Rest>(rest)...);
        return builder.toString();
    }
};
inline constexpr SerializeInvoker serializationForCSS{};

template<typename StyleType, typename... Rest> void serializationForCSSOnOptionalLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest)
{
    if (!value)
        return;
    serializationForCSS(builder, context, style, *value, std::forward<Rest>(rest)...);
}

template<typename StyleType, typename... Rest> void serializationForCSSOnTupleLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, ASCIILiteral separator, Rest&&... rest)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<OptionalLike T>(const T& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, style, *element, rest...);
        },
        [&](const auto& element) {
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, style, element, rest...);
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename StyleType, typename... Rest> void serializationForCSSOnTupleLikeCoalescing(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, ASCIILiteral separator, Rest&&... rest)
{
    if constexpr (std::tuple_size_v<StyleType> == 2) {
        if (get<0>(value) != get<1>(value)) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { get<0>(value), get<1>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        serializationForCSS(builder, context, style, get<0>(value), std::forward<Rest>(rest)...);
    } else if constexpr (std::tuple_size_v<StyleType> == 4) {
        if (get<3>(value) != get<1>(value)) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { get<0>(value), get<1>(value), get<2>(value), get<3>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        if (get<2>(value) != get<0>(value)) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { get<0>(value), get<1>(value), get<2>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        if (get<1>(value) != get<0>(value)) {
            serializationForCSSOnTupleLike(builder, context, style, std::tuple { get<0>(value), get<1>(value) }, separator, std::forward<Rest>(rest)...);
            return;
        }
        serializationForCSS(builder, context, style, get<0>(value), std::forward<Rest>(rest)...);
    }
}

template<typename StyleType, typename... Rest> void serializationForCSSOnRangeLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, ASCIILiteral separator, Rest&&... rest)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        builder.append(std::exchange(swappedSeparator, separator));
        serializationForCSS(builder, context, style, element, rest...);
    }
}

template<typename StyleType, typename... Rest> void serializationForCSSOnVariantLike(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest)
{
    WTF::switchOn(value, [&](const auto& alternative) { serializationForCSS(builder, context, style, alternative, std::forward<Rest>(rest)...); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const StyleType&, Rest&&...)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        serializationForCSSOnOptionalLike(builder, context, style, value, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        if constexpr (SerializationCoalescing<StyleType> == SerializationCoalescingType::Minimal)
            serializationForCSSOnTupleLikeCoalescing(builder, context, style, value, SerializationSeparatorString<StyleType>, std::forward<Rest>(rest)...);
        else
            serializationForCSSOnTupleLike(builder, context, style, value, SerializationSeparatorString<StyleType>, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        serializationForCSSOnRangeLike(builder, context, style, value, SerializationSeparatorString<StyleType>, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const StyleType& value, Rest&&... rest)
    {
        serializationForCSSOnVariantLike(builder, context, style, value, std::forward<Rest>(rest)...);
    }
};

// Constrained for `TreatAsNonConverting`.
template<NonConverting StyleType> struct Serialize<StyleType> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle&, const StyleType& value, Rest&&... rest)
    {
        CSS::serializationForCSS(builder, context, value, std::forward<Rest>(rest)...);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename StyleType> struct Serialize<FunctionNotation<Name, StyleType>> {
    template<typename... Rest> void operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const FunctionNotation<Name, StyleType>& value, Rest&&... rest)
    {
        builder.append(nameLiteralForSerialization(value.name), '(');
        serializationForCSS(builder, context, style, value.parameters, std::forward<Rest>(rest)...);
        builder.append(')');
    }
};

// MARK: - Evaluation

// Types that want to participate in evaluation overloading must specialize the following interface:
//
//    template<> struct WebCore::Style::Evaluation<StyleType> {
//        decltype(auto) operator()(const StyleType&, ...);
//    };

template<typename, typename> struct Evaluation;

template<typename StyleType, typename Result, typename T1> concept HasTwoParameterEvaluate = requires {
    Evaluation<StyleType, Result> { }(std::declval<const StyleType&>(), std::declval<T1>());
};

template<typename StyleType, typename Result, typename T1, typename T2> concept HasThreeParameterEvaluate = requires {
    Evaluation<StyleType, Result> { }(std::declval<const StyleType&>(), std::declval<T1>(), std::declval<T2>());
};

template<typename Result> struct EvaluationInvoker {
    template<typename StyleType> Result operator()(const StyleType& value) const
    {
        return Evaluation<StyleType, Result> { }(value);
    }

    template<typename StyleType, typename T1> Result operator()(const StyleType& value, T1&& t1) const
    {
        if constexpr (HasTwoParameterEvaluate<StyleType, Result, T1>)
            return Evaluation<StyleType, Result> { }(value, std::forward<T1>(t1));
        else
            return operator()(value);
    }

    template<typename StyleType, typename T1, typename T2> Result operator()(const StyleType& value, T1&& t1, T2&& t2) const
    {
        if constexpr (HasThreeParameterEvaluate<StyleType, Result, T1, T2>)
            return Evaluation<StyleType, Result> { }(value, std::forward<T1>(t1), std::forward<T2>(t2));
        else
            return operator()(value, std::forward<T1>(t1));
    }
};
template<typename Result> inline constexpr EvaluationInvoker<Result> evaluate{};

// Constrained for `TreatAsVariantLike`.
template<VariantLike StyleType, typename Result> struct Evaluation<StyleType, Result> {
    template<typename... Rest> Result operator()(const StyleType& value, Rest&&... rest)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return evaluate<Result>(alternative, std::forward<Rest>(rest)...); });
    }
};

// Specialization for `TupleLike` (wrapper).
template<TupleLike StyleType, typename Result> requires (std::tuple_size_v<StyleType> == 1) struct Evaluation<StyleType, Result> {
    template<typename... Rest> Result operator()(const StyleType& value, Rest&&... rest)
    {
        return evaluate<Result>(get<0>(value), std::forward<Rest>(rest)...);
    }
};

// MARK: - Blending

// All non-tuple-like leaf types must specialize `Blending` with the following member functions:
//
//    template<> struct WebCore::Style::Blending<StyleType> {
//        [optional default=(a==b)] bool equals(const StyleType&, const StyleType&);
//        [optional default=true] bool canBlend(const StyleType&, const StyleType&);
//        [optional default=false] bool requiresInterpolationForAccumulativeIteration(const StyleType&, const StyleType&);
//        StyleType blend(const StyleType&, const StyleType&, const [BlendingContext|Interpolation::Context]&);
//    };
//
// or, if a RenderStyle is needed for blending:
//
//    template<> struct WebCore::Style::Blending<StyleType> {
//        [optional default=(a==b)] bool equals(const StyleType&, const StyleType&, const RenderStyle&, const RenderStyle&);
//        [optional default=true] bool canBlend(const StyleType&, const StyleType&, const RenderStyle&, const RenderStyle&);
//        [optional default=false] bool requiresInterpolationForAccumulativeIteration(const StyleType&, const StyleType&, const RenderStyle&, const RenderStyle&);
//        StyleType blend(const StyleType&, const StyleType&, const RenderStyle&, const RenderStyle&, const [BlendingContext|Interpolation::Context]&);
//    };

template<typename> struct Blending;

template<typename StyleType> concept HasEqualsForBlendingWithoutRenderStyle = requires {
    { Blending<StyleType>{}.equals(std::declval<const StyleType&>(), std::declval<const StyleType&>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasEqualsForBlendingWithRenderStyle = requires {
    { Blending<StyleType>{}.equals(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const RenderStyle&>(), std::declval<const RenderStyle&>()) } -> std::same_as<bool>;
};

template<typename StyleType> concept HasCanBlendWithoutRenderStyleAndWithoutCompositeOperation = requires {
    { Blending<StyleType>{}.canBlend(std::declval<const StyleType&>(), std::declval<const StyleType&>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasCanBlendWithRenderStyleAndWithoutCompositeOperation = requires {
    { Blending<StyleType>{}.canBlend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const RenderStyle&>(), std::declval<const RenderStyle&>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasCanBlendWithoutRenderStyleAndWithCompositeOperation = requires {
    { Blending<StyleType>{}.canBlend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<CompositeOperation>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasCanBlendWithRenderStyleAndWithCompositeOperation = requires {
    { Blending<StyleType>{}.canBlend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const RenderStyle&>(), std::declval<const RenderStyle&>(), std::declval<CompositeOperation>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasRequiresInterpolationForAccumulativeIterationWithoutRenderStyle = requires {
    { Blending<StyleType>{}.requiresInterpolationForAccumulativeIteration(std::declval<const StyleType&>(), std::declval<const StyleType&>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasRequiresInterpolationForAccumulativeIterationWithRenderStyle = requires {
    { Blending<StyleType>{}.requiresInterpolationForAccumulativeIteration(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const RenderStyle&>(), std::declval<const RenderStyle&>()) } -> std::same_as<bool>;
};
template<typename StyleType> concept HasBlendWithoutRenderStyleAndWithInterpolationContext = requires {
    { Blending<StyleType>{}.blend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const Interpolation::Context&>()) } -> std::same_as<StyleType>;
};
template<typename StyleType> concept HasBlendWithoutRenderStyleAndWithBlendingContext = requires {
    { Blending<StyleType>{}.blend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const BlendingContext&>()) } -> std::same_as<StyleType>;
};
template<typename StyleType> concept HasBlendWithRenderStyleAndWithInterpolationContext = requires {
    { Blending<StyleType>{}.blend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const RenderStyle&>(), std::declval<const RenderStyle&>(), std::declval<const Interpolation::Context&>()) } -> std::same_as<StyleType>;
};
template<typename StyleType> concept HasBlendWithRenderStyleAndWithBlendingContext = requires {
    { Blending<StyleType>{}.blend(std::declval<const StyleType&>(), std::declval<const StyleType&>(), std::declval<const RenderStyle&>(), std::declval<const RenderStyle&>(), std::declval<const BlendingContext&>()) } -> std::same_as<StyleType>;
};

struct EqualsForBlendingInvoker {
    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b) const -> bool
    {
        if constexpr (HasEqualsForBlendingWithoutRenderStyle<StyleType>)
            return Blending<StyleType>{}.equals(a, b);
        else
            return a == b;
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) const -> bool
    {
        if constexpr (HasEqualsForBlendingWithRenderStyle<StyleType>)
            return Blending<StyleType>{}.equals(a, b, aStyle, bStyle);
        else
            return this->operator()(a, b);
    }
};
inline constexpr EqualsForBlendingInvoker equalsForBlending{};

struct CanBlendInvoker {
    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b) const -> bool
    {
        if constexpr (HasCanBlendWithoutRenderStyleAndWithoutCompositeOperation<StyleType>)
            return Blending<StyleType>{}.canBlend(a, b);
        else
            return true;
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, CompositeOperation compositeOperation) const -> bool
    {
        if constexpr (HasCanBlendWithoutRenderStyleAndWithCompositeOperation<StyleType>)
            return Blending<StyleType>{}.canBlend(a, b, compositeOperation);
        else
            return this->operator()(a, b);
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) const -> bool
    {
        if constexpr (HasCanBlendWithRenderStyleAndWithoutCompositeOperation<StyleType>)
            return Blending<StyleType>{}.canBlend(a, b, aStyle, bStyle);
        else
            return this->operator()(a, b);
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, CompositeOperation compositeOperation) const -> bool
    {
        if constexpr (HasCanBlendWithRenderStyleAndWithCompositeOperation<StyleType>)
            return Blending<StyleType>{}.canBlend(a, b, aStyle, bStyle, compositeOperation);
        else if constexpr (HasCanBlendWithRenderStyleAndWithoutCompositeOperation<StyleType>)
            return Blending<StyleType>{}.canBlend(a, b, aStyle, bStyle);
        else if constexpr (HasCanBlendWithoutRenderStyleAndWithCompositeOperation<StyleType>)
            return Blending<StyleType>{}.canBlend(a, b, compositeOperation);
        else
            return this->operator()(a, b);
    }
};
inline constexpr CanBlendInvoker canBlend{};

struct RequiresInterpolationForAccumulativeIterationInvoker {
    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b) const -> bool
    {
        if constexpr (HasRequiresInterpolationForAccumulativeIterationWithoutRenderStyle<StyleType>)
            return Blending<StyleType>{}.requiresInterpolationForAccumulativeIteration(a, b);
        else
            return false;
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) const -> bool
    {
        if constexpr (HasRequiresInterpolationForAccumulativeIterationWithRenderStyle<StyleType>)
            return Blending<StyleType>{}.requiresInterpolationForAccumulativeIteration(a, b, aStyle, bStyle);
        else
            return this->operator()(a, b);
    }
};
inline constexpr RequiresInterpolationForAccumulativeIterationInvoker requiresInterpolationForAccumulativeIteration{};

struct BlendInvoker {
    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const Interpolation::Context& context) const -> StyleType
    {
        return Blending<StyleType>{}.blend(a, b, context);
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const BlendingContext& context) const -> StyleType
    {
        return Blending<StyleType>{}.blend(a, b, context);
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const Interpolation::Context& context) const -> StyleType
    {
        if constexpr (HasBlendWithRenderStyleAndWithInterpolationContext<StyleType> || HasBlendWithRenderStyleAndWithBlendingContext<StyleType>)
            return Blending<StyleType>{}.blend(a, b, aStyle, bStyle, context);
        else
            return this->operator()(a, b, context);
    }

    template<typename StyleType> auto operator()(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) const -> StyleType
    {
        if constexpr (HasBlendWithRenderStyleAndWithBlendingContext<StyleType>)
            return Blending<StyleType>{}.blend(a, b, aStyle, bStyle, context);
        else
            return this->operator()(a, b, context);
    }
};
inline constexpr BlendInvoker blend{};

// Utilities for blending

template<typename StyleType> auto equalsForBlendingOnOptionalLike(const StyleType& a, const StyleType& b) -> bool
{
    if (a && b)
        return WebCore::Style::equalsForBlending(*a, *b);
    return !a && !b;
}

template<typename StyleType> auto equalsForBlendingOnOptionalLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (a && b)
        return WebCore::Style::equalsForBlending(*a, *b, aStyle, bStyle);
    return !a && !b;
}

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

template<typename StyleType> auto requiresInterpolationForAccumulativeIterationOnOptionalLike(const StyleType& a, const StyleType& b) -> bool
{
    if (a && b)
        return WebCore::Style::requiresInterpolationForAccumulativeIteration(*a, *b);
    return false;
}

template<typename StyleType> auto requiresInterpolationForAccumulativeIterationOnOptionalLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (a && b)
        return WebCore::Style::requiresInterpolationForAccumulativeIteration(*a, *b, aStyle, bStyle);
    return false;
}

template<typename StyleType> auto blendOnOptionalLike(const StyleType& a, const StyleType& b, const auto& context) -> StyleType
{
    if (a && b)
        return WebCore::Style::blend(*a, *b, context);
    return std::nullopt;
}

template<typename StyleType> auto blendOnOptionalLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> StyleType
{
    if (a && b)
        return WebCore::Style::blend(*a, *b, aStyle, bStyle, context);
    return std::nullopt;
}

template<typename StyleType> auto equalsForBlendingOnTupleLike(const StyleType& a, const StyleType& b) -> bool
{
    return WTF::apply([&](const auto& ...pair) {
        return (WebCore::Style::equalsForBlending(std::get<0>(pair), std::get<1>(pair)) && ...);
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto equalsForBlendingOnTupleLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    return WTF::apply([&](const auto& ...pair) {
        return (WebCore::Style::equalsForBlending(std::get<0>(pair), std::get<1>(pair), aStyle, bStyle) && ...);
    }, WTF::tuple_zip(a, b));
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

template<typename StyleType> auto requiresInterpolationForAccumulativeIterationOnTupleLike(const StyleType& a, const StyleType& b) -> bool
{
    return WTF::apply([&](const auto& ...pair) {
        return (WebCore::Style::requiresInterpolationForAccumulativeIteration(std::get<0>(pair), std::get<1>(pair)) || ...);
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto requiresInterpolationForAccumulativeIterationOnTupleLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    return WTF::apply([&](const auto& ...pair) {
        return (WebCore::Style::requiresInterpolationForAccumulativeIteration(std::get<0>(pair), std::get<1>(pair), aStyle, bStyle) || ...);
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto blendOnTupleLike(const StyleType& a, const StyleType& b, const auto& context) -> StyleType
{
    return WTF::apply([&](const auto& ...pair) {
        return StyleType { WebCore::Style::blend(std::get<0>(pair), std::get<1>(pair), context)... };
    }, WTF::tuple_zip(a, b));
}

template<typename StyleType> auto blendOnTupleLike(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> StyleType
{
    return WTF::apply([&](const auto& ...pair) {
        return StyleType { WebCore::Style::blend(std::get<0>(pair), std::get<1>(pair), aStyle, bStyle, context)... };
    }, WTF::tuple_zip(a, b));
}

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike StyleType> struct Blending<StyleType> {
    constexpr auto equals(const StyleType& a, const StyleType& b) -> bool
    {
        return equalsForBlendingOnOptionalLike(a, b);
    }
    constexpr auto equals(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return equalsForBlendingOnOptionalLike(a, b, aStyle, bStyle);
    }
    constexpr auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return canBlendOnOptionalLike(a, b);
    }
    constexpr auto canBlend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return canBlendOnOptionalLike(a, b, aStyle, bStyle);
    }
    constexpr auto requiresInterpolationForAccumulativeIteration(const StyleType& a, const StyleType& b) -> bool
    {
        return requiresInterpolationForAccumulativeIterationOnOptionalLike(a, b);
    }
    constexpr auto requiresInterpolationForAccumulativeIteration(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return requiresInterpolationForAccumulativeIterationOnOptionalLike(a, b, aStyle, bStyle);
    }
    auto blend(const StyleType& a, const StyleType& b, const auto& context) -> StyleType
    {
        return blendOnOptionalLike(a, b, context);
    }
    auto blend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> StyleType
    {
        return blendOnOptionalLike(a, b, aStyle, bStyle, context);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike StyleType> struct Blending<StyleType> {
    constexpr auto equals(const StyleType& a, const StyleType& b) -> bool
    {
        return equalsForBlendingOnTupleLike(a, b);
    }
    constexpr auto equals(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return equalsForBlendingOnTupleLike(a, b, aStyle, bStyle);
    }
    constexpr auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return canBlendOnTupleLike(a, b);
    }
    constexpr auto canBlend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return canBlendOnTupleLike(a, b, aStyle, bStyle);
    }
    constexpr auto requiresInterpolationForAccumulativeIteration(const StyleType& a, const StyleType& b) -> bool
    {
        return requiresInterpolationForAccumulativeIterationOnTupleLike(a, b);
    }
    constexpr auto requiresInterpolationForAccumulativeIteration(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return requiresInterpolationForAccumulativeIterationOnTupleLike(a, b, aStyle, bStyle);
    }
    auto blend(const StyleType& a, const StyleType& b, const auto& context) -> StyleType
    {
        return blendOnTupleLike(a, b, context);
    }
    auto blend(const StyleType& a, const StyleType& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> StyleType
    {
        return blendOnTupleLike(a, b, aStyle, bStyle, context);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Blending<Constant<C>> {
    auto blend(const Constant<C>&, const Constant<C>&, const auto&) -> Constant<C>
    {
        return { };
    }
    auto blend(const Constant<C>&, const Constant<C>&, const RenderStyle&, const RenderStyle&, const auto&) -> Constant<C>
    {
        return { };
    }
};

// Specialization for `Variant`.
template<typename... StyleTypes> struct Blending<Variant<StyleTypes...>> {
    auto equals(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            []<typename T>(const T& a, const T& b) -> bool {
                return WebCore::Style::equalsForBlending(a, b);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
    auto equals(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> bool {
                return WebCore::Style::equalsForBlending(a, b, aStyle, bStyle);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
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
    auto requiresInterpolationForAccumulativeIteration(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            []<typename T>(const T& a, const T& b) -> bool {
                return WebCore::Style::requiresInterpolationForAccumulativeIteration(a, b);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
    auto requiresInterpolationForAccumulativeIteration(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> bool {
                return WebCore::Style::requiresInterpolationForAccumulativeIteration(a, b, aStyle, bStyle);
            },
            [](const auto&, const auto&) -> bool {
                return false;
            }
        ), a, b);
    }
    auto blend(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const auto& context) -> Variant<StyleTypes...>
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
    auto blend(const Variant<StyleTypes...>& a, const Variant<StyleTypes...>& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> Variant<StyleTypes...>
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
    auto equals(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::equalsForBlending(a[i], b[i]))
                return false;
        }
        return true;
    }
    auto equals(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::equalsForBlending(a[i], b[i], aStyle, bStyle))
                return false;
        }
        return true;
    }
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
    auto requiresInterpolationForAccumulativeIteration(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (WebCore::Style::requiresInterpolationForAccumulativeIteration(a[i], b[i]))
                return true;
        }
        return false;
    }
    auto requiresInterpolationForAccumulativeIteration(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (WebCore::Style::requiresInterpolationForAccumulativeIteration(a[i], b[i], aStyle, bStyle))
                return true;
        }
        return false;
    }
    auto blend(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const auto& context) -> SpaceSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename SpaceSeparatedVector<StyleType, inlineCapacity>::Container result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], context));
        return { WTFMove(result) };
    }
    auto blend(const SpaceSeparatedVector<StyleType, inlineCapacity>& a, const SpaceSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> SpaceSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename SpaceSeparatedVector<StyleType, inlineCapacity>::Container result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], aStyle, bStyle, context));
        return { WTFMove(result) };
    }
};

// Specialization for `CommaSeparatedVector`.
template<typename StyleType, size_t inlineCapacity> struct Blending<CommaSeparatedVector<StyleType, inlineCapacity>> {
    auto equals(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::equalsForBlending(a[i], b[i]))
                return false;
        }
        return true;
    }
    auto equals(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!WebCore::Style::equalsForBlending(a[i], b[i], aStyle, bStyle))
                return false;
        }
        return true;
    }
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
    auto requiresInterpolationForAccumulativeIteration(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (WebCore::Style::requiresInterpolationForAccumulativeIteration(a[i], b[i]))
                return true;
        }
        return false;
    }
    auto requiresInterpolationForAccumulativeIteration(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (WebCore::Style::requiresInterpolationForAccumulativeIteration(a[i], b[i], aStyle, bStyle))
                return true;
        }
        return false;
    }
    auto blend(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const auto& context) -> CommaSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename CommaSeparatedVector<StyleType, inlineCapacity>::Container result;
        result.reserveInitialCapacity(size);
        for (size_t i = 0; i < size; ++i)
            result.append(WebCore::Style::blend(a[i], b[i], context));
        return { WTFMove(result) };
    }
    auto blend(const CommaSeparatedVector<StyleType, inlineCapacity>& a, const CommaSeparatedVector<StyleType, inlineCapacity>& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const auto& context) -> CommaSeparatedVector<StyleType, inlineCapacity>
    {
        auto size = a.size();
        typename CommaSeparatedVector<StyleType, inlineCapacity>::Container result;
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

struct IsZeroInvoker {
    template<typename T> bool operator()(const T& value) const
    {
        if constexpr (HasIsZero<T>)
            return value.isZero();
        else
            return IsZero<T>{}(value);
    }
};
inline constexpr IsZeroInvoker isZero{};

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

// MARK: - IsKnownZero

// All leaf types that want to conform to IsKnownZero must implement
// the following:
//
//    template<> struct WebCore::Style::IsKnownZero<StyleType> {
//        bool operator()(const StyleType&);
//    };
//
// or have a member function such that the type matches the
// `HasIsKnownZero` concept.

template<typename> struct IsKnownZero;

struct IsKnownZeroInvoker {
    template<typename T> bool operator()(const T& value) const
    {
        if constexpr (HasIsKnownZero<T>)
            return value.isKnownZero();
        else if constexpr (HasIsZero<T>)
            return !value.isZero();
        else
            return IsKnownZero<T>{}(value);
    }
};
inline constexpr IsKnownZeroInvoker isKnownZero{};

// Constrained for `TreatAsTupleLike`.
template<TupleLike T> struct IsKnownZero<T> {
    bool operator()(const T& value)
    {
        return WTF::apply([&](const auto& ...x) { return (isKnownZero(x) && ...); }, value);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike T> struct IsKnownZero<T> {
    bool operator()(const T& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isKnownZero(alternative); });
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

struct IsEmptyInvoker {
    template<typename T> bool operator()(const T& value) const
    {
        if constexpr (HasIsEmpty<T>)
            return value.isEmpty();
        else
            return IsEmpty<T>{}(value);
    }
};
inline constexpr IsEmptyInvoker isEmpty{};

// Specialization for `SpaceSeparatedSize`.
template<typename T> struct IsEmpty<SpaceSeparatedSize<T>> {
    bool operator()(const auto& value)
    {
        return isZero(value.width()) || isZero(value.height());
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedPoint`.
template<typename T> struct IsEmpty<MinimallySerializingSpaceSeparatedPoint<T>> {
    bool operator()(const auto& value)
    {
        return isZero(value.x()) || isZero(value.y());
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedSize`.
template<typename T> struct IsEmpty<MinimallySerializingSpaceSeparatedSize<T>> {
    bool operator()(const auto& value)
    {
        return isZero(value.width()) || isZero(value.height());
    }
};

// MARK: - IsKnownEmpty

// All leaf types that want to conform to IsKnownEmpty must implement
// the following:
//
//    template<> struct WebCore::Style::IsKnownEmpty<StyleType> {
//        bool operator()(const StyleType&);
//    };
//
// or have a member function such that the type matches the
// `HasIsKnownEmpty` concept.

template<typename> struct IsKnownEmpty;

struct IsKnownEmptyInvoker {
    template<typename T> bool operator()(const T& value) const
    {
        if constexpr (HasIsKnownEmpty<T>)
            return value.isKnownEmpty();
        else if constexpr (HasIsEmpty<T>)
            return value.isEmpty();
        else
            return IsKnownEmpty<T>{}(value);
    }
};
inline constexpr IsKnownEmptyInvoker isKnownEmpty{};

// Specialization for `SpaceSeparatedSize`.
template<typename T> struct IsKnownEmpty<SpaceSeparatedSize<T>> {
    bool operator()(const auto& value)
    {
        return isKnownZero(value.width()) || isKnownZero(value.height());
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedPoint`.
template<typename T> struct IsKnownEmpty<MinimallySerializingSpaceSeparatedPoint<T>> {
    bool operator()(const auto& value)
    {
        return isKnownZero(value.x()) || isKnownZero(value.y());
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedSize`.
template<typename T> struct IsKnownEmpty<MinimallySerializingSpaceSeparatedSize<T>> {
    bool operator()(const auto& value)
    {
        return isKnownZero(value.width()) || isKnownZero(value.height());
    }
};

// MARK: - Logging

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike T> TextStream& operator<<(TextStream& ts, const T&)
{
    return ts;
}

// Constrained for `TreatAsTupleLike`.
template<TupleLike T> TextStream& operator<<(TextStream& ts, const T& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparatorString<T>);
    return ts;
}

// Constrained for `TreatAsRangeLike`.
template<RangeLike T> TextStream& operator<<(TextStream& ts, const T& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparatorString<T>);
    return ts;
}

// Constrained for `TreatAsVariantLike`.
template<VariantLike T> TextStream& operator<<(TextStream& ts, const T& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

} // namespace Style
} // namespace WebCore
