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

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaParserFastPath.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"
#include "CSSTokenizer.h"
#include <optional>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename T, typename... Ts>
struct MetaParser {
    template<typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options, NOESCAPE F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<T>()));

        if (auto result = MetaParserFastPath<T, Ts...>::parse(string, state, options, visitor))
            return result;

        CSSTokenizer tokenizer(string);
        auto range = tokenizer.tokenRange();
        range.consumeWhitespace();

        auto result = MetaConsumer<T, Ts...>::consume(range, state, WTF::move(symbolsAllowed), options);
        if (!result || !range.atEnd())
            return std::optional<ResultType> { };
        if constexpr (!sizeof...(Ts))
            return std::make_optional(visitor(WTF::move(*result)));
        else
            return std::make_optional(WTF::visit(visitor, WTF::move(*result)));
    }

    template<typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, NOESCAPE F&&... f)
    {
        return parse(string, state, WTF::move(symbolsAllowed), CSSPropertyParserOptions { }, std::forward<F>(f)...);
    }

    template<typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, CSSPropertyParserOptions options, NOESCAPE F&&... f)
    {
        return parse(string, state, CSSCalcSymbolsAllowed { }, options, std::forward<F>(f)...);
    }

    template<typename... F>
        requires (sizeof...(F) > 0)
    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, NOESCAPE F&&... f)
    {
        return parse(string, state, CSSCalcSymbolsAllowed { }, CSSPropertyParserOptions { }, std::forward<F>(f)...);
    }

    static decltype(auto) parse(StringView string, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed = { }, CSSPropertyParserOptions options = { })
    {
        using ResultType = typename MetaConsumeResult<T, Ts...>::type;

        return parse(string, state, WTF::move(symbolsAllowed), options,
            [](auto&& value) {
                return ResultType { WTF::move(value) };
            }
        );
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
