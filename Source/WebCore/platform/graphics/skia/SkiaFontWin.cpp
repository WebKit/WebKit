/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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
#include "SkiaFontWin.h"

#include "AffineTransform.h"
#include "Gradient.h"
#include "Pattern.h"
#include "PlatformContextSkia.h"
#include "PlatformSupport.h"
#include "SimpleFontData.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include "SkShader.h"
#include "SkTemplates.h"

namespace WebCore {

#if !USE(SKIA_TEXT)
bool windowsCanHandleDrawTextShadow(GraphicsContext* context)
{
    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    ColorSpace shadowColorSpace;

    bool hasShadow = context->getShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);
    return !hasShadow || (!shadowBlur && (shadowColor.alpha() == 255) && (context->fillColor().alpha() == 255));
}

bool windowsCanHandleTextDrawing(GraphicsContext* context)
{
    if (!windowsCanHandleTextDrawingWithoutShadow(context))
        return false;

    // Check for shadow effects.
    if (!windowsCanHandleDrawTextShadow(context))
        return false;

    return true;
}

bool windowsCanHandleTextDrawingWithoutShadow(GraphicsContext* context)
{
    // Check for non-translation transforms. Sometimes zooms will look better in
    // Skia, and sometimes better in Windows. The main problem is that zooming
    // in using Skia will show you the hinted outlines for the smaller size,
    // which look weird. All else being equal, it's better to use Windows' text
    // drawing, so we don't check for zooms.
    const AffineTransform& matrix = context->getCTM();
    if (matrix.b() || matrix.c()) // Check for skew.
        return false;

    // Check for stroke effects.
    if (context->platformContext()->getTextDrawingMode() != TextModeFill)
        return false;

    // Check for gradients.
    if (context->fillGradient() || context->strokeGradient())
        return false;

    // Check for patterns.
    if (context->fillPattern() || context->strokePattern())
        return false;

    if (!context->platformContext()->isNativeFontRenderingAllowed())
        return false;

    return true;
}
#endif

static void skiaDrawText(SkCanvas* canvas,
                         const SkPoint& point,
                         SkPaint* paint,
                         const WORD* glyphs,
                         const int* advances,
                         const GOFFSET* offsets,
                         int numGlyphs)
{
    // Reserve space for 64 SkPoints on the stack. If numGlyphs is larger, the array
    // will dynamically allocate it space for numGlyph glyphs. This is used to store
    // the computed x,y locations. In the case where offsets==null, then we use it
    // to store (twice as many) SkScalars for x[]
    static const size_t kLocalGlyphMax = 64;

    SkScalar x = point.fX;
    SkScalar y = point.fY;
    if (offsets) {
        SkAutoSTArray<kLocalGlyphMax, SkPoint> storage(numGlyphs);
        SkPoint* pos = storage.get();
        for (int i = 0; i < numGlyphs; i++) {
            // GDI has dv go up, so we negate it
            pos[i].set(x + SkIntToScalar(offsets[i].du),
                       y + -SkIntToScalar(offsets[i].dv));
            x += SkIntToScalar(advances[i]);
        }
        canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, *paint);
    } else {
        SkAutoSTArray<kLocalGlyphMax * 2, SkScalar> storage(numGlyphs);
        SkScalar* xpos = storage.get();
        for (int i = 0; i < numGlyphs; i++) {
            xpos[i] = x;
            x += SkIntToScalar(advances[i]);
        }
        canvas->drawPosTextH(glyphs, numGlyphs * sizeof(uint16_t),
                             xpos, y, *paint);
    }
}

// Lookup the current system settings for font smoothing.
// We cache these values for performance, but if the browser has away to be
// notified when these change, we could re-query them at that time.
static uint32_t getDefaultGDITextFlags()
{
    static bool gInited;
    static uint32_t gFlags;
    if (!gInited) {
        BOOL enabled;
        gFlags = 0;
        if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) && enabled) {
            gFlags |= SkPaint::kAntiAlias_Flag;

            UINT smoothType;
            if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothType, 0)) {
                if (FE_FONTSMOOTHINGCLEARTYPE == smoothType)
                    gFlags |= SkPaint::kLCDRenderText_Flag;
            }
        }
        gInited = true;
    }
    return gFlags;
}

static void setupPaintForFont(SkPaint* paint, PlatformContextSkia* pcs,
                              SkTypeface* face, float size, int quality)
{
    paint->setTextSize(SkFloatToScalar(size));
    paint->setTypeface(face);

    // turn quality into text flags
    uint32_t textFlags;
    switch (quality) {
    case NONANTIALIASED_QUALITY:
        textFlags = 0;
        break;
    case ANTIALIASED_QUALITY:
        textFlags = SkPaint::kAntiAlias_Flag;
        break;
    case CLEARTYPE_QUALITY:
        textFlags = (SkPaint::kAntiAlias_Flag | SkPaint::kLCDRenderText_Flag);
        break;
    default:
        textFlags = getDefaultGDITextFlags();
        break;
    }
    // only allow features that SystemParametersInfo allows
    textFlags &= getDefaultGDITextFlags();

    // do this check after our switch on lfQuality
    if (!pcs->couldUseLCDRenderedText()) {
        textFlags &= ~SkPaint::kLCDRenderText_Flag;
        // If we *just* clear our request for LCD, then GDI seems to
        // sometimes give us AA text, and sometimes give us BW text. Since the
        // original intent was LCD, we want to force AA (rather than BW), so we
        // add a special bit to tell Skia to do its best to avoid the BW: by
        // drawing LCD offscreen and downsampling that to AA.
        textFlags |= SkPaint::kGenA8FromLCD_Flag;
    }

    static const uint32_t textFlagsMask = SkPaint::kAntiAlias_Flag |
                                          SkPaint::kLCDRenderText_Flag |
                                          SkPaint::kGenA8FromLCD_Flag;

    // now copy in just the text flags
    SkASSERT(!(textFlags & ~textFlagsMask));
    uint32_t flags = paint->getFlags();
    flags &= ~textFlagsMask;
    flags |= textFlags;
    paint->setFlags(flags);
}

static void paintSkiaText(GraphicsContext* context, HFONT hfont,
                          SkTypeface* face, float size, int quality,
                          int numGlyphs,
                          const WORD* glyphs,
                          const int* advances,
                          const GOFFSET* offsets,
                          const SkPoint* origin)
{
    PlatformContextSkia* platformContext = context->platformContext();
    SkCanvas* canvas = platformContext->canvas();
    TextDrawingModeFlags textMode = platformContext->getTextDrawingMode();
    // Ensure font load for printing, because PDF device needs it.
    if (canvas->getTopDevice()->getDeviceCapabilities() & SkDevice::kVector_Capability)
        PlatformSupport::ensureFontLoaded(hfont);

    // Filling (if necessary). This is the common case.
    SkPaint paint;
    platformContext->setupPaintForFilling(&paint);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    setupPaintForFont(&paint, platformContext, face, size, quality);

    bool didFill = false;

    if ((textMode & TextModeFill) && (SkColorGetA(paint.getColor()) || paint.getLooper())) {
        skiaDrawText(canvas, *origin, &paint, &glyphs[0], &advances[0], &offsets[0], numGlyphs);
        didFill = true;
    }

    // Stroking on top (if necessary).
    if ((textMode & TextModeStroke)
        && platformContext->getStrokeStyle() != NoStroke
        && platformContext->getStrokeThickness() > 0) {

        paint.reset();
        platformContext->setupPaintForStroking(&paint, 0, 0);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        setupPaintForFont(&paint, platformContext, face, size, quality);

        if (didFill) {
            // If there is a shadow and we filled above, there will already be
            // a shadow. We don't want to draw it again or it will be too dark
            // and it will go on top of the fill.
            //
            // Note that this isn't strictly correct, since the stroke could be
            // very thick and the shadow wouldn't account for this. The "right"
            // thing would be to draw to a new layer and then draw that layer
            // with a shadow. But this is a lot of extra work for something
            // that isn't normally an issue.
            paint.setLooper(0);
        }

        skiaDrawText(canvas, *origin, &paint, &glyphs[0], &advances[0], &offsets[0], numGlyphs);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void paintSkiaText(GraphicsContext* context,
                   const FontPlatformData& data,
                   int numGlyphs,
                   const WORD* glyphs,
                   const int* advances,
                   const GOFFSET* offsets,
                   const SkPoint* origin)
{
    paintSkiaText(context, data.hfont(), data.typeface(), data.size(), data.lfQuality(),
                  numGlyphs, glyphs, advances, offsets, origin);
}

void paintSkiaText(GraphicsContext* context,
                   HFONT hfont,
                   int numGlyphs,
                   const WORD* glyphs,
                   const int* advances,
                   const GOFFSET* offsets,
                   const SkPoint* origin)
{
    int size;
    int quality;
    SkTypeface* face = CreateTypefaceFromHFont(hfont, &size, &quality);
    SkAutoUnref aur(face);

    paintSkiaText(context, hfont, face, size, quality, numGlyphs, glyphs, advances, offsets, origin);
}

}  // namespace WebCore
