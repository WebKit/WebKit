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

RefPtr<CSSValue> consumePositionTryFallbacks(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'position-try-fallbacks'> = none | [ [<dashed-ident> || <try-tactic>] | <'position-area'> ]#
    // https://drafts.csswg.org/css-anchor-position-1/#propdef-position-try-fallbacks

    if (auto result = consumeIdent<CSSValueNone>(range))
        return result;

    auto consumeFallback = [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        // Try to parse <'position-area'>
        auto rangeCopy = range;
        // consumePositionArea accepts 'none', so detect and reject it beforehand.
        if (range.peek().id() == CSSValueNone)
            return nullptr;
        if (auto positionArea = consumePositionArea(range, state))
            return positionArea;

        range = rangeCopy;

        // Try to parse [<dashed-ident> || <try-tactic>]
        // <try-tactic> = flip-block || flip-inline || flip-start || flip-x || flip-y
        auto tryRuleIdent = consumeDashedIdentRaw(range);

        Vector<CSSValueID, 5> tryTactics;
        while (auto tactic = consumeIdentRaw<CSSValueFlipBlock, CSSValueFlipInline, CSSValueFlipStart, CSSValueFlipX, CSSValueFlipY>(range)) {
            if (tryTactics.contains(*tactic))
                return nullptr;
            tryTactics.append(*tactic);
        }

        if (tryRuleIdent.isNull())
            tryRuleIdent = consumeDashedIdentRaw(range);

        CSSValueListBuilder list;
        if (!tryRuleIdent.isNull())
            list.append(CSSPrimitiveValue::createCustomIdent(tryRuleIdent));
        for (auto tactic : tryTactics)
            list.append(CSSPrimitiveValue::create(tactic));

        // At least one @position-try rule ident or tactic must be present.
        if (list.isEmpty())
            return nullptr;

        return CSSValueList::createSpaceSeparated(WTF::move(list));
    };

    return consumeListSeparatedBy<',', OneOrMore, ListOptimization::SingleValue>(range, consumeFallback);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

