/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "TextExtractionURLCache.h"

namespace WebKit {

void TextExtractionURLCache::clear()
{
    m_shortenedStringToURLMap.clear();
    m_urlToShortenedStringMap.clear();
    m_shortenedStringCounts.clear();
}

String TextExtractionURLCache::add(const String& shortenedString, const URL& originalURL, ExtractedURLType type)
{
    if (shortenedString.isEmpty() || originalURL.isEmpty())
        return shortenedString;

    if (auto existingString = m_urlToShortenedStringMap.get(originalURL); !existingString.isEmpty())
        return existingString;

    auto addSuffix = [&](StringView string, unsigned suffix) {
        auto fullStopIndex = string.reverseFind('.');
        if (isASCIIDigit(string[string.length() - 1]))
            return makeString(string, '-', suffix);

        if (type == ExtractedURLType::Link) {
            auto slashIndex = string.reverseFind('/');
            if ((slashIndex == notFound) || (fullStopIndex != notFound && slashIndex > fullStopIndex)) {
                if (slashIndex == notFound && fullStopIndex != notFound)
                    return makeString(string, '/', suffix);
                return makeString(string, suffix);
            }
        }

        if (fullStopIndex == notFound)
            return makeString(string, suffix);

        if (fullStopIndex && isASCIIDigit(string[fullStopIndex - 1]))
            return makeString(string.left(fullStopIndex), '-', suffix, string.right(string.length() - fullStopIndex));

        return makeString(string.left(fullStopIndex), suffix, string.right(string.length() - fullStopIndex));
    };

    m_shortenedStringCounts.add(shortenedString);

    if (auto addResult = m_shortenedStringToURLMap.add(shortenedString, originalURL); addResult.isNewEntry) {
        m_urlToShortenedStringMap.set(originalURL, shortenedString);
        return shortenedString;
    }

    auto count = m_shortenedStringCounts.count(shortenedString);
    String newShortenedString;
    do {
        newShortenedString = addSuffix(shortenedString, count++);
    } while (!m_shortenedStringToURLMap.add(newShortenedString, originalURL).isNewEntry);
    m_urlToShortenedStringMap.add(originalURL, newShortenedString);

    return newShortenedString;
}

URL TextExtractionURLCache::urlForShortenedString(const String& string) const
{
    if (string.isEmpty())
        return { };

    return m_shortenedStringToURLMap.get(string);
}

} // namespace WebKit
