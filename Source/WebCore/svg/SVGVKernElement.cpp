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

namespace WebCore {

inline SVGVKernElement::SVGVKernElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::vkernTag));
}

Ref<SVGVKernElement> SVGVKernElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGVKernElement(tagName, document));
}

bool SVGVKernElement::buildVerticalKerningPair(SVGKerningPair& kerningPair) const
{
    String u1 = attributeWithoutSynchronization(SVGNames::u1Attr);
    String g1 = attributeWithoutSynchronization(SVGNames::g1Attr);
    String u2 = attributeWithoutSynchronization(SVGNames::u2Attr);
    String g2 = attributeWithoutSynchronization(SVGNames::g2Attr);
    if ((u1.isEmpty() && g1.isEmpty()) || (u2.isEmpty() && g2.isEmpty()))
        return false;

    if (parseGlyphName(g1, kerningPair.glyphName1)
        && parseGlyphName(g2, kerningPair.glyphName2)
        && parseKerningUnicodeString(u1, kerningPair.unicodeRange1, kerningPair.unicodeName1)
        && parseKerningUnicodeString(u2, kerningPair.unicodeRange2, kerningPair.unicodeName2)) {
        bool ok = false;
        kerningPair.kerning = attributeWithoutSynchronization(SVGNames::kAttr).string().toFloat(&ok);
        return ok;
    }
    return false;
}

}

#endif // ENABLE(SVG_FONTS)
