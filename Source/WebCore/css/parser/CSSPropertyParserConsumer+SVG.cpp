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
#include "CSSPropertyParserConsumer+SVG.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumePaint(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <paint> = none | <color> | <url> [none | <color>]? | context-fill | context-stroke
    // https://svgwg.org/svg2-draft/painting.html#SpecifyingStrokePaint

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    auto url = consumeURL(range);
    if (url) {
        RefPtr<CSSValue> parsedValue;
        if (range.peek().id() == CSSValueNone)
            parsedValue = consumeIdent(range);
        else
            parsedValue = consumeColor(range, context);
        if (parsedValue)
            return CSSValueList::createSpaceSeparated(url.releaseNonNull(), parsedValue.releaseNonNull());
        return url;
    }
    return consumeColor(range, context);
}

RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'paint-order'> = normal | [ fill || stroke || markers ]
    // https://svgwg.org/svg2-draft/painting.html#PaintOrderProperty

    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> paintTypeList;
    RefPtr<CSSPrimitiveValue> fill;
    RefPtr<CSSPrimitiveValue> stroke;
    RefPtr<CSSPrimitiveValue> markers;
    do {
        CSSValueID id = range.peek().id();
        if (id == CSSValueFill && !fill)
            fill = consumeIdent(range);
        else if (id == CSSValueStroke && !stroke)
            stroke = consumeIdent(range);
        else if (id == CSSValueMarkers && !markers)
            markers = consumeIdent(range);
        else
            return nullptr;
        paintTypeList.append(id);
    } while (!range.atEnd());

    // After parsing we serialize the paint-order list. Since it is not possible to
    // pop a last list items from CSSValueList without bigger cost, we create the
    // list after parsing.
    CSSValueID firstPaintOrderType = paintTypeList.at(0);
    CSSValueListBuilder paintOrderList;
    switch (firstPaintOrderType) {
    case CSSValueFill:
    case CSSValueStroke:
        paintOrderList.append(firstPaintOrderType == CSSValueFill ? fill.releaseNonNull() : stroke.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueMarkers)
                paintOrderList.append(markers.releaseNonNull());
        }
        break;
    case CSSValueMarkers:
        paintOrderList.append(markers.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueStroke)
                paintOrderList.append(stroke.releaseNonNull());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return CSSValueList::createSpaceSeparated(WTFMove(paintOrderList));
}

RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'stroke-dasharray'> = none | [ [ <length-percentage> | <number> ]+ ]#
    // https://svgwg.org/svg2-draft/painting.html#StrokeDashing

    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder dashes;
    do {
        RefPtr<CSSPrimitiveValue> dash = consumeLengthPercentage(range, context, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
        if (!dash)
            dash = consumeNumber(range, context, ValueRange::NonNegative);
        if (!dash || (consumeCommaIncludingWhitespace(range) && range.atEnd()))
            return nullptr;
        dashes.append(dash.releaseNonNull());
    } while (!range.atEnd());
    return CSSValueList::createCommaSeparated(WTFMove(dashes));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
