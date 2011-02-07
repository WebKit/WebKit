/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ComplexTextControllerLinux.h"

#include "Font.h"
#include "TextRun.h"

#include <unicode/normlzr.h>

namespace WebCore {

// Harfbuzz uses 26.6 fixed point values for pixel offsets. However, we don't
// handle subpixel positioning so this function is used to truncate Harfbuzz
// values to a number of pixels.
static int truncateFixedPointToInteger(HB_Fixed value)
{
    return value >> 6;
}

ComplexTextController::ComplexTextController(const TextRun& run, unsigned startingX, const Font* font)
    : m_font(font)
    , m_run(getNormalizedTextRun(run, m_normalizedRun, m_normalizedBuffer))
    , m_wordSpacingAdjustment(0)
    , m_padding(0)
    , m_padPerWordBreak(0)
    , m_padError(0)
    , m_letterSpacing(0)
{
    // Do not use |run| inside this constructor. Use |m_run| instead.

    memset(&m_item, 0, sizeof(m_item));
    // We cannot know, ahead of time, how many glyphs a given script run
    // will produce. We take a guess that script runs will not produce more
    // than twice as many glyphs as there are code points plus a bit of
    // padding and fallback if we find that we are wrong.
    createGlyphArrays((m_run.length() + 2) * 2);

    m_item.log_clusters = new unsigned short[m_run.length()];

    m_item.face = 0;
    m_item.font = allocHarfbuzzFont();

    m_item.item.bidiLevel = m_run.rtl();

    m_item.string = m_run.characters();
    m_item.stringLength = m_run.length();

    reset(startingX);
}

ComplexTextController::~ComplexTextController()
{
    fastFree(m_item.font);
    deleteGlyphArrays();
    delete[] m_item.log_clusters;
}

bool ComplexTextController::isWordBreak(unsigned index)
{
    return index && isCodepointSpace(m_item.string[index]) && !isCodepointSpace(m_item.string[index - 1]);
}

int ComplexTextController::determineWordBreakSpacing(unsigned logClustersIndex)
{
    int wordBreakSpacing = 0;
    // The first half of the conjunction works around the case where
    // output glyphs aren't associated with any codepoints by the
    // clusters log.
    if (logClustersIndex < m_item.item.length
        && isWordBreak(m_item.item.pos + logClustersIndex)) {
        wordBreakSpacing = m_wordSpacingAdjustment;

        if (m_padding > 0) {
            int toPad = roundf(m_padPerWordBreak + m_padError);
            m_padError += m_padPerWordBreak - toPad;

            if (m_padding < toPad)
                toPad = m_padding;
            m_padding -= toPad;
            wordBreakSpacing += toPad;
        }
    }
    return wordBreakSpacing;
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void ComplexTextController::setPadding(int padding)
{
    m_padding = padding;
    if (!m_padding)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each space. The last space gets the smaller amount, if
    // any.
    unsigned numWordBreaks = 0;

    for (unsigned i = 0; i < m_item.stringLength; i++) {
        if (isWordBreak(i))
            numWordBreaks++;
    }

    if (numWordBreaks)
        m_padPerWordBreak = m_padding / numWordBreaks;
    else
        m_padPerWordBreak = 0;
}

void ComplexTextController::reset(unsigned offset)
{
    m_indexOfNextScriptRun = 0;
    m_offsetX = offset;
}

// Advance to the next script run, returning false when the end of the
// TextRun has been reached.
bool ComplexTextController::nextScriptRun()
{
    // Ensure we're not pointing at the small caps buffer.
    m_item.string = m_run.characters();

    if (!hb_utf16_script_run_next(0, &m_item.item, m_run.characters(), m_run.length(), &m_indexOfNextScriptRun))
        return false;

    // It is actually wrong to consider script runs at all in this code.
    // Other WebKit code (e.g. Mac) segments complex text just by finding
    // the longest span of text covered by a single font.
    // But we currently need to call hb_utf16_script_run_next anyway to fill
    // in the harfbuzz data structures to e.g. pick the correct script's shaper.
    // So we allow that to run first, then do a second pass over the range it
    // found and take the largest subregion that stays within a single font.
    m_currentFontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos], false).fontData;
    unsigned endOfRun;
    for (endOfRun = 1; endOfRun < m_item.item.length; ++endOfRun) {
        const SimpleFontData* nextFontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos + endOfRun], false).fontData;
        if (nextFontData != m_currentFontData)
            break;
    }
    m_item.item.length = endOfRun;
    m_indexOfNextScriptRun = m_item.item.pos + endOfRun;

    setupFontForScriptRun();
    shapeGlyphs();
    setGlyphXPositions(rtl());

    return true;
}

float ComplexTextController::widthOfFullRun()
{
    float widthSum = 0;
    while (nextScriptRun())
        widthSum += width();

    return widthSum;
}

void ComplexTextController::setupFontForScriptRun()
{
    FontDataVariant fontDataVariant = AutoVariant;
    // Determine if this script run needs to be converted to small caps.
    // nextScriptRun() will always send us a run of the same case, because a
    // case change while in small-caps mode always results in different
    // FontData, so we only need to check the first character's case.
    if (m_font->isSmallCaps() && u_islower(m_item.string[m_item.item.pos])) {
        m_smallCapsString = String(m_run.data(m_item.item.pos), m_item.item.length);
        m_smallCapsString.makeUpper();
        m_item.string = m_smallCapsString.characters();
        m_item.item.pos = 0;
        fontDataVariant = SmallCapsVariant;
    }
    const FontData* fontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos], false, fontDataVariant).fontData;
    const FontPlatformData& platformData = fontData->fontDataForCharacter(' ')->platformData();
    m_item.face = platformData.harfbuzzFace();
    void* opaquePlatformData = const_cast<FontPlatformData*>(&platformData);
    m_item.font->userData = opaquePlatformData;

    int size = platformData.size();
    m_item.font->x_ppem = size;
    m_item.font->y_ppem = size;
    // x_ and y_scale are the conversion factors from font design space (fEmSize) to 1/64th of device pixels in 16.16 format.
    const int devicePixelFraction = 64;
    const int multiplyFor16Dot16 = 1 << 16;
    int scale = devicePixelFraction * size * multiplyFor16Dot16 / platformData.emSizeInFontUnits();
    m_item.font->x_scale = scale;
    m_item.font->y_scale = scale;
}

HB_FontRec* ComplexTextController::allocHarfbuzzFont()
{
    HB_FontRec* font = reinterpret_cast<HB_FontRec*>(fastMalloc(sizeof(HB_FontRec)));
    memset(font, 0, sizeof(HB_FontRec));
    font->klass = &harfbuzzSkiaClass;
    font->userData = 0;

    return font;
}

void ComplexTextController::deleteGlyphArrays()
{
    delete[] m_item.glyphs;
    delete[] m_item.attributes;
    delete[] m_item.advances;
    delete[] m_item.offsets;
    delete[] m_glyphs16;
    delete[] m_xPositions;
}

void ComplexTextController::createGlyphArrays(int size)
{
    m_item.glyphs = new HB_Glyph[size];
    m_item.attributes = new HB_GlyphAttributes[size];
    m_item.advances = new HB_Fixed[size];
    m_item.offsets = new HB_FixedPoint[size];

    m_glyphs16 = new uint16_t[size];
    m_xPositions = new SkScalar[size];

    m_item.num_glyphs = size;
    m_glyphsArrayCapacity = size; // Save the GlyphArrays size.
    resetGlyphArrays();
}

void ComplexTextController::resetGlyphArrays()
{
    int size = m_item.num_glyphs;
    // All the types here don't have pointers. It is safe to reset to
    // zero unless Harfbuzz breaks the compatibility in the future.
    memset(m_item.glyphs, 0, size * sizeof(HB_Glyph));
    memset(m_item.attributes, 0, size * sizeof(HB_GlyphAttributes));
    memset(m_item.advances, 0, size * sizeof(HB_Fixed));
    memset(m_item.offsets, 0, size * sizeof(HB_FixedPoint));
    memset(m_glyphs16, 0, size * sizeof(uint16_t));
    memset(m_xPositions, 0, size * sizeof(SkScalar));
}

void ComplexTextController::shapeGlyphs()
{
    // HB_ShapeItem() resets m_item.num_glyphs. If the previous call to
    // HB_ShapeItem() used less space than was available, the capacity of
    // the array may be larger than the current value of m_item.num_glyphs.
    // So, we need to reset the num_glyphs to the capacity of the array.
    m_item.num_glyphs = m_glyphsArrayCapacity;
    resetGlyphArrays();
    while (!HB_ShapeItem(&m_item)) {
        // We overflowed our arrays. Resize and retry.
        // HB_ShapeItem fills in m_item.num_glyphs with the needed size.
        deleteGlyphArrays();
        // The |+ 1| here is a workaround for a bug in Harfbuzz: the Khmer
        // shaper (at least) can fail because of insufficient glyph buffers
        // and request 0 additional glyphs: throwing us into an infinite
        // loop.
        createGlyphArrays(m_item.num_glyphs + 1);
    }
}

void ComplexTextController::setGlyphXPositions(bool isRTL)
{
    const double rtlFlip = isRTL ? -1 : 1;
    double position = 0;

    // logClustersIndex indexes logClusters for the first codepoint of the current glyph.
    // Each time we advance a glyph, we skip over all the codepoints that contributed to the current glyph.
    int logClustersIndex = 0;

    // Iterate through the glyphs in logical order, flipping for RTL where necessary.
    // Glyphs are positioned starting from m_offsetX; in RTL mode they go leftwards from there.
    for (size_t i = 0; i < m_item.num_glyphs; ++i) {
        while (static_cast<unsigned>(logClustersIndex) < m_item.item.length && logClusters()[logClustersIndex] < i)
            logClustersIndex++;

        // If the current glyph is just after a space, add in the word spacing.
        position += determineWordBreakSpacing(logClustersIndex);

        m_glyphs16[i] = m_item.glyphs[i];
        double offsetX = truncateFixedPointToInteger(m_item.offsets[i].x);
        double advance = truncateFixedPointToInteger(m_item.advances[i]);
        if (isRTL)
            offsetX -= advance;

        m_xPositions[i] = m_offsetX + (position * rtlFlip) + offsetX;

        if (m_currentFontData->isZeroWidthSpaceGlyph(m_glyphs16[i]))
            continue;

        // At the end of each cluster, add in the letter spacing.
        if (i + 1 == m_item.num_glyphs || m_item.attributes[i + 1].clusterStart)
            position += m_letterSpacing;

        position += advance;
    }
    m_pixelWidth = std::max(position, 0.0);
    m_offsetX += m_pixelWidth * rtlFlip;
}

void ComplexTextController::normalizeSpacesAndMirrorChars(const UChar* source, bool rtl, UChar* destination, int length)
{
    int position = 0;
    bool error = false;
    // Iterate characters in source and mirror character if needed.
    while (position < length) {
        UChar32 character;
        int nextPosition = position;
        U16_NEXT(source, nextPosition, length, character);
        if (Font::treatAsSpace(character))
            character = ' ';
        else if (Font::treatAsZeroWidthSpace(character))
            character = zeroWidthSpace;
        else if (rtl)
            character = u_charMirror(character);
        U16_APPEND(destination, position, length, character, error);
        ASSERT(!error);
        position = nextPosition;
    }
}

const TextRun& ComplexTextController::getNormalizedTextRun(const TextRun& originalRun, OwnPtr<TextRun>& normalizedRun, OwnArrayPtr<UChar>& normalizedBuffer)
{
    // Normalize the text run in three ways:
    // 1) Convert the |originalRun| to NFC normalized form if combining diacritical marks
    // (U+0300..) are used in the run. This conversion is necessary since most OpenType
    // fonts (e.g., Arial) don't have substitution rules for the diacritical marks in
    // their GSUB tables.
    //
    // Note that we don't use the icu::Normalizer::isNormalized(UNORM_NFC) API here since
    // the API returns FALSE (= not normalized) for complex runs that don't require NFC
    // normalization (e.g., Arabic text). Unless the run contains the diacritical marks,
    // Harfbuzz will do the same thing for us using the GSUB table.
    // 2) Convert spacing characters into plain spaces, as some fonts will provide glyphs
    // for characters like '\n' otherwise.
    // 3) Convert mirrored characters such as parenthesis for rtl text.

    // Convert to NFC form if the text has diacritical marks.
    icu::UnicodeString normalizedString;
    UErrorCode error = U_ZERO_ERROR;

    for (int16_t i = 0; i < originalRun.length(); ++i) {
        UChar ch = originalRun[i];
        if (::ublock_getCode(ch) == UBLOCK_COMBINING_DIACRITICAL_MARKS) {
            icu::Normalizer::normalize(icu::UnicodeString(originalRun.characters(),
                                       originalRun.length()), UNORM_NFC, 0 /* no options */,
                                       normalizedString, error);
            if (U_FAILURE(error))
                return originalRun;
            break;
        }
    }

    // Normalize space and mirror parenthesis for rtl text.
    int normalizedBufferLength;
    const UChar* sourceText;
    if (normalizedString.isEmpty()) {
        normalizedBufferLength = originalRun.length();
        sourceText = originalRun.characters();
    } else {
        normalizedBufferLength = normalizedString.length();
        sourceText = normalizedString.getBuffer();
    }

    normalizedBuffer = adoptArrayPtr(new UChar[normalizedBufferLength + 1]);

    normalizeSpacesAndMirrorChars(sourceText, originalRun.rtl(), normalizedBuffer.get(), normalizedBufferLength);

    normalizedRun.set(new TextRun(originalRun));
    normalizedRun->setText(normalizedBuffer.get(), normalizedBufferLength);
    return *normalizedRun;
}

} // namespace WebCore
