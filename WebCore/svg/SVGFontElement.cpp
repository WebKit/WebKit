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

#include "Document.h"
#include "Font.h"
#include "GlyphPageTreeNode.h"
#include "SVGGlyphElement.h"
#include "SVGMissingGlyphElement.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/ASCIICType.h>

using namespace WTF;

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

void SVGFontElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
}

void SVGFontElement::invalidateGlyphCache()
{
    if (m_isGlyphCacheValid) {
        m_glyphMap.clear();
        m_kerningPairs.clear();
    }
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
        } else if (child->hasTagName(hkernTag)) {
            SVGHKernElement* hkern = static_cast<SVGHKernElement*>(child);
            SVGHorizontalKerningPair kerningPair = hkern->buildHorizontalKerningPair();
            m_kerningPairs.append(kerningPair);
        }
    }
        
    m_isGlyphCacheValid = true;
}

static bool stringMatchesUnicodeRange(const String& unicodeString, const UnicodeRanges& ranges, const HashSet<String>& unicodeValues)
{
    if (unicodeString.isEmpty())
        return false;

    if (!ranges.isEmpty()) {
        UChar firstChar = unicodeString[0];
        const UnicodeRanges::const_iterator end = ranges.end();
        for (UnicodeRanges::const_iterator it = ranges.begin(); it != end; ++it) {
            if (firstChar >= it->first && firstChar <= it->second)
                return true;
        }
    }

    if (!unicodeValues.isEmpty())
        return unicodeValues.contains(unicodeString);
    
    return false;
}

static bool stringMatchesGlyphName(const String& glyphName, const HashSet<String>& glyphValues)
{
    if (glyphName.isEmpty())
        return false;

    if (!glyphValues.isEmpty())
        return glyphValues.contains(glyphName);
    
    return false;
}
    
static bool matches(const String& u1, const String& g1, const String& u2, const String& g2, const SVGHorizontalKerningPair& kerningPair)
{
    if (!stringMatchesUnicodeRange(u1, kerningPair.unicodeRange1, kerningPair.unicodeName1)
        && !stringMatchesGlyphName(g1, kerningPair.glyphName1))
        return false;

    if (!stringMatchesUnicodeRange(u2, kerningPair.unicodeRange2, kerningPair.unicodeName2)
        && !stringMatchesGlyphName(g2, kerningPair.glyphName2))
        return false;

    return true;
}
    
float SVGFontElement::getHorizontalKerningPairForStringsAndGlyphs(const String& u1, const String& g1, const String& u2, const String& g2) const
{
    if (m_kerningPairs.isEmpty())
        return 0.0f;

    KerningPairVector::const_iterator it = m_kerningPairs.end() - 1;
    const KerningPairVector::const_iterator begin = m_kerningPairs.begin() - 1;
    for (; it != begin; --it) {
        if (matches(u1, g1, u2, g2, *it))
            return it->kerning;
    }

    return 0.0f;
}

void SVGFontElement::getGlyphIdentifiersForString(const String& string, Vector<SVGGlyphIdentifier>& glyphs) const
{
    ensureGlyphCache();
    m_glyphMap.get(string, glyphs);
}

}

#endif // ENABLE(SVG_FONTS)
