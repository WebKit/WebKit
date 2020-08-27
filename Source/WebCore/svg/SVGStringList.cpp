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
#include "SVGStringList.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

bool SVGStringList::parse(StringView data, UChar delimiter)
{
    clearItems();

    auto isSVGSpaceOrDelimiter = [delimiter](auto c) {
        return isSVGSpace(c) || c == delimiter;
    };

    return readCharactersForParsing(data, [&](auto buffer) {
        skipOptionalSVGSpaces(buffer);

        while (buffer.hasCharactersRemaining()) {
            auto start = buffer.position();
            
            // FIXME: It would be a nice improvement to add a variant of skipUntil which worked
            // with lambda predicates.
            while (buffer.hasCharactersRemaining() && !isSVGSpaceOrDelimiter(*buffer))
                ++buffer;

            if (buffer.position() == start)
                break;

            m_items.append(String(start, buffer.position() - start));
            skipOptionalSVGSpacesOrDelimiter(buffer, delimiter);
        }

        // FIXME: Should this clearItems() on failure like SVGTransformList does?

        return buffer.atEnd();
    });
}

String SVGStringList::valueAsString() const
{
    StringBuilder builder;

    for (const auto& string : m_items) {
        if (builder.length())
            builder.append(' ');

        builder.append(string);
    }

    return builder.toString();
}

}
