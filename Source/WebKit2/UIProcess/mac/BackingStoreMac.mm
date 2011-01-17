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

#include "BackingStore.h"

#include "CGUtilities.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

void BackingStore::paint(PlatformGraphicsContext context, const IntRect& rect)
{
    ASSERT(m_bitmapContext);

    paintBitmapContext(context, m_bitmapContext.get(), rect.location(), rect);
}

void BackingStore::incorporateUpdate(const UpdateInfo& updateInfo)
{
    ASSERT(m_size == updateInfo.viewSize);

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(updateInfo.updateRectBounds.size(), updateInfo.bitmapHandle);
    if (!bitmap)
        return;

    if (!m_bitmapContext) {
        RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
        
        m_bitmapContext.adoptCF(CGBitmapContextCreate(0, m_size.width(), m_size.height(), 8, m_size.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
        
        // We want the origin to be in the top left corner so flip the backing store context.
        CGContextTranslateCTM(m_bitmapContext.get(), 0, m_size.height());
        CGContextScaleCTM(m_bitmapContext.get(), 1, -1);
    }

    scroll(updateInfo.scrollRect, updateInfo.scrollDelta);

    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();

    GraphicsContext graphicsContext(m_bitmapContext.get());

    // Paint all update rects.
    for (size_t i = 0; i < updateInfo.updateRects.size(); ++i) {
        IntRect updateRect = updateInfo.updateRects[i];
        IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());

        bitmap->paint(graphicsContext, updateRect.location(), srcRect);
    }
}

void BackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (scrollDelta.isZero())
        return;

    CGContextSaveGState(m_bitmapContext.get());

    CGContextClipToRect(m_bitmapContext.get(), scrollRect);

    CGPoint destination = CGPointMake(scrollRect.x() + scrollDelta.width(), scrollRect.y() + scrollDelta.height());
    paintBitmapContext(m_bitmapContext.get(), m_bitmapContext.get(), destination, scrollRect);

    CGContextRestoreGState(m_bitmapContext.get());
}

} // namespace WebKit
