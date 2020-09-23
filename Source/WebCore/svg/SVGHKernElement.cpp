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

Optional<SVGKerningPair> SVGHKernElement::buildHorizontalKerningPair() const
{
    // FIXME: Can this be shared with SVGVKernElement::buildVerticalKerningPair?

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
