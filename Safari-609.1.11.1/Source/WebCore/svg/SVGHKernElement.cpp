/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "SVGHKernElement.h"

#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGHKernElement);

inline SVGHKernElement::SVGHKernElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::hkernTag));
}

Ref<SVGHKernElement> SVGHKernElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGHKernElement(tagName, document));
}

bool SVGHKernElement::buildHorizontalKerningPair(SVGKerningPair& kerningPair) const
{
    auto& u1 = attributeWithoutSynchronization(SVGNames::u1Attr);
    auto& g1 = attributeWithoutSynchronization(SVGNames::g1Attr);
    auto& u2 = attributeWithoutSynchronization(SVGNames::u2Attr);
    auto& g2 = attributeWithoutSynchronization(SVGNames::g2Attr);
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
