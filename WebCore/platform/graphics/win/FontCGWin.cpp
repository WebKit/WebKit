/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
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

    if (platformData.syntheticOblique()) {
        static float skew = -tanf(syntheticObliqueAngle * acosf(0) / 90.0f);
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

}
