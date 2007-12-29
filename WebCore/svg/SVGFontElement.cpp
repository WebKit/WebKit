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
#include "FontData.h"
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

void SVGFontElement::collectGlyphs(const Font& font)
{
    m_glyphMap.clear();

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

    SVGFontData* svgFontData = 0;

#if !PLATFORM(QT)
    // Why doesn't Qt have primaryFont()? The Qt guys will see an assertion, if buildGlyphIdentifier() is called :(
    const FontData* fontData = font.primaryFont();
    ASSERT(fontData);

    svgFontData = fontData->svgFontData();
#endif

    String glyphString;
    SVGGlyphIdentifier identifier;

    for (; it != end; ++it) {
        identifier = (*it)->buildGlyphIdentifier(svgFontData);
        glyphString = (*it)->getAttribute(unicodeAttr);

        // TODO: To support glyph strings consisting of more than one character (ie. 'ffl' ligatures)
        // we need another hashing scheme. Glyph <-> SVGGlyphIdentifier is not enough.

        // TODO: We skip glyphs with empty paths, this is not correct if the <glyph> has no d="" but children!
        
        if (glyphString.length() != 1 || identifier.pathData.isEmpty())
            continue;

        const GlyphData& glyphData = font.glyphDataForCharacter(glyphString[0], false /* TODO: no rtl, is this correct in all cases? */);
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
