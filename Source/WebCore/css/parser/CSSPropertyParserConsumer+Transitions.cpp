/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Transitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeSingleTransitionPropertyIdent(CSSParserTokenRange& range, const CSSParserToken& token)
{
    if (token.id() == CSSValueAll)
        return consumeIdent(range);
    if (auto property = token.parseAsCSSPropertyID()) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(property);
    }
    return consumeCustomIdent(range);
}

RefPtr<CSSValue> consumeSingleTransitionPropertyOrNone(CSSParserTokenRange& range, const CSSParserContext&)
{
    // This variant of consumeSingleTransitionProperty is used for the slightly different
    // parse rules used for the 'transition' shorthand which allows 'none':
    //
    // <single-transition-or-none> = [ none | <single-transition-property> ]
    // https://drafts.csswg.org/css-transitions/#single-transition-property

    auto& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return consumeIdent(range);

    return consumeSingleTransitionPropertyIdent(range, token);
}

RefPtr<CSSValue> consumeSingleTransitionProperty(CSSParserTokenRange& range, const CSSParserContext&)
{
    // "The <custom-ident> production in <single-transition-property> also excludes the keyword
    //  none, in addition to the keywords always excluded from <custom-ident>."
    //
    // <single-transition-property> = all | <custom-ident>;
    // https://drafts.csswg.org/css-transitions/#single-transition-property

    auto& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return nullptr;

    return consumeSingleTransitionPropertyIdent(range, token);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
