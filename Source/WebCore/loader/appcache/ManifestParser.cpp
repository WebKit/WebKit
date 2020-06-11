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
#include "ManifestParser.h"

#include "TextResourceDecoder.h"
#include <wtf/URL.h>
#include <wtf/text/StringView.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

enum Mode { Explicit, Fallback, OnlineAllowlist, Unknown };

static StringView manifestPath(const URL& manifestURL)
{
    auto manifestPath = manifestURL.path();
    ASSERT(manifestPath[0] == '/');
    manifestPath = manifestPath.substring(0, manifestPath.reverseFind('/') + 1);
    ASSERT(manifestPath[0] == manifestPath[manifestPath.length() - 1]);
    return manifestPath;
}

bool parseManifest(const URL& manifestURL, const String& manifestMIMEType, const char* data, int length, Manifest& manifest)
{
    ASSERT(manifest.explicitURLs.isEmpty());
    ASSERT(manifest.onlineAllowedURLs.isEmpty());
    ASSERT(manifest.fallbackURLs.isEmpty());
    manifest.allowAllNetworkRequests = false;

    auto manifestPath = WebCore::manifestPath(manifestURL);

    const char cacheManifestMIMEType[] = "text/cache-manifest";
    bool allowFallbackNamespaceOutsideManfestPath = equalLettersIgnoringASCIICase(manifestMIMEType, cacheManifestMIMEType);

    Mode mode = Explicit;

    String manifestString = TextResourceDecoder::create(ASCIILiteral::fromLiteralUnsafe(cacheManifestMIMEType), "UTF-8")->decodeAndFlush(data, length);
    
    // Look for the magic signature: "^\xFEFF?CACHE MANIFEST[ \t]?" (the BOM is removed by TextResourceDecoder).
    // Example: "CACHE MANIFEST #comment" is a valid signature.
    // Example: "CACHE MANIFEST;V2" is not.
    const char manifestSignature[] = "CACHE MANIFEST";
    if (!manifestString.startsWith(manifestSignature))
        return false;
    
    StringView manifestAfterSignature = StringView(manifestString).substring(sizeof(manifestSignature) - 1);
    auto upconvertedCharacters = manifestAfterSignature.upconvertedCharacters();
    const UChar* p = upconvertedCharacters;
    const UChar* end = p + manifestAfterSignature.length();

    if (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        return false;

    // Skip to the end of the line.
    while (p < end && *p != '\r' && *p != '\n')
        p++;

    while (1) {
        // Skip whitespace
        while (p < end && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t'))
            p++;
        
        if (p == end)
            break;
        
        const UChar* lineStart = p;
        
        // Find the end of the line
        while (p < end && *p != '\r' && *p != '\n')
            p++;
        
        // Check if we have a comment
        if (*lineStart == '#')
            continue;
        
        // Get rid of trailing whitespace
        const UChar* tmp = p - 1;
        while (tmp > lineStart && (*tmp == ' ' || *tmp == '\t'))
            tmp--;
        
        String line(lineStart, tmp - lineStart + 1);

        if (line == "CACHE:") 
            mode = Explicit;
        else if (line == "FALLBACK:")
            mode = Fallback;
        else if (line == "NETWORK:")
            mode = OnlineAllowlist;
        else if (line.endsWith(':'))
            mode = Unknown;
        else if (mode == Unknown)
            continue;
        else if (mode == Explicit || mode == OnlineAllowlist) {
            auto upconvertedLineCharacters = StringView(line).upconvertedCharacters();
            const UChar* p = upconvertedLineCharacters;
            const UChar* lineEnd = p + line.length();
            
            // Look for whitespace separating the URL from subsequent ignored tokens.
            while (p < lineEnd && *p != '\t' && *p != ' ') 
                p++;

            if (mode == OnlineAllowlist && p - upconvertedLineCharacters == 1 && line[0] == '*') {
                // Wildcard was found.
                manifest.allowAllNetworkRequests = true;
                continue;
            }

            URL url(manifestURL, line.substring(0, p - upconvertedLineCharacters));
            
            if (!url.isValid())
                continue;

            url.removeFragmentIdentifier();
            
            if (!equalIgnoringASCIICase(url.protocol(), manifestURL.protocol()))
                continue;
            
            if (mode == Explicit && manifestURL.protocolIs("https") && !protocolHostAndPortAreEqual(manifestURL, url))
                continue;
            
            if (mode == Explicit)
                manifest.explicitURLs.add(url.string());
            else
                manifest.onlineAllowedURLs.append(url);
            
        } else if (mode == Fallback) {
            auto upconvertedLineCharacters = StringView(line).upconvertedCharacters();
            const UChar* p = upconvertedLineCharacters;
            const UChar* lineEnd = p + line.length();
            
            // Look for whitespace separating the two URLs
            while (p < lineEnd && *p != '\t' && *p != ' ') 
                p++;

            if (p == lineEnd) {
                // There was no whitespace separating the URLs.
                continue;
            }
            
            URL namespaceURL(manifestURL, line.substring(0, p - upconvertedLineCharacters));
            if (!namespaceURL.isValid())
                continue;
            namespaceURL.removeFragmentIdentifier();

            if (!protocolHostAndPortAreEqual(manifestURL, namespaceURL))
                continue;

            // Although <https://html.spec.whatwg.org/multipage/offline.html#parsing-cache-manifests> (07/06/2017) saids
            // that we should always prefix match the manifest path we only do so if the manifest was served with a non-
            // standard HTTP Content-Type header for web compatibility.
            if (!allowFallbackNamespaceOutsideManfestPath && !namespaceURL.path().startsWith(manifestPath))
                continue;

            // Skip whitespace separating fallback namespace from URL.
            while (p < lineEnd && (*p == '\t' || *p == ' '))
                p++;

            // Look for whitespace separating the URL from subsequent ignored tokens.
            const UChar* fallbackStart = p;
            while (p < lineEnd && *p != '\t' && *p != ' ') 
                p++;

            URL fallbackURL(manifestURL, String(fallbackStart, p - fallbackStart));
            if (!fallbackURL.isValid())
                continue;
            fallbackURL.removeFragmentIdentifier();

            if (!protocolHostAndPortAreEqual(manifestURL, fallbackURL))
                continue;

            manifest.fallbackURLs.append(std::make_pair(namespaceURL, fallbackURL));            
        } else 
            ASSERT_NOT_REACHED();
    }

    return true;
}

}
