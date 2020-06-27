/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SVGTransformList.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

ExceptionOr<RefPtr<SVGTransform>> SVGTransformList::consolidate()
{
    auto result = canAlterList();
    if (result.hasException())
        return result.releaseException();
    ASSERT(result.releaseReturnValue());

    // Spec: If the list was empty, then a value of null is returned.
    if (m_items.isEmpty())
        return nullptr;

    if (m_items.size() == 1)
        return makeRefPtr(at(0).get());

    auto newItem = SVGTransform::create(concatenate());
    clearItems();

    auto item = append(WTFMove(newItem));
    commitChange();
    return makeRefPtr(item.get());
}

AffineTransform SVGTransformList::concatenate() const
{
    AffineTransform result;
    for (auto& transform : m_items)
        result *= transform->matrix()->value();
    return result;
}

template<typename CharacterType> bool SVGTransformList::parseGeneric(StringParsingBuffer<CharacterType>& buffer)
{
    bool delimParsed = false;
    while (buffer.hasCharactersRemaining()) {
        delimParsed = false;
        skipOptionalSVGSpaces(buffer);
        
        auto parsedTransformType = SVGTransformable::parseTransformType(buffer);
        if (!parsedTransformType)
            return false;

        auto parsedTransformValue = SVGTransformable::parseTransformValue(*parsedTransformType, buffer);
        if (!parsedTransformValue)
            return false;

        append(SVGTransform::create(*parsedTransformValue));

        skipOptionalSVGSpaces(buffer);

        if (skipExactly(buffer, ','))
            delimParsed = true;

        skipOptionalSVGSpaces(buffer);
    }
    return !delimParsed;
}

void SVGTransformList::parse(StringView value)
{
    clearItems();

    bool parsingSucceeded = readCharactersForParsing(value, [&](auto buffer) {
        return parseGeneric(buffer);
    });
    
    if (!parsingSucceeded)
        clearItems();
}

bool SVGTransformList::parse(StringParsingBuffer<LChar>& buffer)
{
    return parseGeneric(buffer);
}

bool SVGTransformList::parse(StringParsingBuffer<UChar>& buffer)
{
    return parseGeneric(buffer);
}

String SVGTransformList::valueAsString() const
{
    StringBuilder builder;
    for (const auto& transfrom : m_items) {
        if (builder.length())
            builder.append(' ');

        builder.append(transfrom->value().valueAsString());
    }
    return builder.toString();
}

}

