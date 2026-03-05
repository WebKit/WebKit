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

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"
#include "StylePrimitiveNumericTypes.h"
#include <optional>
#include <type_traits>
#include <wtf/Brigand.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

enum CSSParserMode : uint8_t;
enum class ValueRange : uint8_t;

namespace CSSPropertyParserHelpers {

// MARK: - Meta Consumers

/// The result of a meta consume.
/// To be used with a list of `CSS` types (e.g. `ConsumeResult<CSS::Angle<Range>, CSS::Percentage<Range>, CSS::Keyword::None>`), which will yield a
/// result type of either a Variant of those types (e.g.`Variant<CSS::Angle<Range>, CSS::Percentage<Range>, CSS::Keyword::None>`) or the type
/// itself if only a single type was specified.
template<typename... Ts>
struct MetaConsumeResult {
    using TypeList = brigand::list<Ts...>;
    using type = VariantOrSingle<TypeList>;
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
template<typename... Ts>
struct MetaConsumerUnroller {
    template<CSSParserTokenType, typename ResultType>
    static std::nullopt_t consume(CSSParserTokenRange&, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
    {
        return std::nullopt;
    }

    template<CSSParserTokenType, typename ResultType, typename F>
    static std::nullopt_t consume(CSSParserTokenRange&, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions, NOESCAPE F&&)
    {
        return std::nullopt;
    }
};

// Actionable case, checks if the `Consumer` defined for type `T` supports the
// current token, trying to consume if it does, and in either case, falling
// back to recursively trying the same on the remainder of the type list `Ts...`.
template<typename T, typename... Ts>
struct MetaConsumerUnroller<T, Ts...> {
    template<CSSParserTokenType tokenType, typename ResultType>
    static std::optional<ResultType> consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
    {
        using Consumer = MetaConsumerDispatcher<tokenType, ConsumerDefinition<T>>;
        if constexpr (Consumer::supported) {
            if (auto result = Consumer::consume(range, state, symbolsAllowed, options))
                return { T { *result } };
        }
        return MetaConsumerUnroller<Ts...>::template consume<tokenType, ResultType>(range, state, symbolsAllowed, options);
    }

    template<CSSParserTokenType tokenType, typename ResultType, typename F>
    static std::optional<ResultType> consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options, NOESCAPE F&& functor)
    {
        using Consumer = MetaConsumerDispatcher<tokenType, ConsumerDefinition<T>>;
        if constexpr (Consumer::supported) {
            if (auto result = Consumer::consume(range, state, symbolsAllowed, options))
                return std::make_optional(functor(T { *result }));
        }
        return MetaConsumerUnroller<Ts...>::template consume<tokenType, ResultType>(range, state, symbolsAllowed, options, std::forward<F>(functor));
    }
};

// The `MetaConsumer` is the main driver of token consumption, dispatching
// to a `MetaConsumerUnroller` based on token type. Caller use this directly.
// An example use that attempts to consumer either a <number> or <percentage>
// looks like (argument list elided for brevity):
//
//    auto result = MetaConsumer<CSS::Percentage<R>, CSS::Number<R>>::consume(range, ...);
//
// If a caller wants to avoid the overhead of switching on the returned variant
// result, an alternative overload of `consume` is provided which takes an additional
// `functor` argument which gets called with the result:
//
//    auto result = MetaConsumer<CSS::Percentage<R>, CSS::Number<R>>::consume(range, ...,
//        [](CSS::Percentage<R> percentage) { ... },
//        [](CSS::Number<R> number) { ... }
//    );
template<typename T, typename... Ts>
struct MetaConsumer {
    static_assert(WTF::all<HasConsumerDefinition::check<T>(), HasConsumerDefinition::check<Ts>()...>);

    using Unroller = MetaConsumerUnroller<T, Ts...>;

    template<typename... F>
    static decltype(auto) consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options, F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<T>()));

        switch (range.peek().type()) {
        case FunctionToken:
            return Unroller::template consume<FunctionToken, ResultType>(range, state, WTF::move(symbolsAllowed), options, visitor);

        case NumberToken:
            return Unroller::template consume<NumberToken, ResultType>(range, state, WTF::move(symbolsAllowed), options, visitor);

        case PercentageToken:
            return Unroller::template consume<PercentageToken, ResultType>(range, state, WTF::move(symbolsAllowed), options, visitor);

        case DimensionToken:
            return Unroller::template consume<DimensionToken, ResultType>(range, state, WTF::move(symbolsAllowed), options, visitor);

        case IdentToken:
            return Unroller::template consume<IdentToken, ResultType>(range, state, WTF::move(symbolsAllowed), options, visitor);

        default:
            return std::optional<ResultType> { };
        }
    }

    // Overloaded with the `CSSPropertyParserOptions` parameter removed so it can be defaulted when using the continuation functor parameters.
    template<typename... F>
    static decltype(auto) consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, F&&... f)
    {
        return consume(range, state, WTF::move(symbolsAllowed), { }, std::forward<F>(f)...);
    }

    // Overloaded with the `CSSCalcSymbolsAllowed` parameter removed so it can be defaulted when using the continuation functor parameters.
    template<typename... F>
    static decltype(auto) consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSPropertyParserOptions options, F&&... f)
    {
        return consume(range, state, { }, options, std::forward<F>(f)...);
    }

    // Overloaded with the `CSSPropertyParserOptions` and `CSSCalcSymbolsAllowed` parameters removed so they can be defaulted when using the continuation functor parameters.
    template<typename... F>
    static decltype(auto) consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, F&&... f)
    {
        return consume(range, state, { }, { }, std::forward<F>(f)...);
    }

    // Overloaded with no continuation functor parameters allowing a for simplified interface when returning a single value / or Variant is acceptable.
    static decltype(auto) consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed = { }, CSSPropertyParserOptions options = { })
    {
        using ResultType = typename MetaConsumeResult<T, Ts...>::type;

        return consume(range, state, WTF::move(symbolsAllowed), options,
            [](auto&& value) {
                return ResultType { WTF::move(value) };
            }
        );
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
