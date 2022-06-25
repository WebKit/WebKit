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
#include "SVGPointList.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

bool SVGPointList::parse(StringView value)
{
    clearItems();

    return readCharactersForParsing(value, [&](auto buffer) {
        skipOptionalSVGSpaces(buffer);

        bool delimParsed = false;
        while (buffer.hasCharactersRemaining()) {
            delimParsed = false;

            auto xPos = parseNumber(buffer);
            if (!xPos)
                return false;

            auto yPos = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
            if (!yPos)
                return false;

            skipOptionalSVGSpaces(buffer);

            if (skipExactly(buffer, ','))
                delimParsed = true;

            skipOptionalSVGSpaces(buffer);

            append(SVGPoint::create({ *xPos, *yPos }));
        }

        // FIXME: Should this clearItems() on failure like SVGTransformList does?

        return !delimParsed;
    });
}

String SVGPointList::valueAsString() const
{
    StringBuilder builder;

    for (const auto& point : m_items) {
        if (builder.length())
            builder.append(' ');

        builder.append(point->x(), ' ', point->y());
    }

    return builder.toString();
}

}
