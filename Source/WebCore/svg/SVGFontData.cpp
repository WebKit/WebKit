/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGFontData.h"

#include "GlyphPage.h"
#include "RenderElement.h"
#include "SVGAltGlyphElement.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGGlyph.h"
#include "SVGGlyphElement.h"
#include "SVGNames.h"
#include "SVGTextRunRenderingContext.h"
#include "TextRun.h"
#include "WidthIterator.h"
#include "XMLNames.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

static String createStringWithMirroredCharacters(StringView);
static void computeNormalizedSpaces(const TextRun&, bool mirror, String& normalizedSpacesStringCache);

SVGFontData::SVGFontData(SVGFontFaceElement* fontFaceElement)
    : m_svgFontFaceElement(fontFaceElement)
    , m_horizontalOriginX(fontFaceElement->horizontalOriginX())
    , m_horizontalOriginY(fontFaceElement->horizontalOriginY())
    , m_horizontalAdvanceX(fontFaceElement->horizontalAdvanceX())
    , m_verticalOriginX(fontFaceElement->verticalOriginX())
    , m_verticalOriginY(fontFaceElement->verticalOriginY())
    , m_verticalAdvanceY(fontFaceElement->verticalAdvanceY())
{
    ASSERT_ARG(fontFaceElement, fontFaceElement);
}

void SVGFontData::initializeFont(Font* font, float fontSize)
{
    ASSERT(font);

    SVGFontFaceElement* svgFontFaceElement = this->svgFontFaceElement();
    ASSERT(svgFontFaceElement);

    font->setZeroWidthSpaceGlyph(0);
    font->determinePitch();

    unsigned unitsPerEm = svgFontFaceElement->unitsPerEm();
    float scale = scaleEmToUnits(fontSize, unitsPerEm);
    float xHeight = svgFontFaceElement->xHeight() * scale;
    float ascent = svgFontFaceElement->ascent() * scale;
    float descent = svgFontFaceElement->descent() * scale;
    float lineGap = 0.1f * fontSize;

    const GlyphPage* glyphPageZero = font->glyphPage(0);

    if (!xHeight && glyphPageZero) {
        // Fallback if x_heightAttr is not specified for the font element.
        Glyph letterXGlyph = glyphPageZero->glyphDataForCharacter('x').glyph;
        xHeight = letterXGlyph ? font->widthForGlyph(letterXGlyph) : 2 * ascent / 3;
    }

    FontMetrics& fontMetrics = font->fontMetrics();
    fontMetrics.setUnitsPerEm(unitsPerEm);
    fontMetrics.setAscent(ascent);
    fontMetrics.setDescent(descent);
    fontMetrics.setLineGap(lineGap);
    fontMetrics.setLineSpacing(roundf(ascent) + roundf(descent) + roundf(lineGap));
    fontMetrics.setXHeight(xHeight);

    if (!glyphPageZero) {
        font->setSpaceGlyph(0);
        font->setSpaceWidths(0);
        font->setAvgCharWidth(0);
        font->setMaxCharWidth(ascent);
        return;
    }

    // Calculate space width.
    Glyph spaceGlyph = glyphPageZero->glyphDataForCharacter(' ').glyph;
    font->setSpaceGlyph(spaceGlyph);
    font->setSpaceWidths(font->widthForGlyph(spaceGlyph));

    // Estimate average character width.
    Glyph numeralZeroGlyph = glyphPageZero->glyphDataForCharacter('0').glyph;
    font->setAvgCharWidth(numeralZeroGlyph ? font->widthForGlyph(numeralZeroGlyph) : font->spaceWidth());

    // Estimate maximum character width.
    Glyph letterWGlyph = glyphPageZero->glyphDataForCharacter('W').glyph;
    font->setMaxCharWidth(letterWGlyph ? font->widthForGlyph(letterWGlyph) : ascent);
}

float SVGFontData::widthForSVGGlyph(Glyph glyph, float fontSize) const
{
    SVGFontFaceElement* svgFontFaceElement = this->svgFontFaceElement();
    ASSERT(svgFontFaceElement);

    SVGFontElement* associatedFontElement = svgFontFaceElement->associatedFontElement();
    ASSERT(associatedFontElement);

    SVGGlyph svgGlyph = associatedFontElement->svgGlyphForGlyph(glyph);
    SVGGlyphElement::inheritUnspecifiedAttributes(svgGlyph, this);
    return svgGlyph.horizontalAdvanceX * scaleEmToUnits(fontSize, svgFontFaceElement->unitsPerEm());
}

bool SVGFontData::applySVGGlyphSelection(WidthIterator& iterator, GlyphData& glyphData, bool mirror, int currentCharacter, unsigned& advanceLength, String& normalizedSpacesStringCache) const
{
    const TextRun& run = iterator.run();
    Vector<SVGGlyph::ArabicForm>& arabicForms = iterator.arabicForms();
    ASSERT(run.charactersLength() >= static_cast<unsigned>(currentCharacter));

    SVGFontFaceElement* svgFontFaceElement = this->svgFontFaceElement();
    ASSERT(svgFontFaceElement);

    SVGFontElement* associatedFontElement = svgFontFaceElement->associatedFontElement();
    ASSERT(associatedFontElement);

    RenderObject* renderObject = nullptr;
    if (TextRun::RenderingContext* renderingContext = run.renderingContext())
        renderObject = &static_cast<SVGTextRunRenderingContext*>(renderingContext)->renderer();

    String language;
    bool isVerticalText = false;
    Vector<String> altGlyphNames;

    if (renderObject) {
        RenderElement& parentRenderer = is<RenderElement>(*renderObject) ? downcast<RenderElement>(*renderObject) : *renderObject->parent();

        isVerticalText = parentRenderer.style().svgStyle().isVerticalWritingMode();
        if (Element* parentRendererElement = parentRenderer.element()) {
            language = parentRendererElement->getAttribute(XMLNames::langAttr);

            if (is<SVGAltGlyphElement>(*parentRendererElement)) {
                SVGAltGlyphElement& altGlyph = downcast<SVGAltGlyphElement>(*parentRendererElement);
                if (!altGlyph.hasValidGlyphElements(altGlyphNames))
                    altGlyphNames.clear();
            }
        }
    }

    Vector<SVGGlyph> glyphs;
    if (!altGlyphNames.isEmpty()) {
        for (auto& name : altGlyphNames)
            associatedFontElement->collectGlyphsForGlyphName(name, glyphs);

        // Assign the unicodeStringLength now that its known.
        for (auto& glyph : glyphs)
            glyph.unicodeStringLength = run.length();

        // Do not check alt glyphs for compatibility. Just return the first one.
        // Later code will fail if we do not do this and the glyph is incompatible.
        if (!glyphs.isEmpty()) {
            SVGGlyph& svgGlyph = glyphs[0];
            iterator.setLastGlyphName(svgGlyph.glyphName);
            glyphData.glyph = svgGlyph.tableEntry;
            advanceLength = svgGlyph.unicodeStringLength;
            return true;
        }
    } else {
        // Associate text with arabic forms, if needed.
        computeNormalizedSpaces(run, mirror, normalizedSpacesStringCache);
        auto remainingTextInRun = normalizedSpacesStringCache.substring(currentCharacter);

        if (!currentCharacter && arabicForms.isEmpty())
            arabicForms = charactersWithArabicForm(remainingTextInRun, mirror);

        associatedFontElement->collectGlyphsForString(remainingTextInRun, glyphs);
    }

    for (auto& glyph : glyphs) {
        if (glyph.isPartOfLigature)
            continue;
        if (!isCompatibleGlyph(glyph, isVerticalText, language, arabicForms, currentCharacter, currentCharacter + glyph.unicodeStringLength))
            continue;
        iterator.setLastGlyphName(glyph.glyphName);
        glyphData.glyph = glyph.tableEntry;
        advanceLength = glyph.unicodeStringLength;
        return true;
    }

    iterator.setLastGlyphName(String());
    return false;
}

bool SVGFontData::fillSVGGlyphPage(GlyphPage* pageToFill, unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength, const Font* font) const
{
    ASSERT(font->isCustomFont());
    ASSERT(font->isSVGFont());

    SVGFontFaceElement* fontFaceElement = this->svgFontFaceElement();
    ASSERT(fontFaceElement);

    SVGFontElement* fontElement = fontFaceElement->associatedFontElement();
    ASSERT(fontElement);

    if (bufferLength == length)
        return fillBMPGlyphs(fontElement, pageToFill, offset, length, buffer, font);

    ASSERT(bufferLength == 2 * length);
    return fillNonBMPGlyphs(fontElement, pageToFill, offset, length, buffer, font);
}

bool SVGFontData::fillBMPGlyphs(SVGFontElement* fontElement, GlyphPage* pageToFill, unsigned offset, unsigned length, UChar* buffer, const Font* font) const
{
    bool haveGlyphs = false;
    Vector<SVGGlyph> glyphs;
    for (unsigned i = 0; i < length; ++i) {
        String lookupString(buffer + i, 1);
        fontElement->collectGlyphsForString(lookupString, glyphs);
        if (glyphs.isEmpty()) {
            pageToFill->setGlyphDataForIndex(offset + i, 0, 0);
            continue;
        }

        // Associate entry in glyph page with first valid SVGGlyph.
        // If there are multiple valid ones, just take the first one. WidthIterator will take
        // care of matching to the correct glyph, if multiple ones are available, as that's
        // only possible within the context of a string (eg. arabic form matching).
        haveGlyphs = true;
        pageToFill->setGlyphDataForIndex(offset + i, glyphs.first().tableEntry, font);
        glyphs.clear();
    }

    return haveGlyphs;
}

bool SVGFontData::fillNonBMPGlyphs(SVGFontElement* fontElement, GlyphPage* pageToFill, unsigned offset, unsigned length, UChar* buffer, const Font* font) const
{
    bool haveGlyphs = false;
    Vector<SVGGlyph> glyphs;
    for (unsigned i = 0; i < length; ++i) {
        // Each character here consists of a surrogate pair
        String lookupString(buffer + i * 2, 2);
        fontElement->collectGlyphsForString(lookupString, glyphs);
        if (glyphs.isEmpty()) {
            pageToFill->setGlyphDataForIndex(offset + i, 0, 0);
            continue;
        }

        // Associate entry in glyph page with first valid SVGGlyph.
        // If there are multiple valid ones, just take the first one. WidthIterator will take
        // care of matching to the correct glyph, if multiple ones are available, as that's
        // only possible within the context of a string (eg. arabic form matching).
        haveGlyphs = true;
        pageToFill->setGlyphDataForIndex(offset + i, glyphs.first().tableEntry, font);
        glyphs.clear();
    }

    return haveGlyphs;
}

void computeNormalizedSpaces(const TextRun& run, bool mirror, String& normalizedSpacesStringCache)
{
    if (normalizedSpacesStringCache.length() == static_cast<unsigned>(run.charactersLength()))
        return;
    if (run.is8Bit())
        normalizedSpacesStringCache = FontCascade::normalizeSpaces(run.characters8(), run.charactersLength());
    else
        normalizedSpacesStringCache = FontCascade::normalizeSpaces(run.characters16(), run.charactersLength());
    if (mirror)
        normalizedSpacesStringCache = createStringWithMirroredCharacters(normalizedSpacesStringCache);
}

String createStringWithMirroredCharacters(StringView string)
{
    unsigned length = string.length();
    StringBuilder mirroredCharacters;
    mirroredCharacters.reserveCapacity(length);
    for (unsigned i = 0; i < length; ) {
        UChar32 character;
        U16_NEXT(string, i, length, character);
        mirroredCharacters.append(u_charMirror(character));
    }
    return mirroredCharacters.toString();
}

} // namespace WebCore

#endif
