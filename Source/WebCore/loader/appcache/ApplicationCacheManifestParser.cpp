/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ApplicationCacheManifestParser.h"

#include "ParsingUtilities.h"
#include "TextResourceDecoder.h"
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

enum class ApplicationCacheParserMode { Explicit, Fallback, OnlineAllowlist, Unknown };

static StringView manifestPath(const URL& manifestURL)
{
    auto manifestPath = manifestURL.path();
    ASSERT(manifestPath[0] == '/');
    manifestPath = manifestPath.substring(0, manifestPath.reverseFind('/') + 1);
    ASSERT(manifestPath[0] == manifestPath[manifestPath.length() - 1]);
    return manifestPath;
}

template<typename CharacterType> static constexpr bool isManifestWhitespace(CharacterType character)
{
    return character == ' ' || character == '\t';
}

template<typename CharacterType> static constexpr bool isManifestNewline(CharacterType character)
{
    return character == '\n' || character == '\r';
}

template<typename CharacterType> static constexpr bool isManifestWhitespaceOrNewline(CharacterType character)
{
    return isManifestWhitespace(character) || isManifestNewline(character);
}

template<typename CharacterType> static URL makeManifestURL(const URL& manifestURL, const CharacterType* start, const CharacterType* end)
{
    URL url(manifestURL, String(start, end - start));
    url.removeFragmentIdentifier();
    return url;
}

template<typename CharacterType> static constexpr CharacterType cacheManifestIdentifier[] = { 'C', 'A', 'C', 'H', 'E', ' ', 'M', 'A', 'N', 'I', 'F', 'E', 'S', 'T' };
template<typename CharacterType> static constexpr CharacterType cacheModeIdentifier[] = { 'C', 'A', 'C', 'H', 'E' };
template<typename CharacterType> static constexpr CharacterType fallbackModeIdentifier[] = { 'F', 'A', 'L', 'L', 'B', 'A', 'C', 'K' };
template<typename CharacterType> static constexpr CharacterType networkModeIdentifier[] = { 'N', 'E', 'T', 'W', 'O', 'R', 'K' };

Optional<ApplicationCacheManifest> parseApplicationCacheManifest(const URL& manifestURL, const String& manifestMIMEType, const char* data, int length)
{
    static constexpr const char cacheManifestMIMEType[] = "text/cache-manifest";
    bool allowFallbackNamespaceOutsideManifestPath = equalLettersIgnoringASCIICase(manifestMIMEType, cacheManifestMIMEType);
    auto manifestPath = WebCore::manifestPath(manifestURL);

    auto manifestString = TextResourceDecoder::create(ASCIILiteral::fromLiteralUnsafe(cacheManifestMIMEType), "UTF-8")->decodeAndFlush(data, length);

    return readCharactersForParsing(manifestString, [&](auto buffer) -> Optional<ApplicationCacheManifest> {
        using CharacterType = typename decltype(buffer)::CharacterType;
    
        ApplicationCacheManifest manifest;
        auto mode = ApplicationCacheParserMode::Explicit;

        // Look for the magic signature: "^\xFEFF?CACHE MANIFEST[ \t]?" (the BOM is removed by TextResourceDecoder).
        // Example: "CACHE MANIFEST #comment" is a valid signature.
        // Example: "CACHE MANIFEST;V2" is not.
        if (!skipCharactersExactly(buffer, cacheManifestIdentifier<CharacterType>))
            return WTF::nullopt;
    
        if (buffer.hasCharactersRemaining() && !isManifestWhitespaceOrNewline(*buffer))
            return WTF::nullopt;

        // Skip to the end of the line.
        skipUntil<CharacterType, isManifestNewline>(buffer);

        while (1) {
            // Skip whitespace
            skipWhile<CharacterType, isManifestWhitespaceOrNewline>(buffer);
            
            if (buffer.atEnd())
                break;
            
            auto lineStart = buffer.position();
            
            // Find the end of the line
            skipUntil<CharacterType, isManifestNewline>(buffer);
            
            // Line is a comment, skip to the next line.
            if (*lineStart == '#')
                continue;
            
            // Get rid of trailing whitespace
            auto lineEnd = buffer.position() - 1;
            while (lineEnd > lineStart && isManifestWhitespace(*lineEnd))
                --lineEnd;

            auto lineBuffer = StringParsingBuffer { lineStart, lineEnd + 1 };

            if (lineBuffer[lineBuffer.lengthRemaining() - 1] == ':') {
                if (skipCharactersExactly(lineBuffer, cacheModeIdentifier<CharacterType>) && lineBuffer.lengthRemaining() == 1) {
                    mode = ApplicationCacheParserMode::Explicit;
                    continue;
                }
                if (skipCharactersExactly(lineBuffer, fallbackModeIdentifier<CharacterType>) && lineBuffer.lengthRemaining() == 1) {
                    mode = ApplicationCacheParserMode::Fallback;
                    continue;
                }
                if (skipCharactersExactly(lineBuffer, networkModeIdentifier<CharacterType>) && lineBuffer.lengthRemaining() == 1) {
                    mode = ApplicationCacheParserMode::OnlineAllowlist;
                    continue;
                }

                // If the line (excluding the trailing whitespace) ends with a ':' and isn't one of the known mode
                // headers, transition to the 'Unknown' mode.
                mode = ApplicationCacheParserMode::Unknown;
                continue;
            }
    
            switch (mode) {
            case ApplicationCacheParserMode::Unknown:
                continue;
            
            case ApplicationCacheParserMode::Explicit: {
                // Look for whitespace separating the URL from subsequent ignored tokens.
                skipUntil<CharacterType, isManifestWhitespace>(lineBuffer);

                auto url = makeManifestURL(manifestURL, lineStart, lineBuffer.position());
                if (!url.isValid())
                    continue;
                
                if (!equalIgnoringASCIICase(url.protocol(), manifestURL.protocol()))
                    continue;
                
                if (manifestURL.protocolIs("https") && !protocolHostAndPortAreEqual(manifestURL, url))
                    continue;
                
                manifest.explicitURLs.add(url.string());
                continue;
            }

            case ApplicationCacheParserMode::OnlineAllowlist: {
                // Look for whitespace separating the URL from subsequent ignored tokens.
                skipUntil<CharacterType, isManifestWhitespace>(lineBuffer);

                if (lineBuffer.position() - lineStart == 1 && *lineStart == '*') {
                    // Wildcard was found.
                    manifest.allowAllNetworkRequests = true;
                    continue;
                }
                
                auto url = makeManifestURL(manifestURL, lineStart, lineBuffer.position());
                if (!url.isValid())
                    continue;
                
                if (!equalIgnoringASCIICase(url.protocol(), manifestURL.protocol()))
                    continue;

                manifest.onlineAllowedURLs.append(url);
                continue;
            }
            
            case ApplicationCacheParserMode::Fallback: {
                // Look for whitespace separating the two URLs
                skipUntil<CharacterType, isManifestWhitespace>(lineBuffer);

                if (lineBuffer.atEnd()) {
                    // There was no whitespace separating the URLs.
                    continue;
                }

                auto namespaceURL = makeManifestURL(manifestURL, lineStart, lineBuffer.position());
                if (!namespaceURL.isValid())
                    continue;

                if (!protocolHostAndPortAreEqual(manifestURL, namespaceURL))
                    continue;

                // Although <https://html.spec.whatwg.org/multipage/offline.html#parsing-cache-manifests> (07/06/2017) saids
                // that we should always prefix match the manifest path we only do so if the manifest was served with a non-
                // standard HTTP Content-Type header for web compatibility.
                if (!allowFallbackNamespaceOutsideManifestPath && !namespaceURL.path().startsWith(manifestPath))
                    continue;

                // Skip whitespace separating fallback namespace from URL.
                skipWhile<CharacterType, isManifestWhitespace>(lineBuffer);

                auto fallbackStart = lineBuffer.position();

                // Look for whitespace separating the URL from subsequent ignored tokens.
                skipUntil<CharacterType, isManifestWhitespace>(lineBuffer);

                auto fallbackURL = makeManifestURL(manifestURL, fallbackStart, lineBuffer.position());
                if (!fallbackURL.isValid())
                    continue;

                if (!protocolHostAndPortAreEqual(manifestURL, fallbackURL))
                    continue;

                manifest.fallbackURLs.append(std::make_pair(namespaceURL, fallbackURL));
                continue;
            }
            }
            
            ASSERT_NOT_REACHED();
        }

        return manifest;
    });
}

}
