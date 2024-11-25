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
#include "CSSPropertyParserConsumer+Contain.h"

#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeContain(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'contain'> = none | strict | content | [ [size | inline-size] || layout || style || paint ]
    // https://drafts.csswg.org/css-contain-2/#propdef-contain

    if (auto singleValue = consumeIdent<CSSValueNone, CSSValueStrict, CSSValueContent>(range))
        return singleValue;

    RefPtr<CSSPrimitiveValue> size;
    RefPtr<CSSPrimitiveValue> inlineSize;
    RefPtr<CSSPrimitiveValue> layout;
    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> paint;

    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueSize:
            if (size)
                return nullptr;
            size = consumeIdent(range);
            break;
        case CSSValueInlineSize:
            if (inlineSize || size)
                return nullptr;
            inlineSize = consumeIdent(range);
            break;
        case CSSValueLayout:
            if (layout)
                return nullptr;
            layout = consumeIdent(range);
            break;
        case CSSValuePaint:
            if (paint)
                return nullptr;
            paint = consumeIdent(range);
            break;
        case CSSValueStyle:
            if (style)
                return nullptr;
            style = consumeIdent(range);
            break;
        default:
            return nullptr;
        }
    }

    CSSValueListBuilder list;
    if (size)
        list.append(size.releaseNonNull());
    if (inlineSize)
        list.append(inlineSize.releaseNonNull());
    if (layout)
        list.append(layout.releaseNonNull());
    if (style)
        list.append(style.releaseNonNull());
    if (paint)
        list.append(paint.releaseNonNull());

    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
