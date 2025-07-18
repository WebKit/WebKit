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
#import "PlatformCALayerRemote.h"
#import "RemoteImageBufferSetProxy.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "SwapBuffersDisplayRequirement.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ImageBufferPixelFormat.h>
#import <WebCore/PlatformCALayerClient.h>
#import <wtf/Scope.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerWithInProcessRenderingBackingStore);

using namespace WebCore;

void RemoteLayerWithInProcessRenderingBackingStore::Buffer::discard()
{
    imageBuffer = nullptr;
}

bool RemoteLayerWithInProcessRenderingBackingStore::hasFrontBuffer() const
{
    return m_contentsBufferHandle || !!m_bufferSet.m_frontBuffer;
}

bool RemoteLayerWithInProcessRenderingBackingStore::frontBufferMayBeVolatile() const
{
    RefPtr frontBuffer = m_bufferSet.m_frontBuffer;
    return frontBuffer && frontBuffer->volatilityState() == WebCore::VolatilityState::Volatile;
}

void RemoteLayerWithInProcessRenderingBackingStore::clearBackingStore()
{
    m_bufferSet.clearBuffers();
    RemoteLayerBackingStore::clearBackingStore();
}

static std::optional<ImageBufferBackendHandle> handleFromBuffer(ImageBuffer& buffer)
{
    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(buffer.toBackendSharing());
    return sharing ? sharing->takeBackendHandle(SharedMemory::Protection::ReadOnly) : std::nullopt;
}

std::optional<ImageBufferBackendHandle> RemoteLayerWithInProcessRenderingBackingStore::frontBufferHandle() const
{
    if (RefPtr protectedBuffer = m_bufferSet.m_frontBuffer)
        return handleFromBuffer(*protectedBuffer);
    return std::nullopt;
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<DynamicContentScalingDisplayList> RemoteLayerWithInProcessRenderingBackingStore::displayListHandle() const
{
    if (auto list = m_layer->owner()->platformCALayerDynamicContentScalingDisplayList(m_layer.ptr()))
        return list;
    if (RefPtr frontBuffer = m_bufferSet.m_frontBuffer)
        return frontBuffer->dynamicContentScalingDisplayList();
    return std::nullopt;
}

DynamicContentScalingResourceCache RemoteLayerWithInProcessRenderingBackingStore::ensureDynamicContentScalingResourceCache()
{
    if (!m_dynamicContentScalingResourceCache)
        m_dynamicContentScalingResourceCache = DynamicContentScalingResourceCache::create();
    return m_dynamicContentScalingResourceCache;
}
#endif

void RemoteLayerWithInProcessRenderingBackingStore::createContextAndPaintContents()
{
    RefPtr frontBuffer = m_bufferSet.m_frontBuffer;
    if (!frontBuffer) {
        ASSERT(m_layer->owner()->platformCALayerDelegatesDisplay(m_layer.ptr()));
        return;
    }

    GraphicsContext& context = frontBuffer->context();
    GraphicsContextStateSaver outerSaver(context);
    WebCore::FloatRect layerBounds { { }, m_parameters.size };

    m_bufferSet.prepareBufferForDisplay(layerBounds, m_dirtyRegion, m_paintingRects, drawingRequiresClearedPixels());
    drawInContext(frontBuffer->context());
}

class ImageBufferBackingStoreFlusher final : public ThreadSafeImageBufferSetFlusher {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ImageBufferBackingStoreFlusher);
    WTF_MAKE_NONCOPYABLE(ImageBufferBackingStoreFlusher);
public:
    static std::unique_ptr<ImageBufferBackingStoreFlusher> create(std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> imageBufferFlusher)
    {
        return std::unique_ptr<ImageBufferBackingStoreFlusher> { new ImageBufferBackingStoreFlusher(WTFMove(imageBufferFlusher)) };
    }

    bool flushAndCollectHandles(HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&) final
    {
        m_imageBufferFlusher->flush();
        return true;
    }

private:
    ImageBufferBackingStoreFlusher(std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> imageBufferFlusher)
        : m_imageBufferFlusher(WTFMove(imageBufferFlusher))
    {
    }
    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> m_imageBufferFlusher;
};

std::unique_ptr<ThreadSafeImageBufferSetFlusher> RemoteLayerWithInProcessRenderingBackingStore::createFlusher(ThreadSafeImageBufferSetFlusher::FlushType flushType)
{
    if (flushType == ThreadSafeImageBufferSetFlusher::FlushType::BackendHandlesOnly)
        return nullptr;
    RefPtr frontBuffer = m_bufferSet.m_frontBuffer;
    frontBuffer->flushDrawingContextAsync();
    return ImageBufferBackingStoreFlusher::create(frontBuffer->createFlusher());
}

bool RemoteLayerWithInProcessRenderingBackingStore::setBufferVolatile(RefPtr<WebCore::ImageBuffer>& buffer, bool forcePurge)
{
    if (!buffer || buffer->volatilityState() == VolatilityState::Volatile)
        return true;

    if (forcePurge) {
        buffer->setVolatileAndPurgeForTesting();
        return true;
    }
    buffer->releaseGraphicsContext();
    return buffer->setVolatile();
}

SetNonVolatileResult RemoteLayerWithInProcessRenderingBackingStore::setBufferNonVolatile(Buffer& buffer)
{
    RefPtr imageBuffer = buffer.imageBuffer;
    if (!imageBuffer)
        return SetNonVolatileResult::Valid; // Not really valid but the caller only checked the Empty state.

    if (imageBuffer->volatilityState() == VolatilityState::NonVolatile)
        return SetNonVolatileResult::Valid;

    return imageBuffer->setNonVolatile();
}

bool RemoteLayerWithInProcessRenderingBackingStore::setBufferVolatile(BufferType bufferType, bool forcePurge)
{
    if (m_parameters.type != Type::IOSurface)
        return true;

    switch (bufferType) {
    case BufferType::Front:
        return setBufferVolatile(m_bufferSet.m_frontBuffer, forcePurge);
    case BufferType::Back:
        return setBufferVolatile(m_bufferSet.m_backBuffer, forcePurge);
    case BufferType::SecondaryBack:
        return setBufferVolatile(m_bufferSet.m_secondaryBackBuffer, forcePurge);
    }

    return true;
}

template<typename ImageBufferType>
static RefPtr<ImageBuffer> allocateBufferInternal(RemoteLayerBackingStore::Type type, const WebCore::FloatSize& logicalSize, WebCore::RenderingPurpose purpose, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::ImageBufferPixelFormat pixelFormat, WebCore::ImageBufferCreationContext& creationContext)
{
    switch (type) {
    case RemoteLayerBackingStore::Type::IOSurface:
        return WebCore::ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    case RemoteLayerBackingStore::Type::Bitmap:
        return WebCore::ImageBuffer::create<ImageBufferShareableBitmapBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    }
}

RefPtr<WebCore::ImageBuffer> RemoteLayerWithInProcessRenderingBackingStore::allocateBuffer()
{
    ImageBufferCreationContext creationContext;
    creationContext.surfacePool = WebCore::IOSurfacePool::sharedPoolSingleton();

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_parameters.includeDisplayList == WebCore::IncludeDynamicContentScalingDisplayList::Yes) {
        creationContext.dynamicContentScalingResourceCache = ensureDynamicContentScalingResourceCache();
        return allocateBufferInternal<DynamicContentScalingBifurcatedImageBuffer>(type(), size(), RenderingPurpose::LayerBacking, scale(), colorSpace(), pixelFormat(), creationContext);
    }
#endif

    return allocateBufferInternal<ImageBuffer>(type(), size(), RenderingPurpose::LayerBacking, scale(), colorSpace(), pixelFormat(), creationContext);
}

void RemoteLayerWithInProcessRenderingBackingStore::ensureFrontBuffer()
{
    if (m_bufferSet.m_frontBuffer)
        return;

    m_bufferSet.m_frontBuffer = allocateBuffer();
    m_bufferSet.m_frontBufferIsCleared = true;
}

void RemoteLayerWithInProcessRenderingBackingStore::prepareToDisplay()
{
    ASSERT(!m_frontBufferFlushers.size());

    RefPtr collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return;
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " prepareToDisplay()");

    m_contentsBufferHandle = std::nullopt;
    auto displayRequirement = m_bufferSet.swapBuffersForDisplay(hasEmptyDirtyRegion(), supportsPartialRepaint());
    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsNoDisplay)
        return;

    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsFullDisplay)
        setNeedsDisplay();

    dirtyRepaintCounterIfNecessary();
    ensureFrontBuffer();
}

void RemoteLayerWithInProcessRenderingBackingStore::encodeBufferAndBackendInfos(IPC::Encoder& encoder) const
{
    auto encodeBuffer = [&](const RefPtr<WebCore::ImageBuffer>& buffer) {
        if (buffer) {
            encoder << std::optional { BufferAndBackendInfo::fromImageBuffer(*buffer) };
            return;
        }

        encoder << std::optional<BufferAndBackendInfo>();
    };

    encodeBuffer(m_bufferSet.m_frontBuffer);
    encodeBuffer(m_bufferSet.m_backBuffer);
    encodeBuffer(m_bufferSet.m_secondaryBackBuffer);
}

void RemoteLayerWithInProcessRenderingBackingStore::dump(WTF::TextStream& ts) const
{
    ts.dumpProperty("front buffer"_s, m_bufferSet.m_frontBuffer);
    ts.dumpProperty("back buffer"_s, m_bufferSet.m_backBuffer);
    ts.dumpProperty("secondaryBack buffer"_s, m_bufferSet.m_secondaryBackBuffer);
    ts.dumpProperty("is opaque"_s, isOpaque());
}

} // namespace WebKit
