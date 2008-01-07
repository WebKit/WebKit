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
    , m_maximumHashKeyLength(0)
{
}

SVGFontElement::~SVGFontElement()
{
}

void SVGFontElement::addGlyphToCache(SVGGlyphElement* glyphElement)
{
    ASSERT(glyphElement);

    String glyphString = glyphElement->getAttribute(unicodeAttr);
    if (glyphString.isEmpty()) // No unicode property, means that glyph will be used in <altGlyph> situations!
        return;

    SVGGlyphIdentifier identifier = glyphElement->buildGlyphIdentifier();
    identifier.isValid = true;

    if (glyphString.length() > m_maximumHashKeyLength)
        m_maximumHashKeyLength = glyphString.length();

    GlyphHashMap::iterator glyphsIt = m_glyphMap.find(glyphString);
    if (glyphsIt == m_glyphMap.end()) {
        Vector<SVGGlyphIdentifier> glyphs;
        glyphs.append(identifier);

        m_glyphMap.add(glyphString, glyphs);
    } else {
        Vector<SVGGlyphIdentifier>& glyphs = (*glyphsIt).second;
        glyphs.append(identifier);
    }
}

void SVGFontElement::removeGlyphFromCache(SVGGlyphElement* glyphElement)
{
    ASSERT(glyphElement);

    String glyphString = glyphElement->getAttribute(unicodeAttr);
    if (glyphString.isEmpty()) // No unicode property, means that glyph will be used in <altGlyph> situations!
        return;

    GlyphHashMap::iterator glyphsIt = m_glyphMap.find(glyphString);
    ASSERT(glyphsIt != m_glyphMap.end());

    Vector<SVGGlyphIdentifier>& glyphs = (*glyphsIt).second;

    if (glyphs.size() == 1)
        m_glyphMap.remove(glyphString);
    else {
        SVGGlyphIdentifier identifier = glyphElement->buildGlyphIdentifier();
        identifier.isValid = true;

        Vector<SVGGlyphIdentifier>::iterator it = glyphs.begin();
        Vector<SVGGlyphIdentifier>::iterator end = glyphs.end();

        unsigned int position = 0;
        for (; it != end; ++it) {
            if ((*it) == identifier)
                break;

            position++;
        }

        ASSERT(position < glyphs.size());
        glyphs.remove(position);
    }

    // If we remove a glyph from cache, whose unicode property length is equal to
    // m_maximumHashKeyLength then we need to recalculate the hash key length, because there
    // is either no more glyph with that length, or there are still more glyphs with the maximum length.
    if (glyphString.length() == m_maximumHashKeyLength) {
        m_maximumHashKeyLength = 0;

        GlyphHashMap::iterator it = m_glyphMap.begin();
        GlyphHashMap::iterator end = m_glyphMap.end();

        for (; it != end; ++it) {
            if ((*it).first.length() > m_maximumHashKeyLength)
                m_maximumHashKeyLength = (*it).first.length();
        }
    }
}

SVGMissingGlyphElement* SVGFontElement::firstMissingGlyphElement() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(missing_glyphTag))
            return static_cast<SVGMissingGlyphElement*>(child);
    }

    return 0;
}

const Vector<SVGGlyphIdentifier>& SVGFontElement::glyphIdentifiersForString(const String& string) const
{
    GlyphHashMap::const_iterator it = m_glyphMap.find(string);
    if (it == m_glyphMap.end()) {
        static Vector<SVGGlyphIdentifier> s_emptyGlyphList;
        return s_emptyGlyphList;
    }

    return (*it).second;
}

}

#endif // ENABLE(SVG_FONTS)
