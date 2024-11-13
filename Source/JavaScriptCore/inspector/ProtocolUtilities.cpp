/*
 * Copyright (C) 2024 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProtocolUtilities.h"

#include <wtf/text/TextPosition.h>

namespace Inspector {

namespace Protocol {

ErrorStringOr<TextPosition> parseSourcePosition(Ref<JSON::Object>&& payload)
{
    auto lineValue = payload->getInteger("line"_s);
    if (!lineValue)
        return makeUnexpected("Unexpected non-integer line"_s);

    auto columnValue = payload->getInteger("column"_s);
    if (!columnValue)
        return makeUnexpected("Unexpected non-integer column"_s);

    return { { OrdinalNumber::fromZeroBasedInt(*lineValue), OrdinalNumber::fromZeroBasedInt(*columnValue) } };
}

Ref<GenericTypes::SourcePosition> buildSourcePosition(const TextPosition& position)
{
    return GenericTypes::SourcePosition::create()
        .setLine(position.m_line.zeroBasedInt())
        .setColumn(position.m_column.zeroBasedInt())
        .release();
}

ErrorStringOr<std::pair<TextPosition, TextPosition>> parseSourceRange(Ref<JSON::Object>&& payload)
{
    auto startObject = payload->getObject("start"_s);
    if (!startObject)
        return makeUnexpected("Unexpected non-object start"_s);

    auto start = parseSourcePosition(startObject.releaseNonNull());
    if (!start)
        return makeUnexpected(start.error());

    auto endObject = payload->getObject("end"_s);
    if (!endObject)
        return makeUnexpected("Unexpected non-object end"_s);

    auto end = parseSourcePosition(endObject.releaseNonNull());
    if (!end)
        return makeUnexpected(end.error());

    if (*start > *end)
        return makeUnexpected("Unexpected end before start"_s);

    return { { *start, *end } };
}

Ref<GenericTypes::SourceRange> buildSourceRange(const std::pair<TextPosition, TextPosition>& range)
{
    return GenericTypes::SourceRange::create()
        .setStart(buildSourcePosition(range.first))
        .setEnd(buildSourcePosition(range.second))
        .release();
}

} // namespace Protocol

} // namespace Inspector
