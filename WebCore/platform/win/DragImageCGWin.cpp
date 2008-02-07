/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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
#include "DragImage.h"

#include "CachedImage.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "RetainPtr.h"

#include <CoreGraphics/CoreGraphics.h>

#include <windows.h>

namespace WebCore {

HBITMAP allocImage(HDC dc, IntSize size, CGContextRef *targetRef)
{
    HBITMAP hbmp;
    BITMAPINFO bmpInfo = {0};
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = size.width();
    bmpInfo.bmiHeader.biHeight = size.height();
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 32;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    LPVOID bits;
    hbmp = CreateDIBSection(dc, &bmpInfo, DIB_RGB_COLORS, &bits, 0, 0);

    if (!targetRef)
        return hbmp;

    CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapContext = CGBitmapContextCreate(bits, bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight, 8,
                                                       bmpInfo.bmiHeader.biWidth * 4, deviceRGB, 
                                                       kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    CGColorSpaceRelease(deviceRGB);
    if (!bitmapContext) {
        DeleteObject(hbmp);
        return 0;
    }

    *targetRef = bitmapContext;
    return hbmp;
}

static CGContextRef createCgContextFromBitmap(HBITMAP bitmap)
{
    BITMAP info;
    GetObject(bitmap, sizeof(info), &info);
    ASSERT(info.bmBitsPixel == 32);

    CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapContext = CGBitmapContextCreate(info.bmBits, info.bmWidth, info.bmHeight, 8,
                                                       info.bmWidthBytes, deviceRGB, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    CGColorSpaceRelease(deviceRGB);
    return bitmapContext;
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    // FIXME: due to the way drag images are done on windows we need 
    // to preprocess the alpha channel <rdar://problem/5015946>

    if (!image)
        return 0;
    CGContextRef targetContext;
    CGContextRef srcContext;
    CGImageRef srcImage;
    IntSize srcSize = dragImageSize(image);
    IntSize dstSize(static_cast<int>(srcSize.width() * scale.width()), static_cast<int>(srcSize.height() * scale.height()));
    HBITMAP hbmp = 0;
    HDC dc = GetDC(0);
    HDC dstDC = CreateCompatibleDC(dc);
    if (!dstDC)
        goto exit;

    hbmp = allocImage(dstDC, dstSize, &targetContext);
    if (!hbmp)
        goto exit;

    srcContext = createCgContextFromBitmap(image);
    srcImage = CGBitmapContextCreateImage(srcContext);
    CGRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size = dstSize;
    CGContextDrawImage(targetContext, rect, srcImage);
    CGImageRelease(srcImage);
    CGContextRelease(srcContext);
    CGContextRelease(targetContext);
    ::DeleteObject(image);
    image = 0;

exit:
    if (!hbmp)
        hbmp = image;
    if (dstDC)
        DeleteDC(dstDC);
    ReleaseDC(0, dc);
    return hbmp;
}
    
DragImageRef createDragImageFromImage(Image* img)
{
    HBITMAP hbmp = 0;
    HDC dc = GetDC(0);
    HDC workingDC = CreateCompatibleDC(dc);
    CGContextRef drawContext = 0;
    if (!workingDC)
        goto exit;

    hbmp = allocImage(workingDC, img->size(), &drawContext);

    if (!hbmp)
        goto exit;

    if (!drawContext) {
        ::DeleteObject(hbmp);
        hbmp = 0;
    }

    CGImageRef srcImage = img->getCGImageRef();
    CGRect rect;
    rect.size = img->size();
    rect.origin.x = 0;
    rect.origin.y = -rect.size.height;
    static const CGFloat white [] = {1.0, 1.0, 1.0, 1.0};
    CGContextScaleCTM(drawContext, 1, -1);
    CGContextSetFillColor(drawContext, white);
    CGContextFillRect(drawContext, rect);
    CGContextSetBlendMode(drawContext, kCGBlendModeNormal);
    CGContextDrawImage(drawContext, rect, srcImage);
    CGContextRelease(drawContext);

exit:
    if (workingDC)
        DeleteDC(workingDC);
    ReleaseDC(0, dc);
    return hbmp;
}
    
}
