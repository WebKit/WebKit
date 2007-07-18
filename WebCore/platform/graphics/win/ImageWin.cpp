/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "Image.h"
#include "BitmapImage.h"
#include "GraphicsContext.h"
#include <ApplicationServices/ApplicationServices.h>

#include <winsock2.h>
#include <windows.h>
#include "PlatformString.h"
#include "MIMETypeRegistry.h"
#include "SharedBuffer.h"

// This function loads resources from WebKit
PassRefPtr<WebCore::SharedBuffer> loadResourceIntoBuffer(const char*);

namespace WebCore {

void BitmapImage::initPlatformData()
{
}

void BitmapImage::invalidatePlatformData()
{
}

Image* Image::loadPlatformResource(const char *name)
{
    RefPtr<SharedBuffer> buffer = loadResourceIntoBuffer(name);
    BitmapImage* img = new BitmapImage;
    img->setData(buffer.release(), true);
    return img;
}

bool BitmapImage::getHBITMAP(HBITMAP bmp)
{
    ASSERT(bmp);

    BITMAP bmpInfo;
    GetObject(bmp, sizeof(BITMAP), &bmpInfo);

    ASSERT(bmpInfo.bmBitsPixel == 32);
    int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
    
    CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
    CGContextRef cgContext = CGBitmapContextCreate(bmpInfo.bmBits, bmpInfo.bmWidth, bmpInfo.bmHeight,
        8, bmpInfo.bmWidthBytes, deviceRGB, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst);
  
    GraphicsContext gc(cgContext);
    IntSize imageSize = BitmapImage::size();
    draw(&gc, FloatRect(0, 0, bmpInfo.bmWidth, bmpInfo.bmHeight), FloatRect(0, 0, imageSize.width(), imageSize.height()), CompositeCopy);

    // Do cleanup
    CGContextRelease(cgContext);
    CGColorSpaceRelease(deviceRGB);
    return true;
}

} // namespace WebCore
