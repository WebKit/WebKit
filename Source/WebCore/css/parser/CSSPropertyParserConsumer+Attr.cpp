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
#include "CSSPropertyParserConsumer+Attr.h"

#include "CSSAttrValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, const CSSParserContext& context)
{
    // Standard says this should be:
    //
    // <attr()>    = attr( <attr-name> <attr-type>? , <declaration-value>?)
    // <attr-name> = [ <ident-token> '|' ]? <ident-token>
    // <attr-type> = type( <syntax> ) | string | <attr-unit>
    // https://drafts.csswg.org/css-values-5/#funcdef-attr

    // FIXME: Add support for complete <attr-name> syntax, including namespace support.
    // FIXME: Add support for <attr-type> syntax

    if (args.peek().type() != IdentToken)
        return nullptr;

    CSSParserToken token = args.consumeIncludingWhitespace();

    AtomString attrName;
    if (context.isHTMLDocument)
        attrName = token.value().convertToASCIILowercaseAtom();
    else
        attrName = token.value().toAtomString();

    if (!args.atEnd() && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    RefPtr<CSSValue> fallback;
    if (args.peek().type() == StringToken) {
        token = args.consumeIncludingWhitespace();
        fallback = CSSPrimitiveValue::create(token.value().toString());
    }

    if (!args.atEnd())
        return nullptr;

    auto attr = CSSAttrValue::create(WTFMove(attrName), WTFMove(fallback));
    // FIXME: Consider moving to a CSSFunctionValue with a custom-ident rather than a special CSS_ATTR primitive value.

    return CSSPrimitiveValue::create(WTFMove(attr));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
