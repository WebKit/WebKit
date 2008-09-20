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
    
// Returns the number of characters consumed or 0 if no range was found.
static unsigned parseUnicodeRange(const UChar* characters, unsigned length, pair<unsigned, unsigned>& range)
{
    if (length < 2)
        return 0;
    if (characters[0] != 'U')
        return 0;
    if (characters[1] != '+')
        return 0;
    
    // Parse the starting hex number (or its prefix).
    unsigned start = 0;
    unsigned startLength = 0;
    for (unsigned i = 2; i < length; ++i) {
        if (!isASCIIHexDigit(characters[i]))
            break;
        if (++startLength > 6)
            return 0;
        start = (start << 4) | toASCIIHexValue(characters[i]);
    }
    
    // Handle the case of ranges separated by "-" sign.
    if (2 + startLength < length && characters[2 + startLength] == '-') {
        if (!startLength)
            return 0;
        
        // Parse the ending hex number (or its prefix).
        unsigned end = 0;
        unsigned endLength = 0;
        for (unsigned i = 2 + startLength + 1; i < length; ++i) {
            if (!isASCIIHexDigit(characters[i]))
                break;
            if (++endLength > 6)
                return 0;
            end = (end << 4) | toASCIIHexValue(characters[i]);
        }
        
        if (!endLength)
            return 0;
        
        range.first = start;
        range.second = end;
        return 2 + startLength + 1 + endLength;
    }
    
    // Handle the case of a number with some optional trailing question marks.
    unsigned end = start;
    for (unsigned i = 2 + startLength; i < length; ++i) {
        if (characters[i] != '?')
            break;
        if (++startLength > 6)
            return 0;
        start <<= 4;
        end = (end << 4) | 0xF;
    }
    
    if (!startLength)
        return 0;
    
    range.first = start;
    range.second = end;
    return 2 + startLength;
}
    
static bool parseUnicodeRangeList(const UChar* characters, unsigned length, Vector<pair<unsigned, unsigned> >& ranges)
{
    ranges.clear();
    if (!length)
        return true;
    
    const UChar* remainingCharacters = characters;
    unsigned remainingLength = length;
    
    while (1) {
        pair<unsigned, unsigned> range;
        unsigned charactersConsumed = parseUnicodeRange(remainingCharacters, remainingLength, range);
        if (charactersConsumed) {
            ranges.append(range);
            remainingCharacters += charactersConsumed;
            remainingLength -= charactersConsumed;
        } else {
            if (!remainingLength)
                return false;
            UChar character = remainingCharacters[0];
            if (character == ',')
                return false;
            ranges.append(make_pair(character, character));
            ++remainingCharacters;
            --remainingLength;
        }
        if (!remainingLength)
            return true;
        if (remainingCharacters[0] != ',')
            return false;
        ++remainingCharacters;
        --remainingLength;
    }
}

static bool stringMatchesUnicodeRange(const String& unicodeString, const String& unicodeRangeSpec)
{
    Vector<pair<unsigned, unsigned> > ranges;
    if (!parseUnicodeRangeList(unicodeRangeSpec.characters(), unicodeRangeSpec.length(), ranges))
        return false;
    
    if (unicodeString.length() != ranges.size())
        return false;
    
    for (size_t i = 0; i < unicodeString.length(); ++i) {
        UChar c = unicodeString[i];
        if (c < ranges[i].first || c > ranges[i].second)
            return false;
    }
    
    return true;
}
    
static bool matches(const String& u1, const String& g1, const String& u2, const String& g2, const SVGHorizontalKerningPair& kerningPair)
{
    if (kerningPair.unicode1.length() && !stringMatchesUnicodeRange(u1, kerningPair.unicode1))
        return false;
    if (kerningPair.glyphName1.length() && kerningPair.glyphName1 != g1)
        return false;
    
    if (kerningPair.unicode2.length() && !stringMatchesUnicodeRange(u2, kerningPair.unicode2))
        return false;
    if (kerningPair.glyphName2.length() && kerningPair.glyphName2 != g2)
        return false;
    
    return true;
}
    
bool SVGFontElement::getHorizontalKerningPairForStringsAndGlyphs(const String& u1, const String& g1, const String& u2, const String& g2, SVGHorizontalKerningPair& kerningPair) const
{
    for (size_t i = 0; i < m_kerningPairs.size(); ++i) {
        if (matches(u1, g1, u2, g2, m_kerningPairs[i])) {
            kerningPair = m_kerningPairs[i];
            return true;
        }        
    }
    
    return false;
}

void SVGFontElement::getGlyphIdentifiersForString(const String& string, Vector<SVGGlyphIdentifier>& glyphs) const
{
    ensureGlyphCache();
    m_glyphMap.get(string, glyphs);
}

}

#endif // ENABLE(SVG_FONTS)
