/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Ident.h"

#include "CSSParserIdioms.h"
#include "CSSValuePool.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return std::nullopt;
    return range.consumeIncludingWhitespace().id();
}

RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (auto result = consumeIdentRaw(range))
        return CSSPrimitiveValue::create(*result);
    return nullptr;
}

static std::optional<CSSValueID> consumeIdentRangeRaw(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return std::nullopt;
    return consumeIdentRaw(range);
}

RefPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    auto value = consumeIdentRangeRaw(range, lower, upper);
    if (!value)
        return nullptr;
    return CSSPrimitiveValue::create(*value);
}

// MARK: <custom-ident>
// https://drafts.csswg.org/css-values/#custom-idents

String consumeCustomIdentRaw(CSSParserTokenRange& range, bool shouldLowercase)
{
    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
        return String();
    auto identifier = range.consumeIncludingWhitespace().value();
    return shouldLowercase ? identifier.convertToASCIILowercase() : identifier.toString();
}

RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange& range, bool shouldLowercase)
{
    auto identifier = consumeCustomIdentRaw(range, shouldLowercase);
    if (identifier.isNull())
        return nullptr;
    return CSSPrimitiveValue::createCustomIdent(WTFMove(identifier));
}

// MARK: <dashed-ident>
// https://drafts.csswg.org/css-values/#dashed-idents

RefPtr<CSSPrimitiveValue> consumeDashedIdent(CSSParserTokenRange& range, bool shouldLowercase)
{
    auto rangeCopy = range;
    auto result = consumeCustomIdent(range, shouldLowercase);
    if (result && result->stringValue().startsWith("--"_s))
        return result;
    range = rangeCopy;
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
