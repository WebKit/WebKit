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

#import "config.h"
#import "ChunkedUpdateDrawingAreaProxy.h"

#import "DrawingAreaMessageKinds.h"
#import "DrawingAreaProxyMessageKinds.h"
#import "UpdateChunk.h"
#import "WKAPICast.h"
#import "WKView.h"
#import "WebPageProxy.h"

using namespace WebCore;

namespace WebKit {

WebPageProxy* ChunkedUpdateDrawingAreaProxy::page()
{
    return toImpl([m_webView pageRef]);
}

void ChunkedUpdateDrawingAreaProxy::ensureBackingStore()
{
    if (m_bitmapContext)
        return;

    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    const IntSize& viewSize = size();
    m_bitmapContext.adoptCF(CGBitmapContextCreate(0, viewSize.width(), viewSize.height(), 8, viewSize.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
    
    // Flip the bitmap context coordinate system.
    CGContextTranslateCTM(m_bitmapContext.get(), 0, viewSize.height());
    CGContextScaleCTM(m_bitmapContext.get(), 1, -1);
}

void ChunkedUpdateDrawingAreaProxy::invalidateBackingStore()
{
    m_bitmapContext = 0;
}

bool ChunkedUpdateDrawingAreaProxy::platformPaint(const IntRect& rect, CGContextRef context)
{
    if (!m_bitmapContext)
        return false;

    CGContextSaveGState(context);

    // Use the copy blend mode when drawing a background.
    if (page()->drawsBackground())
        CGContextSetBlendMode(context, kCGBlendModeCopy);

    // Flip the destination.
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -m_size.height());

    RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(m_bitmapContext.get()));
    CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image.get()), CGImageGetHeight(image.get())), image.get());

    CGContextRestoreGState(context);
    return true;
}

void ChunkedUpdateDrawingAreaProxy::drawUpdateChunkIntoBackingStore(UpdateChunk* updateChunk)
{
    ensureBackingStore();

    RetainPtr<CGImageRef> image(updateChunk->createImage());
    IntRect updateChunkRect = updateChunk->rect();

    CGContextSaveGState(m_bitmapContext.get());

    // Use the copy blend mode to replace existing content.
    CGContextSetBlendMode(m_bitmapContext.get(), kCGBlendModeCopy);

    // Flip the destination.
    CGContextScaleCTM(m_bitmapContext.get(), 1, -1);
    CGContextTranslateCTM(m_bitmapContext.get(), 0, -(updateChunkRect.y() + updateChunkRect.maxY()));

    CGContextDrawImage(m_bitmapContext.get(), updateChunkRect, image.get());

    CGContextRestoreGState(m_bitmapContext.get());

    [m_webView setNeedsDisplayInRect:NSRectFromCGRect(updateChunkRect)];
}

} // namespace WebKit
