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
#include "CSSPropertyParserConsumer+Box.h"

#include "CSSCalcSymbolTable.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeMarginPhysical(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    // <margin-physical> = <length-percentage> | auto
    // https://drafts.csswg.org/css-box/#margin-physical

    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    auto unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    auto anchorSizePolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorSizePolicy::Allow : AnchorSizePolicy::Forbid;
    return consumeLengthPercentage(range, context, ValueRange::All, unitless, UnitlessZeroQuirk::Allow, AnchorPolicy::Forbid, anchorSizePolicy);
}

RefPtr<CSSValue> consumeMarginTrim(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'margin-trim'> = none | [ block || inline ] | [ block-start || inline-start || block-end || inline-end ]
    // https://drafts.csswg.org/css-box/#margin-trim

    auto firstValue = range.peek().id();
    if (firstValue == CSSValueBlock || firstValue == CSSValueInline || firstValue == CSSValueNone)
        return consumeIdent(range).releaseNonNull();
    Vector<CSSValueID, 4> idents;
    while (auto ident = consumeIdentRaw<CSSValueBlockStart, CSSValueBlockEnd, CSSValueInlineStart, CSSValueInlineEnd>(range)) {
        if (idents.contains(*ident))
            return nullptr;
        idents.append(*ident);
    }
    // Try to serialize into either block or inline form
    if (idents.size() == 2) {
        if (idents.contains(CSSValueBlockStart) && idents.contains(CSSValueBlockEnd))
            return CSSPrimitiveValue::create(CSSValueBlock);
        if (idents.contains(CSSValueInlineStart) && idents.contains(CSSValueInlineEnd))
            return CSSPrimitiveValue::create(CSSValueInline);
    }
    CSSValueListBuilder list;
    for (auto ident : idents)
        list.append(CSSPrimitiveValue::create(ident));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
