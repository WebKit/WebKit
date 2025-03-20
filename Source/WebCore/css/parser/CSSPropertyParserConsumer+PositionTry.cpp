/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+PositionTry.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumePositionTryFallbacks(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // none | [ [<dashed-ident> || <try-tactic>] | <'position-area'> ]#
    if (auto result = consumeIdent<CSSValueNone>(range))
        return result;

    auto consume = [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        // <'position-area'>
        auto rangeCopy = range;
        if (range.peek().id() == CSSValueNone)
            return nullptr;
        if (auto positionArea = consumePositionArea(range, context))
            return positionArea;

        range = rangeCopy;

        // [<dashed-ident> || <try-tactic>]
        auto tryRuleIdent = consumeDashedIdentRaw(range);

        Vector<CSSValueID, 3> idents;
        while (auto ident = consumeIdentRaw<CSSValueFlipBlock, CSSValueFlipInline, CSSValueFlipStart>(range)) {
            if (idents.contains(*ident))
                return nullptr;
            idents.append(*ident);
        }

        CSSValueListBuilder list;
        for (auto ident : idents)
            list.append(CSSPrimitiveValue::create(ident));

        if (tryRuleIdent.isNull())
            tryRuleIdent = consumeDashedIdentRaw(range);

        if (!tryRuleIdent.isNull())
            list.insert(0, CSSPrimitiveValue::createCustomIdent(tryRuleIdent));

        return CSSValueList::createSpaceSeparated(WTFMove(list));
    };
    return consumeListSeparatedBy<',', OneOrMore, ListOptimization::SingleValue>(range, consume);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

