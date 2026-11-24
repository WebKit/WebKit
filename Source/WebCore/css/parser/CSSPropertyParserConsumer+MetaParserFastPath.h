/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"
#include "CSSTokenizer.h"
#include "StylePrimitiveNumericTypes.h"
#include <optional>
#include <span>
#include <wtf/ASCIICType.h>
#include <wtf/Brigand.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename... Ts>
struct MetaParserFastPathUnroller {
    template<CSSParserTokenType, typename ResultType, typename F>
    static std::nullopt_t parse(const LexedNumericToken&, CSS::PropertyParserState&, CSSPropertyParserOptions, NOESCAPE const F&)
    {
        return std::nullopt;
    }
};

template<typename T, typename... Ts>
struct MetaParserFastPathUnroller<T, Ts...> {
    template<CSSParserTokenType tokenType, typename ResultType, typename F>
    static std::optional<ResultType> parse(const LexedNumericToken& token, CSS::PropertyParserState& state, CSSPropertyParserOptions options, NOESCAPE const F& functor)
    {
        using Consumer = MetaConsumerDispatcher<tokenType, ConsumerDefinition<T>>;
        if constexpr (Consumer::supported) {
            if constexpr (requires { Consumer::interpret(token, state, options); }) {
                if (auto result = Consumer::interpret(token, state, options))
                    return std::make_optional(functor(T { *result }));
            } else
                return std::nullopt;
        }
        return MetaParserFastPathUnroller<Ts...>::template parse<tokenType, ResultType>(token, state, options, functor);
    }
};

template<typename T, typename... Ts>
struct MetaParserFastPath {
    static_assert(WTF::all<HasConsumerDefinition::check<T>(), HasConsumerDefinition::check<Ts>()...>);

    using Unroller = MetaParserFastPathUnroller<T, Ts...>;

    template<typename CharacterType, typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(std::span<const CharacterType> characters, CSS::PropertyParserState& state, CSSPropertyParserOptions options, NOESCAPE F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<T>()));

        auto trimmedCharacters = characters;
        while (!trimmedCharacters.empty() && isASCIIWhitespace(trimmedCharacters.front()))
            trimmedCharacters = trimmedCharacters.subspan(1);
        while (!trimmedCharacters.empty() && isASCIIWhitespace(trimmedCharacters.back()))
            trimmedCharacters = trimmedCharacters.first(trimmedCharacters.size() - 1);

        auto token = lexNumericToken(trimmedCharacters);
        if (!token)
            return std::optional<ResultType> { };

        switch (token->type()) {
        case NumberToken:
            return Unroller::template parse<NumberToken, ResultType>(*token, state, options, visitor);

        case PercentageToken:
            return Unroller::template parse<PercentageToken, ResultType>(*token, state, options, visitor);

        case DimensionToken:
            return Unroller::template parse<DimensionToken, ResultType>(*token, state, options, visitor);

        default:
            return std::optional<ResultType> { };
        }
    }

    template<typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, CSSPropertyParserOptions options, NOESCAPE F&&... f)
    {
        if (string.is8Bit())
            return parse(string.span8(), state, options, std::forward<F>(f)...);
        return parse(string.span16(), state, options, std::forward<F>(f)...);
    }

    template<typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, NOESCAPE F&&... f)
    {
        return parse(string, state, CSSPropertyParserOptions { }, std::forward<F>(f)...);
    }

    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, CSSPropertyParserOptions options = { })
    {
        using ResultType = typename MetaConsumeResult<T, Ts...>::type;

        return parse(string, state, options,
            [](auto&& value) {
                return ResultType { WTF::move(value) };
            }
        );
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
