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

#import "config.h"
#import "BackingStore.h"

#import "CGUtilities.h"
#import "ShareableBitmap.h"
#import "UpdateInfo.h"
#import "WebPageProxy.h"
#import <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

void BackingStore::paint(PlatformGraphicsContext context, const IntRect& rect)
{
    if (m_cgLayer) {
        CGContextSaveGState(context);
        CGContextClipToRect(context, rect);

        CGContextScaleCTM(context, 1, -1);
        CGContextDrawLayerAtPoint(context, CGPointMake(0, -m_size.height()), m_cgLayer.get());

        CGContextRestoreGState(context);
        return;
    }

    ASSERT(m_bitmapContext);
    paintBitmapContext(context, m_bitmapContext.get(), rect.location(), rect, m_deviceScaleFactor);
}

CGContextRef BackingStore::backingStoreContext()
{
    if (m_cgLayer)
        return CGLayerGetContext(m_cgLayer.get());

    // Try to create a layer.
    if (CGContextRef containingWindowContext = m_webPageProxy->containingWindowGraphicsContext()) {
        m_cgLayer.adoptCF(CGLayerCreateWithContext(containingWindowContext, NSSizeToCGSize(m_size), 0));
        CGContextRef layerContext = CGLayerGetContext(m_cgLayer.get());
        
        CGContextSetBlendMode(layerContext, kCGBlendModeCopy);

        // We want the origin to be in the top left corner so flip the backing store context.
        CGContextTranslateCTM(layerContext, 0, m_size.height());
        CGContextScaleCTM(layerContext, 1, -1);

        if (m_bitmapContext) {
            // Paint the contents of the bitmap into the layer context.
            paintBitmapContext(layerContext, m_bitmapContext.get(), CGPointZero, CGRectMake(0, 0, m_size.width(), m_size.height()), m_deviceScaleFactor);
            m_bitmapContext = nullptr;
        }

        return layerContext;
    }

    if (!m_bitmapContext) {
        RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());

        IntSize scaledSize(m_size); 
        scaledSize.scale(m_deviceScaleFactor); 
        m_bitmapContext.adoptCF(CGBitmapContextCreate(0, scaledSize.width(), scaledSize.height(), 8, scaledSize.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

        CGContextSetBlendMode(m_bitmapContext.get(), kCGBlendModeCopy);

        CGContextScaleCTM(m_bitmapContext.get(), m_deviceScaleFactor, m_deviceScaleFactor); 

        // We want the origin to be in the top left corner so flip the backing store context.
        CGContextTranslateCTM(m_bitmapContext.get(), 0, m_size.height());
        CGContextScaleCTM(m_bitmapContext.get(), 1, -1);
    }

    return m_bitmapContext.get();
}

void BackingStore::incorporateUpdate(ShareableBitmap* bitmap, const UpdateInfo& updateInfo)
{
    CGContextRef context = backingStoreContext();

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();

    GraphicsContext graphicsContext(context);

    // Paint all update rects.
    for (size_t i = 0; i < updateInfo.updateRects.size(); ++i) {
        IntRect updateRect = updateInfo.updateRects[i];
        IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());

        bitmap->paint(graphicsContext, updateInfo.deviceScaleFactor, updateRect.location(), srcRect);
    }
}

void BackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    if (scrollOffset.isZero())
        return;

    if (m_cgLayer) {
        CGContextRef layerContext = CGLayerGetContext(m_cgLayer.get());

        // Scroll the layer by painting it into itself with the given offset.
        CGContextSaveGState(layerContext);
        CGContextClipToRect(layerContext, scrollRect);
        CGContextScaleCTM(layerContext, 1, -1);
        CGContextDrawLayerAtPoint(layerContext, CGPointMake(scrollOffset.width(), -m_size.height() - scrollOffset.height()), m_cgLayer.get());
        CGContextRestoreGState(layerContext);

        return;
    }

    ASSERT(m_bitmapContext);

    CGContextSaveGState(m_bitmapContext.get());
    CGContextClipToRect(m_bitmapContext.get(), scrollRect);
    CGPoint destination = CGPointMake(scrollRect.x() + scrollOffset.width(), scrollRect.y() + scrollOffset.height());
    paintBitmapContext(m_bitmapContext.get(), m_bitmapContext.get(), destination, scrollRect, m_deviceScaleFactor);
    CGContextRestoreGState(m_bitmapContext.get());
}

} // namespace WebKit
