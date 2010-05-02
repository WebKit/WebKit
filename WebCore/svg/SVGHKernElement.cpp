/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>
   Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
   Copyright (C) 2008 Eric Seidel <eric@webkit.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGHKernElement.h"

#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGFontData.h"
#include "SVGNames.h"
#include "SimpleFontData.h"
#include "XMLNames.h"

namespace WebCore {

using namespace SVGNames;

SVGHKernElement::SVGHKernElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
{
}

SVGHKernElement::~SVGHKernElement()
{
}

void SVGHKernElement::insertedIntoDocument()
{
    Node* fontNode = parentNode();
    if (fontNode && fontNode->hasTagName(SVGNames::fontTag)) {
        if (SVGFontElement* element = static_cast<SVGFontElement*>(fontNode))
            element->invalidateGlyphCache();
    }
}

void SVGHKernElement::removedFromDocument()
{
    Node* fontNode = parentNode();
    if (fontNode && fontNode->hasTagName(SVGNames::fontTag)) {
        if (SVGFontElement* element = static_cast<SVGFontElement*>(fontNode))
            element->invalidateGlyphCache();
    }
}

SVGHorizontalKerningPair SVGHKernElement::buildHorizontalKerningPair() const
{
    SVGHorizontalKerningPair kerningPair;

    // FIXME: KerningPairs shouldn't be created on parsing errors.
    parseGlyphName(getAttribute(g1Attr), kerningPair.glyphName1);
    parseGlyphName(getAttribute(g2Attr), kerningPair.glyphName2);
    parseKerningUnicodeString(getAttribute(u1Attr), kerningPair.unicodeRange1, kerningPair.unicodeName1);
    parseKerningUnicodeString(getAttribute(u2Attr), kerningPair.unicodeRange2, kerningPair.unicodeName2);
    kerningPair.kerning = getAttribute(kAttr).string().toFloat();

    return kerningPair;
}

}

#endif // ENABLE(SVG_FONTS)
