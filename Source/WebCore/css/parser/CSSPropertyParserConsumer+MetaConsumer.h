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

#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include <optional>
#include <type_traits>
#include <wtf/Brigand.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

enum CSSParserMode : uint8_t;
enum class ValueRange : uint8_t;

namespace CSSPropertyParserHelpers {

// MARK: - Meta Consumers

template<typename... Ts>
using MetaConsumerVariantWrapper = typename std::variant<Ts...>;

/// The result of a meta consume.
/// To be used with a list of `raw` types. e.g. `ConsumeResult<AngleRaw, PercentRaw, NoneRaw>`, which will yield a
/// result type of `std::variant<AngleRaw, UnevaluatedCalc<AngleRaw>, PercentRaw, UnevaluatedCalc<PercentRaw>, NoneRaw>`.
template<typename... Ts>
struct MetaConsumeResult {
    using TypeList = brigand::flatten<
        brigand::transform<brigand::list<Ts...>, ConsumerDefinition<brigand::_1>>
    >;
    using type = brigand::wrap<TypeList, MetaConsumerVariantWrapper>;
};


template<CSSParserTokenType tokenType, typename Consumer, typename = void>
struct MetaConsumerDispatcher {
    static constexpr bool supported = false;
};

template<typename Consumer>
struct MetaConsumerDispatcher<FunctionToken, Consumer, typename std::void_t<typename Consumer::FunctionToken>> {
    static constexpr bool supported = true;
    template<typename... Args>
    static decltype(auto) consume(Args&&... args)
    {
        return Consumer::FunctionToken::consume(std::forward<Args>(args)...);
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<NumberToken, Consumer, typename std::void_t<typename Consumer::NumberToken>> {
    static constexpr bool supported = true;
    template<typename... Args>
    static decltype(auto) consume(Args&&... args)
    {
        return Consumer::NumberToken::consume(std::forward<Args>(args)...);
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<PercentageToken, Consumer, typename std::void_t<typename Consumer::PercentageToken>> {
    static constexpr bool supported = true;
    template<typename... Args>
    static decltype(auto) consume(Args&&... args)
    {
        return Consumer::PercentageToken::consume(std::forward<Args>(args)...);
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<DimensionToken, Consumer, typename std::void_t<typename Consumer::DimensionToken>> {
    static constexpr bool supported = true;
    template<typename... Args>
    static decltype(auto) consume(Args&&... args)
    {
        return Consumer::DimensionToken::consume(std::forward<Args>(args)...);
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<IdentToken, Consumer, typename std::void_t<typename Consumer::IdentToken>> {
    static constexpr bool supported = true;
    template<typename... Args>
    static decltype(auto) consume(Args&&... args)
    {
        return Consumer::IdentToken::consume(std::forward<Args>(args)...);
    }
};

// The `MetaConsumerUnroller` gives each type in the consumer list (`Ts...`)
// a chance to consume the token. It recursively peels off types from the
// type list, checks if the consumer supports this token type, and then calls
// to the MetaConsumerDispatcher to actually call right `consume` function.

// Empty case, used to indicate no more types remain to try.
template<CSSParserTokenType tokenType, typename ResultType, typename... Ts>
struct MetaConsumerUnroller {
    template<typename... Args>
    static std::nullopt_t consume(Args&&...)
    {
        return std::nullopt;
    }
};

// Actionable case, checks if the `Consumer` defined for type `T` supports the
// current token, trying to consume if it does, and in either case, falling
// back to recursively trying the same on the remainder of the type list `Ts...`.
template<CSSParserTokenType tokenType, typename ResultType, typename T, typename... Ts>
struct MetaConsumerUnroller<tokenType, ResultType, T, Ts...> {
    template<typename... Args>
    static std::optional<ResultType> consume(Args&&... args)
    {
        using Consumer = MetaConsumerDispatcher<tokenType, ConsumerDefinition<T>>;
        if constexpr (Consumer::supported) {
            if (auto result = Consumer::consume(args...))
                return {{ *result }};
        }
        return MetaConsumerUnroller<tokenType, ResultType, Ts...>::consume(args...);
    }
};

// The `MetaConsumer` is the main driver of token consumption, dispatching
// to a `MetaConsumerUnroller` based on token type. Caller use this directly.
// An example use that attempts to consumer either a <number> or <percentage>
// looks like:
//
//    auto result = MetaConsumer<PercentRaw, NumberRaw>::consume(range, ...);
//
// (Argument list elided for brevity)
template<typename... Ts>
struct MetaConsumer {
    using ResultType = typename MetaConsumeResult<Ts...>::type;

    template<typename... Args>
    static std::optional<ResultType> consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options, Args&&... args)
    {
        switch (range.peek().type()) {
        case FunctionToken:
            return MetaConsumerUnroller<FunctionToken, ResultType, Ts...>::consume(range, WTFMove(symbolsAllowed), options, std::forward<Args>(args)...);

        case NumberToken:
            return MetaConsumerUnroller<NumberToken, ResultType, Ts...>::consume(range, WTFMove(symbolsAllowed), options, std::forward<Args>(args)...);

        case PercentageToken:
            return MetaConsumerUnroller<PercentageToken, ResultType, Ts...>::consume(range, WTFMove(symbolsAllowed), options, std::forward<Args>(args)...);

        case DimensionToken:
            return MetaConsumerUnroller<DimensionToken, ResultType, Ts...>::consume(range, WTFMove(symbolsAllowed), options, std::forward<Args>(args)...);

        case IdentToken:
            return MetaConsumerUnroller<IdentToken, ResultType, Ts...>::consume(range, WTFMove(symbolsAllowed), options, std::forward<Args>(args)...);

        default:
            return { };
        }
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
