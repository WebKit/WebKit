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
#include "CSSPropertyParserConsumer+WillChange.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'will-change'> = auto | <animateable-feature>#
    // https://drafts.csswg.org/css-will-change/#propdef-will-change

    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    // Every comma-separated list of identifiers is a valid will-change value,
    // unless the list includes an explicitly disallowed identifier.
    CSSValueListBuilder values;
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueContents:
        case CSSValueScrollPosition:
            values.append(consumeIdent(range).releaseNonNull());
            break;
        case CSSValueNone:
        case CSSValueAll:
        case CSSValueAuto:
            return nullptr;
        default:
            if (range.peek().type() != IdentToken)
                return nullptr;
            CSSPropertyID propertyID = cssPropertyID(range.peek().value());
            if (propertyID == CSSPropertyWillChange)
                return nullptr;
            if (!isExposed(propertyID, &context.propertySettings))
                propertyID = CSSPropertyInvalid;
            if (propertyID != CSSPropertyInvalid) {
                values.append(CSSPrimitiveValue::create(propertyID));
                range.consumeIncludingWhitespace();
                break;
            }
            if (auto customIdent = consumeCustomIdent(range)) {
                // Append properties we don't recognize, but that are legal.
                values.append(customIdent.releaseNonNull());
                break;
            }
            return nullptr;
        }
        // This is a comma separated list
        if (!range.atEnd() && !consumeCommaIncludingWhitespace(range))
            return nullptr;
    }
    return CSSValueList::createCommaSeparated(WTFMove(values));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
