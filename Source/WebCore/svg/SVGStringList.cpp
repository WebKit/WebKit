/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

#include "SVGParserUtilities.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

void SVGStringList::setFromCommaSeparatedTokens(StringView string)
{
    m_delimiter = ',';
    clearItems();
    parseCommaSeparatedTokens(string);

    if (m_items.isEmpty())
        m_items.append(emptyString());
}

void SVGStringList::setFromSpaceSeparatedTokens(StringView string)
{
    m_delimiter = ' ';
    clearItems();
    parseSpaceSeparatedTokens(string);

    if (m_items.isEmpty())
        m_items.append(emptyString());
}

// FIXME: This should use a general-purpose WTF space-separated token parser. https://webkit.org/b/312153
// https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#set-of-comma-separated-tokens
void SVGStringList::parseCommaSeparatedTokens(StringView data)
{
    for (auto token : data.splitAllowingEmptyEntries(','))
        m_items.append(token.trim(isASCIIWhitespace).toString());
}

// FIXME: This should use a general-purpose WTF space-separated token parser. https://webkit.org/b/312153
// https://bugs.webkit.org/show_bug.cgi?id=312153
// https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#set-of-space-separated-tokens
void SVGStringList::parseSpaceSeparatedTokens(StringView data)
{
    readCharactersForParsing(data, [&](auto buffer) {
        skipOptionalSVGSpaces(buffer);

        while (buffer.hasCharactersRemaining()) {
            auto start = buffer.position();

            skipUntil<isASCIIWhitespace>(buffer);

            if (buffer.position() == start)
                break;

            m_items.append(String({ start, buffer.position() }));
            skipOptionalSVGSpaces(buffer);
        }

        return buffer.atEnd();
    });
}

String SVGStringList::valueAsString() const
{
    auto delimiter = m_delimiter == ',' ? ", "_s : " "_s;
    return makeString(interleave(m_items, delimiter));
}

}
