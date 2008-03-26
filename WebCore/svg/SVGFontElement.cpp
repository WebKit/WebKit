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
    , m_isGlyphCacheValid(false)
{
}

SVGFontElement::~SVGFontElement()
{
}

void SVGFontElement::addGlyphToCache(SVGGlyphElement* glyphElement)
{
    if (m_isGlyphCacheValid)
        m_glyphMap.clear();
    m_isGlyphCacheValid = false;
}

void SVGFontElement::removeGlyphFromCache(SVGGlyphElement* glyphElement)
{
    if (m_isGlyphCacheValid)
        m_glyphMap.clear();
    m_isGlyphCacheValid = false;
}

SVGMissingGlyphElement* SVGFontElement::firstMissingGlyphElement() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(missing_glyphTag))
            return static_cast<SVGMissingGlyphElement*>(child);
    }

    return 0;
}

void SVGFontElement::ensureGlyphCache() const
{
    if (m_isGlyphCacheValid)
        return;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(glyphTag)) {
            SVGGlyphElement* glyph = static_cast<SVGGlyphElement*>(child);
            String unicode = glyph->getAttribute(unicodeAttr);
            if (unicode.length())
                m_glyphMap.add(unicode, glyph->buildGlyphIdentifier());
        }
    }
        
    m_isGlyphCacheValid = true;
}

void SVGFontElement::getGlyphIdentifiersForString(const String& string, Vector<SVGGlyphIdentifier>& glyphs) const
{
    ensureGlyphCache();
    m_glyphMap.get(string, glyphs);
}

}

#endif // ENABLE(SVG_FONTS)
