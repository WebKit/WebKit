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
#include "CSSPropertyParserConsumer+ViewTransition.h"

#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeViewTransitionClass(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'view-transition-class'> = none | <custom-ident>+
    // https://drafts.csswg.org/css-view-transitions-2/#view-transition-class-prop

    if (auto noneValue = consumeIdent<CSSValueNone>(range))
        return noneValue;

    CSSValueListBuilder list;
    do {
        if (range.peek().id() == CSSValueNone)
            return nullptr;

        auto ident = consumeCustomIdent(range);
        if (!ident)
            return nullptr;

        list.append(ident.releaseNonNull());
    } while (!range.atEnd());

    if (list.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeViewTransitionTypes(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'types'> = none | <custom-ident>+
    // https://www.w3.org/TR/css-view-transitions-2/#descdef-view-transition-types

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueListBuilder list;
    do {
        if (range.peek().id() == CSSValueNone)
            return nullptr;
        auto type = consumeCustomIdent(range);
        if (!type)
            return nullptr;
        if (type->customIdent().startsWith("-ua-"_s))
            return nullptr;
        list.append(type.releaseNonNull());
    } while (!range.atEnd());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
