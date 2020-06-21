/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGVKernElement.h"

#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGVKernElement);

inline SVGVKernElement::SVGVKernElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::vkernTag));
}

Ref<SVGVKernElement> SVGVKernElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGVKernElement(tagName, document));
}

Optional<SVGKerningPair> SVGVKernElement::buildVerticalKerningPair() const
{
    // FIXME: Can this be shared with SVGHKernElement::buildVerticalKerningPair?

    auto& u1 = attributeWithoutSynchronization(SVGNames::u1Attr);
    auto& g1 = attributeWithoutSynchronization(SVGNames::g1Attr);
    if (u1.isEmpty() && g1.isEmpty())
        return WTF::nullopt;

    auto& u2 = attributeWithoutSynchronization(SVGNames::u2Attr);
    auto& g2 = attributeWithoutSynchronization(SVGNames::g2Attr);
    if (u2.isEmpty() && g2.isEmpty())
        return WTF::nullopt;

    auto glyphName1 = parseGlyphName(g1);
    if (!glyphName1)
        return WTF::nullopt;
    auto glyphName2 = parseGlyphName(g2);
    if (!glyphName2)
        return WTF::nullopt;
    auto unicodeString1 = parseKerningUnicodeString(u1);
    if (!unicodeString1)
        return WTF::nullopt;
    auto unicodeString2 = parseKerningUnicodeString(u2);
    if (!unicodeString2)
        return WTF::nullopt;

    bool ok = false;
    auto kerning = attributeWithoutSynchronization(SVGNames::kAttr).string().toFloat(&ok);
    if (!ok)
        return WTF::nullopt;

    return SVGKerningPair {
        WTFMove(unicodeString1->first),
        WTFMove(unicodeString1->second),
        WTFMove(*glyphName1),
        WTFMove(unicodeString2->first),
        WTFMove(unicodeString2->second),
        WTFMove(*glyphName2),
        kerning
    };
}

}

#endif // ENABLE(SVG_FONTS)
