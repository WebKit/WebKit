/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSLinkParameters.h"

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
#include "CSSTokenizer.h"
#include "CSSVariableData.h"
#include <pal/text/TextEncoding.h>
#include <wtf/URL.h>

namespace WebCore {

// FIXME: `param` is not yet a CSS value keyword, so it cannot be matched with
// CSSParserToken::functionId(). Compare the function name directly until the
// url() modifier form needs the keyword too.
static bool isParamFunction(const CSSParserToken& token)
{
    return token.type() == FunctionToken && equalLettersIgnoringASCIICase(token.value(), "param"_s);
}

// <param()> = param( <dashed-ident> , <declaration-value>? )
// https://drafts.csswg.org/css-link-params/#funcdef-param
static std::optional<CSSLinkParameter> consumeParamFunction(CSSParserTokenRange& range)
{
    auto block = range.consumeBlock();
    block.consumeWhitespace();

    auto& nameToken = block.consumeIncludingWhitespace();
    if (nameToken.type() != IdentToken || !isCustomPropertyName(nameToken.value()))
        return { };

    auto name = nameToken.value().toAtomString();

    // The value is optional; an omitted value is an empty value.
    if (block.atEnd())
        return CSSLinkParameter { WTF::move(name), CSSVariableData::create(block) };

    if (block.consume().type() != CommaToken)
        return { };
    block.consumeWhitespace();

    return CSSLinkParameter { WTF::move(name), CSSVariableData::create(block) };
}

// Link parameters are carried in the fragment directive, so that they are delimited
// from any other fragment identifier the resource is also addressed by.
// https://wicg.github.io/scroll-to-text-fragment/#fragmentdirective
static String linkParameterDirective(const URL& url)
{
    if (!url.hasFragmentIdentifier())
        return { };

    auto urlWithoutDirective = url;
    return urlWithoutDirective.consumeFragmentDirective();
}

CSSLinkParameterList parseLinkParametersFromFragment(const URL& url)
{
    auto directive = linkParameterDirective(url);
    if (directive.isEmpty())
        return { };

    // The directive arrives percent-encoded; `param(--color, green)` contains a
    // space that must be decoded before the value can be tokenized.
    auto decoded = PAL::decodeURLEscapeSequences(directive);

    auto tokenizer = CSSTokenizer::tryCreate(decoded);
    if (!tokenizer)
        return { };

    auto range = tokenizer->tokenRange();
    range.consumeWhitespace();

    // Multiple parameters are joined with "&".
    CSSLinkParameterList parameters;
    while (!range.atEnd()) {
        if (!isParamFunction(range.peek()))
            return { };

        auto parameter = consumeParamFunction(range);
        if (!parameter)
            return { };

        // A later duplicate wins over an earlier one.
        parameters.removeAllMatching([&](auto& existing) {
            return existing.name == parameter->name;
        });
        parameters.append(WTF::move(*parameter));

        range.consumeWhitespace();
        if (range.atEnd())
            break;

        auto& separator = range.consume();
        if (separator.type() != DelimiterToken || separator.delimiter() != '&')
            return { };
        range.consumeWhitespace();
    }

    return parameters;
}

bool linkParameterListsAreEqual(const CSSLinkParameterList& a, const CSSLinkParameterList& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || a[i].value.get() != b[i].value.get())
            return false;
    }

    return true;
}

} // namespace WebCore
