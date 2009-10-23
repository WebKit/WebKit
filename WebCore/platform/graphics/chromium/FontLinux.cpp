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

static bool adjustTextRenderMode(SkPaint* paint, bool isCanvasMultiLayered)
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be compositied correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer.
    if (isCanvasMultiLayered)
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

    SkCanvas* canvas = gc->platformContext()->canvas();
    int textMode = gc->platformContext()->getTextDrawingMode();
    bool haveMultipleLayers = isCanvasMultiLayered(canvas);

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & cTextFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, haveMultipleLayers);
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
        adjustTextRenderMode(&paint, haveMultipleLayers);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->strokeColor().rgb());

        if (textMode & cTextFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            paint.setLooper(0)->safeUnref();
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
    {
        // Do not use |run| inside this constructor. Use |m_run| instead.

        memset(&m_item, 0, sizeof(m_item));
        // We cannot know, ahead of time, how many glyphs a given script run
        // will produce. We take a guess that script runs will not produce more
        // than twice as many glyphs as there are code points and fallback if
        // we find that we are wrong.
        m_maxGlyphs = m_run.length() * 2;
        createGlyphArrays();

        m_item.log_clusters = new unsigned short[m_run.length()];

        m_item.face = 0;
        m_item.font = allocHarfbuzzFont();

        m_item.string = m_run.characters();
        m_item.stringLength = m_run.length();
        m_item.item.bidiLevel = m_run.rtl();

        reset();
    }

    ~TextRunWalker()
    {
        fastFree(m_item.font);
        deleteGlyphArrays();
        delete[] m_item.log_clusters;
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
        }

        setupFontForScriptRun();

        if (!shapeGlyphs())
            return false;
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
        // Convert the |originalRun| to NFC normalized form if combining diacritical marks
        // (U+0300..) are used in the run. This conversion is necessary since most OpenType
        // fonts (e.g., Arial) don't have substitution rules for the diacritical marks in
        // their GSUB tables.
        //
        // Note that we don't use the icu::Normalizer::isNormalized(UNORM_NFC) API here since
        // the API returns FALSE (= not normalized) for complex runs that don't require NFC
        // normalization (e.g., Arabic text). Unless the run contains the diacritical marks,
        // Harfbuzz will do the same thing for us using the GSUB table.
        for (unsigned i = 0; i < originalRun.length(); ++i) {
            UBlockCode block = ::ublock_getCode(originalRun[i]);
            if (block == UBLOCK_COMBINING_DIACRITICAL_MARKS) {
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

        m_normalizedRun.set(new TextRun(originalRun));
        m_normalizedRun->setText(m_normalizedBuffer.get(), normalizedString.length());
        return *m_normalizedRun;
    }

    void setupFontForScriptRun()
    {
        const FontData* fontData = m_font->fontDataAt(0);
        if (!fontData->containsCharacters(m_item.string + m_item.item.pos, m_item.item.length))
            fontData = m_font->fontDataForCharacters(m_item.string + m_item.item.pos, m_item.item.length);
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

    bool createGlyphArrays()
    {
        m_item.glyphs = new HB_Glyph[m_maxGlyphs];
        m_item.attributes = new HB_GlyphAttributes[m_maxGlyphs];
        m_item.advances = new HB_Fixed[m_maxGlyphs];
        m_item.offsets = new HB_FixedPoint[m_maxGlyphs];
        m_glyphs16 = new uint16_t[m_maxGlyphs];
        m_xPositions = new SkScalar[m_maxGlyphs];

        return m_item.glyphs
            && m_item.attributes
            && m_item.advances
            && m_item.offsets
            && m_glyphs16
            && m_xPositions;
    }

    bool expandGlyphArrays()
    {
        deleteGlyphArrays();
        m_maxGlyphs <<= 1;
        return createGlyphArrays();
    }

    bool shapeGlyphs()
    {
        for (;;) {
            m_item.num_glyphs = m_maxGlyphs;
            HB_ShapeItem(&m_item);
            if (m_item.num_glyphs < m_maxGlyphs)
                break;

            // We overflowed our arrays. Resize and retry.
            if (!expandGlyphArrays())
                return false;
        }

        return true;
    }

    void setGlyphXPositions(bool isRTL)
    {
        m_pixelWidth = 0;
        for (unsigned i = 0; i < m_item.num_glyphs; ++i) {
            int index;
            if (isRTL)
                index = m_item.num_glyphs - (i + 1);
            else
                index = i;

            m_glyphs16[i] = m_item.glyphs[i];
            m_xPositions[index] = m_offsetX + m_pixelWidth;
            m_pixelWidth += truncateFixedPointToInteger(m_item.advances[index]);
        }
        m_offsetX += m_pixelWidth;
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
    unsigned m_maxGlyphs; // Current size of all the Harfbuzz arrays.

    OwnPtr<TextRun> m_normalizedRun;
    OwnArrayPtr<UChar> m_normalizedBuffer; // A buffer for normalized run.
    const TextRun& m_run;
    bool m_iterateBackwards;
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
    bool haveMultipleLayers = isCanvasMultiLayered(canvas);

    while (walker.nextScriptRun()) {
        if (fill) {
            walker.fontPlatformDataForScriptRun()->setupPaint(&fillPaint);
            adjustTextRenderMode(&fillPaint, haveMultipleLayers);
            canvas->drawPosTextH(walker.glyphs(), walker.length() << 1, walker.xPositions(), point.y(), fillPaint);
        }

        if (stroke) {
            walker.fontPlatformDataForScriptRun()->setupPaint(&strokePaint);
            adjustTextRenderMode(&strokePaint, haveMultipleLayers);
            canvas->drawPosTextH(walker.glyphs(), walker.length() << 1, walker.xPositions(), point.y(), strokePaint);
        }
    }
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */) const
{
    TextRunWalker walker(run, 0, this);
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
        for (glyphIndex = 0; glyphIndex < walker.length(); ++glyphIndex) {
            if (x < truncateFixedPointToInteger(advances[glyphIndex]))
                break;
            x -= truncateFixedPointToInteger(advances[glyphIndex]);
        }
    }

    return glyphIndex;
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, int x,
                                          bool includePartialGlyphs) const
{
    // (Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.)
    TextRunWalker walker(run, 0, this);

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

        if (x < walker.width()) {
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
                                            const IntPoint& point, int height,
                                            int from, int to) const
{
    int fromX = -1, toX = -1, fromAdvance = -1, toAdvance = -1;
    TextRunWalker walker(run, 0, this);

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

        if (fromX == -1 && from < walker.numCodePoints()) {
            // |from| is within this script run. So we index the clusters log to
            // find which glyph this code-point contributed to and find its x
            // position.
            int glyph = walker.logClusters()[from];
            fromX = base + walker.xPositions()[glyph];
            fromAdvance = walker.advances()[glyph];
        } else
            from -= walker.numCodePoints();

        if (toX == -1 && to < walker.numCodePoints()) {
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
