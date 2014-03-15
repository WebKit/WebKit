/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc.  All rights reserved.
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
#include "DragImage.h"

#include "BitmapInfo.h"
#include "CachedImage.h"
#include "GraphicsContextCG.h"
#include "HWndDC.h"
#include "Image.h"

#include <CoreGraphics/CoreGraphics.h>
#include <wtf/RetainPtr.h>
#include <wtf/win/GDIObject.h>

#include <windows.h>

namespace WebCore {

void deallocContext(CGContextRef target)
{
    CGContextRelease(target);
}

GDIObject<HBITMAP> allocImage(HDC dc, IntSize size, CGContextRef *targetRef)
{
    BitmapInfo bmpInfo = BitmapInfo::create(size);

    LPVOID bits;
    auto hbmp = adoptGDIObject(::CreateDIBSection(dc, &bmpInfo, DIB_RGB_COLORS, &bits, 0, 0));

    if (!targetRef)
        return hbmp;

    CGContextRef bitmapContext = CGBitmapContextCreate(bits, bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight, 8,
                                                       bmpInfo.bmiHeader.biWidth * 4, deviceRGBColorSpaceRef(),
                                                       kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    if (!bitmapContext)
        return GDIObject<HBITMAP>();

    *targetRef = bitmapContext;
    return hbmp;
}

static CGContextRef createCgContextFromBitmap(HBITMAP bitmap)
{
    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    CGContextRef bitmapContext = CGBitmapContextCreate(info.bmBits, info.bmWidth, info.bmHeight, 8,
                                                       info.bmWidthBytes, deviceRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    return bitmapContext;
}

DragImageRef scaleDragImage(DragImageRef imageRef, FloatSize scale)
{
    // FIXME: due to the way drag images are done on windows we need 
    // to preprocess the alpha channel <rdar://problem/5015946>
    if (!imageRef)
        return 0;

    GDIObject<HBITMAP> hbmp;
    auto image = adoptGDIObject(imageRef);

    IntSize srcSize = dragImageSize(image.get());
    IntSize dstSize(static_cast<int>(srcSize.width() * scale.width()), static_cast<int>(srcSize.height() * scale.height()));

    HWndDC dc(0);
    auto dstDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!dstDC)
        goto exit;

    CGContextRef targetContext;
    hbmp = allocImage(dstDC.get(), dstSize, &targetContext);
    if (!hbmp)
        goto exit;

    CGContextRef srcContext = createCgContextFromBitmap(image.get());
    CGImageRef srcImage = CGBitmapContextCreateImage(srcContext);
    CGRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size = dstSize;
    CGContextDrawImage(targetContext, rect, srcImage);
    CGImageRelease(srcImage);
    CGContextRelease(srcContext);
    CGContextRelease(targetContext);

exit:
    if (!hbmp)
        hbmp.swap(image);
    return hbmp.leak();
}
    
DragImageRef createDragImageFromImage(Image* img, ImageOrientationDescription)
{
    HWndDC dc(0);
    auto workingDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!workingDC)
        return 0;

    CGContextRef drawContext = 0;
    auto hbmp = allocImage(workingDC.get(), img->size(), &drawContext);
    if (!hbmp || !drawContext)
        return 0;

    CGImageRef srcImage = img->getCGImageRef();
    CGRect rect;
    rect.size = img->size();
    rect.origin.x = 0;
    rect.origin.y = -rect.size.height;
    static const CGFloat white [] = {1.0, 1.0, 1.0, 1.0};
    CGContextScaleCTM(drawContext, 1, -1);
    CGContextSetFillColor(drawContext, white);
    CGContextFillRect(drawContext, rect);
    if (srcImage) {
        CGContextSetBlendMode(drawContext, kCGBlendModeNormal);
        CGContextDrawImage(drawContext, rect, srcImage);
    }
    CGContextRelease(drawContext);

    return hbmp.leak();
}
    
}
