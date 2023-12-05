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
#import "RemoteLayerWithInProcessRenderingBackingStore.h"

#import "ImageBufferShareableBitmapBackend.h"
#import "ImageBufferShareableMappedIOSurfaceBackend.h"
#import "Logging.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "SwapBuffersDisplayRequirement.h"
#import <WebCore/IOSurfacePool.h>

namespace WebKit {

using namespace WebCore;

SwapBuffersDisplayRequirement RemoteLayerWithInProcessRenderingBackingStore::prepareBuffers()
{
    m_contentsBufferHandle = std::nullopt;

    auto displayRequirement = SwapBuffersDisplayRequirement::NeedsNoDisplay;

    // Make the previous front buffer non-volatile early, so that we can dirty the whole layer if it comes back empty.
    if (!hasFrontBuffer() || setFrontBufferNonVolatile() == SetNonVolatileResult::Empty)
        displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;
    else if (!hasEmptyDirtyRegion())
        displayRequirement = SwapBuffersDisplayRequirement::NeedsNormalDisplay;

    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsNoDisplay)
        return displayRequirement;

    if (!supportsPartialRepaint())
        displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;

    auto result = swapToValidFrontBuffer();
    if (!hasFrontBuffer() || result == SetNonVolatileResult::Empty)
        displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " prepareBuffers() - " << displayRequirement);
    return displayRequirement;
}

bool RemoteLayerWithInProcessRenderingBackingStore::setBufferVolatile(Buffer& buffer)
{
    if (!buffer.imageBuffer || buffer.imageBuffer->volatilityState() == VolatilityState::Volatile)
        return true;

    buffer.imageBuffer->releaseGraphicsContext();
    return buffer.imageBuffer->setVolatile();
}

SetNonVolatileResult RemoteLayerWithInProcessRenderingBackingStore::setBufferNonVolatile(Buffer& buffer)
{
    if (!buffer.imageBuffer)
        return SetNonVolatileResult::Valid; // Not really valid but the caller only checked the Empty state.

    if (buffer.imageBuffer->volatilityState() == VolatilityState::NonVolatile)
        return SetNonVolatileResult::Valid;

    return buffer.imageBuffer->setNonVolatile();
}

bool RemoteLayerWithInProcessRenderingBackingStore::setBufferVolatile(BufferType bufferType)
{
    if (m_parameters.type != Type::IOSurface)
        return true;

    switch (bufferType) {
    case BufferType::Front:
        return setBufferVolatile(m_frontBuffer);

    case BufferType::Back:
        return setBufferVolatile(m_backBuffer);

    case BufferType::SecondaryBack:
        return setBufferVolatile(m_secondaryBackBuffer);
    }

    return true;
}

SetNonVolatileResult RemoteLayerWithInProcessRenderingBackingStore::setFrontBufferNonVolatile()
{
    if (m_parameters.type != Type::IOSurface)
        return SetNonVolatileResult::Valid;

    return setBufferNonVolatile(m_frontBuffer);
}

RefPtr<WebCore::ImageBuffer> RemoteLayerWithInProcessRenderingBackingStore::allocateBuffer() const
{
    switch (type()) {
    case RemoteLayerBackingStore::Type::IOSurface: {
        ImageBufferCreationContext creationContext;
        creationContext.surfacePool = &WebCore::IOSurfacePool::sharedPool();
        return WebCore::ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBackend>(size(), scale(), colorSpace(), pixelFormat(), WebCore::RenderingPurpose::LayerBacking, creationContext);
    }
    case RemoteLayerBackingStore::Type::Bitmap:
        return WebCore::ImageBuffer::create<ImageBufferShareableBitmapBackend>(size(), scale(), colorSpace(), pixelFormat(), WebCore::RenderingPurpose::LayerBacking, { });
    }
}

void RemoteLayerWithInProcessRenderingBackingStore::prepareToDisplay()
{
    ASSERT(!m_frontBufferFlushers.size());

    auto* collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT(needsDisplay());

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " prepareToDisplay()");

    if (performDelegatedLayerDisplay())
        return;

    auto displayRequirement = prepareBuffers();
    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsNoDisplay)
        return;

    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsFullDisplay)
        setNeedsDisplay();

    dirtyRepaintCounterIfNecessary();
    ensureFrontBuffer();
}

SetNonVolatileResult RemoteLayerWithInProcessRenderingBackingStore::swapToValidFrontBuffer()
{
    // Sometimes, we can get two swaps ahead of the render server.
    // If we're using shared IOSurfaces, we must wait to modify
    // a surface until it no longer has outstanding clients.
    if (m_parameters.type == Type::IOSurface) {
        if (!m_backBuffer.imageBuffer || m_backBuffer.imageBuffer->isInUse()) {
            std::swap(m_backBuffer, m_secondaryBackBuffer);

            // When pulling the secondary back buffer out of hibernation (to become
            // the new front buffer), if it is somehow still in use (e.g. we got
            // three swaps ahead of the render server), just give up and discard it.
            if (m_backBuffer.imageBuffer && m_backBuffer.imageBuffer->isInUse())
                m_backBuffer.discard();
        }
    }

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_displayListBuffer)
        m_displayListBuffer->releaseGraphicsContext();
#endif

    m_contentsBufferHandle = std::nullopt;
    std::swap(m_frontBuffer, m_backBuffer);
    return setBufferNonVolatile(m_frontBuffer);
}

} // namespace WebKit
