/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CGUtilities.h"

#include <wtf/RetainPtr.h>

namespace WebKit {
    
void paintBitmapContext(CGContextRef context, CGContextRef bitmapContext, CGPoint destination, CGRect source)
{
    void* bitmapData = CGBitmapContextGetData(bitmapContext);
    ASSERT(bitmapData);

    size_t imageWidth = CGBitmapContextGetWidth(bitmapContext);
    size_t imageHeight = CGBitmapContextGetHeight(bitmapContext);

    size_t bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapContext);

    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithData(0, bitmapData, bytesPerRow * imageHeight, 0));
    RetainPtr<CGImageRef> image(AdoptCF, CGImageCreate(imageWidth, imageHeight, 
                                                       CGBitmapContextGetBitsPerComponent(bitmapContext),
                                                       CGBitmapContextGetBitsPerPixel(bitmapContext), 
                                                       bytesPerRow,
                                                       CGBitmapContextGetColorSpace(bitmapContext), 
                                                       CGBitmapContextGetBitmapInfo(bitmapContext), 
                                                       dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    CGContextSaveGState(context);

    CGContextClipToRect(context, CGRectMake(destination.x, destination.y, source.size.width, source.size.height));
    CGContextScaleCTM(context, 1, -1);

    CGFloat destX = destination.x - source.origin.x;
    CGFloat destY = -static_cast<CGFloat>(imageHeight) - destination.y + source.origin.y;

    CGContextDrawImage(context, CGRectMake(destX, destY, imageWidth, imageHeight), image.get());
    CGContextRestoreGState(context);
}
    
} // namespace WebKit

