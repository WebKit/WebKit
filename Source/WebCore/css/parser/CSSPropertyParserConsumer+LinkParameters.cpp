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
#include "CSSPropertyParserConsumer+LinkParameters.h"

#include "CSSCustomPropertySyntax.h"
#include "CSSLinkParameter.h"
#include "CSSParamValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSSubstitutionParser.h"
#include "CSSValueKeywords.h"
#include "CSSVariableData.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// <param()>    = param( <param-spec> , <declaration-value>? )
// <param-spec> = color | accent-color | [ <dashed-ident> <css-type>? ]
// https://drafts.csswg.org/css-link-params/#funcdef-param
static std::optional<CSS::ParamSpec> consumeParamSpec(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    switch (range.peek().id()) {
    case CSSValueColor:
        range.consumeIncludingWhitespace();
        return CSS::ParamSpec { CSS::Keyword::Color { } };
    case CSSValueAccentColor:
        range.consumeIncludingWhitespace();
        return CSS::ParamSpec { CSS::Keyword::AccentColor { } };
    default:
        break;
    }

    auto name = consumeUnresolvedDashedIdent(range, state);
    if (!name)
        return { };

    std::optional<CSS::TypeSpecifier> type;
    if (!range.atEnd() && range.peek().type() != CommaToken) {
        auto syntax = CSSCustomPropertySyntax::consumeType(range);
        if (!syntax)
            return { };
        type = CSS::TypeSpecifier { WTF::move(*syntax) };
    }

    return CSS::ParamSpec { CSS::ParamSpec::Custom { WTF::move(*name), WTF::move(type) } };
}

std::optional<CSS::ParamFunction> consumeParamFunctionRaw(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().functionId() != CSSValueParam)
        return { };

    auto arguments = consumeFunction(range);

    auto spec = consumeParamSpec(arguments, state);
    if (!spec)
        return { };

    // The value may be empty but the comma is required.
    // https://github.com/w3c/csswg-drafts/issues/13767
    if (!consumeCommaIncludingWhitespace(arguments))
        return { };

    // A value containing substitutions cannot be resolved until computed-value time, so
    // fail here and let the declaration be stored unresolved instead. The longhand parser
    // runs before the substitution path, so succeeding would swallow the substitution.
    if (CSSSubstitutionParser::containsSubstitutionFunctions(arguments, state.context))
        return { };

    return CSS::ParamFunction { CSS::LinkParameter { WTF::move(*spec), CSS::DeclarationValue { CSSVariableData::create(arguments) } } };
}

RefPtr<CSSValue> consumeParamFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto parameter = consumeParamFunctionRaw(range, state))
        return CSSParamValue::create(WTF::move(*parameter));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
