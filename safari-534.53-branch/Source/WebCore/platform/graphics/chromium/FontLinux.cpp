/*
 * Copyright (c) 2007, 2008, 2010 Google Inc. All rights reserved.
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

#include "ComplexTextControllerLinux.h"
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

#include <wtf/unicode/Unicode.h>

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool Font::canExpandAroundIdeographsInComplexText()
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
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs), storage2(numGlyphs), storage3(numGlyphs);
    SkPoint* pos = storage.get();
    SkPoint* vPosBegin = storage2.get();
    SkPoint* vPosEnd = storage3.get();

    bool isVertical = font->platformData().orientation() == Vertical;
    for (int i = 0; i < numGlyphs; i++) {
        SkScalar myWidth = SkFloatToScalar(adv[i].width());
        pos[i].set(x, y);
        if (isVertical) {
            vPosBegin[i].set(x + myWidth, y);
            vPosEnd[i].set(x + myWidth, y - myWidth);
        }
        x += myWidth;
        y += SkFloatToScalar(adv[i].height());
    }

    gc->platformContext()->prepareForSoftwareDraw();

    SkCanvas* canvas = gc->platformContext()->canvas();
    TextDrawingModeFlags textMode = gc->platformContext()->getTextDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->fillColor().rgb());

        if (isVertical) {
            SkPath path;
            for (int i = 0; i < numGlyphs; ++i) {
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                canvas->drawTextOnPath(glyphs + i, 2, path, 0, paint);
            }
        } else
            canvas->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }

    if ((textMode & TextModeStroke)
        && gc->platformContext()->getStrokeStyle() != NoStroke
        && gc->platformContext()->getStrokeThickness() > 0) {

        SkPaint paint;
        gc->platformContext()->setupPaintForStroking(&paint, 0, 0);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->strokeColor().rgb());

        if (textMode & TextModeFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            SkSafeUnref(paint.setLooper(0));
        }

        if (isVertical) {
            SkPath path;
            for (int i = 0; i < numGlyphs; ++i) {
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                canvas->drawTextOnPath(glyphs + i, 2, path, 0, paint);
            }
        } else
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
    TextDrawingModeFlags textMode = gc->platformContext()->getTextDrawingMode();
    bool fill = textMode & TextModeFill;
    bool stroke = (textMode & TextModeStroke)
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

    ComplexTextController controller(run, point.x(), point.y(), this);
    controller.setWordSpacingAdjustment(wordSpacing());
    controller.setLetterSpacingAdjustment(letterSpacing());
    controller.setPadding(run.expansion());

    if (run.rtl()) {
        // FIXME: this causes us to shape the text twice -- once to compute the width and then again
        // below when actually rendering.  Change ComplexTextController to match platform/mac and
        // platform/chromium/win by having it store the shaped runs, so we can reuse the results.
        controller.reset(point.x() + controller.widthOfFullRun());
        // We need to set the padding again because ComplexTextController layout consumed the value.
        // Fixing the above problem would help here too.
        controller.setPadding(run.expansion());
    }

    while (controller.nextScriptRun()) {
        if (fill) {
            controller.fontPlatformDataForScriptRun()->setupPaint(&fillPaint);
            adjustTextRenderMode(&fillPaint, gc->platformContext());
            canvas->drawPosText(controller.glyphs(), controller.length() << 1, controller.positions(), fillPaint);
        }

        if (stroke) {
            controller.fontPlatformDataForScriptRun()->setupPaint(&strokePaint);
            adjustTextRenderMode(&strokePaint, gc->platformContext());
            canvas->drawPosText(controller.glyphs(), controller.length() << 1, controller.positions(), strokePaint);
        }
    }
}

void Font::drawEmphasisMarksForComplexText(GraphicsContext* /* context */, const TextRun& /* run */, const AtomicString& /* mark */, const FloatPoint& /* point */, int /* from */, int /* to */) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */, GlyphOverflow* /* glyphOverflow */) const
{
    ComplexTextController controller(run, 0, 0, this);
    controller.setWordSpacingAdjustment(wordSpacing());
    controller.setLetterSpacingAdjustment(letterSpacing());
    controller.setPadding(run.expansion());
    return controller.widthOfFullRun();
}

static int glyphIndexForXPositionInScriptRun(const ComplexTextController& controller, int targetX)
{
    // Iterate through the glyphs in logical order, seeing whether targetX falls between the previous
    // position and halfway through the current glyph.
    // FIXME: this code probably belongs in ComplexTextController.
    int lastX = controller.offsetX() - (controller.rtl() ? -controller.width() : controller.width());
    for (int glyphIndex = 0; static_cast<unsigned>(glyphIndex) < controller.length(); ++glyphIndex) {
        int advance = truncateFixedPointToInteger(controller.advances()[glyphIndex]);
        int nextX = static_cast<int>(controller.positions()[glyphIndex].x()) + advance / 2;
        if (std::min(nextX, lastX) <= targetX && targetX <= std::max(nextX, lastX))
            return glyphIndex;
        lastX = nextX;
    }

    return controller.length() - 1;
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
                                          bool includePartialGlyphs) const
{
    // FIXME: This truncation is not a problem for HTML, but only affects SVG, which passes floating-point numbers
    // to Font::offsetForPosition(). Bug http://webkit.org/b/40673 tracks fixing this problem.
    int targetX = static_cast<int>(xFloat);

    // (Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.)
    ComplexTextController controller(run, 0, 0, this);
    controller.setWordSpacingAdjustment(wordSpacing());
    controller.setLetterSpacingAdjustment(letterSpacing());
    controller.setPadding(run.expansion());
    if (run.rtl()) {
        // See FIXME in drawComplexText.
        controller.reset(controller.widthOfFullRun());
        controller.setPadding(run.expansion());
    }

    unsigned basePosition = 0;

    int x = controller.offsetX();
    while (controller.nextScriptRun()) {
        int nextX = controller.offsetX();

        if (std::min(x, nextX) <= targetX && targetX <= std::max(x, nextX)) {
            // The x value in question is within this script run.
            const int glyphIndex = glyphIndexForXPositionInScriptRun(controller, targetX);

            // Now that we have a glyph index, we have to turn that into a
            // code-point index. Because of ligatures, several code-points may
            // have gone into a single glyph. We iterate over the clusters log
            // and find the first code-point which contributed to the glyph.

            // Some shapers (i.e. Khmer) will produce cluster logs which report
            // that /no/ code points contributed to certain glyphs. Because of
            // this, we take any code point which contributed to the glyph in
            // question, or any subsequent glyph. If we run off the end, then
            // we take the last code point.
            const unsigned short* log = controller.logClusters();
            for (unsigned j = 0; j < controller.numCodePoints(); ++j) {
                if (log[j] >= glyphIndex)
                    return basePosition + j;
            }

            return basePosition + controller.numCodePoints() - 1;
        }

        basePosition += controller.numCodePoints();
    }

    return basePosition;
}

// Return the rectangle for selecting the given range of code-points in the TextRun.
FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const FloatPoint& point, int height,
                                            int from, int to) const
{
    int fromX = -1, toX = -1;
    ComplexTextController controller(run, 0, 0, this);
    controller.setWordSpacingAdjustment(wordSpacing());
    controller.setLetterSpacingAdjustment(letterSpacing());
    controller.setPadding(run.expansion());
    if (run.rtl()) {
        // See FIXME in drawComplexText.
        controller.reset(controller.widthOfFullRun());
        controller.setPadding(run.expansion());
    }

    // Iterate through the script runs in logical order, searching for the run covering the positions of interest.
    while (controller.nextScriptRun() && (fromX == -1 || toX == -1)) {
        if (fromX == -1 && from >= 0 && static_cast<unsigned>(from) < controller.numCodePoints()) {
            // |from| is within this script run. So we index the clusters log to
            // find which glyph this code-point contributed to and find its x
            // position.
            int glyph = controller.logClusters()[from];
            fromX = controller.positions()[glyph].x();
            if (controller.rtl())
                fromX += truncateFixedPointToInteger(controller.advances()[glyph]);
        } else
            from -= controller.numCodePoints();

        if (toX == -1 && to >= 0 && static_cast<unsigned>(to) < controller.numCodePoints()) {
            int glyph = controller.logClusters()[to];
            toX = controller.positions()[glyph].x();
            if (controller.rtl())
                toX += truncateFixedPointToInteger(controller.advances()[glyph]);
        } else
            to -= controller.numCodePoints();
    }

    // The position in question might be just after the text.
    if (fromX == -1)
        fromX = controller.offsetX();
    if (toX == -1)
        toX = controller.offsetX();

    ASSERT(fromX != -1 && toX != -1);

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);

    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

} // namespace WebCore
