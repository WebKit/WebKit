/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    CGContextRef cgContext = context->platformContext();

    bool originalShouldUseFontSmoothing = wkCGContextGetShouldSmoothFonts(cgContext);
    bool newShouldUseFontSmoothing = shouldUseSmoothing();
    
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
    if (drawFont)
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

    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    context->getShadow(shadowSize, shadowBlur, shadowColor);

    bool hasSimpleShadow = context->textDrawingMode() == cTextFill && shadowColor.isValid() && !shadowBlur;
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context->clearShadow();
        Color fillColor = context->fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        context->setFillColor(shadowFillColor);
        CGContextSetTextPosition(cgContext, point.x() + shadowSize.width(), point.y() + shadowSize.height());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        if (font->syntheticBoldOffset()) {
            CGContextSetTextPosition(cgContext, point.x() + shadowSize.width() + font->syntheticBoldOffset(), point.y() + shadowSize.height());
            CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
        }
        context->setFillColor(fillColor);
    }

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->syntheticBoldOffset()) {
        CGContextSetTextPosition(cgContext, point.x() + font->syntheticBoldOffset(), point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    if (hasSimpleShadow)
        context->setShadow(shadowSize, shadowBlur, shadowColor);

    if (originalShouldUseFontSmoothing != newShouldUseFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
}

}
