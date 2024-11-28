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
#include "CSSPropertyParserConsumer+Conditional.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <single-container-name> = <custom-ident excluding=[none,and,or,not]>+
    // https://drafts.csswg.org/css-conditional-5/#propdef-container-name

    switch (range.peek().id()) {
    case CSSValueNone:
    case CSSValueAnd:
    case CSSValueOr:
    case CSSValueNot:
        return nullptr;
    default:
        if (auto ident = consumeCustomIdent(range))
            return ident;
        return nullptr;
    }
}

RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'container-name'> = none | <custom-ident excluding=[none,and,or,not]>+
    // https://drafts.csswg.org/css-conditional-5/#propdef-container-name

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder list;
    do {
        auto name = consumeSingleContainerName(range, context);
        if (!name)
            break;
        list.append(name.releaseNonNull());
    } while (!range.atEnd());
    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
