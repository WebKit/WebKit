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

#include "CSSParserToken.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include <optional>
#include <type_traits>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCalcSymbolTable;

namespace CSSPropertyParserHelpers {

// MARK: - Meta Consumers

template<typename Consumer, typename = void>
struct TransformApplier {
    template<typename T> static decltype(auto) apply(T value) { return value; }
};

template<typename Consumer>
struct TransformApplier<Consumer, typename std::void_t<typename Consumer::Transformer>> {
    template<typename T> static decltype(auto) apply(T value) { return Consumer::Transformer::transform(value); }
};

template<typename Consumer, typename T>
static decltype(auto) applyTransform(T value)
{
    return TransformApplier<Consumer>::apply(value);
}

// Transformers:

template<typename ResultType>
struct IdentityTransformer {
    using Result = ResultType;
    static Result transform(Result value) { return value; }
};

template<typename ResultType>
struct RawIdentityTransformer {
    using Result = std::optional<ResultType>;

    static Result transform(Result value) { return value; }

    template<typename... Types>
    static Result transform(const std::variant<Types...>& value)
    {
        return WTF::switchOn(value, [] (auto value) -> Result { return value; });
    }

    template<typename T>
    static Result transform(std::optional<T> value)
    {
        if (value)
            return transform(*value);
        return std::nullopt;
    }
};

template<typename Parent, typename ResultType>
struct RawVariantTransformerBase {
    using Result = std::optional<ResultType>;

    template<typename... Types>
    static Result transform(const std::variant<Types...>& value)
    {
        return WTF::switchOn(value, [] (auto value) { return Parent::transform(value); });
    }

    static typename Result::value_type transform(typename Result::value_type value)
    {
        return value;
    }

    template<typename T>
    static Result transform(std::optional<T> value)
    {
        if (value)
            return Parent::transform(*value);
        return std::nullopt;
    }
};

struct NumberOrPercentDividedBy100Transformer : RawVariantTransformerBase<NumberOrPercentDividedBy100Transformer, double> {
    using RawVariantTransformerBase<NumberOrPercentDividedBy100Transformer, double>::transform;

    static double transform(NumberRaw value)
    {
        return value.value;
    }

    static double transform(PercentRaw value)
    {
        return value.value / 100.0;
    }
};

// MARK: MetaConsumerDispatcher

template<CSSParserTokenType tokenType, typename Consumer, typename = void>
struct MetaConsumerDispatcher {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&...)
    {
        return { };
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<FunctionToken, Consumer, typename std::void_t<typename Consumer::FunctionToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::FunctionToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<NumberToken, Consumer, typename std::void_t<typename Consumer::NumberToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::NumberToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<PercentageToken, Consumer, typename std::void_t<typename Consumer::PercentageToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::PercentageToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<DimensionToken, Consumer, typename std::void_t<typename Consumer::DimensionToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::DimensionToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<IdentToken, Consumer, typename std::void_t<typename Consumer::IdentToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::IdentToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer, typename... Args>
auto consumeMetaConsumer(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, Args&&... args) -> typename Consumer::Result
{
    switch (range.peek().type()) {
    case FunctionToken:
        return MetaConsumerDispatcher<FunctionToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case NumberToken:
        return MetaConsumerDispatcher<NumberToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case PercentageToken:
        return MetaConsumerDispatcher<PercentageToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case DimensionToken:
        return MetaConsumerDispatcher<DimensionToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case IdentToken:
        return MetaConsumerDispatcher<IdentToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    default:
        return { };
    }
}

// MARK: SameTokenMetaConsumer

template<typename Transformer, typename ConsumersTuple, size_t I>
struct SameTokenMetaConsumerApplier {
    template<typename... Args>
    static typename Transformer::Result consume(Args&&... args)
    {
        using SelectedConsumer = std::tuple_element_t<I - 1, ConsumersTuple>;
        if (auto result = Transformer::transform(SelectedConsumer::consume(std::forward<Args>(args)...)))
            return result;
        return SameTokenMetaConsumerApplier<Transformer, ConsumersTuple, I - 1>::consume(std::forward<Args>(args)...);
    }
};

template<typename Transformer, typename ConsumersTuple>
struct SameTokenMetaConsumerApplier<Transformer, ConsumersTuple, 0> {
    template<typename... Args>
    static typename Transformer::Result consume(Args&&...)
    {
        return typename Transformer::Result { };
    }
};

template<typename Transformer, typename T, typename... Ts>
struct SameTokenMetaConsumer {
    static_assert(std::conjunction_v<std::bool_constant<Ts::tokenType == T::tokenType>...>, "All Consumers passed to SameTokenMetaConsumer must have the same tokenType");
    using ConsumersTuple = std::tuple<T, Ts...>;

    static constexpr CSSParserTokenType tokenType = T::tokenType;
    template<typename... Args>
    static typename Transformer::Result consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, Args&&... args)
    {
        ASSERT(range.peek().type() == tokenType);
        return SameTokenMetaConsumerApplier<Transformer, ConsumersTuple, std::tuple_size_v<ConsumersTuple>>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
