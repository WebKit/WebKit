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

#include "config.h"
#include "CSSPropertyParserConsumer+URL.h"

#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+KeywordDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserState.h"
#include "CSSURLValue.h"
#include "CSSValueKeywords.h"
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: <url>
// https://drafts.csswg.org/css-values/#urls

// <url> = <url()> | <src()>
//
// <url()> = url( <string> <url-modifier>* ) | <url-token>
// <src()> = src( <string> <url-modifier>* )

// <url-modifier> = <crossorigin-modifier> | <integrity-modifier> | <referrerpolicy-modifier>
//
// <crossorigin-modifier> = crossorigin( anonymous | use-credentials )
// <integrity-modifier> = integrity( <string> )
// <referrerpolicy-modifier> = referrerpolicy( no-referrer | no-referrer-when-downgrade | same-origin | origin | strict-origin | origin-when-cross-origin | strict-origin-when-cross-origin | unsafe-url)

std::optional<CSS::URL> consumeURLRaw(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto& token = range.peek();
    if (token.type() == UrlToken) {
        auto result = CSS::completeURL(token.value().toString(), state.context);
        if (!result)
            return { };
        range.consumeIncludingWhitespace();
        return result;
    }

    switch (token.functionId()) {
    case CSSValueUrl: {
        CSSParserTokenRangeGuard guard { range };

        auto args = consumeFunction(range);

        auto string = consumeStringRaw(args);
        if (string.isNull())
            return { };
        auto result = CSS::completeURL(string.toString(), state.context);
        if (!result)
            return { };

        if (!state.context.cssURLModifiersEnabled) {
            if (!args.atEnd())
                return { };
        } else {
            while (!args.atEnd()) {
                switch (args.peek().functionId()) {
                case CSSValueCrossorigin: {
                    if (result->modifiers.crossorigin)
                        return { };
                    auto crossoriginArgs = consumeFunction(args);
                    auto crossoriginValue = MetaConsumer<
                        CSS::Keyword::Anonymous,
                        CSS::Keyword::UseCredentials
                    >::consume(crossoriginArgs, state);
                    if (!crossoriginValue || !crossoriginArgs.atEnd())
                        return { };
                    result->modifiers.crossorigin = CSS::URLCrossoriginFunction { .parameters = { *crossoriginValue } };
                    break;
                }
                case CSSValueIntegrity: {
                    if (result->modifiers.integrity)
                        return { };
                    auto integrityArgs = consumeFunction(args);
                    auto integrityValue = consumeStringRaw(integrityArgs);
                    if (integrityValue.isNull() || !integrityArgs.atEnd())
                        return { };
                    result->modifiers.integrity = CSS::URLIntegrityFunction { .parameters = { integrityValue.toString() } };
                    break;
                }
                case CSSValueReferrerpolicy: {
                    if (result->modifiers.referrerpolicy)
                        return { };
                    auto referrerpolicyArgs = consumeFunction(args);
                    auto referrerpolicyValue = MetaConsumer<
                        CSS::Keyword::NoReferrer,
                        CSS::Keyword::NoReferrerWhenDowngrade,
                        CSS::Keyword::SameOrigin,
                        CSS::Keyword::Origin,
                        CSS::Keyword::StrictOrigin,
                        CSS::Keyword::OriginWhenCrossOrigin,
                        CSS::Keyword::StrictOriginWhenCrossOrigin,
                        CSS::Keyword::UnsafeUrl
                    >::consume(referrerpolicyArgs, state);
                    if (!referrerpolicyValue || !referrerpolicyArgs.atEnd())
                        return { };
                    result->modifiers.referrerpolicy = CSS::URLReferrerpolicyFunction { .parameters = { *referrerpolicyValue } };
                    break;
                }
                default:
                    return { };
                }
            }
        }

        guard.commit();

        return result;
    }

    default:
        break;
    }

    return { };
}

RefPtr<CSSValue> consumeURL(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto rawURL = consumeURLRaw(range, state))
        return CSSURLValue::create(WTFMove(*rawURL));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
