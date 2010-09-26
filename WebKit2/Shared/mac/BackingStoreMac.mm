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

#include "BackingStore.h"

#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

PassOwnPtr<GraphicsContext> BackingStore::createGraphicsContext()
{
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> bitmapContext(AdoptCF, CGBitmapContextCreate(data(), m_size.width(), m_size.height(), 8,  m_size.width() * 4, colorSpace.get(), 
                                                                         kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

    // We want the origin to be in the top left corner so flip the backing store context.
    CGContextTranslateCTM(bitmapContext.get(), 0, m_size.height());
    CGContextScaleCTM(bitmapContext.get(), 1, -1);

    return adoptPtr(new GraphicsContext(bitmapContext.get()));
}

void BackingStore::paint(WebCore::GraphicsContext* context, const WebCore::IntRect& clipRect)
{
    // FIXME: Honor the clip rect!
    OwnPtr<GraphicsContext> sourceContext(createGraphicsContext());

    // FIXME: This creates an extra copy.
    RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(sourceContext->platformContext()));

    CGContextRef cgContext = context->platformContext();
    
    CGContextSaveGState(cgContext);
    CGContextDrawImage(context->platformContext(), CGRectMake(0, 0, CGImageGetWidth(image.get()), CGImageGetHeight(image.get())), image.get());
    CGContextRestoreGState(cgContext);
}
        
} // namespace WebKit
