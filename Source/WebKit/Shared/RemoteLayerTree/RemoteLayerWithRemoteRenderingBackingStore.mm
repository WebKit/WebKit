/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerWithRemoteRenderingBackingStore.h"

#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteRenderingBackendProxy.h"
#import "SwapBuffersDisplayRequirement.h"

namespace WebKit {

using namespace WebCore;

// Called after buffer swapping in the GPU process.
void RemoteLayerWithRemoteRenderingBackingStore::applySwappedBuffers(RefPtr<ImageBuffer>&& front, RefPtr<ImageBuffer>&& back, RefPtr<ImageBuffer>&& secondaryBack, SwapBuffersDisplayRequirement displayRequirement)
{
    m_contentsBufferHandle = std::nullopt;

    if (front != m_backBuffer.imageBuffer || back != m_frontBuffer.imageBuffer || displayRequirement != SwapBuffersDisplayRequirement::NeedsNormalDisplay)
        m_previouslyPaintedRect = std::nullopt;

    m_frontBuffer.imageBuffer = WTFMove(front);
    m_backBuffer.imageBuffer = WTFMove(back);
    m_secondaryBackBuffer.imageBuffer = WTFMove(secondaryBack);

    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsNoDisplay)
        return;

    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsFullDisplay)
        setNeedsDisplay();

    dirtyRepaintCounterIfNecessary();
    ensureFrontBuffer();
}

RefPtr<WebCore::ImageBuffer> RemoteLayerWithRemoteRenderingBackingStore::allocateBuffer() const
{
    auto* collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    OptionSet<WebCore::ImageBufferOptions> options;
    if (type() == RemoteLayerBackingStore::Type::IOSurface)
        options.add(WebCore::ImageBufferOptions::Accelerated);
    return collection->layerTreeContext().ensureRemoteRenderingBackendProxy().createImageBuffer(size(), WebCore::RenderingPurpose::LayerBacking, scale(), colorSpace(), pixelFormat(), options);
}

} // namespace WebKit

