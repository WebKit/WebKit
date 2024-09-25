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

#pragma once

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - Ident

std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange&, CSSValueID lower, CSSValueID upper);

template<typename... emptyBaseCase> bool identMatches(CSSValueID)
{
    return false;
}

template<CSSValueID head, CSSValueID... tail> bool identMatches(CSSValueID id)
{
    return id == head || identMatches<tail...>(id);
}

template<CSSValueID... names> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return std::nullopt;
    return range.consumeIncludingWhitespace().id();
}

template<CSSValueID... names> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return nullptr;
    return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().id());
}

template<typename Predicate, typename... Args> std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range, Predicate&& predicate, Args&&... args)
{
    if (auto keyword = range.peek().id(); predicate(keyword, std::forward<Args>(args)...)) {
        range.consumeIncludingWhitespace();
        return keyword;
    }
    return std::nullopt;
}

template<typename Predicate, typename... Args> RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range, Predicate&& predicate, Args&&... args)
{
    if (auto keyword = range.peek().id(); predicate(keyword, std::forward<Args>(args)...)) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(keyword);
    }
    return nullptr;
}

template<typename Map>
std::optional<typename Map::ValueType> consumeIdentUsingMapping(CSSParserTokenRange& range, Map& map)
{
    if (auto value = map.tryGet(range.peek().id())) {
        range.consumeIncludingWhitespace();
        return std::make_optional(*value);
    }
    return std::nullopt;
}

template<typename Map>
std::optional<typename Map::ValueType> peekIdentUsingMapping(CSSParserTokenRange& range, Map& map)
{
    if (auto value = map.tryGet(range.peek().id()))
        return std::make_optional(*value);
    return std::nullopt;
}

// MARK: <custom-ident>
// https://drafts.csswg.org/css-values/#custom-idents

String consumeCustomIdentRaw(CSSParserTokenRange&, bool shouldLowercase = false);
RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange&, bool shouldLowercase = false);

// MARK: <dashed-ident>
// https://drafts.csswg.org/css-values/#dashed-idents

String consumeDashedIdentRaw(CSSParserTokenRange&, bool shouldLowercase = false);
RefPtr<CSSPrimitiveValue> consumeDashedIdent(CSSParserTokenRange&, bool shouldLowercase = false);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
