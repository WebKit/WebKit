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

#import "DynamicContentScalingBifurcatedImageBuffer.h"
#import "ImageBufferShareableBitmapBackend.h"
#import "ImageBufferShareableMappedIOSurfaceBackend.h"
#import "Logging.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "SwapBuffersDisplayRequirement.h"
#import <WebCore/IOSurfacePool.h>
#import <WebCore/PlatformCALayerClient.h>
#import <wtf/Scope.h>

namespace WebKit {

using namespace WebCore;

void RemoteLayerWithInProcessRenderingBackingStore::Buffer::discard()
{
    imageBuffer = nullptr;
}

bool RemoteLayerWithInProcessRenderingBackingStore::hasFrontBuffer() const
{
    return m_contentsBufferHandle || !!m_frontBuffer.imageBuffer;
}

bool RemoteLayerWithInProcessRenderingBackingStore::frontBufferMayBeVolatile() const
{
    if (!m_frontBuffer)
        return false;
    return m_frontBuffer.imageBuffer->volatilityState() == WebCore::VolatilityState::Volatile;
}


void RemoteLayerWithInProcessRenderingBackingStore::clearBackingStore()
{
    m_frontBuffer.discard();
    m_backBuffer.discard();
    m_secondaryBackBuffer.discard();
    m_contentsBufferHandle = std::nullopt;
}

static std::optional<ImageBufferBackendHandle> handleFromBuffer(ImageBuffer& buffer)
{
    auto* sharing = buffer.toBackendSharing();
    if (is<ImageBufferBackendHandleSharing>(sharing))
        return downcast<ImageBufferBackendHandleSharing>(*sharing).takeBackendHandle();

    return std::nullopt;
}

std::optional<ImageBufferBackendHandle> RemoteLayerWithInProcessRenderingBackingStore::frontBufferHandle() const
{
    if (RefPtr protectedBuffer = m_frontBuffer.imageBuffer)
        return handleFromBuffer(*protectedBuffer);
    return std::nullopt;
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<ImageBufferBackendHandle> RemoteLayerWithInProcessRenderingBackingStore::displayListHandle() const
{
    if (RefPtr frontBuffer = m_frontBuffer.imageBuffer)
        return frontBuffer->dynamicContentScalingDisplayList();
    return std::nullopt;
}
#endif

void RemoteLayerWithInProcessRenderingBackingStore::createContextAndPaintContents()
{
    if (!m_frontBuffer.imageBuffer) {
        ASSERT(m_layer->owner()->platformCALayerDelegatesDisplay(m_layer));
        return;
    }

    auto markFrontBufferNotCleared = makeScopeExit([&]() {
        m_frontBuffer.isCleared = false;
    });

    auto clipAndDrawInContext = [&](GraphicsContext& context) {
        if (m_paintingRects.size() == 1)
            context.clip(m_paintingRects[0]);
        else {
            Path clipPath;
            for (auto rect : m_paintingRects)
                clipPath.addRect(rect);
            context.clipPath(clipPath);
        }

        if (drawingRequiresClearedPixels() && !m_frontBuffer.isCleared)
            context.clearRect(layerBounds());

        drawInContext(context);
    };

    GraphicsContext& context = m_frontBuffer.imageBuffer->context();

    // We never need to copy forward when using display list drawing, since we don't do partial repaint.
    // FIXME: Copy forward logic is duplicated in RemoteImageBufferSet, find a good place to share.
    if (RefPtr imageBuffer = m_backBuffer.imageBuffer; !m_dirtyRegion.contains(layerBounds()) && imageBuffer) {
        if (!m_previouslyPaintedRect)
            context.drawImageBuffer(*imageBuffer, { 0, 0 }, { CompositeOperator::Copy });
        else {
            Region copyRegion(*m_previouslyPaintedRect);
            copyRegion.subtract(m_dirtyRegion);
            IntRect copyRect = copyRegion.bounds();
            if (!copyRect.isEmpty())
                context.drawImageBuffer(*imageBuffer, copyRect, copyRect, { CompositeOperator::Copy });
        }
    }

    clipAndDrawInContext(context);
}

Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> RemoteLayerWithInProcessRenderingBackingStore::createFlushers()
{
    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers;

    m_frontBuffer.imageBuffer->flushDrawingContextAsync();
    flushers.append(m_frontBuffer.imageBuffer->createFlusher());

    return flushers;
}


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

template<typename ImageBufferType>
static RefPtr<ImageBuffer> allocateBufferInternal(RemoteLayerBackingStore::Type type, const WebCore::FloatSize& logicalSize, WebCore::RenderingPurpose purpose, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, WebCore::ImageBufferCreationContext& creationContext)
{
    switch (type) {
    case RemoteLayerBackingStore::Type::IOSurface:
        return WebCore::ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    case RemoteLayerBackingStore::Type::Bitmap:
        return WebCore::ImageBuffer::create<ImageBufferShareableBitmapBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    }
}

RefPtr<WebCore::ImageBuffer> RemoteLayerWithInProcessRenderingBackingStore::allocateBuffer() const
{
    auto purpose = m_layer->containsBitmapOnly() ? WebCore::RenderingPurpose::BitmapOnlyLayerBacking : WebCore::RenderingPurpose::LayerBacking;
    ImageBufferCreationContext creationContext;
    creationContext.surfacePool = &WebCore::IOSurfacePool::sharedPool();

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_parameters.includeDisplayList == IncludeDisplayList::Yes)
        return allocateBufferInternal<DynamicContentScalingBifurcatedImageBuffer>(type(), size(), purpose, scale(), colorSpace(), pixelFormat(), creationContext);
#endif

    return allocateBufferInternal<ImageBuffer>(type(), size(), purpose, scale(), colorSpace(), pixelFormat(), creationContext);
}

void RemoteLayerWithInProcessRenderingBackingStore::ensureFrontBuffer()
{
    if (m_frontBuffer.imageBuffer)
        return;

    m_frontBuffer.imageBuffer = allocateBuffer();
    m_frontBuffer.isCleared = true;
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

    m_contentsBufferHandle = std::nullopt;
    std::swap(m_frontBuffer, m_backBuffer);
    return setBufferNonVolatile(m_frontBuffer);
}

void RemoteLayerWithInProcessRenderingBackingStore::encodeBufferAndBackendInfos(IPC::Encoder& encoder) const
{
    auto encodeBuffer = [&](const Buffer& buffer) {
        if (buffer.imageBuffer) {
            encoder << std::optional { BufferAndBackendInfo::fromImageBuffer(*buffer.imageBuffer) };
            return;
        }

        encoder << std::optional<BufferAndBackendInfo>();
    };

    encodeBuffer(m_frontBuffer);
    encodeBuffer(m_backBuffer);
    encodeBuffer(m_secondaryBackBuffer);
}

void RemoteLayerWithInProcessRenderingBackingStore::dump(WTF::TextStream& ts) const
{
    ts.dumpProperty("front buffer", m_frontBuffer.imageBuffer);
    ts.dumpProperty("back buffer", m_backBuffer.imageBuffer);
    ts.dumpProperty("secondaryBack buffer", m_secondaryBackBuffer.imageBuffer);
    ts.dumpProperty("is opaque", isOpaque());
}

} // namespace WebKit
