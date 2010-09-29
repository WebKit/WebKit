/*
 * Copyright (c) 2007, 2008, Google Inc. All rights reserved.
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
#include "Font.h"

#include "FloatRect.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "HarfbuzzSkia.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkTemplates.h"
#include "SkTypeface.h"
#include "SkUtils.h"

#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

static bool isCanvasMultiLayered(SkCanvas* canvas)
{
    SkCanvas::LayerIter layerIterator(canvas, false);
    layerIterator.next();
    return !layerIterator.done();
}

static void adjustTextRenderMode(SkPaint* paint, PlatformContextSkia* skiaContext)
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be compositied correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer or when the compositor is being used.
    if (isCanvasMultiLayered(skiaContext->canvas()) || skiaContext->isDrawingToImageBuffer())
        paint->setLCDRenderText(false);
}

void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point) const {
    SkASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t)); // compile-time assert

    const GlyphBufferGlyph* glyphs = glyphBuffer.glyphs(from);
    SkScalar x = SkFloatToScalar(point.x());
    SkScalar y = SkFloatToScalar(point.y());

    // FIXME: text rendering speed:
    // Android has code in their WebCore fork to special case when the
    // GlyphBuffer has no advances other than the defaults. In that case the
    // text drawing can proceed faster. However, it's unclear when those
    // patches may be upstreamed to WebKit so we always use the slower path
    // here.
    const GlyphBufferAdvance* adv = glyphBuffer.advances(from);
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs);
    SkPoint* pos = storage.get();

    for (int i = 0; i < numGlyphs; i++) {
        pos[i].set(x, y);
        x += SkFloatToScalar(adv[i].width());
        y += SkFloatToScalar(adv[i].height());
    }

    gc->platformContext()->prepareForSoftwareDraw();

    SkCanvas* canvas = gc->platformContext()->canvas();
    int textMode = gc->platformContext()->getTextDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & cTextFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->fillColor().rgb());
        canvas->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }

    if ((textMode & cTextStroke)
        && gc->platformContext()->getStrokeStyle() != NoStroke
        && gc->platformContext()->getStrokeThickness() > 0) {

        SkPaint paint;
        gc->platformContext()->setupPaintForStroking(&paint, 0, 0);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->strokeColor().rgb());

        if (textMode & cTextFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            SkSafeUnref(paint.setLooper(0));
        }

        canvas->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }
}

// Harfbuzz uses 26.6 fixed point values for pixel offsets. However, we don't
// handle subpixel positioning so this function is used to truncate Harfbuzz
// values to a number of pixels.
static int truncateFixedPointToInteger(HB_Fixed value)
{
    return value >> 6;
}

// TextRunWalker walks a TextRun and presents each script run in sequence. A
// TextRun is a sequence of code-points with the same embedding level (i.e. they
// are all left-to-right or right-to-left). A script run is a subsequence where
// all the characters have the same script (e.g. Arabic, Thai etc). Shaping is
// only ever done with script runs since the shapers only know how to deal with
// a single script.
//
// After creating it, the script runs are either iterated backwards or forwards.
// It defaults to backwards for RTL and forwards otherwise (which matches the
// presentation order), however you can set it with |setBackwardsIteration|.
//
// Once you have setup the object, call |nextScriptRun| to get the first script
// run. This will return false when the iteration is complete. At any time you
// can call |reset| to start over again.
class TextRunWalker {
public:
    TextRunWalker(const TextRun& run, unsigned startingX, const Font* font)
        : m_font(font)
        , m_startingX(startingX)
        , m_offsetX(m_startingX)
        , m_run(getTextRun(run))
        , m_iterateBackwards(m_run.rtl())
        , m_wordSpacingAdjustment(0)
        , m_padding(0)
        , m_padError(0)
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

        int length = m_run.length();
        m_item.stringLength = length;

        if (!m_item.item.bidiLevel)
            m_item.string = m_run.characters();
        else {
            // Assume mirrored character is in the same Unicode multilingual plane as the original one.
            UChar* string = new UChar[length];
            mirrorCharacters(string, m_run.characters(), length);
            m_item.string = string;
        }

        reset();
    }

    ~TextRunWalker()
    {
        fastFree(m_item.font);
        deleteGlyphArrays();
        delete[] m_item.log_clusters;
        if (m_item.item.bidiLevel)
            delete[] m_item.string;
    }

    // setWordSpacingAdjustment sets a delta (in pixels) which is applied at
    // each word break in the TextRun.
    void setWordSpacingAdjustment(int wordSpacingAdjustment)
    {
        m_wordSpacingAdjustment = wordSpacingAdjustment;
    }

    // setLetterSpacingAdjustment sets an additional number of pixels that is
    // added to the advance after each output cluster. This matches the behaviour
    // of WidthIterator::advance.
    //
    // (NOTE: currently does nothing because I don't know how to get the
    // cluster information from Harfbuzz.)
    void setLetterSpacingAdjustment(int letterSpacingAdjustment)
    {
        m_letterSpacing = letterSpacingAdjustment;
    }

    bool isWordBreak(unsigned i, bool isRTL)
    {
        if (!isRTL)
            return i && isCodepointSpace(m_item.string[i]) && !isCodepointSpace(m_item.string[i - 1]);
        return i != m_item.stringLength - 1 && isCodepointSpace(m_item.string[i]) && !isCodepointSpace(m_item.string[i + 1]);
    }

    // setPadding sets a number of pixels to be distributed across the TextRun.
    // WebKit uses this to justify text.
    void setPadding(int padding)
    {
        m_padding = padding;
        if (!m_padding)
            return;

        // If we have padding to distribute, then we try to give an equal
        // amount to each space. The last space gets the smaller amount, if
        // any.
        unsigned numWordBreaks = 0;
        bool isRTL = m_iterateBackwards;

        for (unsigned i = 0; i < m_item.stringLength; i++) {
            if (isWordBreak(i, isRTL))
                numWordBreaks++;
        }

        if (numWordBreaks)
            m_padPerWordBreak = m_padding / numWordBreaks;
        else
            m_padPerWordBreak = 0;
    }

    void reset()
    {
        if (m_iterateBackwards)
            m_indexOfNextScriptRun = m_run.length() - 1;
        else
            m_indexOfNextScriptRun = 0;
        m_offsetX = m_startingX;
    }

    // Set the x offset for the next script run. This affects the values in
    // |xPositions|
    void setXOffsetToZero()
    {
        m_offsetX = 0;
    }

    bool rtl() const
    {
        return m_run.rtl();
    }

    void setBackwardsIteration(bool isBackwards)
    {
        m_iterateBackwards = isBackwards;
        reset();
    }

    // Advance to the next script run, returning false when the end of the
    // TextRun has been reached.
    bool nextScriptRun()
    {
        if (m_iterateBackwards) {
            // In right-to-left mode we need to render the shaped glyph backwards and
            // also render the script runs themselves backwards. So given a TextRun:
            //    AAAAAAACTTTTTTT   (A = Arabic, C = Common, T = Thai)
            // we render:
            //    TTTTTTCAAAAAAA
            // (and the glyphs in each A, C and T section are backwards too)
            if (!hb_utf16_script_run_prev(&m_numCodePoints, &m_item.item, m_run.characters(), m_run.length(), &m_indexOfNextScriptRun))
                return false;
        } else {
            if (!hb_utf16_script_run_next(&m_numCodePoints, &m_item.item, m_run.characters(), m_run.length(), &m_indexOfNextScriptRun))
                return false;

            // It is actually wrong to consider script runs at all in this code.
            // Other WebKit code (e.g. Mac) segments complex text just by finding
            // the longest span of text covered by a single font.
            // But we currently need to call hb_utf16_script_run_next anyway to fill
            // in the harfbuzz data structures to e.g. pick the correct script's shaper.
            // So we allow that to run first, then do a second pass over the range it
            // found and take the largest subregion that stays within a single font.
            const FontData* glyphData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos], false, false).fontData;
            unsigned endOfRun;
            for (endOfRun = 1; endOfRun < m_item.item.length; ++endOfRun) {
                const FontData* nextGlyphData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos + endOfRun], false, false).fontData;
                if (nextGlyphData != glyphData)
                    break;
            }
            m_item.item.length = endOfRun;
            m_indexOfNextScriptRun = m_item.item.pos + endOfRun;
        }

        setupFontForScriptRun();
        shapeGlyphs();
        setGlyphXPositions(rtl());

        return true;
    }

    const uint16_t* glyphs() const
    {
        return m_glyphs16;
    }

    // Return the length of the array returned by |glyphs|
    const unsigned length() const
    {
        return m_item.num_glyphs;
    }

    // Return the x offset for each of the glyphs. Note that this is translated
    // by the current x offset and that the x offset is updated for each script
    // run.
    const SkScalar* xPositions() const
    {
        return m_xPositions;
    }

    // Get the advances (widths) for each glyph.
    const HB_Fixed* advances() const
    {
        return m_item.advances;
    }

    // Return the width (in px) of the current script run.
    const unsigned width() const
    {
        return m_pixelWidth;
    }

    // Return the cluster log for the current script run. For example:
    //   script run: f i a n c é  (fi gets ligatured)
    //   log clutrs: 0 0 1 2 3 4
    // So, for each input code point, the log tells you which output glyph was
    // generated for it.
    const unsigned short* logClusters() const
    {
        return m_item.log_clusters;
    }

    // return the number of code points in the current script run
    const unsigned numCodePoints() const
    {
        return m_numCodePoints;
    }

    const FontPlatformData* fontPlatformDataForScriptRun()
    {
        return reinterpret_cast<FontPlatformData*>(m_item.font->userData);
    }

    float widthOfFullRun()
    {
        float widthSum = 0;
        while (nextScriptRun())
            widthSum += width();

        return widthSum;
    }

private:
    const TextRun& getTextRun(const TextRun& originalRun)
    {
        // Normalize the text run in two ways:
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
        for (int i = 0; i < originalRun.length(); ++i) {
            UChar ch = originalRun[i];
            UBlockCode block = ::ublock_getCode(ch);
            if (block == UBLOCK_COMBINING_DIACRITICAL_MARKS || (Font::treatAsSpace(ch) && ch != ' ')) {
                return getNormalizedTextRun(originalRun);
            }
        }
        return originalRun;
    }

    const TextRun& getNormalizedTextRun(const TextRun& originalRun)
    {
        icu::UnicodeString normalizedString;
        UErrorCode error = U_ZERO_ERROR;
        icu::Normalizer::normalize(icu::UnicodeString(originalRun.characters(), originalRun.length()), UNORM_NFC, 0 /* no options */, normalizedString, error);
        if (U_FAILURE(error))
            return originalRun;

        m_normalizedBuffer.set(new UChar[normalizedString.length() + 1]);
        normalizedString.extract(m_normalizedBuffer.get(), normalizedString.length() + 1, error);
        ASSERT(U_SUCCESS(error));

        for (int i = 0; i < normalizedString.length(); ++i) {
            if (Font::treatAsSpace(m_normalizedBuffer[i]))
                m_normalizedBuffer[i] = ' ';
        }

        m_normalizedRun.set(new TextRun(originalRun));
        m_normalizedRun->setText(m_normalizedBuffer.get(), normalizedString.length());
        return *m_normalizedRun;
    }

    void setupFontForScriptRun()
    {
        const FontData* fontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos], false, false).fontData;
        const FontPlatformData& platformData = fontData->fontDataForCharacter(' ')->platformData();
        m_item.face = platformData.harfbuzzFace();
        void* opaquePlatformData = const_cast<FontPlatformData*>(&platformData);
        m_item.font->userData = opaquePlatformData;
    }

    HB_FontRec* allocHarfbuzzFont()
    {
        HB_FontRec* font = reinterpret_cast<HB_FontRec*>(fastMalloc(sizeof(HB_FontRec)));
        memset(font, 0, sizeof(HB_FontRec));
        font->klass = &harfbuzzSkiaClass;
        font->userData = 0;
        // The values which harfbuzzSkiaClass returns are already scaled to
        // pixel units, so we just set all these to one to disable further
        // scaling.
        font->x_ppem = 1;
        font->y_ppem = 1;
        font->x_scale = 1;
        font->y_scale = 1;

        return font;
    }

    void deleteGlyphArrays()
    {
        delete[] m_item.glyphs;
        delete[] m_item.attributes;
        delete[] m_item.advances;
        delete[] m_item.offsets;
        delete[] m_glyphs16;
        delete[] m_xPositions;
    }

    void createGlyphArrays(int size)
    {
        m_item.glyphs = new HB_Glyph[size];
        memset(m_item.glyphs, 0, size * sizeof(HB_Glyph));
        m_item.attributes = new HB_GlyphAttributes[size];
        memset(m_item.attributes, 0, size * sizeof(HB_GlyphAttributes));
        m_item.advances = new HB_Fixed[size];
        memset(m_item.advances, 0, size * sizeof(HB_Fixed));
        m_item.offsets = new HB_FixedPoint[size];
        memset(m_item.offsets, 0, size * sizeof(HB_FixedPoint));

        m_glyphs16 = new uint16_t[size];
        m_xPositions = new SkScalar[size];

        m_item.num_glyphs = size;
    }

    void shapeGlyphs()
    {
        for (;;) {
            if (HB_ShapeItem(&m_item))
                break;

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

    void setGlyphXPositions(bool isRTL)
    {
        double position = 0;
        // logClustersIndex indexes logClusters for the first (or last when
        // RTL) codepoint of the current glyph.  Each time we advance a glyph,
        // we skip over all the codepoints that contributed to the current
        // glyph.
        unsigned logClustersIndex = isRTL ? m_item.num_glyphs - 1 : 0;

        for (unsigned iter = 0; iter < m_item.num_glyphs; ++iter) {
            // Glyphs are stored in logical order, but for layout purposes we
            // always go left to right.
            int i = isRTL ? m_item.num_glyphs - iter - 1 : iter;

            m_glyphs16[i] = m_item.glyphs[i];
            double offsetX = truncateFixedPointToInteger(m_item.offsets[i].x);
            m_xPositions[i] = m_offsetX + position + offsetX;

            double advance = truncateFixedPointToInteger(m_item.advances[i]);
            // The first half of the conjuction works around the case where
            // output glyphs aren't associated with any codepoints by the
            // clusters log.
            if (logClustersIndex < m_item.item.length
                && isWordBreak(m_item.item.pos + logClustersIndex, isRTL)) {
                advance += m_wordSpacingAdjustment;

                if (m_padding > 0) {
                    unsigned toPad = roundf(m_padPerWordBreak + m_padError);
                    m_padError += m_padPerWordBreak - toPad;

                    if (m_padding < toPad)
                        toPad = m_padding;
                    m_padding -= toPad;
                    advance += toPad;
                }
            }

            // We would like to add m_letterSpacing after each cluster, but I
            // don't know where the cluster information is. This is typically
            // fine for Roman languages, but breaks more complex languages
            // terribly.
            // advance += m_letterSpacing;

            if (isRTL) {
                while (logClustersIndex > 0 && logClusters()[logClustersIndex] == i)
                    logClustersIndex--;
            } else {
                while (logClustersIndex < m_item.item.length && logClusters()[logClustersIndex] == i)
                    logClustersIndex++;
            }

            position += advance;
        }

        m_pixelWidth = position;
        m_offsetX += m_pixelWidth;
    }

    static bool isCodepointSpace(HB_UChar16 c)
    {
        // This matches the logic in RenderBlock::findNextLineBreak
        return c == ' ' || c == '\t';
    }

    void mirrorCharacters(UChar* destination, const UChar* source, int length) const
    {
        int position = 0;
        bool error = false;
        // Iterate characters in source and mirror character if needed.
        while (position < length) {
            UChar32 character;
            int nextPosition = position;
            U16_NEXT(source, nextPosition, length, character);
            character = u_charMirror(character);
            U16_APPEND(destination, position, length, character, error);
            ASSERT(!error);
            position = nextPosition;
        }
    }

    const Font* const m_font;
    HB_ShaperItem m_item;
    uint16_t* m_glyphs16; // A vector of 16-bit glyph ids.
    SkScalar* m_xPositions; // A vector of x positions for each glyph.
    ssize_t m_indexOfNextScriptRun; // Indexes the script run in |m_run|.
    const unsigned m_startingX; // Offset in pixels of the first script run.
    unsigned m_offsetX; // Offset in pixels to the start of the next script run.
    unsigned m_pixelWidth; // Width (in px) of the current script run.
    unsigned m_numCodePoints; // Code points in current script run.

    OwnPtr<TextRun> m_normalizedRun;
    OwnArrayPtr<UChar> m_normalizedBuffer; // A buffer for normalized run.
    const TextRun& m_run;
    bool m_iterateBackwards;
    int m_wordSpacingAdjustment; // delta adjustment (pixels) for each word break.
    float m_padding; // pixels to be distributed over the line at word breaks.
    float m_padPerWordBreak; // pixels to be added to each word break.
    float m_padError; // |m_padPerWordBreak| might have a fractional component.
                      // Since we only add a whole number of padding pixels at
                      // each word break we accumulate error. This is the
                      // number of pixels that we are behind so far.
    unsigned m_letterSpacing; // pixels to be added after each glyph.
};

static void setupForTextPainting(SkPaint* paint, SkColor color)
{
    paint->setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    paint->setColor(color);
}

void Font::drawComplexText(GraphicsContext* gc, const TextRun& run,
                           const FloatPoint& point, int from, int to) const
{
    if (!run.length())
        return;

    SkCanvas* canvas = gc->platformContext()->canvas();
    int textMode = gc->platformContext()->getTextDrawingMode();
    bool fill = textMode & cTextFill;
    bool stroke = (textMode & cTextStroke)
               && gc->platformContext()->getStrokeStyle() != NoStroke
               && gc->platformContext()->getStrokeThickness() > 0;

    if (!fill && !stroke)
        return;

    SkPaint strokePaint, fillPaint;
    if (fill) {
        gc->platformContext()->setupPaintForFilling(&fillPaint);
        setupForTextPainting(&fillPaint, gc->fillColor().rgb());
    }
    if (stroke) {
        gc->platformContext()->setupPaintForStroking(&strokePaint, 0, 0);
        setupForTextPainting(&strokePaint, gc->strokeColor().rgb());
    }

    TextRunWalker walker(run, point.x(), this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());
    walker.setPadding(run.padding());

    while (walker.nextScriptRun()) {
        if (fill) {
            walker.fontPlatformDataForScriptRun()->setupPaint(&fillPaint);
            adjustTextRenderMode(&fillPaint, gc->platformContext());
            canvas->drawPosTextH(walker.glyphs(), walker.length() << 1, walker.xPositions(), point.y(), fillPaint);
        }

        if (stroke) {
            walker.fontPlatformDataForScriptRun()->setupPaint(&strokePaint);
            adjustTextRenderMode(&strokePaint, gc->platformContext());
            canvas->drawPosTextH(walker.glyphs(), walker.length() << 1, walker.xPositions(), point.y(), strokePaint);
        }
    }
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */, GlyphOverflow* /* glyphOverflow */) const
{
    TextRunWalker walker(run, 0, this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());
    return walker.widthOfFullRun();
}

static int glyphIndexForXPositionInScriptRun(const TextRunWalker& walker, int x)
{
    const HB_Fixed* advances = walker.advances();
    int glyphIndex;
    if (walker.rtl()) {
        for (glyphIndex = walker.length() - 1; glyphIndex >= 0; --glyphIndex) {
            if (x < truncateFixedPointToInteger(advances[glyphIndex]))
                break;
            x -= truncateFixedPointToInteger(advances[glyphIndex]);
        }
    } else {
        for (glyphIndex = 0; static_cast<unsigned>(glyphIndex) < walker.length(); ++glyphIndex) {
            if (x < truncateFixedPointToInteger(advances[glyphIndex]))
                break;
            x -= truncateFixedPointToInteger(advances[glyphIndex]);
        }
    }

    return glyphIndex;
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
                                          bool includePartialGlyphs) const
{
    // FIXME: This truncation is not a problem for HTML, but only affects SVG, which passes floating-point numbers
    // to Font::offsetForPosition(). Bug http://webkit.org/b/40673 tracks fixing this problem.
    int x = static_cast<int>(xFloat);

    // (Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.)
    TextRunWalker walker(run, 0, this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());

    // If this is RTL text, the first glyph from the left is actually the last
    // code point. So we need to know how many code points there are total in
    // order to subtract. This is different from the length of the TextRun
    // because UTF-16 surrogate pairs are a single code point, but 32-bits long.
    // In LTR we leave this as 0 so that we get the correct value for
    // |basePosition|, below.
    unsigned totalCodePoints = 0;
    if (walker.rtl()) {
        ssize_t offset = 0;
        while (offset < run.length()) {
            utf16_to_code_point(run.characters(), run.length(), &offset);
            totalCodePoints++;
        }
    }

    unsigned basePosition = totalCodePoints;

    // For RTL:
    //   code-point order:  abcd efg hijkl
    //   on screen:         lkjih gfe dcba
    //                                ^   ^
    //                                |   |
    //                  basePosition--|   |
    //                 totalCodePoints----|
    // Since basePosition is currently the total number of code-points, the
    // first thing we do is decrement it so that it's pointing to the start of
    // the current script-run.
    //
    // For LTR, basePosition is zero so it already points to the start of the
    // first script run.
    while (walker.nextScriptRun()) {
        if (walker.rtl())
            basePosition -= walker.numCodePoints();

        if (x >= 0 && static_cast<unsigned>(x) < walker.width()) {
            // The x value in question is within this script run. We consider
            // each glyph in presentation order and stop when we find the one
            // covering this position.
            const int glyphIndex = glyphIndexForXPositionInScriptRun(walker, x);

            // Now that we have a glyph index, we have to turn that into a
            // code-point index. Because of ligatures, several code-points may
            // have gone into a single glyph. We iterate over the clusters log
            // and find the first code-point which contributed to the glyph.

            // Some shapers (i.e. Khmer) will produce cluster logs which report
            // that /no/ code points contributed to certain glyphs. Because of
            // this, we take any code point which contributed to the glyph in
            // question, or any subsequent glyph. If we run off the end, then
            // we take the last code point.
            const unsigned short* log = walker.logClusters();
            for (unsigned j = 0; j < walker.numCodePoints(); ++j) {
                if (log[j] >= glyphIndex)
                    return basePosition + j;
            }

            return basePosition + walker.numCodePoints() - 1;
        }

        x -= walker.width();

        if (!walker.rtl())
            basePosition += walker.numCodePoints();
    }

    return basePosition;
}

// Return the rectangle for selecting the given range of code-points in the TextRun.
FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const FloatPoint& point, int height,
                                            int from, int to) const
{
    int fromX = -1, toX = -1, fromAdvance = -1, toAdvance = -1;
    TextRunWalker walker(run, 0, this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());

    // Base will point to the x offset for the current script run. Note that, in
    // the LTR case, width will be 0.
    int base = walker.rtl() ? walker.widthOfFullRun() : 0;
    const int leftEdge = base;

    // We want to enumerate the script runs in code point order in the following
    // code. This call also resets |walker|.
    walker.setBackwardsIteration(false);

    while (walker.nextScriptRun() && (fromX == -1 || toX == -1)) {
        // TextRunWalker will helpfully accululate the x offsets for different
        // script runs for us. For this code, however, we always want the x offsets
        // to start from zero so we call this before each script run.
        walker.setXOffsetToZero();

        if (walker.rtl())
            base -= walker.width();

        if (fromX == -1 && from >= 0 && static_cast<unsigned>(from) < walker.numCodePoints()) {
            // |from| is within this script run. So we index the clusters log to
            // find which glyph this code-point contributed to and find its x
            // position.
            int glyph = walker.logClusters()[from];
            fromX = base + walker.xPositions()[glyph];
            fromAdvance = walker.advances()[glyph];
        } else
            from -= walker.numCodePoints();

        if (toX == -1 && to >= 0 && static_cast<unsigned>(to) < walker.numCodePoints()) {
            int glyph = walker.logClusters()[to];
            toX = base + walker.xPositions()[glyph];
            toAdvance = walker.advances()[glyph];
        } else
            to -= walker.numCodePoints();

        if (!walker.rtl())
            base += walker.width();
    }

    // The position in question might be just after the text.
    const int rightEdge = base;
    if (fromX == -1 && !from)
        fromX = leftEdge;
    else if (walker.rtl())
       fromX += truncateFixedPointToInteger(fromAdvance);

    if (toX == -1 && !to)
        toX = rightEdge;

    ASSERT(fromX != -1 && toX != -1);

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);

    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

} // namespace WebCore
