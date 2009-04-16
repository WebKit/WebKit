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

#include "TransformationMatrix.h"
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
    CGContextRelease(m_data->m_cgContext);
    m_data->m_hdc = hdc;
    setPaintingDisabled(!m_data->m_cgContext);
    if (m_data->m_cgContext) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
    }
}

// FIXME: Is it possible to merge getWindowsContext and createWindowsBitmap into a single API
// suitable for all clients?
HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // FIXME: Should a bitmap be created also when a shadow is set?
    if (mayCreateBitmap && inTransparencyLayer()) {
        if (dstRect.isEmpty())
            return 0;

        // Create a bitmap DC in which to draw.
        BITMAPINFO bitmapInfo;
        bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth         = dstRect.width(); 
        bitmapInfo.bmiHeader.biHeight        = dstRect.height();
        bitmapInfo.bmiHeader.biPlanes        = 1;
        bitmapInfo.bmiHeader.biBitCount      = 32;
        bitmapInfo.bmiHeader.biCompression   = BI_RGB;
        bitmapInfo.bmiHeader.biSizeImage     = 0;
        bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biClrUsed       = 0;
        bitmapInfo.bmiHeader.biClrImportant  = 0;

        void* pixels = 0;
        HBITMAP bitmap = ::CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
        if (!bitmap)
            return 0;

        HDC bitmapDC = ::CreateCompatibleDC(m_data->m_hdc);
        ::SelectObject(bitmapDC, bitmap);

        // Fill our buffer with clear if we're going to alpha blend.
        if (supportAlphaBlend) {
            BITMAP bmpInfo;
            GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
            int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
            memset(bmpInfo.bmBits, 0, bufferSize);
        }

        // Make sure we can do world transforms.
        SetGraphicsMode(bitmapDC, GM_ADVANCED);

        // Apply a translation to our context so that the drawing done will be at (0,0) of the bitmap.
        XFORM xform;
        xform.eM11 = 1.0f;
        xform.eM12 = 0.0f;
        xform.eM21 = 0.0f;
        xform.eM22 = 1.0f;
        xform.eDx = -dstRect.x();
        xform.eDy = -dstRect.y();
        ::SetWorldTransform(bitmapDC, &xform);

        return bitmapDC;
    }

    CGContextFlush(platformContext());
    m_data->save();
    return m_data->m_hdc;
}

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
        CGContextDrawImage(m_data->m_cgContext, dstRect, image);
        
        // Delete all our junk.
        CGImageRelease(image);
        CGContextRelease(bitmapContext);
        ::DeleteDC(hdc);
        ::DeleteObject(bitmap);

        return;
    }

    m_data->restore();
}

GraphicsContext::WindowsBitmap::WindowsBitmap(HDC hdc, IntSize size)
    : m_hdc(0)
    , m_size(size)
{
    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = m_size.width(); 
    bitmapInfo.bmiHeader.biHeight = m_size.height();
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed = 0;
    bitmapInfo.bmiHeader.biClrImportant = 0;

    m_bitmap = CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_bitmapBuffer), 0, 0);
    if (!m_bitmap)
        return;

    m_hdc = CreateCompatibleDC(hdc);
    SelectObject(m_hdc, m_bitmap);

    BITMAP bmpInfo;
    GetObject(m_bitmap, sizeof(bmpInfo), &bmpInfo);
    m_bytesPerRow = bmpInfo.bmWidthBytes;
    m_bitmapBufferLength = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;

    SetGraphicsMode(m_hdc, GM_ADVANCED);
}

GraphicsContext::WindowsBitmap::~WindowsBitmap()
{
    if (!m_bitmap)
        return;

    DeleteDC(m_hdc);
    DeleteObject(m_bitmap);
}

GraphicsContext::WindowsBitmap* GraphicsContext::createWindowsBitmap(IntSize size)
{
    return new WindowsBitmap(m_data->m_hdc, size);
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
    CGContextDrawImage(m_data->m_cgContext, CGRectMake(point.x(), point.y(), image->size().width(), image->size().height()), cgImage.get());   
}

void GraphicsContextPlatformPrivate::concatCTM(const TransformationMatrix& transform)
{
    if (!m_hdc)
        return;

    CGAffineTransform mat = transform;
    XFORM xform;
    xform.eM11 = mat.a;
    xform.eM12 = mat.b;
    xform.eM21 = mat.c;
    xform.eM22 = mat.d;
    xform.eDx = mat.tx;
    xform.eDy = mat.ty;

    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    float radius = (focusRingWidth() - 1) / 2.0f;
    int offset = radius + focusRingOffset();
    CGColorRef colorRef = color.isValid() ? createCGColor(color) : 0;

    CGMutablePathRef focusRingPath = CGPathCreateMutable();
    const Vector<IntRect>& rects = focusRingRects();
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

}
