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

#include "FontData.h"
#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "TextStyle.h"
#include "UniscribeController.h"
#include <ApplicationServices/ApplicationServices.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <wtf/MathExtras.h>

namespace WebCore {

const int syntheticObliqueAngle = 14;

void Font::drawGlyphs(GraphicsContext* graphicsContext, const FontData* font, const GlyphBuffer& glyphBuffer, 
                      int from, int numGlyphs, const FloatPoint& point) const
{
    CGContextRef cgContext = graphicsContext->platformContext();

    uint32_t oldFontSmoothingStyle = wkSetFontSmoothingStyle(cgContext);

    const FontPlatformData& platformData = font->platformData();
    //NSFont* drawFont;
    //if ([gContext isDrawingToScreen]) {
    //    drawFont = [platformData.font screenFont];
    //    if (drawFont != platformData.font)
    //        // We are getting this in too many places (3406411); use ERROR so it only prints on debug versions for now. (We should debug this also, eventually).
    //        LOG_ERROR("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.",
    //            [[[platformData.font fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    //} else {
    //    drawFont = [platformData.font printerFont];
    //    if (drawFont != platformData.font)
    //        NSLog(@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.",
    //            [[[platformData.font fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    //}

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

    //wkSetCGFontRenderingMode(cgContext, drawFont);
    CGContextSetFontSize(cgContext, platformData.size());
    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->m_syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, point.x() + font->m_syntheticBoldOffset, point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    wkRestoreFontSmoothingStyle(cgContext, oldFontSmoothingStyle);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h,
                                            int from, int to) const
{
    UniscribeController it(this, run, style);
    it.advance(from);
    float beforeWidth = it.runWidthSoFar();
    it.advance(to);
    float afterWidth = it.runWidthSoFar();

    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (style.rtl()) {
        it.advance(run.length());
        float totalWidth = it.runWidthSoFar();
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
    } 
    
    return FloatRect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);
}

void Font::drawComplexText(GraphicsContext* context, const TextRun& run, const TextStyle& style, const FloatPoint& point,
                           int from, int to) const
{
    // This glyph buffer holds our glyphs + advances + font data for each glyph.
    GlyphBuffer glyphBuffer;

    float startX = point.x();
    UniscribeController controller(this, run, style);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    float afterWidth = controller.runWidthSoFar();

    if (style.rtl()) {
        controller.advance(run.length());
        startX += controller.runWidthSoFar() - afterWidth;
    } else
        startX += beforeWidth;

    // Draw the glyph buffer now at the starting point returned in startX.
    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, glyphBuffer, run, style, startPoint);
}

float Font::floatWidthForComplexText(const TextRun& run, const TextStyle& style) const
{
    UniscribeController controller(this, run, style);
    controller.advance(run.length());
    return controller.runWidthSoFar();
}

int Font::offsetForPositionForComplexText(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    UniscribeController controller(this, run, style);
    return controller.offsetForPosition(x, includePartialGlyphs);
}

}
