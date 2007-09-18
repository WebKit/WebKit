/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UniscribeController.h"
#include "Font.h"
#include "FontData.h"
#include "TextStyle.h"
#include <wtf/MathExtras.h>

namespace WebCore {

// FIXME: Rearchitect this to be more like WidthIterator in Font.cpp.  Have an advance() method
// that does stuff in that method instead of doing everything in the constructor.  Have advance()
// take the GlyphBuffer as an arg so that we don't have to populate the glyph buffer when
// measuring.
UniscribeController::UniscribeController(const Font* font, const TextRun& run, const TextStyle& style)
: m_font(*font)
, m_run(run)
, m_style(style)
, m_end(run.length())
, m_currentCharacter(0)
, m_runWidthSoFar(0)
, m_computingOffsetPosition(false)
, m_includePartialGlyphs(false)
, m_offsetX(0)
, m_offsetPosition(0)
{
    m_padding = m_style.padding();
    if (!m_padding)
        m_padPerSpace = 0;
    else {
        float numSpaces = 0;
        for (int s = 0; s < m_run.length(); s++)
            if (Font::treatAsSpace(m_run[s]))
                numSpaces++;

        if (numSpaces == 0)
            m_padPerSpace = 0;
        else
            m_padPerSpace = ceilf(m_style.padding() / numSpaces);
    }

    // Null out our uniscribe structs
    resetControlAndState();
}

int UniscribeController::offsetForPosition(int x, bool includePartialGlyphs)
{
    m_computingOffsetPosition = true;
    m_includePartialGlyphs = includePartialGlyphs;
    m_offsetX = x;
    m_offsetPosition = 0;
    advance(m_run.length());
    if (m_computingOffsetPosition) {
        // The point is to the left or to the right of the entire run.
        if (m_offsetX >= m_runWidthSoFar && m_style.ltr() || m_offsetX < 0 && m_style.rtl())
            m_offsetPosition = m_end;
    }
    m_computingOffsetPosition = false;
    return m_offsetPosition;
}

void UniscribeController::advance(unsigned offset, GlyphBuffer* glyphBuffer)
{
    // FIXME: We really want to be using a newer version of Uniscribe that supports the new OpenType
    // functions.  Those functions would allow us to turn off kerning and ligatures.  Without being able
    // to do that, we will have buggy line breaking and metrics when simple and complex text are close
    // together (the complex code path will narrow the text because of kerning and ligatures and then
    // when bidi processing splits into multiple runs, the simple portions will get wider and cause us to
    // spill off the edge of a line).
    if (static_cast<int>(offset) > m_end)
        offset = m_end;

    // Itemize the string.
    const UChar* cp = m_run.data(m_currentCharacter);
    int length = offset - m_currentCharacter;
    if (length <= 0)
        return;

    // We break up itemization of the string if small caps is involved.
    // Adjust the characters to account for small caps if it is set.
    if (m_font.isSmallCaps()) {
        // FIXME: It's inconsistent that we use logical order when itemizing, since this
        // does not match normal RTL.
        Vector<UChar> smallCapsBuffer(length);
        memcpy(smallCapsBuffer.data(), cp, length * sizeof(UChar));
        bool isSmallCaps = false;
        unsigned indexOfCaseShift = m_style.rtl() ? length - 1 : 0;
        const UChar* curr = m_style.rtl() ? cp + length  - 1: cp;
        const UChar* end = m_style.rtl() ? cp - 1: cp + length;
        while (curr != end) {
            int index = curr - cp;
            UChar c = smallCapsBuffer[index];
            UChar newC;
            curr = m_style.rtl() ? curr - 1 : curr + 1;
            if (U_GET_GC_MASK(c) & U_GC_M_MASK)
                continue;
            if (!u_isUUppercase(c) && (newC = u_toupper(c)) != c) {
                smallCapsBuffer[index] = newC;
                if (!isSmallCaps) {
                    isSmallCaps = true;
                    int itemStart = m_style.rtl() ? index : indexOfCaseShift;
                    int itemLength = m_style.rtl() ? indexOfCaseShift - index : index - indexOfCaseShift;
                    itemizeShapeAndPlace(smallCapsBuffer.data() + itemStart, itemLength, false, glyphBuffer);
                    indexOfCaseShift = index;
                }
            } else if (isSmallCaps) {
                isSmallCaps = false;
                int itemStart = m_style.rtl() ? index : indexOfCaseShift;
                int itemLength = m_style.rtl() ? indexOfCaseShift - index : index - indexOfCaseShift;
                itemizeShapeAndPlace(smallCapsBuffer.data() + itemStart, itemLength, true, glyphBuffer);
                indexOfCaseShift = index;
            }
        }
        
        int itemLength = m_style.rtl() ? indexOfCaseShift + 1 : length - indexOfCaseShift;
        if (itemLength) {
            int itemStart = m_style.rtl() ? 0 : indexOfCaseShift;
            itemizeShapeAndPlace(smallCapsBuffer.data() + itemStart, itemLength, isSmallCaps, glyphBuffer);
        }
    } else
        itemizeShapeAndPlace(cp, length, false, glyphBuffer);
}

void UniscribeController::itemizeShapeAndPlace(const UChar* cp, unsigned length, bool smallCaps, GlyphBuffer* glyphBuffer)
{
    // ScriptItemize (in Windows XP versions prior to SP2) can overflow by 1.  This is why there is an extra empty item
    // hanging out at the end of the array
    m_items.resize(6);
    int numItems = 0;
    while (ScriptItemize(cp, length, m_items.size() - 1, &m_control, &m_state, m_items.data(), &numItems) == E_OUTOFMEMORY) {
        m_items.resize(m_items.size() * 2);
        resetControlAndState();
    }
    m_items.resize(numItems + 1);

    if (m_style.rtl()) {
        for (int i = m_items.size() - 2; i >= 0; i--) {
            if (!shapeAndPlaceItem(cp, i, smallCaps, glyphBuffer))
                return;
        }
    } else {
        for (unsigned i = 0; i < m_items.size() - 1; i++) {
            if (!shapeAndPlaceItem(cp, i, smallCaps, glyphBuffer))
                return;
        }
    }

    m_currentCharacter += length;
}

void UniscribeController::resetControlAndState()
{
    memset(&m_control, 0, sizeof(SCRIPT_CONTROL));
    memset(&m_state, 0, sizeof(SCRIPT_STATE));

    // Set up the correct direction for the run.
    m_state.uBidiLevel = m_style.rtl();
    
    // Lock the correct directional override.
    m_state.fOverrideDirection = m_style.directionalOverride();
}

bool UniscribeController::shapeAndPlaceItem(const UChar* cp, unsigned i, bool smallCaps, GlyphBuffer* glyphBuffer)
{
    // Determine the string for this item.
    const UChar* str = cp + m_items[i].iCharPos;
    int len = m_items[i+1].iCharPos - m_items[i].iCharPos;
    SCRIPT_ITEM item = m_items[i];

    // Get our current FontData that we are using.
    unsigned dataIndex = 0;
    const FontData* fontData = m_font.fontDataAt(dataIndex);
    if (smallCaps)
        fontData = fontData->smallCapsFontData(m_font.fontDescription());

    // Set up buffers to hold the results of shaping the item.
    Vector<WORD> glyphs;
    Vector<WORD> clusters;
    Vector<SCRIPT_VISATTR> visualAttributes;
    clusters.resize(len);
     
    // Shape the item.  This will provide us with glyphs for the item.  We will
    // attempt to shape using the first available FontData.  If the shaping produces a result with missing
    // glyphs, then we will fall back to the next FontData.
    // FIXME: This isn't as good as per-glyph fallback, but in practice it should be pretty good, since
    // items are broken up by "shaping engine", meaning unique scripts will be broken up into
    // separate items.
    bool lastResortFontTried = false;
    while (fontData) {
        // The recommended size for the glyph buffer is 1.5 * the character length + 16 in the uniscribe docs.
        // Apparently this is a good size to avoid having to make repeated calls to ScriptShape.
        glyphs.resize(1.5 * len + 16);
        visualAttributes.resize(glyphs.size());
   
        if (shape(str, len, item, fontData, glyphs, clusters, visualAttributes))
            break;
        
        // Try again with the next font in the list.
        if (lastResortFontTried) {
            fontData = 0;
            break;
        }
        
        fontData = m_font.fontDataAt(++dataIndex);
        if (!fontData) {
            // Out of fonts.  Get a font data based on the actual characters.
            fontData = m_font.fontDataForCharacters(str, len);
            lastResortFontTried = true;
        }
        if (smallCaps)
            fontData = fontData->smallCapsFontData(m_font.fontDescription());
    }

    // Just give up.  We were unable to shape.
    if (!fontData)
        return true;

    // We now have a collection of glyphs.
    Vector<GOFFSET> offsets;
    Vector<int> advances;
    offsets.resize(glyphs.size());
    advances.resize(glyphs.size());
    int glyphCount = 0;
    HRESULT placeResult = ScriptPlace(0, fontData->scriptCache(), glyphs.data(), glyphs.size(), visualAttributes.data(),
                                      &item.a, advances.data(), offsets.data(), 0);
    if (placeResult == E_PENDING) {
        // The script cache isn't primed with enough info yet.  We need to select our HFONT into
        // a DC and pass the DC in to ScriptPlace.
        HDC hdc = GetDC(0);
        HFONT hfont = fontData->platformData().hfont();
        HFONT oldFont = (HFONT)SelectObject(hdc, hfont);
        placeResult = ScriptPlace(hdc, fontData->scriptCache(), glyphs.data(), glyphs.size(), visualAttributes.data(),
                                  &item.a, advances.data(), offsets.data(), 0);
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
    }
    
    if (FAILED(placeResult) || glyphs.isEmpty())
        return true;

    // Convert all chars that should be treated as spaces to use the space glyph.
    // We also create a map that allows us to quickly go from space glyphs or rounding
    // hack glyphs back to their corresponding characters.
    Vector<int> spaceCharacters(glyphs.size());
    spaceCharacters.fill(-1);
    Vector<int> roundingHackCharacters(glyphs.size());
    roundingHackCharacters.fill(-1);
    Vector<int> roundingHackWordBoundaries(glyphs.size());
    roundingHackWordBoundaries.fill(-1);
    unsigned logicalSpaceWidth = fontData->m_spaceWidth * 32.0f;
    float roundedSpaceWidth = roundf(fontData->m_spaceWidth);

    for (int k = 0; k < len; k++) {
        UChar ch = *(str + k);
        if (Font::treatAsSpace(ch)) {
            // Substitute in the space glyph at the appropriate place in the glyphs
            // array.
            glyphs[clusters[k]] = fontData->m_spaceGlyph;
            advances[clusters[k]] = logicalSpaceWidth;
            spaceCharacters[clusters[k]] = m_currentCharacter + k + m_items[i].iCharPos;
        }

        if (Font::isRoundingHackCharacter(ch))
            roundingHackCharacters[clusters[k]] = m_currentCharacter + k + m_items[i].iCharPos;

        int boundary = k + m_currentCharacter + m_items[i].iCharPos;
        if (boundary < m_run.length() &&
            Font::isRoundingHackCharacter(*(str + k + 1)))
            roundingHackWordBoundaries[clusters[k]] = boundary;
    }

    // Populate our glyph buffer with this information.
    bool hasExtraSpacing = m_font.letterSpacing() || m_font.wordSpacing() || m_padding;
    
    float leftEdge = m_runWidthSoFar;

    for (unsigned k = 0; k < glyphs.size(); k++) {
        Glyph glyph = glyphs[k];
        float advance = advances[k] / 32.0f;
        float offsetX = offsets[k].du / 32.0f;
        float offsetY = offsets[k].dv / 32.0f;

        // Match AppKit's rules for the integer vs. non-integer rendering modes.
        float roundedAdvance = roundf(advance);
        if (!m_font.isPrinterFont() && !fontData->isSystemFont()) {
            advance = roundedAdvance;
            offsetX = roundf(offsetX);
            offsetY = roundf(offsetY);
        }
       
        // We special case spaces in two ways when applying word rounding.
        // First, we round spaces to an adjusted width in all fonts.
        // Second, in fixed-pitch fonts we ensure that all glyphs that
        // match the width of the space glyph have the same width as the space glyph.
        if (roundedAdvance == roundedSpaceWidth && (fontData->m_treatAsFixedPitch || glyph == fontData->m_spaceGlyph) &&
            m_style.applyWordRounding())
            advance = fontData->m_adjustedSpaceWidth;

        if (hasExtraSpacing) {
            // If we're a glyph with an advance, go ahead and add in letter-spacing.
            // That way we weed out zero width lurkers.  This behavior matches the fast text code path.
            if (advance && m_font.letterSpacing())
                advance += m_font.letterSpacing();

            // Handle justification and word-spacing.
            if (glyph == fontData->m_spaceGlyph) {
                // Account for padding. WebCore uses space padding to justify text.
                // We distribute the specified padding over the available spaces in the run.
                if (m_padding) {
                    // Use leftover padding if not evenly divisible by number of spaces.
                    if (m_padding < m_padPerSpace) {
                        advance += m_padding;
                        m_padding = 0;
                    } else {
                        advance += m_padPerSpace;
                        m_padding -= m_padPerSpace;
                    }
                }

                // Account for word-spacing.
                int characterIndex = spaceCharacters[k];
                if (characterIndex > 0 && !Font::treatAsSpace(*m_run.data(characterIndex-1)) && m_font.wordSpacing())
                    advance += m_font.wordSpacing();
            }
        }

        // Deal with the float/integer impedance mismatch between CG and WebCore. "Words" (characters 
        // followed by a character defined by isRoundingHackCharacter()) are always an integer width.
        // We adjust the width of the last character of a "word" to ensure an integer width.
        // Force characters that are used to determine word boundaries for the rounding hack
        // to be integer width, so the following words will start on an integer boundary.
        int roundingHackIndex = roundingHackCharacters[k];
        if (m_style.applyWordRounding() && roundingHackIndex != -1)
            advance = ceilf(advance);

        // Check to see if the next character is a "rounding hack character", if so, adjust the
        // width so that the total run width will be on an integer boundary.
        int position = m_currentCharacter + len;
        bool lastGlyph = (k == glyphs.size() - 1) && (m_style.rtl() ? i == 0 : i == m_items.size() - 2) && (position >= m_end);
        if ((m_style.applyWordRounding() && roundingHackWordBoundaries[k] != -1) ||
            (m_style.applyRunRounding() && lastGlyph)) { 
            float totalWidth = m_runWidthSoFar + advance;
            advance += ceilf(totalWidth) - totalWidth;
        }

        m_runWidthSoFar += advance;

        // FIXME: We need to take the GOFFSETS for combining glyphs and store them in the glyph buffer
        // as well, so that when the time comes to draw those glyphs, we can apply the appropriate
        // translation.
        if (glyphBuffer) {
            FloatSize size(offsetX, offsetY);
            glyphBuffer->add(glyph, fontData, advance, &size);
        }

        // Mutate the glyph array to contain our altered advances.
        if (m_computingOffsetPosition)
            advances[k] = advance;
    }

    while (m_computingOffsetPosition && m_offsetX >= leftEdge && m_offsetX < m_runWidthSoFar) {
        // The position is somewhere inside this run.
        int trailing = 0;
        ScriptXtoCP(m_offsetX - leftEdge, clusters.size(), glyphs.size(), clusters.data(), visualAttributes.data(),
                    advances.data(), &item.a, &m_offsetPosition, &trailing);
        if (trailing && m_includePartialGlyphs && m_offsetPosition < len - 1) {
            m_offsetPosition += m_currentCharacter + m_items[i].iCharPos;
            m_offsetX += m_style.rtl() ? -trailing : trailing;
        } else {
            m_computingOffsetPosition = false;
            m_offsetPosition += m_currentCharacter + m_items[i].iCharPos;
            if (trailing && m_includePartialGlyphs)
               m_offsetPosition++;
            return false;
        }
    }

    return true;
}

bool UniscribeController::shape(const UChar* str, int len, SCRIPT_ITEM item, const FontData* fontData,
                                Vector<WORD>& glyphs, Vector<WORD>& clusters,
                                Vector<SCRIPT_VISATTR>& visualAttributes)
{
    HDC hdc = 0;
    HFONT oldFont = 0;
    HRESULT shapeResult = E_PENDING;
    int glyphCount = 0;
    do {
        shapeResult = ScriptShape(hdc, fontData->scriptCache(), str, len, glyphs.size(), &item.a,
                                  glyphs.data(), clusters.data(), visualAttributes.data(), &glyphCount);
        if (shapeResult == E_PENDING) {
            // The script cache isn't primed with enough info yet.  We need to select our HFONT into
            // a DC and pass the DC in to ScriptShape.
            ASSERT(!hdc);
            hdc = GetDC(0);
            HFONT hfont = fontData->platformData().hfont();
            oldFont = (HFONT)SelectObject(hdc, hfont);
        } else if (shapeResult == E_OUTOFMEMORY) {
            // Need to resize our buffers.
            glyphs.resize(glyphs.size() * 2);
            visualAttributes.resize(glyphs.size());
        }
    } while (shapeResult == E_PENDING || shapeResult == E_OUTOFMEMORY);

    if (hdc) {
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
    }

    if (FAILED(shapeResult))
        return false;
    
    // FIXME: We need to do better than this.  Falling back on the entire item is not good enough.
    // We may still have missing glyphs even if we succeeded.  We need to treat missing glyphs as
    // a failure so that we will fall back to another font.
    bool containsMissingGlyphs = false;
    SCRIPT_FONTPROPERTIES* fontProperties = fontData->scriptFontProperties();
    for (int i = 0; i < glyphCount; i++) {
        WORD glyph = glyphs[i];
        if (glyph == fontProperties->wgDefault) {
            containsMissingGlyphs = true;
            break;
        }
    }

    if (containsMissingGlyphs)
        return false;

    glyphs.resize(glyphCount);
    visualAttributes.resize(glyphCount);

    return true;
}

}
