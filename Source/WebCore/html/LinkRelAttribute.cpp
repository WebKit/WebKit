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

#include "LinkIconType.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

LinkRelAttribute::LinkRelAttribute()
{
}

// Keep LinkRelAttribute::isSupported() in sync when updating this constructor.
LinkRelAttribute::LinkRelAttribute(const String& rel)
{
    if (equalLettersIgnoringASCIICase(rel, "stylesheet"))
        isStyleSheet = true;
    else if (equalLettersIgnoringASCIICase(rel, "icon") || equalLettersIgnoringASCIICase(rel, "shortcut icon"))
        iconType = LinkIconType::Favicon;
    else if (equalLettersIgnoringASCIICase(rel, "apple-touch-icon"))
        iconType = LinkIconType::TouchIcon;
    else if (equalLettersIgnoringASCIICase(rel, "apple-touch-icon-precomposed"))
        iconType = LinkIconType::TouchPrecomposedIcon;
    else if (equalLettersIgnoringASCIICase(rel, "dns-prefetch"))
        isDNSPrefetch = true;
    else if (RuntimeEnabledFeatures::sharedFeatures().linkPreloadEnabled() && equalLettersIgnoringASCIICase(rel, "preload"))
        isLinkPreload = true;
    else if (equalLettersIgnoringASCIICase(rel, "alternate stylesheet") || equalLettersIgnoringASCIICase(rel, "stylesheet alternate")) {
        isStyleSheet = true;
        isAlternate = true;
    } else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        String relCopy = rel;
        relCopy.replace('\n', ' ');
        for (auto word : StringView(relCopy).split(' ')) {
            if (equalLettersIgnoringASCIICase(word, "stylesheet"))
                isStyleSheet = true;
            else if (equalLettersIgnoringASCIICase(word, "alternate"))
                isAlternate = true;
            else if (equalLettersIgnoringASCIICase(word, "icon"))
                iconType = LinkIconType::Favicon;
            else if (equalLettersIgnoringASCIICase(word, "apple-touch-icon"))
                iconType = LinkIconType::TouchIcon;
            else if (equalLettersIgnoringASCIICase(word, "apple-touch-icon-precomposed"))
                iconType = LinkIconType::TouchPrecomposedIcon;
#if ENABLE(LINK_PREFETCH)
            else if (equalLettersIgnoringASCIICase(word, "prefetch"))
                isLinkPrefetch = true;
            else if (equalLettersIgnoringASCIICase(word, "subresource"))
                isLinkSubresource = true;
#endif
        }
    }
}

// https://html.spec.whatwg.org/#linkTypes
bool LinkRelAttribute::isSupported(StringView attribute)
{
    static const char* const supportedAttributes[] = {
        "alternate", "dns-prefetch", "icon", "stylesheet", "apple-touch-icon", "apple-touch-icon-precomposed",
#if ENABLE(LINK_PREFETCH)
        "prefetch", "subresource",
#endif
    };

    for (auto* supportedAttribute : supportedAttributes) {
        if (equalIgnoringASCIICase(attribute, supportedAttribute))
            return true;
    }

    if (RuntimeEnabledFeatures::sharedFeatures().linkPreloadEnabled() && equalIgnoringASCIICase(attribute, "preload"))
        return true;

    return false;
}

}
