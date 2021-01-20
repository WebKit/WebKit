/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "LinkIconCollector.h"

#include "Document.h"
#include "ElementChildIterator.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "LinkIconType.h"

namespace WebCore {

const unsigned defaultTouchIconWidth = 60;

static unsigned iconSize(const LinkIcon& icon)
{
    if (icon.size)
        return *icon.size;

    if (icon.type == LinkIconType::TouchIcon || icon.type == LinkIconType::TouchPrecomposedIcon)
        return defaultTouchIconWidth;

    return 0;
}

static int compareIcons(const LinkIcon& a, const LinkIcon& b)
{
    // Apple Touch icons always come first.
    if (a.type == LinkIconType::Favicon && b.type != LinkIconType::Favicon)
        return 1;
    if (a.type == LinkIconType::Favicon && b.type != LinkIconType::Favicon)
        return -1;

    unsigned aSize = iconSize(a);
    unsigned bSize = iconSize(b);

    if (bSize > aSize)
        return 1;
    if (bSize < aSize)
        return -1;

    // A Precomposed icon should come first if both icons have the same size.
    if (a.type != LinkIconType::TouchPrecomposedIcon && b.type == LinkIconType::TouchPrecomposedIcon)
        return 1;
    if (b.type != LinkIconType::TouchPrecomposedIcon && a.type == LinkIconType::TouchPrecomposedIcon)
        return -1;

    return 0;
}

auto LinkIconCollector::iconsOfTypes(OptionSet<LinkIconType> iconTypes) -> Vector<LinkIcon>
{
    auto head = makeRefPtr(m_document.head());
    if (!head)
        return { };

    Vector<LinkIcon> icons;

    for (auto& linkElement : childrenOfType<HTMLLinkElement>(*head)) {
        if (!linkElement.iconType())
            continue;

        auto iconType = *linkElement.iconType();
        if (!iconTypes.contains(iconType))
            continue;

        auto url = linkElement.href();
        if (!url.protocolIsInHTTPFamily())
            continue;

        // This icon size parsing is a little wonky - it only parses the first
        // part of the size, "60x70" becomes "60". This is for compatibility reasons
        // and is probably good enough for now.
        Optional<unsigned> iconSize;

        if (linkElement.sizes().length()) {
            bool ok;
            unsigned size = linkElement.sizes().item(0).string().stripWhiteSpace().toUInt(&ok);
            if (ok)
                iconSize = size;
        }

        Vector<std::pair<String, String>> attributes;
        if (linkElement.hasAttributes()) {
            attributes.reserveCapacity(linkElement.attributeCount());
            for (const Attribute& attribute : linkElement.attributesIterator())
                attributes.uncheckedAppend({ attribute.localName(), attribute.value() });
        }

        icons.append({ url, iconType, linkElement.type(), iconSize, WTFMove(attributes) });
    }

    std::sort(icons.begin(), icons.end(), [](auto& a, auto& b) {
        return compareIcons(a, b) < 0;
    });

    return icons;
}

}
