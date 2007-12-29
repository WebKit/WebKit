/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

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
#include "SVGFontElement.h"

#include "Font.h"
#include "GlyphPageTreeNode.h"
#include "SVGGlyphElement.h"
#include "SVGMissingGlyphElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"

namespace WebCore {

using namespace SVGNames;

SVGFontElement::SVGFontElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
{
}

SVGFontElement::~SVGFontElement()
{
}

void SVGFontElement::parseMappedAttribute(MappedAttribute* attr)
{
    SVGStyledElement::parseMappedAttribute(attr);
}

void SVGFontElement::collectGlyphs(const Font& font)
{
    Vector<SVGGlyphElement*> glyphElements;
    SVGMissingGlyphElement* missingGlyphElement = 0;

    for (Node* child = lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(glyphTag))
            glyphElements.append(static_cast<SVGGlyphElement*>(child));
        else if (child->hasTagName(missing_glyphTag))
            missingGlyphElement = static_cast<SVGMissingGlyphElement*>(child);
    }

    Vector<SVGGlyphElement*>::iterator it = glyphElements.begin();
    Vector<SVGGlyphElement*>::iterator end = glyphElements.end();

    m_glyphMap.clear();

    SVGGlyphIdentifier identifier;

    // Only supports single glyph <-> single character situations & glyphs rendered by paths.
    for (; it != end; ++it) {
        String pathDataString = (*it)->getAttribute(dAttr);
        String glyphString = (*it)->getAttribute(unicodeAttr);

        // To support glyph strings consisting of more than one character (ie. 'ffl' ligatures) we need another hashing scheme.
        // Glyph <-> SVGGlyphIdentifier is not enough.
        if (glyphString.length() != 1 || pathDataString.isEmpty())
            continue;

        const GlyphData& glyphData = font.glyphDataForCharacter(glyphString[0], false /* TODO: no rtl, is this correct in all cases? */);

        identifier.pathData = Path();
        pathFromSVGData(identifier.pathData, pathDataString);

        //fprintf(stderr, "FOUND NEW GLYPH! pathDataString: '%s' glyphString: '%s'  code: '%u'\n", pathDataString.latin1().data(), glyphString.latin1().data(), glyphData.glyph);
        m_glyphMap.add(glyphData.glyph, identifier);
    }
}

SVGGlyphIdentifier SVGFontElement::glyphIdentifierForGlyphCode(const Glyph& code) const
{
    GlyphHashMap& hashMap = const_cast<SVGFontElement*>(this)->m_glyphMap;

    GlyphHashMap::iterator it = hashMap.find(code);
    if (it == hashMap.end())
        return SVGGlyphIdentifier();

    return it->second;
}

}

#endif // ENABLE(SVG_FONTS)
