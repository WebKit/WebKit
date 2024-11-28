/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Primitives.h"

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserOptions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

bool consumeCommaIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != CommaToken)
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

bool consumeSlashIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != DelimiterToken || value.delimiter() != '/')
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

CSSParserTokenRange consumeFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken);
    CSSParserTokenRange contents = range.consumeBlock();
    range.consumeWhitespace();
    contents.consumeWhitespace();
    return contents;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
