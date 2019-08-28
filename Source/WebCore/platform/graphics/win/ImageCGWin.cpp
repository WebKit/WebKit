/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Image.h"

#if USE(CG)

#include "BitmapImage.h"
#include "BitmapInfo.h"
#include "GraphicsContextCG.h"
#include <windows.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

RefPtr<BitmapImage> BitmapImage::create(HBITMAP hBitmap)
{
    DIBSECTION dibSection;
    if (!GetObject(hBitmap, sizeof(DIBSECTION), &dibSection))
        return 0;

    ASSERT(dibSection.dsBm.bmBitsPixel == 32);
    if (dibSection.dsBm.bmBitsPixel != 32)
        return 0;

    ASSERT(dibSection.dsBm.bmBits);
    if (!dibSection.dsBm.bmBits)
        return 0;

    RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreate(dibSection.dsBm.bmBits, dibSection.dsBm.bmWidth, dibSection.dsBm.bmHeight, 8,
        dibSection.dsBm.bmWidthBytes, sRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst));

    // The BitmapImage takes ownership of this.
    RetainPtr<CGImageRef> cgImage = adoptCF(CGBitmapContextCreateImage(bitmapContext.get()));

    return adoptRef(new BitmapImage(WTFMove(cgImage)));
}

bool BitmapImage::getHBITMAPOfSize(HBITMAP bmp, const IntSize* size)
{
    ASSERT(bmp);

    BITMAP bmpInfo;
    GetObject(bmp, sizeof(BITMAP), &bmpInfo);

    ASSERT(bmpInfo.bmBitsPixel == 32);
    int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
    
    CGContextRef cgContext = CGBitmapContextCreate(bmpInfo.bmBits, bmpInfo.bmWidth, bmpInfo.bmHeight,
        8, bmpInfo.bmWidthBytes, sRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst);
  
    GraphicsContext gc(cgContext);

    FloatSize imageSize = BitmapImage::size();
    if (size)
        drawFrameMatchingSourceSize(gc, FloatRect(0.0f, 0.0f, bmpInfo.bmWidth, bmpInfo.bmHeight), *size, CompositeCopy);
    else
        draw(gc, FloatRect(0.0f, 0.0f, bmpInfo.bmWidth, bmpInfo.bmHeight), FloatRect(0.0f, 0.0f, imageSize.width(), imageSize.height()), { CompositeCopy });

    // Do cleanup
    CGContextRelease(cgContext);

    return true;
}

void BitmapImage::drawFrameMatchingSourceSize(GraphicsContext& ctxt, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator compositeOp)
{
    size_t frames = frameCount();
    for (size_t i = 0; i < frames; ++i) {
        CGImageRef image = frameImageAtIndex(i).get();
        if (image && CGImageGetHeight(image) == static_cast<size_t>(srcSize.height()) && CGImageGetWidth(image) == static_cast<size_t>(srcSize.width())) {
            size_t currentFrame = m_currentFrame;
            m_currentFrame = i;
            draw(ctxt, dstRect, FloatRect(0.0f, 0.0f, srcSize.width(), srcSize.height()), { compositeOp });
            m_currentFrame = currentFrame;
            return;
        }
    }

    // No image of the correct size was found, fallback to drawing the current frame
    FloatSize imageSize = BitmapImage::size();
    draw(ctxt, dstRect, FloatRect(0.0f, 0.0f, imageSize.width(), imageSize.height()), { compositeOp });
}

} // namespace WebCore

#endif
