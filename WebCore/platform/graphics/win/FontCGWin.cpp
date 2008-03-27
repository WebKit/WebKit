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

#include "AffineTransform.h"
#include "FloatConversion.h"
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

static inline CGFloat toCGFloat(FIXED f)
{
    return f.value + f.fract / CGFloat(65536.0);
}

static CGPathRef createPathForGlyph(HDC hdc, Glyph glyph)
{
    CGMutablePathRef path = CGPathCreateMutable();

    static const MAT2 identity = { 0, 1,  0, 0,  0, 0,  0, 1 };
    GLYPHMETRICS glyphMetrics;
    // GGO_NATIVE matches the outline perfectly when Windows font smoothing is off.
    // GGO_NATIVE | GGO_UNHINTED does not match perfectly either when Windows font smoothing is on or off.
    DWORD outlineLength = GetGlyphOutline(hdc, glyph, GGO_GLYPH_INDEX | GGO_NATIVE, &glyphMetrics, 0, 0, &identity);
    ASSERT(outlineLength >= 0);
    if (outlineLength < 0)
        return path;

    Vector<UInt8> outline(outlineLength);
    GetGlyphOutline(hdc, glyph, GGO_GLYPH_INDEX | GGO_NATIVE, &glyphMetrics, outlineLength, outline.data(), &identity);

    unsigned offset = 0;
    while (offset < outlineLength) {
        LPTTPOLYGONHEADER subpath = reinterpret_cast<LPTTPOLYGONHEADER>(outline.data() + offset);
        ASSERT(subpath->dwType == TT_POLYGON_TYPE);
        if (subpath->dwType != TT_POLYGON_TYPE)
            return path;

        CGPathMoveToPoint(path, 0, toCGFloat(subpath->pfxStart.x), toCGFloat(subpath->pfxStart.y));

        unsigned subpathOffset = sizeof(*subpath);
        while (subpathOffset < subpath->cb) {
            LPTTPOLYCURVE segment = reinterpret_cast<LPTTPOLYCURVE>(reinterpret_cast<UInt8*>(subpath) + subpathOffset);
            switch (segment->wType) {
                case TT_PRIM_LINE:
                    for (unsigned i = 0; i < segment->cpfx; i++)
                        CGPathAddLineToPoint(path, 0, toCGFloat(segment->apfx[i].x), toCGFloat(segment->apfx[i].y));
                    break;

                case TT_PRIM_QSPLINE:
                    for (unsigned i = 0; i < segment->cpfx; i++) {
                        CGFloat x = toCGFloat(segment->apfx[i].x);
                        CGFloat y = toCGFloat(segment->apfx[i].y);
                        CGFloat cpx;
                        CGFloat cpy;

                        if (i == segment->cpfx - 2) {
                            cpx = toCGFloat(segment->apfx[i + 1].x);
                            cpy = toCGFloat(segment->apfx[i + 1].y);
                            i++;
                        } else {
                            cpx = (toCGFloat(segment->apfx[i].x) + toCGFloat(segment->apfx[i + 1].x)) / 2;
                            cpy = (toCGFloat(segment->apfx[i].y) + toCGFloat(segment->apfx[i + 1].y)) / 2;
                        }

                        CGPathAddQuadCurveToPoint(path, 0, x, y, cpx, cpy);
                    }
                    break;

                case TT_PRIM_CSPLINE:
                    for (unsigned i = 0; i < segment->cpfx; i += 3) {
                        CGFloat cp1x = toCGFloat(segment->apfx[i].x);
                        CGFloat cp1y = toCGFloat(segment->apfx[i].y);
                        CGFloat cp2x = toCGFloat(segment->apfx[i + 1].x);
                        CGFloat cp2y = toCGFloat(segment->apfx[i + 1].y);
                        CGFloat x = toCGFloat(segment->apfx[i + 2].x);
                        CGFloat y = toCGFloat(segment->apfx[i + 2].y);

                        CGPathAddCurveToPoint(path, 0, cp1x, cp1y, cp2x, cp2y, x, y);
                    }
                    break;

                default:
                    ASSERT_NOT_REACHED();
                    return path;
            }

            subpathOffset += sizeof(*segment) + (segment->cpfx - 1) * sizeof(segment->apfx[0]);
        }
        CGPathCloseSubpath(path);
        offset += subpath->cb;
    }
    return path;
}

void Font::drawGlyphs(GraphicsContext* graphicsContext, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, 
                      int from, int numGlyphs, const FloatPoint& point) const
{
    if (font->m_font.useGDI()) {
        Color fillColor = graphicsContext->fillColor();
        if (!fillColor.alpha())
            return;

        // We have to convert CG's two-dimensional floating point advances to just horizontal integer advances.
        Vector<int, 2048> gdiAdvances;
        int totalWidth = 0;
        for (int i = 0; i < numGlyphs; i++) {
            gdiAdvances.append(lroundf(glyphBuffer.advanceAt(from + i)));
            totalWidth += gdiAdvances[i];
        }

        bool drawIntoBitmap = false;
        int drawingMode = graphicsContext->textDrawingMode();
        if (drawingMode == cTextFill) {
            drawIntoBitmap = fillColor.alpha() != 255 || graphicsContext->inTransparencyLayer();
            if (!drawIntoBitmap) {
                IntSize size;
                int blur;
                Color color;
                graphicsContext->getShadow(size, blur, color);
                drawIntoBitmap = !size.isEmpty() || blur;
            }
        }

        HDC hdc = 0;
        OwnPtr<GraphicsContext::WindowsBitmap> bitmap;
        IntRect textRect;
        if (!drawIntoBitmap)
            hdc = graphicsContext->getWindowsContext(textRect, true, false);
        if (!hdc) {
            drawIntoBitmap = true;
            // We put slop into this rect, since glyphs can overflow the ascent/descent bounds and the left/right edges.
            // FIXME: Can get glyphs' optical bounds (even from CG) to get this right.
            // FIXME: Line gap does not make sense as a horizontal distance.
            int lineGap = font->lineGap();
            textRect = IntRect(point.x() - lineGap, point.y() - font->ascent() - lineGap, totalWidth + 2 * lineGap, font->lineSpacing());
            bitmap.set(graphicsContext->createWindowsBitmap(textRect.size()));
            memset(bitmap->buffer(), 255, bitmap->bufferLength());
            hdc = bitmap->hdc();

            XFORM xform;
            xform.eM11 = 1.0f;
            xform.eM12 = 0.0f;
            xform.eM21 = 0.0f;
            xform.eM22 = 1.0f;
            xform.eDx = -textRect.x();
            xform.eDy = -textRect.y();
            SetWorldTransform(hdc, &xform);
        }

        SelectObject(hdc, font->m_font.hfont());

        // Set the correct color.
        if (drawIntoBitmap)
            SetTextColor(hdc, RGB(0, 0, 0));
        else
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

        if (drawingMode == cTextFill)
            ExtTextOut(hdc, point.x(), point.y(), ETO_GLYPH_INDEX, 0, (WCHAR*)glyphBuffer.glyphs(from), numGlyphs, gdiAdvances.data());
        else {
            RetainPtr<CGMutablePathRef> path(AdoptCF, CGPathCreateMutable());

            XFORM xform;
            GetWorldTransform(hdc, &xform);
            AffineTransform hdcTransform(xform.eM11, xform.eM21, xform.eM12, xform.eM22, xform.eDx, xform.eDy);
            CGAffineTransform initialGlyphTransform = hdcTransform.isInvertible() ? hdcTransform.inverse() : CGAffineTransformIdentity;
            initialGlyphTransform.tx = 0;
            initialGlyphTransform.ty = 0;
            CGAffineTransform glyphTranslation = CGAffineTransformIdentity;

            for (unsigned i = 0; i < numGlyphs; ++i) {
                RetainPtr<CGPathRef> glyphPath(AdoptCF, createPathForGlyph(hdc, glyphBuffer.glyphAt(from + i)));
                CGAffineTransform glyphTransform = CGAffineTransformConcat(initialGlyphTransform, glyphTranslation);
                CGPathAddPath(path.get(), &glyphTransform, glyphPath.get());
                glyphTranslation = CGAffineTransformTranslate(glyphTranslation, gdiAdvances[i], 0);
            }

            CGContextRef cgContext = graphicsContext->platformContext();
            CGContextSaveGState(cgContext);

            BOOL fontSmoothingEnabled = false;
            SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &fontSmoothingEnabled, 0);
            CGContextSetShouldAntialias(cgContext, fontSmoothingEnabled);

            CGContextScaleCTM(cgContext, 1.0, -1.0);
            CGContextTranslateCTM(cgContext, point.x() + glyphBuffer.offsetAt(from).width(), -(point.y() + glyphBuffer.offsetAt(from).height()));

            if (drawingMode & cTextFill) {
                CGContextAddPath(cgContext, path.get());
                CGContextFillPath(cgContext);
            }
            if (drawingMode & cTextStroke) {
                CGContextAddPath(cgContext, path.get());
                CGContextStrokePath(cgContext);
            }
            CGContextRestoreGState(cgContext);
        }

        if (drawIntoBitmap) {
            UInt8* buffer = bitmap->buffer();
            unsigned bufferLength = bitmap->bufferLength();
            for (unsigned i = 0; i < bufferLength; i += 4) {
                // Use green, which is always in the middle.
                UInt8 alpha = (255 - buffer[i + 1]) * fillColor.alpha() / 255;
                buffer[i] = fillColor.blue();
                buffer[i + 1] = fillColor.green();
                buffer[i + 2] = fillColor.red();
                buffer[i + 3] = alpha;
            }
            graphicsContext->drawWindowsBitmap(bitmap.get(), textRect.topLeft());
        } else
            graphicsContext->releaseWindowsContext(hdc, textRect, true, false);

        return;
    }

    CGContextRef cgContext = graphicsContext->platformContext();

    uint32_t oldFontSmoothingStyle = wkSetFontSmoothingStyle(cgContext, font->m_allowFontSmoothing);

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

    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    graphicsContext->getShadow(shadowSize, shadowBlur, shadowColor);

    bool hasSimpleShadow = graphicsContext->textDrawingMode() == cTextFill && shadowColor.isValid() && !shadowBlur && !shadowSize.isEmpty();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        graphicsContext->clearShadow();
        Color fillColor = graphicsContext->fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        graphicsContext->setFillColor(shadowFillColor);
        CGContextSetTextPosition(cgContext, point.x() + shadowSize.width(), point.y() + shadowSize.height());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        if (font->m_syntheticBoldOffset) {
            CGContextSetTextPosition(cgContext, point.x() + shadowSize.width() + font->m_syntheticBoldOffset, point.y() + shadowSize.height());
            CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        }
        graphicsContext->setFillColor(fillColor);
    }

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->m_syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, point.x() + font->m_syntheticBoldOffset, point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    if (hasSimpleShadow)
        graphicsContext->setShadow(shadowSize, shadowBlur, shadowColor);

    wkRestoreFontSmoothingStyle(cgContext, oldFontSmoothingStyle);
}

}
