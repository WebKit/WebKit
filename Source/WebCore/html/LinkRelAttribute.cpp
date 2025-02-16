/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#include <wtf/SortedArrayMap.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// https://html.spec.whatwg.org/#linkTypes

struct LinkTypeDetails {
    bool (*isEnabled)(const Document&);
    void (*updateRel)(LinkRelAttribute&);
};

static constexpr std::pair<ComparableLettersLiteral, LinkTypeDetails> linkTypesArray[] = {
    { "alternate"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.isAlternate = true; } } },
    { "apple-touch-icon"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.iconType = LinkIconType::TouchIcon; } } },
    { "apple-touch-icon-precomposed"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.iconType = LinkIconType::TouchPrecomposedIcon; } } },
    { "dns-prefetch"_s, { [](auto document) { return document.settings().linkDNSPrefetchEnabled(); }, [](auto relAttribute) { relAttribute.isDNSPrefetch = true; } } },
    { "expect"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.isInternalResourceLink = true; } } },
    { "icon"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.iconType = LinkIconType::Favicon; } } },
#if ENABLE(APPLICATION_MANIFEST)
    { "manifest"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.isApplicationManifest = true; } } },
#else
    { "manifest"_s, { [](auto) { return false; }, [](auto) { } } },
#endif
    { "modulepreload"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.isLinkModulePreload = true; } } },
    { "preconnect"_s, { [](auto document) { return document.settings().linkPreconnectEnabled(); }, [](auto relAttribute) { relAttribute.isLinkPreconnect = true; } } },
    { "prefetch"_s, { [](auto document) { return document.settings().linkPrefetchEnabled(); }, [](auto relAttribute) { relAttribute.isLinkPrefetch = true; } } },
    { "preload"_s, { [](auto document) { return document.settings().linkPreloadEnabled(); }, [](auto relAttribute) { relAttribute.isLinkPreload = true; } } },
#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    { "spatial-backdrop"_s, {
        [](auto document) { return document.settings().webPageSpatialBackdropEnabled(); },
        [](auto relAttribute) { relAttribute.isSpatialBackdrop = true; }
    } },
#endif
    { "stylesheet"_s, { [](auto) { return true; }, [](auto relAttribute) { relAttribute.isStyleSheet = true; } } },
};

static constexpr SortedArrayMap linkTypes { linkTypesArray };

LinkRelAttribute::LinkRelAttribute(Document& document, StringView rel)
{
    if (auto linkType = linkTypes.tryGet(rel)) {
        if (linkType->isEnabled(document))
            linkType->updateRel(*this);
        return;
    }
    if (equalLettersIgnoringASCIICase(rel, "shortcut icon"_s))
        iconType = LinkIconType::Favicon;
    else if (equalLettersIgnoringASCIICase(rel, "alternate stylesheet"_s) || equalLettersIgnoringASCIICase(rel, "stylesheet alternate"_s)) {
        isStyleSheet = true;
        isAlternate = true;
    } else {
        // Tokenize the rel attribute and set bits based on specific keywords that we find.
        unsigned length = rel.length();
        unsigned start = 0;
        while (start < length) {
            if (isASCIIWhitespace(rel[start])) {
                start++;
                continue;
            }
            unsigned end = start + 1;
            while (end < length && !isASCIIWhitespace(rel[end]))
                end++;
            if (auto linkType = linkTypes.tryGet(rel.substring(start, end - start))) {
                if (linkType->isEnabled(document))
                    linkType->updateRel(*this);
            }
            start = end;
        }
    }
}

bool LinkRelAttribute::isSupported(Document& document, StringView attribute)
{
    if (auto linkType = linkTypes.tryGet(attribute))
        return linkType->isEnabled(document);
    return false;
}

}
