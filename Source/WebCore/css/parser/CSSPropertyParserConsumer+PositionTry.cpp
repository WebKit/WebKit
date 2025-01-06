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
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumePositionTryFallbacks(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (auto result = consumeIdent<CSSValueNone>(range))
        return result;

    auto consume = [](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        Vector<CSSValueID, 3> idents;
        while (auto ident = consumeIdentRaw<CSSValueFlipBlock, CSSValueFlipInline, CSSValueFlipStart>(range)) {
            if (idents.contains(*ident))
                return nullptr;
            idents.append(*ident);
        }
        CSSValueListBuilder list;
        for (auto ident : idents)
            list.append(CSSPrimitiveValue::create(ident));
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    };
    return consumeCommaSeparatedListWithSingleValueOptimization(range, consume);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

