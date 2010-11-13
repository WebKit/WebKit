/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "config.h"
#import "Font.h"

#import "GlyphBuffer.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "SimpleFontData.h"
#import "WebCoreSystemInterface.h"
#import <AppKit/AppKit.h>

#define SYNTHETIC_OBLIQUE_ANGLE 14

#ifdef __LP64__
#define URefCon void*
#else
#define URefCon UInt32
#endif

using namespace std;

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return true;
}

static void showGlyphsWithAdvances(const SimpleFontData* font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
    const FontPlatformData& platformData = font->platformData();
    if (!platformData.isColorBitmapFont()) {
        CGAffineTransform savedMatrix;
        bool isVertical = font->orientation() == Vertical;

        if (isVertical) {
            CGAffineTransform rotateLeftTransform = CGAffineTransformMake(0, -1, 1, 0, 0, 0);

            savedMatrix = CGContextGetTextMatrix(context);
            CGAffineTransform runMatrix = CGAffineTransformConcat(savedMatrix, rotateLeftTransform);
            // Move start point to put glyphs into original region.
            runMatrix.tx = savedMatrix.tx + font->ascent();
            runMatrix.ty = savedMatrix.ty + font->descent();
            CGContextSetTextMatrix(context, runMatrix);
        }

        CGContextShowGlyphsWithAdvances(context, glyphs, advances, count);

        if (isVertical)
            CGContextSetTextMatrix(context, savedMatrix);
    }
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    else {
        if (!count)
            return;

        Vector<CGPoint, 256> positions(count);
        CGAffineTransform matrix = CGAffineTransformInvert(CGContextGetTextMatrix(context));
        positions[0] = CGPointZero;
        for (size_t i = 1; i < count; ++i) {
            CGSize advance = CGSizeApplyAffineTransform(advances[i - 1], matrix);
            positions[i].x = positions[i - 1].x + advance.width;
            positions[i].y = positions[i - 1].y + advance.height;
        }
        CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
    }
#endif
}

void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    CGContextRef cgContext = context->platformContext();
    bool newShouldUseFontSmoothing = shouldUseSmoothing();

    switch(fontDescription().fontSmoothing()) {
    case Antialiased: {
        context->setShouldAntialias(true);
        newShouldUseFontSmoothing = false;
        break;
    }
    case SubpixelAntialiased: {
        context->setShouldAntialias(true);
        newShouldUseFontSmoothing = true;
        break;
    }
    case NoSmoothing: {
        context->setShouldAntialias(false);
        newShouldUseFontSmoothing = false;
        break;
    }
    case AutoSmoothing: {
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break; 
    }
    default: 
        ASSERT_NOT_REACHED();
    }

    bool originalShouldUseFontSmoothing = wkCGContextGetShouldSmoothFonts(cgContext);
    if (originalShouldUseFontSmoothing != newShouldUseFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, newShouldUseFontSmoothing);
    
    const FontPlatformData& platformData = font->platformData();
    NSFont* drawFont;
    if (!isPrinterFont()) {
        drawFont = [platformData.font() screenFont];
        if (drawFont != platformData.font())
            // We are getting this in too many places (3406411); use ERROR so it only prints on debug versions for now. (We should debug this also, eventually).
            LOG_ERROR("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.",
                [[[platformData.font() fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    } else {
        drawFont = [platformData.font() printerFont];
        if (drawFont != platformData.font())
            NSLog(@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.",
                [[[platformData.font() fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    }
    
    CGContextSetFont(cgContext, platformData.cgFont());

    CGAffineTransform matrix = CGAffineTransformIdentity;
    if (drawFont && !platformData.isColorBitmapFont())
        memcpy(&matrix, [drawFont matrix], sizeof(matrix));
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;
    if (platformData.m_syntheticOblique)
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
    CGContextSetTextMatrix(cgContext, matrix);

    if (drawFont) {
        wkSetCGFontRenderingMode(cgContext, drawFont);
        CGContextSetFontSize(cgContext, 1.0f);
    } else
        CGContextSetFontSize(cgContext, platformData.m_size);


    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    ColorSpace fillColorSpace = context->fillColorSpace();
    context->getShadow(shadowOffset, shadowBlur, shadowColor);

    bool hasSimpleShadow = context->textDrawingMode() == cTextFill && shadowColor.isValid() && !shadowBlur && !platformData.isColorBitmapFont();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context->clearShadow();
        Color fillColor = context->fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        context->setFillColor(shadowFillColor, fillColorSpace);
        CGContextSetTextPosition(cgContext, point.x() + shadowOffset.width(), point.y() + shadowOffset.height());
        showGlyphsWithAdvances(font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        if (font->syntheticBoldOffset()) {
            CGContextSetTextPosition(cgContext, point.x() + shadowOffset.width() + font->syntheticBoldOffset(), point.y() + shadowOffset.height());
            showGlyphsWithAdvances(font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        }
        context->setFillColor(fillColor, fillColorSpace);
    }

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    showGlyphsWithAdvances(font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->syntheticBoldOffset()) {
        CGContextSetTextPosition(cgContext, point.x() + font->syntheticBoldOffset(), point.y());
        showGlyphsWithAdvances(font, cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    if (hasSimpleShadow)
        context->setShadow(shadowOffset, shadowBlur, shadowColor, fillColorSpace);

    if (originalShouldUseFontSmoothing != newShouldUseFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
}

}
