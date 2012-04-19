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
#include "PlatformSupport.h"
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

bool Font::canExpandAroundIdeographsInComplexText()
{
    return true;
}

static void setupPaint(SkPaint* paint, const SimpleFontData* fontData, const Font* font, bool shouldAntialias, bool shouldSmoothFonts)
{
    const FontPlatformData& platformData = fontData->platformData();
    const float textSize = platformData.m_size >= 0 ? platformData.m_size : 12;

    paint->setAntiAlias(shouldAntialias);
    paint->setEmbeddedBitmapText(false);
    paint->setTextSize(SkFloatToScalar(textSize));
    paint->setVerticalText(platformData.orientation() == Vertical);
    SkTypeface* typeface = SkCreateTypefaceFromCTFont(platformData.ctFont());
    SkAutoUnref autoUnref(typeface);
    paint->setTypeface(typeface);
    paint->setFakeBoldText(platformData.m_syntheticBold);
    paint->setTextSkewX(platformData.m_syntheticOblique ? -SK_Scalar1 / 4 : 0);
    paint->setAutohinted(false); // freetype specific
    paint->setLCDRenderText(shouldSmoothFonts);
    paint->setSubpixelText(true);
}

// TODO: This needs to be split into helper functions to better scope the
// inputs/outputs, and reduce duplicate code.
// This issue is tracked in https://bugs.webkit.org/show_bug.cgi?id=62989
void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point) const {
    COMPILE_ASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t), GlyphBufferGlyphSize_equals_uint16_t);

    bool shouldSmoothFonts = true;
    bool shouldAntialias = true;
    
    switch (fontDescription().fontSmoothing()) {
    case Antialiased:
        shouldSmoothFonts = false;
        break;
    case SubpixelAntialiased:
        break;
    case NoSmoothing:
        shouldAntialias = false;
        shouldSmoothFonts = false;
        break;
    case AutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break; 
    }
    
    if (!shouldUseSmoothing() || PlatformSupport::layoutTestMode())
        shouldSmoothFonts = false;

    const GlyphBufferGlyph* glyphs = glyphBuffer.glyphs(from);
    SkScalar x = SkFloatToScalar(point.x());
    SkScalar y = SkFloatToScalar(point.y());

    if (font->platformData().orientation() == Vertical)
        y += SkFloatToScalar(font->fontMetrics().floatAscent(IdeographicBaseline) - font->fontMetrics().floatAscent());
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
        x += SkFloatToScalar(adv[i].width);
        y += SkFloatToScalar(adv[i].height);
    }

    SkCanvas* canvas = gc->platformContext()->canvas();
    if (font->platformData().orientation() == Vertical) {
        canvas->save();
        canvas->rotate(-90);
        SkMatrix rotator;
        rotator.reset();
        rotator.setRotate(90);
        rotator.mapPoints(pos, numGlyphs);
    }
    TextDrawingModeFlags textMode = gc->platformContext()->getTextDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        setupPaint(&paint, font, this, shouldAntialias, shouldSmoothFonts);
        gc->platformContext()->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, paint);
    }

    if ((textMode & TextModeStroke)
        && gc->platformContext()->getStrokeStyle() != NoStroke
        && gc->platformContext()->getStrokeThickness() > 0) {

        SkPaint paint;
        gc->platformContext()->setupPaintForStroking(&paint, 0, 0);
        setupPaint(&paint, font, this, shouldAntialias, shouldSmoothFonts);
        gc->platformContext()->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        if (textMode & TextModeFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            paint.setLooper(0);
        }

        canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, paint);
    }
    if (font->platformData().orientation() == Vertical)
        canvas->restore();
}

} // namespace WebCore
