/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "GraphicsContext.h"

#include "AffineTransform.h"
#include "Path.h"

#include <CoreGraphics/CGBitmapContext.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include "GraphicsContextPlatformPrivateCG.h"

using namespace std;

namespace WebCore {

static CGContextRef CGContextWithHDC(HDC hdc, bool hasAlpha)
{
    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
    CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
    BITMAP info;

    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | (hasAlpha ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaNoneSkipFirst);
    CGContextRef context = CGBitmapContextCreate(info.bmBits, info.bmWidth, info.bmHeight, 8,
                                                 info.bmWidthBytes, deviceRGB, bitmapInfo);
    CGColorSpaceRelease(deviceRGB);

    // Flip coords
    CGContextTranslateCTM(context, 0, info.bmHeight);
    CGContextScaleCTM(context, 1, -1);
    
    // Put the HDC In advanced mode so it will honor affine transforms.
    SetGraphicsMode(hdc, GM_ADVANCED);
    
    return context;
}

GraphicsContext::GraphicsContext(HDC hdc, bool hasAlpha)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(CGContextWithHDC(hdc, hasAlpha)))
{
    CGContextRelease(m_data->m_cgContext.get());
    m_data->m_hdc = hdc;
    setPaintingDisabled(!m_data->m_cgContext);
    if (m_data->m_cgContext) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor(), DeviceColorSpace);
        setPlatformStrokeColor(strokeColor(), DeviceColorSpace);
    }
}

// FIXME: Is it possible to merge getWindowsContext and createWindowsBitmap into a single API
// suitable for all clients?
void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    if (mayCreateBitmap && hdc && inTransparencyLayer()) {
        if (dstRect.isEmpty())
            return;

        HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

        // Need to make a CGImage out of the bitmap's pixel buffer and then draw
        // it into our context.
        BITMAP info;
        GetObject(bitmap, sizeof(info), &info);
        ASSERT(info.bmBitsPixel == 32);

        CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
        CGContextRef bitmapContext = CGBitmapContextCreate(info.bmBits, info.bmWidth, info.bmHeight, 8,
                                                           info.bmWidthBytes, deviceRGB, kCGBitmapByteOrder32Little | 
                                                           (supportAlphaBlend ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaNoneSkipFirst));
        CGColorSpaceRelease(deviceRGB);

        CGImageRef image = CGBitmapContextCreateImage(bitmapContext);
        CGContextDrawImage(m_data->m_cgContext.get(), dstRect, image);
        
        // Delete all our junk.
        CGImageRelease(image);
        CGContextRelease(bitmapContext);
        ::DeleteDC(hdc);
        ::DeleteObject(bitmap);

        return;
    }

    m_data->restore();
}

void GraphicsContext::drawWindowsBitmap(WindowsBitmap* image, const IntPoint& point)
{
    RetainPtr<CGColorSpaceRef> deviceRGB(AdoptCF, CGColorSpaceCreateDeviceRGB());
    // FIXME: Creating CFData is non-optimal, but needed to avoid crashing when printing.  Ideally we should 
    // make a custom CGDataProvider that controls the WindowsBitmap lifetime.  see <rdar://6394455>
    RetainPtr<CFDataRef> imageData(AdoptCF, CFDataCreate(kCFAllocatorDefault, image->buffer(), image->bufferLength()));
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithCFData(imageData.get()));
    RetainPtr<CGImageRef> cgImage(AdoptCF, CGImageCreate(image->size().width(), image->size().height(), 8, 32, image->bytesPerRow(), deviceRGB.get(),
                                                         kCGBitmapByteOrder32Little | kCGImageAlphaFirst, dataProvider.get(), 0, true, kCGRenderingIntentDefault));
    CGContextDrawImage(m_data->m_cgContext.get(), CGRectMake(point.x(), point.y(), image->size().width(), image->size().height()), cgImage.get());   
}

void GraphicsContext::drawFocusRing(const Vector<Path>& paths, int width, int offset, const Color& color)
{
    // FIXME: implement
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (paintingDisabled())
        return;

    float radius = (width - 1) / 2.0f;
    offset += radius;
    CGColorRef colorRef = color.isValid() ? createCGColor(color) : 0;

    CGMutablePathRef focusRingPath = CGPathCreateMutable();
    unsigned rectCount = rects.size();
    for (unsigned i = 0; i < rectCount; i++)
        CGPathAddRect(focusRingPath, 0, CGRectInset(rects[i], -offset, -offset));

    CGContextRef context = platformContext();
    CGContextSaveGState(context);

    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);

    wkDrawFocusRing(context, colorRef, radius);

    CGColorRelease(colorRef);

    CGPathRelease(focusRingPath);

    CGContextRestoreGState(context);
}

// Pulled from GraphicsContextCG
static void setCGStrokeColor(CGContextRef context, const Color& color)
{
    CGFloat red, green, blue, alpha;
    color.getRGBA(red, green, blue, alpha);
    CGContextSetRGBStrokeColor(context, red, green, blue, alpha);
}

static const Color& spellingPatternColor() {
    static const Color spellingColor(255, 0, 0);
    return spellingColor;
}

static const Color& grammarPatternColor() {
    static const Color grammarColor(0, 128, 0);
    return grammarColor;
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& point, int width, bool grammar)
{
    if (paintingDisabled())
        return;

    // These are the same for misspelling or bad grammar
    const int patternHeight = 3; // 3 rows
    ASSERT(cMisspellingLineThickness == patternHeight);
    const int patternWidth = 4; // 4 pixels
    ASSERT(patternWidth == cMisspellingLinePatternWidth);

    // Make sure to draw only complete dots.
    // NOTE: Code here used to shift the underline to the left and increase the width
    // to make sure everything gets underlined, but that results in drawing out of
    // bounds (e.g. when at the edge of a view) and could make it appear that the
    // space between adjacent misspelled words was underlined.
    // allow slightly more considering that the pattern ends with a transparent pixel
    int widthMod = width % patternWidth;
    if (patternWidth - widthMod > cMisspellingLinePatternGapWidth)
        width -= widthMod;
      
    // Draw the underline
    CGContextRef context = platformContext();
    CGContextSaveGState(context);

    const Color& patternColor = grammar ? grammarPatternColor() : spellingPatternColor();
    setCGStrokeColor(context, patternColor);

    wkSetPatternPhaseInUserSpace(context, point);
    CGContextSetBlendMode(context, kCGBlendModeNormal);
    
    // 3 rows, each offset by half a pixel for blending purposes
    const CGPoint upperPoints [] = {{point.x(), point.y() + patternHeight - 2.5 }, {point.x() + width, point.y() + patternHeight - 2.5}};
    const CGPoint middlePoints [] = {{point.x(), point.y() + patternHeight - 1.5 }, {point.x() + width, point.y() + patternHeight - 1.5}};
    const CGPoint lowerPoints [] = {{point.x(), point.y() + patternHeight - 0.5 }, {point.x() + width, point.y() + patternHeight - 0.5 }};
    
    // Dash lengths for the top and bottom of the error underline are the same.
    // These are magic.
    static const float edge_dash_lengths[] = {2.0f, 2.0f};
    static const float middle_dash_lengths[] = {2.76f, 1.24f};
    static const float edge_offset = -(edge_dash_lengths[1] - 1.0f) / 2.0f;
    static const float middle_offset = -(middle_dash_lengths[1] - 1.0f) / 2.0f;

    // Line opacities.  Once again, these are magic.
    const float upperOpacity = 0.33f;
    const float middleOpacity = 0.75f;
    const float lowerOpacity = 0.88f;

    //Top line
    CGContextSetLineDash(context, edge_offset, edge_dash_lengths, 
                         sizeof(edge_dash_lengths) / sizeof(edge_dash_lengths[0]));
    CGContextSetAlpha(context, upperOpacity);
    CGContextStrokeLineSegments(context, upperPoints, 2);
 
    // Middle line
    CGContextSetLineDash(context, middle_offset, middle_dash_lengths, 
                         sizeof(middle_dash_lengths) / sizeof(middle_dash_lengths[0]));
    CGContextSetAlpha(context, middleOpacity);
    CGContextStrokeLineSegments(context, middlePoints, 2);
    
    // Bottom line
    CGContextSetLineDash(context, edge_offset, edge_dash_lengths,
                         sizeof(edge_dash_lengths) / sizeof(edge_dash_lengths[0]));
    CGContextSetAlpha(context, lowerOpacity);
    CGContextStrokeLineSegments(context, lowerPoints, 2);

    CGContextRestoreGState(context);
}

void GraphicsContextPlatformPrivate::flush()
{
    CGContextFlush(m_cgContext.get());
}

}
