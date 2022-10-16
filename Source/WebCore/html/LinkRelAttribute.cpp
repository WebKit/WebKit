/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "LinkRelAttribute.h"

#include "Document.h"
#include "LinkIconType.h"
#include "Settings.h"
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Keep LinkRelAttribute::isSupported() in sync when updating this constructor.
LinkRelAttribute::LinkRelAttribute(Document& document, StringView rel)
{
    if (equalLettersIgnoringASCIICase(rel, "stylesheet"_s))
        isStyleSheet = true;
    else if (equalLettersIgnoringASCIICase(rel, "icon"_s) || equalLettersIgnoringASCIICase(rel, "shortcut icon"_s))
        iconType = LinkIconType::Favicon;
    else if (equalLettersIgnoringASCIICase(rel, "apple-touch-icon"_s))
        iconType = LinkIconType::TouchIcon;
    else if (equalLettersIgnoringASCIICase(rel, "apple-touch-icon-precomposed"_s))
        iconType = LinkIconType::TouchPrecomposedIcon;
    else if (equalLettersIgnoringASCIICase(rel, "dns-prefetch"_s))
        isDNSPrefetch = true;
    else if (document.settings().linkPreconnectEnabled() && equalLettersIgnoringASCIICase(rel, "preconnect"_s))
        isLinkPreconnect = true;
    else if (document.settings().linkPreloadEnabled() && equalLettersIgnoringASCIICase(rel, "preload"_s))
        isLinkPreload = true;
    else if (document.settings().linkPrefetchEnabled() && equalLettersIgnoringASCIICase(rel, "prefetch"_s))
        isLinkPrefetch = true;
    else if (equalLettersIgnoringASCIICase(rel, "alternate stylesheet"_s) || equalLettersIgnoringASCIICase(rel, "stylesheet alternate"_s)) {
        isStyleSheet = true;
        isAlternate = true;
#if ENABLE(APPLICATION_MANIFEST)
    } else if (equalLettersIgnoringASCIICase(rel, "manifest"_s)) {
        isApplicationManifest = true;
#endif
    } else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        for (auto line : rel.split('\n')) {
            for (auto word : line.split(' ')) {
                if (equalLettersIgnoringASCIICase(word, "stylesheet"_s))
                    isStyleSheet = true;
                else if (equalLettersIgnoringASCIICase(word, "alternate"_s))
                    isAlternate = true;
                else if (equalLettersIgnoringASCIICase(word, "icon"_s))
                    iconType = LinkIconType::Favicon;
                else if (equalLettersIgnoringASCIICase(word, "apple-touch-icon"_s))
                    iconType = LinkIconType::TouchIcon;
                else if (equalLettersIgnoringASCIICase(word, "apple-touch-icon-precomposed"_s))
                    iconType = LinkIconType::TouchPrecomposedIcon;
            }
        }
    }
}

// https://html.spec.whatwg.org/#linkTypes
bool LinkRelAttribute::isSupported(Document& document, StringView attribute)
{
    static constexpr ASCIILiteral supportedAttributes[] = {
        "alternate"_s, "dns-prefetch"_s, "icon"_s, "stylesheet"_s, "apple-touch-icon"_s, "apple-touch-icon-precomposed"_s,
#if ENABLE(APPLICATION_MANIFEST)
        "manifest"_s,
#endif
    };

    for (auto supportedAttribute : supportedAttributes) {
        if (equalIgnoringASCIICase(attribute, supportedAttribute))
            return true;
    }

    if (document.settings().linkPreconnectEnabled() && equalLettersIgnoringASCIICase(attribute, "preconnect"_s))
        return true;

    if (document.settings().linkPreloadEnabled() && equalLettersIgnoringASCIICase(attribute, "preload"_s))
        return true;

    if (document.settings().linkPrefetchEnabled() && equalLettersIgnoringASCIICase(attribute, "prefetch"_s))
        return true;

    return false;
}

}
