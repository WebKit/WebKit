/*
 * Copyright (C) 2020 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScreencastEncoder.h"

#include <algorithm>
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/RetainPtr.h>

namespace WebKit {

void ScreencastEncoder::imageToARGB(CGImageRef image, uint8_t* argb_data, int width, int height, std::optional<double> scale, int offsetTop)
{
    size_t bitsPerComponent = 8;
    size_t bytesPerPixel = 4;
    size_t bytesPerRow = bytesPerPixel * width;
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(argb_data, width, height, bitsPerComponent, bytesPerRow, colorSpace.get(), kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little));
    double imageWidth = CGImageGetWidth(image);
    double imageHeight = CGImageGetHeight(image);
    // TODO: exclude controls from original screenshot
    double pageHeight = imageHeight - offsetTop;
    double ratio = 1;
    if (scale) {
        ratio = *scale;
    } else if (imageWidth > width || pageHeight > height) {
        ratio = std::min(width / imageWidth, height / pageHeight);
    }
    imageWidth *= ratio;
    imageHeight *= ratio;
    pageHeight *= ratio;
    CGContextDrawImage(context.get(), CGRectMake(0, height - pageHeight, imageWidth, imageHeight), image);
}

} // namespace WebKit
