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

#include "FloatRect.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "HarfBuzzShaper.h"
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

    PlatformContextSkia* platformContext = gc->platformContext();
    TextDrawingModeFlags textMode = gc->platformContext()->getTextDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        font->platformData().setupPaint(&paint);
        gc->platformContext()->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        if (isVertical) {
            SkPath path;
            for (int i = 0; i < numGlyphs; ++i) {
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                platformContext->drawTextOnPath(glyphs + i, 2, path, 0, paint);
            }
        } else
            platformContext->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }

    if ((textMode & TextModeStroke)
        && gc->platformContext()->getStrokeStyle() != NoStroke
        && gc->platformContext()->getStrokeThickness() > 0) {

        SkPaint paint;
        gc->platformContext()->setupPaintForStroking(&paint, 0, 0);
        font->platformData().setupPaint(&paint);
        gc->platformContext()->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        if (textMode & TextModeFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            // Since we use the looper for shadows, we remove it (if any) now.
            paint.setLooper(0);
        }

        if (isVertical) {
            SkPath path;
            for (int i = 0; i < numGlyphs; ++i) {
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                platformContext->drawTextOnPath(glyphs + i, 2, path, 0, paint);
            }
        } else
            platformContext->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }
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

    GlyphBuffer glyphBuffer;
    HarfBuzzShaper shaper(this, run);
    shaper.setDrawRange(from, to);
    if (!shaper.shape(&glyphBuffer))
        return;
    FloatPoint adjustedPoint = shaper.adjustStartPoint(point);
    drawGlyphBuffer(gc, run, glyphBuffer, adjustedPoint);
}

void Font::drawEmphasisMarksForComplexText(GraphicsContext* /* context */, const TextRun& /* run */, const AtomicString& /* mark */, const FloatPoint& /* point */, int /* from */, int /* to */) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */, GlyphOverflow* /* glyphOverflow */) const
{
    HarfBuzzShaper shaper(this, run);
    if (!shaper.shape())
        return 0;
    return shaper.totalWidth();
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
                                          bool includePartialGlyphs) const
{
    // FIXME: This truncation is not a problem for HTML, but only affects SVG, which passes floating-point numbers
    // to Font::offsetForPosition(). Bug http://webkit.org/b/40673 tracks fixing this problem.
    int targetX = static_cast<int>(xFloat);

    HarfBuzzShaper shaper(this, run);
    if (!shaper.shape())
        return 0;
    return shaper.offsetForPosition(targetX);
}

// Return the rectangle for selecting the given range of code-points in the TextRun.
FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const FloatPoint& point, int height,
                                            int from, int to) const
{
    HarfBuzzShaper shaper(this, run);
    if (!shaper.shape())
        return FloatRect();
    return shaper.selectionRect(point, height, from, to);
}

} // namespace WebCore
