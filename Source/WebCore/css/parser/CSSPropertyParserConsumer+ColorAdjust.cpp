/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+ColorAdjust.h"

#include "CSSColorScheme.h"
#include "CSSColorSchemeValue.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSValueKeywords.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

#if ENABLE(DARK_MODE_CSS)

std::optional<CSS::ColorScheme> consumeUnresolvedColorScheme(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'color-scheme'> = normal | [ light | dark | <custom-ident> ]+ && only?
    // https://drafts.csswg.org/css-color-adjust/#propdef-color-scheme

    if (range.peek().id() == CSSValueNormal) {
        range.consumeIncludingWhitespace();

        // NOTE: `normal` is represented in CSS::ColorScheme as an empty list of schemes.
        return CSS::ColorScheme {
            .schemes = { },
            .only = { }
        };
    }

    std::optional<CSS::ColorScheme> result = CSS::ColorScheme {
        .schemes = { },
        .only = { }
    };

    if (range.peek().id() == CSSValueOnly) {
        range.consumeIncludingWhitespace();

        result->only = CSS::Only { };
    }

    while (!range.atEnd()) {
        if (range.peek().type() != IdentToken)
            return { };

        CSSValueID id = range.peek().id();

        switch (id) {
        case CSSValueNormal:
            // `normal` is only allowed as a single value, and was handled earlier.
            // Don't allow it in the list.
            return { };

        case CSSValueOnly:
            // `only` can either appear first, handled before the loop, or last,
            // handled here.

            if (result->only)
                return { };
            range.consumeIncludingWhitespace();
            result->only = CSS::Only { };

            if (!range.atEnd())
                return { };

            break;

        default:
            if (!isValidCustomIdentifier(id))
                return { };

            result->schemes.value.append(CSS::CustomIdentifier { range.consumeIncludingWhitespace().value().toAtomString() });
            break;
        }
    }

    if (result->schemes.isEmpty())
        return { };

    return result;
}

std::optional<CSS::ColorScheme> parseUnresolvedColorScheme(const String& string, const CSSParserContext& context)
{
    CSSTokenizer tokenizer(string);
    CSSParserTokenRange range(tokenizer.tokenRange());

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto result = consumeUnresolvedColorScheme(range, context);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto colorScheme = consumeUnresolvedColorScheme(range, context);
    if (!colorScheme)
        return { };
    return CSSColorSchemeValue::create(WTFMove(*colorScheme));
}

#endif // ENABLE(DARK_MODE_CSS)

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
