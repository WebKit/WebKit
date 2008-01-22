/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Font.h"

#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"
#include "UniscribeController.h"
#include <ApplicationServices/ApplicationServices.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <wtf/MathExtras.h>

namespace WebCore {

const int syntheticObliqueAngle = 14;

void Font::drawGlyphs(GraphicsContext* graphicsContext, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, 
                      int from, int numGlyphs, const FloatPoint& point) const
{
    if (font->m_font.useGDI()) {
        // FIXME: Support alpha blending.
        // FIXME: Support text stroke/fill.
        // FIXME: Support text shadow.
        Color fillColor = graphicsContext->fillColor();
        if (fillColor.alpha() == 0)
            return;

        // We have to convert CG's two-dimensional floating point advances to just horizontal integer advances.
        Vector<int, 2048> gdiAdvances;
        int totalWidth = 0;
        for (int i = 0; i < numGlyphs; i++) {
            gdiAdvances.append(lroundf(glyphBuffer.advanceAt(from + i)));
            totalWidth += gdiAdvances[i];
        }

        // We put slop into this rect, since glyphs can overflow the ascent/descent bounds and the left/right edges.
        int lineGap = font->lineGap();
        IntRect textRect(point.x() - lineGap, point.y() - font->ascent() - lineGap, totalWidth + 2 * lineGap, font->lineSpacing());
        HDC hdc = graphicsContext->getWindowsContext(textRect);
        SelectObject(hdc, font->m_font.hfont());

        // Set the correct color.
        HDC textDrawingDC = hdc;
        /*if (fillColor.hasAlpha() || graphicsContext->inTransparencyLayer()) {
            // GDI can't handle drawing transparent text.  We have to draw into a mask.  We draw black text on a white-filled background.
            // We also do this when inside transparency layers, since GDI also can't draw onto a surface with alpha.
            graphicsContext->save();
            graphicsContext->setFillColor(Color::white);
            textDrawingDC = graphicsContext->getWindowsBitmapContext(textRect);
            SetTextColor(hdc, RGB(0, 0, 0));
        } else*/
            SetTextColor(hdc, RGB(fillColor.red(), fillColor.green(), fillColor.blue()));

        SetBkMode(hdc, TRANSPARENT);
        SetTextAlign(hdc, TA_LEFT | TA_BASELINE);

        // Uniscribe gives us offsets to help refine the positioning of combining glyphs.
        FloatSize translation = glyphBuffer.offsetAt(from);
        if (translation.width() || translation.height()) {
            XFORM xform;
            xform.eM11 = 1.0;
            xform.eM12 = 0;
            xform.eM21 = 0;
            xform.eM22 = 1.0;
            xform.eDx = translation.width();
            xform.eDy = translation.height();
            ModifyWorldTransform(hdc, &xform, MWT_LEFTMULTIPLY);
        }
        ExtTextOut(hdc, point.x(), point.y(), ETO_GLYPH_INDEX, 0, (WCHAR*)glyphBuffer.glyphs(from), numGlyphs, gdiAdvances.data());

        /*if (fillColor.hasAlpha() || graphicsContext->inTransparencyLayer()) {
            // TODOD: We have to walk the bits of the bitmap and invert them.  We also copy over the green component value into the alpha value
            // to keep ClearType looking reasonable.

            // Now that we have drawn the text into a bitmap and inverted it, obtain a CGImageRef mask.
            CGImageRef mask = graphicsContext->releaseWindowsBitmapContextIntoMask(textDrawingDC, textRect);
            
            // Apply the mask to the fill color.
            CGContextRef bitmapContext = graphicsContext->getWindowsCompatibleCGBitmapContext(textRect.size());
            CGFloat red, green, blue, alpha;
            color.getRGBA(red, green, blue, alpha);
            CGContextSetRGBFillColor(context, red, green, blue, alpha);
            CGContextFillRect(bitmapContext, IntRect(0, 0, textRect.width(), textRect.height()));
            CGImageRef fillColorImage = CGBitmapContextCreateImage(bitmapContext);
        
            // Apply the mask.
            CGImageRef finalImage = CGImageCreateWithMask(fillColorImage, mask);

            // The bitmap image needs to be drawn into the HDC.
            graphicsContext->drawImageIntoWindowsContext(hdc, finalImage);

            // Release our images and contexts.
            CGImageRelease(mask);
            CGImageRelease(fillColorImage);
            CGImageRelease(finalImage);
            CGContextRelease(bitmapContext);
        }*/

        graphicsContext->releaseWindowsContext(hdc, textRect);
        return;
    }

    CGContextRef cgContext = graphicsContext->platformContext();

    uint32_t oldFontSmoothingStyle = wkSetFontSmoothingStyle(cgContext);

    const FontPlatformData& platformData = font->platformData();

    CGContextSetFont(cgContext, platformData.cgFont());

    CGAffineTransform matrix = CGAffineTransformIdentity;
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;

    if (platformData.syntheticOblique())
    {
        static float skew = -tanf(syntheticObliqueAngle * acosf(0) / 90);
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, skew, 1, 0, 0));
    }

    // Uniscribe gives us offsets to help refine the positioning of combining glyphs.
    FloatSize translation = glyphBuffer.offsetAt(from);
    if (translation.width() || translation.height())
        CGAffineTransformTranslate(matrix, translation.width(), translation.height());
    
    CGContextSetTextMatrix(cgContext, matrix);

    CGContextSetFontSize(cgContext, platformData.size());
    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->m_syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, point.x() + font->m_syntheticBoldOffset, point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    wkRestoreFontSmoothingStyle(cgContext, oldFontSmoothingStyle);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& point, int h,
                                            int from, int to) const
{
    UniscribeController it(this, run);
    it.advance(from);
    float beforeWidth = it.runWidthSoFar();
    it.advance(to);
    float afterWidth = it.runWidthSoFar();

    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (run.rtl()) {
        it.advance(run.length());
        float totalWidth = it.runWidthSoFar();
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
    } 
    
    return FloatRect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);
}

void Font::drawComplexText(GraphicsContext* context, const TextRun& run, const FloatPoint& point,
                           int from, int to) const
{
    // This glyph buffer holds our glyphs + advances + font data for each glyph.
    GlyphBuffer glyphBuffer;

    float startX = point.x();
    UniscribeController controller(this, run);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    float afterWidth = controller.runWidthSoFar();

    if (run.rtl()) {
        controller.advance(run.length());
        startX += controller.runWidthSoFar() - afterWidth;
    } else
        startX += beforeWidth;

    // Draw the glyph buffer now at the starting point returned in startX.
    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, glyphBuffer, run, startPoint);
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    UniscribeController controller(this, run);
    controller.advance(run.length());
    return controller.runWidthSoFar();
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x, bool includePartialGlyphs) const
{
    UniscribeController controller(this, run);
    return controller.offsetForPosition(x, includePartialGlyphs);
}

}
