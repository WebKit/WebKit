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
#include "ComplexTextControllerHarfBuzz.h"

#include "FloatRect.h"
#include "Font.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"

extern "C" {
#include "harfbuzz-unicode.h"
}

#include <unicode/normlzr.h>

namespace WebCore {

// Harfbuzz uses 26.6 fixed point values for pixel offsets. However, we don't
// handle subpixel positioning so this function is used to truncate Harfbuzz
// values to a number of pixels.
static int truncateFixedPointToInteger(HB_Fixed value)
{
    return value >> 6;
}

ComplexTextController::ComplexTextController(const TextRun& run, int startingX, int startingY, unsigned wordSpacing, unsigned letterSpacing, unsigned padding, const Font* font)
    : m_font(font)
    , m_run(getNormalizedTextRun(run, m_normalizedRun, m_normalizedBuffer))
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
    m_startingY = startingY;

    setWordSpacingAdjustment(wordSpacing);
    setLetterSpacingAdjustment(letterSpacing);
    setPadding(padding);
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
    m_padError = 0;
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

void ComplexTextController::reset(int offset)
{
    m_indexOfNextScriptRun = 0;
    m_offsetX = offset;
}

void ComplexTextController::setupForRTL()
{
    int padding = m_padding;
    // FIXME: this causes us to shape the text twice -- once to compute the width and then again
    // below when actually rendering. Change ComplexTextController to match platform/mac and
    // platform/chromium/win by having it store the shaped runs, so we can reuse the results.
    reset(m_offsetX + widthOfFullRun());
    // We need to set the padding again because ComplexTextController layout consumed the value.
    // Fixing the above problem would help here too.
    setPadding(padding);
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
    SurrogatePairAwareTextIterator iterator(static_cast<const UChar*>(&m_item.string[m_item.item.pos]), 0, static_cast<int>(m_item.item.length), static_cast<int>(m_item.item.length));
    UChar32 character;
    unsigned clusterLength = 0;
    if (!iterator.consume(character, clusterLength))
        return false;
    m_currentFontData = m_font->glyphDataForCharacter(character, false).fontData;
    iterator.advance(clusterLength);
    while (iterator.consume(character, clusterLength)) {
        const SimpleFontData* nextFontData = m_font->glyphDataForCharacter(character, false).fontData;
        if (nextFontData != m_currentFontData)
            break;
        iterator.advance(clusterLength);
    }
    m_item.item.length = iterator.currentCharacter();
    m_indexOfNextScriptRun = m_item.item.pos + iterator.currentCharacter();

    setupFontForScriptRun();
    shapeGlyphs();
    setGlyphPositions(rtl());

    return true;
}

float ComplexTextController::widthOfFullRun()
{
    float widthSum = 0;
    while (nextScriptRun())
        widthSum += width();

    return widthSum;
}

static void setupFontFeatures(const FontFeatureSettings* settings, HB_FaceRec_* hbFace)
{
    if (!settings)
        return;

    if (hbFace->gsub)
        HB_GSUB_Clear_Features(hbFace->gsub);
    if (hbFace->gpos)
        HB_GPOS_Clear_Features(hbFace->gpos);

    HB_UShort scriptIndex = 0;
    HB_GSUB_Select_Script(hbFace->gsub, HB_MAKE_TAG('D', 'F', 'L', 'T'), &scriptIndex);
    size_t numFeatures = settings->size();
    for (size_t i = 0; i < numFeatures; ++i) {
        if (!settings->at(i).value())
            continue;
        HB_UShort featureIndex = 0;
        const UChar* tag = settings->at(i).tag().characters();
        HB_UInt feature = HB_MAKE_TAG(tag[0], tag[1], tag[2], tag[3]);
        if (hbFace->gsub && HB_GSUB_Select_Feature(hbFace->gsub, feature, scriptIndex, 0xffff, &featureIndex) == HB_Err_Ok)
            HB_GSUB_Add_Feature(hbFace->gsub, featureIndex, settings->at(i).value());
        else if (hbFace->gpos && HB_GPOS_Select_Feature(hbFace->gpos, feature, scriptIndex, 0xffff, &featureIndex) == HB_Err_Ok)
            HB_GPOS_Add_Feature(hbFace->gpos, featureIndex, settings->at(i).value());
    }
}

static UChar32 surrogatePairAwareFirstCharacter(const UChar* characters, unsigned length)
{
    if (U16_IS_SURROGATE(characters[0])) {
        if (!U16_IS_SURROGATE_LEAD(characters[0]) || length < 2 || !U16_IS_TRAIL(characters[1]))
            return ' ';
        return U16_GET_SUPPLEMENTARY(characters[0], characters[1]);
    }
    return characters[0];
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
    UChar32 current = surrogatePairAwareFirstCharacter(static_cast<const UChar*>(&m_item.string[m_item.item.pos]), m_item.item.length - m_item.item.pos);
    const FontData* fontData = m_font->glyphDataForCharacter(current, false, fontDataVariant).fontData;
    const FontPlatformData& platformData = fontData->fontDataForCharacter(' ')->platformData();
    m_item.face = platformData.harfbuzzFace()->face();
    // We only need to setup font features at the beginning of the run.
    if (!m_item.item.pos)
        setupFontFeatures(m_font->fontDescription().featureSettings(), m_item.face);
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

void ComplexTextController::deleteGlyphArrays()
{
    delete[] m_item.glyphs;
    delete[] m_item.attributes;
    delete[] m_item.advances;
    delete[] m_item.offsets;
    delete[] m_glyphs16;
    delete[] m_positions;
}

void ComplexTextController::createGlyphArrays(int size)
{
    m_item.glyphs = new HB_Glyph[size];
    m_item.attributes = new HB_GlyphAttributes[size];
    m_item.advances = new HB_Fixed[size];
    m_item.offsets = new HB_FixedPoint[size];

    m_glyphs16 = new uint16_t[size];
    m_positions = new SkPoint[size];

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
    memset(m_positions, 0, size * sizeof(SkPoint));
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

void ComplexTextController::setGlyphPositions(bool isRTL)
{
    const double rtlFlip = isRTL ? -1 : 1;
    double position = 0;

    // logClustersIndex indexes logClusters for the first codepoint of the current glyph.
    // Each time we advance a glyph, we skip over all the codepoints that contributed to the current glyph.
    int logClustersIndex = 0;

    // Iterate through the glyphs in logical order, flipping for RTL where necessary.
    // Glyphs are positioned starting from m_offsetX; in RTL mode they go leftwards from there.
    for (size_t i = 0; i < m_item.num_glyphs; ++i) {
        while (static_cast<unsigned>(logClustersIndex) < m_item.item.length && m_item.log_clusters[logClustersIndex] < i)
            logClustersIndex++;

        // If the current glyph is just after a space, add in the word spacing.
        position += determineWordBreakSpacing(logClustersIndex);

        m_glyphs16[i] = m_item.glyphs[i];
        double offsetX = truncateFixedPointToInteger(m_item.offsets[i].x);
        double offsetY = truncateFixedPointToInteger(m_item.offsets[i].y);
        double advance = truncateFixedPointToInteger(m_item.advances[i]);
        if (isRTL)
            offsetX -= advance;

        m_positions[i].set(m_offsetX + (position * rtlFlip) + offsetX,
                           m_startingY + offsetY);

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
        ASSERT_UNUSED(error, !error);
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

    for (int i = 0; i < originalRun.length(); ++i) {
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

    normalizedRun = adoptPtr(new TextRun(originalRun));
    normalizedRun->setText(normalizedBuffer.get(), normalizedBufferLength);
    return *normalizedRun;
}

int ComplexTextController::glyphIndexForXPositionInScriptRun(int targetX) const
{
    // Iterate through the glyphs in logical order, seeing whether targetX falls between the previous
    // position and halfway through the current glyph.
    // FIXME: this code probably belongs in ComplexTextController.
    int lastX = offsetX() - (rtl() ? -m_pixelWidth : m_pixelWidth);
    for (int glyphIndex = 0; static_cast<unsigned>(glyphIndex) < length(); ++glyphIndex) {
        int advance = truncateFixedPointToInteger(m_item.advances[glyphIndex]);
        int nextX = static_cast<int>(positions()[glyphIndex].x()) + advance / 2;
        if (std::min(nextX, lastX) <= targetX && targetX <= std::max(nextX, lastX))
            return glyphIndex;
        lastX = nextX;
    }

    return length() - 1;
}

int ComplexTextController::offsetForPosition(int targetX)
{
    unsigned basePosition = 0;

    int x = offsetX();
    while (nextScriptRun()) {
        int nextX = offsetX();

        if (std::min(x, nextX) <= targetX && targetX <= std::max(x, nextX)) {
            // The x value in question is within this script run.
            const int glyphIndex = glyphIndexForXPositionInScriptRun(targetX);

            // Now that we have a glyph index, we have to turn that into a
            // code-point index. Because of ligatures, several code-points may
            // have gone into a single glyph. We iterate over the clusters log
            // and find the first code-point which contributed to the glyph.

            // Some shapers (i.e. Khmer) will produce cluster logs which report
            // that /no/ code points contributed to certain glyphs. Because of
            // this, we take any code point which contributed to the glyph in
            // question, or any subsequent glyph. If we run off the end, then
            // we take the last code point.
            for (unsigned j = 0; j < numCodePoints(); ++j) {
                if (m_item.log_clusters[j] >= glyphIndex)
                    return basePosition + j;
            }

            return basePosition + numCodePoints() - 1;
        }

        basePosition += numCodePoints();
    }

    return basePosition;
}

FloatRect ComplexTextController::selectionRect(const FloatPoint& point, int height, int from, int to)
{
    int fromX = -1, toX = -1;
    // Iterate through the script runs in logical order, searching for the run covering the positions of interest.
    while (nextScriptRun() && (fromX == -1 || toX == -1)) {
        if (fromX == -1 && from >= 0 && static_cast<unsigned>(from) < numCodePoints()) {
            // |from| is within this script run. So we index the clusters log to
            // find which glyph this code-point contributed to and find its x
            // position.
            int glyph = m_item.log_clusters[from];
            fromX = positions()[glyph].x();
            if (rtl())
                fromX += truncateFixedPointToInteger(m_item.advances[glyph]);
        } else
            from -= numCodePoints();

        if (toX == -1 && to >= 0 && static_cast<unsigned>(to) < numCodePoints()) {
            int glyph = m_item.log_clusters[to];
            toX = positions()[glyph].x();
            if (rtl())
                toX += truncateFixedPointToInteger(m_item.advances[glyph]);
        } else
            to -= numCodePoints();
    }

    // The position in question might be just after the text.
    if (fromX == -1)
        fromX = offsetX();
    if (toX == -1)
        toX = offsetX();

    ASSERT(fromX != -1 && toX != -1);

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);

    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

void ComplexTextController::glyphsForRange(int from, int to, int& fromGlyph, int& glyphLength)
{
    // Character offsets within the current run. THESE MAY NOT BE IN RANGE and may
    // be negative, etc. The code below handles this.
    int fromChar = from - m_item.item.pos;
    int toChar = to - m_item.item.pos;

    // See if there are any characters in the current run.
    if (!numCodePoints() || fromChar >= static_cast<int>(numCodePoints()) || toChar <= 0) {
        fromGlyph = -1;
        glyphLength = 0;
        return;
    }

    // Compute the starting glyph within this span. |from| and |to| are
    // global offsets that may intersect arbitrarily with our local run.
    fromGlyph = m_item.log_clusters[fromChar < 0 ? 0 : fromChar];
    if (toChar >= static_cast<int>(numCodePoints()))
        glyphLength = length() - fromGlyph;
    else
        glyphLength = m_item.log_clusters[toChar] - fromGlyph;
}

} // namespace WebCore
