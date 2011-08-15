/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkTypeface_mac.h"

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return true;
}

// FIXME: Determine if the Mac port of Chromium using Skia can expand around
// ideographs in complex text. (The Windows and Linux ports for Chromium can't.)
// This issue is tracked in https://bugs.webkit.org/show_bug.cgi?id=62987
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

static void setupPaint(SkPaint* paint, const SimpleFontData* fontData, const Font* font)
{
    const FontPlatformData& platformData = fontData->platformData();
    const float textSize = platformData.m_size >= 0 ? platformData.m_size : 12;

    paint->setAntiAlias(true);
    paint->setEmbeddedBitmapText(false);
    paint->setTextSize(SkFloatToScalar(textSize));
    SkTypeface* typeface = SkCreateTypefaceFromCTFont(platformData.ctFont());
    SkAutoUnref autoUnref(typeface);
    paint->setTypeface(typeface);
    paint->setFakeBoldText(platformData.m_syntheticBold);
    paint->setTextSkewX(platformData.m_syntheticOblique ? -SK_Scalar1 / 4 : 0);
    paint->setAutohinted(false); // freetype specific
    paint->setLCDRenderText(true); // font->fontDescription().fontSmoothing() == SubpixelAntialiased);
}

// TODO: This needs to be split into helper functions to better scope the
// inputs/outputs, and reduce duplicate code.
// This issue is tracked in https://bugs.webkit.org/show_bug.cgi?id=62989
void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point) const {
    COMPILE_ASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t), GlyphBufferGlyphSize_equals_uint16_t);

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
        SkScalar myWidth = SkFloatToScalar(adv[i].width);
        pos[i].set(x, y);
        if (isVertical) {
            vPosBegin[i].set(x + myWidth, y);
            vPosEnd[i].set(x + myWidth, y - myWidth);
        }
        x += myWidth;
        y += SkFloatToScalar(adv[i].height);
    }

    gc->platformContext()->makeGrContextCurrent();

    SkCanvas* canvas = gc->platformContext()->canvas();
    TextDrawingModeFlags textMode = gc->platformContext()->getTextDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        setupPaint(&paint, font, this);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->fillColor().rgb());

        if (isVertical) {
            SkPath path;
            for (int i = 0; i < numGlyphs; ++i) {
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                canvas->drawTextOnPath(glyphs + i, sizeof(uint16_t), path, 0, paint);
            }
        } else
            canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, paint);
    }

    if ((textMode & TextModeStroke)
        && gc->platformContext()->getStrokeStyle() != NoStroke
        && gc->platformContext()->getStrokeThickness() > 0) {

        SkPaint paint;
        gc->platformContext()->setupPaintForStroking(&paint, 0, 0);
        setupPaint(&paint, font, this);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->strokeColor().rgb());

        if (textMode & TextModeFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            paint.setLooper(0);
        }

        if (isVertical) {
            SkPath path;
            for (int i = 0; i < numGlyphs; ++i) {
                path.reset();
                path.moveTo(vPosBegin[i]);
                path.lineTo(vPosEnd[i]);
                canvas->drawTextOnPath(glyphs + i, sizeof(uint16_t), path, 0, paint);
            }
        } else
            canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, paint);
    }
}

} // namespace WebCore
