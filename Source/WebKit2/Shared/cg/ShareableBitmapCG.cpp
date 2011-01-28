/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ShareableBitmap.h"

#include <WebCore/GraphicsContext.h>
#include <wtf/RetainPtr.h>
#include "CGUtilities.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> bitmapContext(AdoptCF, CGBitmapContextCreate(data(), m_size.width(), m_size.height(), 8,  m_size.width() * 4, colorSpace.get(), 
                                                                         kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

    // We want the origin to be in the top left corner so flip the backing store context.
    CGContextTranslateCTM(bitmapContext.get(), 0, m_size.height());
    CGContextScaleCTM(bitmapContext.get(), 1, -1);

    return adoptPtr(new GraphicsContext(bitmapContext.get()));
}

void ShareableBitmap::paint(WebCore::GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    OwnPtr<GraphicsContext> sourceContext(createGraphicsContext());

    paintBitmapContext(context.platformContext(), sourceContext->platformContext(), dstPoint, srcRect);
}
        
} // namespace WebKit
