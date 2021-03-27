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

#if USE(CG)

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

    LPVOID bits = nullptr;
    auto hbmp = adoptGDIObject(::CreateDIBSection(dc, &bmpInfo, DIB_RGB_COLORS, &bits, 0, 0));

    if (!targetRef || !hbmp)
        return hbmp;

    auto bitmapContext = adoptCF(CGBitmapContextCreate(bits, bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight, 8, bmpInfo.bmiHeader.biWidth * 4, sRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst));
    if (!bitmapContext)
        return GDIObject<HBITMAP>();

    *targetRef = bitmapContext.leakRef();
    return hbmp;
}

static RetainPtr<CGContextRef> createCgContextFromBitmap(HBITMAP bitmap)
{
    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    return adoptCF(CGBitmapContextCreate(info.bmBits, info.bmWidth, info.bmHeight, 8, info.bmWidthBytes, sRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst));
}

DragImageRef scaleDragImage(DragImageRef imageRef, FloatSize scale)
{
    // FIXME: due to the way drag images are done on windows we need 
    // to preprocess the alpha channel <rdar://problem/5015946>
    if (!imageRef)
        return 0;

    GDIObject<HBITMAP> hbmp;
    auto image = adoptGDIObject(imageRef);

    auto returnHbmp = [&hbmp, &image] {
        if (!hbmp)
            hbmp.swap(image);
        return hbmp.leak();
    };

    IntSize srcSize = dragImageSize(image.get());
    IntSize dstSize(static_cast<int>(srcSize.width() * scale.width()), static_cast<int>(srcSize.height() * scale.height()));

    HWndDC dc(0);
    auto dstDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!dstDC)
        return returnHbmp();

    CGContextRef targetContext;
    hbmp = allocImage(dstDC.get(), dstSize, &targetContext);
    if (!hbmp)
        return returnHbmp();

    auto srcContext = createCgContextFromBitmap(image.get());
    auto srcImage = adoptCF(CGBitmapContextCreateImage(srcContext.get()));
    CGRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size = dstSize;
    CGContextDrawImage(targetContext, rect, srcImage.get());
    CGContextRelease(targetContext);

    return returnHbmp();
}
    
DragImageRef createDragImageFromImage(Image* img, ImageOrientation)
{
    HWndDC dc(0);
    auto workingDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!workingDC)
        return 0;

    CGContextRef drawContext = 0;
    auto hbmp = allocImage(workingDC.get(), IntSize(img->size()), &drawContext);
    if (!hbmp || !drawContext)
        return 0;

    CGRect rect;
    rect.size = IntSize(img->size());
    rect.origin.x = 0;
    rect.origin.y = -rect.size.height;
    static const CGFloat white [] = {1.0, 1.0, 1.0, 1.0};
    CGContextScaleCTM(drawContext, 1, -1);
    CGContextSetFillColor(drawContext, white);
    CGContextFillRect(drawContext, rect);
    if (auto srcImage = img->nativeImage()) {
        if (auto platformImage = srcImage->platformImage()) {
            CGContextSetBlendMode(drawContext, kCGBlendModeNormal);
            CGContextDrawImage(drawContext, rect, platformImage.get());
        }
    }
    CGContextRelease(drawContext);

    return hbmp.leak();
}
    
}

#endif
