/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "Font.h"

#include "FloatRect.h"
#include "FontCache.h"
#include "FontTranscoder.h"
#include "IntPoint.h"
#include "GlyphBuffer.h"
#include "WidthIterator.h"
#include <wtf/MathExtras.h>
#include <wtf/UnusedParam.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

const uint8_t Font::gRoundingHackCharacterTable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\t*/, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

Font::CodePath Font::s_codePath = Auto;

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font()
    : m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_isPlatformFont(false)
    , m_needsTranscoding(false)
{
}

Font::Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
    : m_fontDescription(fd)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_isPlatformFont(false)
    , m_needsTranscoding(fontTranscoder().needsTranscoding(fd))
{
}

Font::Font(const FontPlatformData& fontData, bool isPrinterFont, FontSmoothingMode fontSmoothingMode)
    : m_fontList(FontFallbackList::create())
    , m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_isPlatformFont(true)
{
    m_fontDescription.setUsePrinterFont(isPrinterFont);
    m_fontDescription.setFontSmoothing(fontSmoothingMode);
    m_needsTranscoding = fontTranscoder().needsTranscoding(fontDescription());
    m_fontList->setPlatformFont(fontData);
}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_fontList(other.m_fontList)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_isPlatformFont(other.m_isPlatformFont)
    , m_needsTranscoding(fontTranscoder().needsTranscoding(other.m_fontDescription))
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontList = other.m_fontList;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_isPlatformFont = other.m_isPlatformFont;
    m_needsTranscoding = other.m_needsTranscoding;
    return *this;
}

bool Font::operator==(const Font& other) const
{
    // Our FontData don't have to be checked, since checking the font description will be fine.
    // FIXME: This does not work if the font was made with the FontPlatformData constructor.
    if ((m_fontList && m_fontList->loadingCustomFonts()) ||
        (other.m_fontList && other.m_fontList->loadingCustomFonts()))
        return false;
    
    FontSelector* first = m_fontList ? m_fontList->fontSelector() : 0;
    FontSelector* second = other.m_fontList ? other.m_fontList->fontSelector() : 0;
    
    return first == second
           && m_fontDescription == other.m_fontDescription
           && m_letterSpacing == other.m_letterSpacing
           && m_wordSpacing == other.m_wordSpacing
           && (m_fontList ? m_fontList->generation() : 0) == (other.m_fontList ? other.m_fontList->generation() : 0);
}

void Font::update(PassRefPtr<FontSelector> fontSelector) const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up 
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway. Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble). Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontList)
        m_fontList = FontFallbackList::create();
    m_fontList->invalidate(fontSelector);
}

void Font::drawText(GraphicsContext* context, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading.
    if (m_fontList && m_fontList->loadingCustomFonts())
        return;
    
    to = (to == -1 ? run.length() : to);

#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont()) {
        drawTextUsingSVGFont(context, run, point, from, to);
        return;
    }
#endif

    if (codePath(run) != Complex)
        return drawSimpleText(context, run, point, from, to);

    return drawComplexText(context, run, point, from, to);
}

float Font::floatWidth(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return floatWidthUsingSVGFont(run);
#endif

    CodePath codePathToUse = codePath(run);
    if (codePathToUse != Complex) {
        // If the complex text implementation cannot return fallback fonts, avoid
        // returning them for simple text as well.
        static bool returnFallbackFonts = canReturnFallbackFontsForComplexText();
        return floatWidthForSimpleText(run, 0, returnFallbackFonts ? fallbackFonts : 0, codePathToUse == SimpleWithGlyphOverflow ? glyphOverflow : 0);
    }

    return floatWidthForComplexText(run, fallbackFonts, glyphOverflow);
}

float Font::floatWidth(const TextRun& run, int extraCharsAvailable, int& charsConsumed, String& glyphName) const
{
#if !ENABLE(SVG_FONTS)
    UNUSED_PARAM(extraCharsAvailable);
#else
    if (primaryFont()->isSVGFont())
        return floatWidthUsingSVGFont(run, extraCharsAvailable, charsConsumed, glyphName);
#endif

    charsConsumed = run.length();
    glyphName = "";

    if (codePath(run) != Complex)
        return floatWidthForSimpleText(run, 0);

    return floatWidthForComplexText(run);
}

FloatRect Font::selectionRectForText(const TextRun& run, const FloatPoint& point, int h, int from, int to) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return selectionRectForTextUsingSVGFont(run, point, h, from, to);
#endif

    to = (to == -1 ? run.length() : to);

    if (codePath(run) != Complex)
        return selectionRectForSimpleText(run, point, h, from, to);

    return selectionRectForComplexText(run, point, h, from, to);
}

int Font::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return offsetForPositionForTextUsingSVGFont(run, x, includePartialGlyphs);
#endif

    if (codePath(run) != Complex)
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

#if ENABLE(SVG_FONTS)
bool Font::isSVGFont() const
{ 
    return primaryFont()->isSVGFont(); 
}
#endif

String Font::normalizeSpaces(const String& string)
{
    const UChar* characters = string.characters();
    unsigned length = string.length();
    Vector<UChar, 256> buffer(length);
    bool didReplacement = false;

    for (unsigned i = 0; i < length; ++i) {
        UChar originalCharacter = characters[i];
        buffer[i] = normalizeSpaces(originalCharacter);
        if (buffer[i] != originalCharacter)
            didReplacement = true;
    }

    return didReplacement ? String(buffer.data(), length) : string;
}

static bool shouldUseFontSmoothing = true;

void Font::setShouldUseSmoothing(bool shouldUseSmoothing)
{
    ASSERT(isMainThread());
    shouldUseFontSmoothing = shouldUseSmoothing;
}

bool Font::shouldUseSmoothing()
{
    return shouldUseFontSmoothing;
}

void Font::setCodePath(CodePath p)
{
    s_codePath = p;
}

Font::CodePath Font::codePath()
{
    return s_codePath;
}

Font::CodePath Font::codePath(const TextRun& run) const
{
    if (s_codePath != Auto)
        return s_codePath;

#if PLATFORM(QT)
    if (run.padding() || run.rtl() || isSmallCaps() || wordSpacing() || letterSpacing())
        return Complex;
#endif

    CodePath result = Simple;

    // Start from 0 since drawing and highlighting also measure the characters before run->from
    for (int i = 0; i < run.length(); i++) {
        const UChar c = run[i];
        if (c < 0x300) // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return Complex;

        if (c < 0x0591 || c == 0x05BE) // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
            continue;
        if (c <= 0x05CF)
            return Complex;

        if (c < 0x0600) // U+0600 through U+1059 Arabic, Syriac, Thaana, Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
            continue;
        if (c <= 0x1059)
            return Complex;

        if (c < 0x1100) // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose; Modern Korean will be precomposed as a result of step A)
            continue;
        if (c <= 0x11FF)
            return Complex;

        if (c < 0x1780) // U+1780 through U+18AF Khmer, Mongolian
            continue;
        if (c <= 0x18AF)
            return Complex;

        if (c < 0x1900) // U+1900 through U+194F Limbu (Unicode 4.0)
            continue;
        if (c <= 0x194F)
            return Complex;

        if (c < 0x1E00) // U+1E00 through U+2000 characters with diacritics and stacked diacritics
            continue;
        if (c <= 0x2000) {
            result = SimpleWithGlyphOverflow;
            continue;
        }

        if (c < 0x20D0) // U+20D0 through U+20FF Combining marks for symbols
            continue;
        if (c <= 0x20FF)
            return Complex;

        if (c < 0xFE20) // U+FE20 through U+FE2F Combining half marks
            continue;
        if (c <= 0xFE2F)
            return Complex;
    }

    if (typesettingFeatures())
        return Complex;

    return result;
}

}
