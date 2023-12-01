/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#import "RemoteLayerBackingStore.h"

#import "ArgumentCoders.h"
#import "DynamicContentScalingImageBufferBackend.h"
#import "ImageBufferBackendHandleSharing.h"
#import "Logging.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeLayers.h"
#import "RemoteLayerTreeNode.h"
#import "ShareableBitmap.h"
#import "SwapBuffersDisplayRequirement.h"
#import "WebCoreArgumentCoders.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/BifurcatedGraphicsContext.h>
#import <WebCore/DynamicContentScalingTypes.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ImageBuffer.h>
#import <WebCore/PlatformCALayerClient.h>
#import <WebCore/PlatformCALayerDelegatedContents.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebLayer.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/FastMalloc.h>
#import <wtf/Noncopyable.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/TextStream.h>

namespace WebKit {

using namespace WebCore;

namespace {

class DelegatedContentsFenceFlusher final : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DelegatedContentsFenceFlusher);
public:
    static std::unique_ptr<DelegatedContentsFenceFlusher> create(Ref<PlatformCALayerDelegatedContentsFence> fence)
    {
        return std::unique_ptr<DelegatedContentsFenceFlusher> { new DelegatedContentsFenceFlusher(WTFMove(fence)) };
    }

    void flush() final
    {
        m_fence->waitFor(delegatedContentsFinishedTimeout);
    }

private:
    DelegatedContentsFenceFlusher(Ref<PlatformCALayerDelegatedContentsFence> fence)
        : m_fence(WTFMove(fence))
    {
    }
    Ref<PlatformCALayerDelegatedContentsFence> m_fence;
};

}

RemoteLayerBackingStore::RemoteLayerBackingStore(PlatformCALayerRemote* layer)
    : m_layer(layer)
    , m_lastDisplayTime(-MonotonicTime::infinity())
{
    if (!m_layer)
        return;

    if (auto* collection = backingStoreCollection())
        collection->backingStoreWasCreated(*this);
}

RemoteLayerBackingStore::~RemoteLayerBackingStore()
{
    clearBackingStore();

    if (!m_layer)
        return;

    if (auto* collection = backingStoreCollection())
        collection->backingStoreWillBeDestroyed(*this);
}

RemoteLayerBackingStoreCollection* RemoteLayerBackingStore::backingStoreCollection() const
{
    if (auto* context = m_layer->context())
        return &context->backingStoreCollection();

    return nullptr;
}

void RemoteLayerBackingStore::ensureBackingStore(const Parameters& parameters)
{
    if (m_parameters == parameters)
        return;

    m_parameters = parameters;

    if (m_frontBuffer) {
        // If we have a valid backing store, we need to ensure that it gets completely
        // repainted the next time display() is called.
        setNeedsDisplay();
    }

    clearBackingStore();
}

void RemoteLayerBackingStore::clearBackingStore()
{
    m_frontBuffer.discard();
    m_backBuffer.discard();
    m_secondaryBackBuffer.discard();
    m_contentsBufferHandle = std::nullopt;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    m_displayListBuffer = nullptr;
#endif
}

#if !LOG_DISABLED
static bool hasValue(const ImageBufferBackendHandle& backendHandle)
{
    return WTF::switchOn(backendHandle,
        [&] (const ShareableBitmap::Handle& handle) {
            return true;
        },
        [&] (const MachSendRight& machSendRight) {
            return !!machSendRight;
        }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        , [&] (const DynamicContentScalingDisplayList& handle) {
            return !!handle.buffer();
        }
#endif
    );
}
#endif

void RemoteLayerBackingStore::encode(IPC::Encoder& encoder) const
{
    auto handleFromBuffer = [](ImageBuffer& buffer) -> std::optional<ImageBufferBackendHandle> {
        auto* sharing = buffer.toBackendSharing();
        if (is<ImageBufferBackendHandleSharing>(sharing))
            return downcast<ImageBufferBackendHandleSharing>(*sharing).takeBackendHandle();

        return std::nullopt;
    };

    encoder << m_parameters.isOpaque;
    encoder << m_parameters.type;

    // FIXME: For simplicity this should be moved to the end of display() once the buffer handles can be created once
    // and stored in m_bufferHandle. http://webkit.org/b/234169
    std::optional<ImageBufferBackendHandle> handle;
    if (m_contentsBufferHandle) {
        ASSERT(m_parameters.type == Type::IOSurface);
        handle = MachSendRight { *m_contentsBufferHandle };
    } else if (RefPtr protectedBuffer = m_frontBuffer.imageBuffer)
        handle = handleFromBuffer(*protectedBuffer);

    // It would be nice to ASSERT(handle && hasValue(*handle)) here, but when we hit the timeout in RemoteImageBufferProxy::ensureBackendCreated(), we don't have a handle.
#if !LOG_DISABLED
    if (!(handle && hasValue(*handle)))
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " encode - no buffer handle; did ensureBackendCreated() time out?");
#endif

    encoder << WTFMove(handle);

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

    encoder << m_previouslyPaintedRect;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<ImageBufferBackendHandle> displayListHandle;
    if (m_displayListBuffer)
        displayListHandle = handleFromBuffer(*m_displayListBuffer);

    encoder << WTFMove(displayListHandle);
#endif
}

bool RemoteLayerBackingStoreProperties::decode(IPC::Decoder& decoder, RemoteLayerBackingStoreProperties& result)
{
    if (!decoder.decode(result.m_isOpaque))
        return false;

    if (!decoder.decode(result.m_type))
        return false;

    if (!decoder.decode(result.m_bufferHandle))
        return false;

    if (!decoder.decode(result.m_frontBufferInfo))
        return false;

    if (!decoder.decode(result.m_backBufferInfo))
        return false;

    if (!decoder.decode(result.m_secondaryBackBufferInfo))
        return false;

    if (!decoder.decode(result.m_paintedRect))
        return false;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (!decoder.decode(result.m_displayListBufferHandle))
        return false;
#endif

    return true;
}

void RemoteLayerBackingStoreProperties::dump(TextStream& ts) const
{
    auto dumpBuffer = [&](const char* name, const std::optional<BufferAndBackendInfo>& bufferInfo) {
        ts.startGroup();
        ts << name << " ";
        if (bufferInfo)
            ts << bufferInfo->resourceIdentifier << " backend generation " << bufferInfo->backendGeneration;
        else
            ts << "none";
        ts.endGroup();
    };
    dumpBuffer("front buffer", m_frontBufferInfo);
    dumpBuffer("back buffer", m_backBufferInfo);
    dumpBuffer("secondaryBack buffer", m_secondaryBackBufferInfo);

    ts.dumpProperty("is opaque", isOpaque());
    ts.dumpProperty("has buffer handle", !!bufferHandle());
}

bool RemoteLayerBackingStore::layerWillBeDisplayed()
{
    auto* collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    return collection->backingStoreWillBeDisplayed(*this);
}

void RemoteLayerBackingStore::setNeedsDisplay(const IntRect rect)
{
    m_dirtyRegion.unite(intersection(layerBounds(), rect));
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    m_dirtyRegion.unite(layerBounds());
}

WebCore::IntRect RemoteLayerBackingStore::layerBounds() const
{
    return IntRect { { }, expandedIntSize(m_parameters.size) };
}

bool RemoteLayerBackingStore::usesDeepColorBackingStore() const
{
#if HAVE(IOSURFACE_RGB10)
    if (m_parameters.type == Type::IOSurface && m_parameters.deepColor)
        return true;
#endif
    return false;
}

PixelFormat RemoteLayerBackingStore::pixelFormat() const
{
    if (usesDeepColorBackingStore())
        return m_parameters.isOpaque ? PixelFormat::RGB10 : PixelFormat::RGB10A8;

    return m_parameters.isOpaque ? PixelFormat::BGRX8 : PixelFormat::BGRA8;
}

unsigned RemoteLayerBackingStore::bytesPerPixel() const
{
    switch (pixelFormat()) {
    case PixelFormat::RGBA8: return 4;
    case PixelFormat::BGRX8: return 4;
    case PixelFormat::BGRA8: return 4;
    case PixelFormat::RGB10: return 4;
    case PixelFormat::RGB10A8: return 5;
    }
    return 4;
}

SetNonVolatileResult RemoteLayerBackingStore::swapToValidFrontBuffer()
{
    ASSERT(!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));

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

// Called after buffer swapping in the GPU process.
void RemoteLayerBackingStore::applySwappedBuffers(RefPtr<ImageBuffer>&& front, RefPtr<ImageBuffer>&& back, RefPtr<ImageBuffer>&& secondaryBack, SwapBuffersDisplayRequirement displayRequirement)
{
    ASSERT(WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));
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

bool RemoteLayerBackingStore::supportsPartialRepaint() const
{
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    // FIXME: Find a way to support partial repaint for backing store that
    // includes a display list without allowing unbounded memory growth.
    return m_parameters.includeDisplayList == IncludeDisplayList::No;
#else
    return true;
#endif
}

void RemoteLayerBackingStore::setDelegatedContents(const WebCore::PlatformCALayerDelegatedContents& contents)
{
    m_contentsBufferHandle = MachSendRight { contents.surface };
    if (contents.finishedFence)
        m_frontBufferFlushers.append(DelegatedContentsFenceFlusher::create(Ref { *contents.finishedFence }));
    m_contentsRenderingResourceIdentifier = contents.surfaceIdentifier;
    m_dirtyRegion = { };
    m_paintingRects.clear();
}

bool RemoteLayerBackingStore::needsDisplay() const
{
    auto* collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_layer->owner()->platformCALayerDelegatesDisplay(m_layer)) {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " needsDisplay() - delegates display");
        return true;
    }

    auto needsDisplayReason = [&]() {
        if (size().isEmpty())
            return BackingStoreNeedsDisplayReason::None;

        auto frontBuffer = bufferForType(RemoteLayerBackingStore::BufferType::Front);
        if (!frontBuffer)
            return BackingStoreNeedsDisplayReason::NoFrontBuffer;

        if (frontBuffer->volatilityState() == WebCore::VolatilityState::Volatile)
            return BackingStoreNeedsDisplayReason::FrontBufferIsVolatile;

        return hasEmptyDirtyRegion() ? BackingStoreNeedsDisplayReason::None : BackingStoreNeedsDisplayReason::HasDirtyRegion;
    }();

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " size " << size() << " needsDisplay() - needs display reason: " << needsDisplayReason);
    return needsDisplayReason != BackingStoreNeedsDisplayReason::None;
}

bool RemoteLayerBackingStore::performDelegatedLayerDisplay()
{
    auto& layerOwner = *m_layer->owner();
    if (layerOwner.platformCALayerDelegatesDisplay(m_layer)) {
        // This can call back to setContents(), setting m_contentsBufferHandle.
        layerOwner.platformCALayerLayerDisplay(m_layer);
        layerOwner.platformCALayerLayerDidDisplay(m_layer);
        return true;
    }
    
    return false;
}

void RemoteLayerBackingStore::prepareToDisplay()
{
    ASSERT(!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));
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

void RemoteLayerBackingStore::dirtyRepaintCounterIfNecessary()
{
    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }
}

void RemoteLayerBackingStore::ensureFrontBuffer()
{
    if (m_frontBuffer.imageBuffer)
        return;

    auto* collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_frontBuffer.imageBuffer = collection->allocateBufferForBackingStore(*this);
    m_frontBuffer.isCleared = true;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (!m_displayListBuffer && m_parameters.includeDisplayList == IncludeDisplayList::Yes) {
        ImageBufferCreationContext creationContext;
        if (type() == RemoteLayerBackingStore::Type::IOSurface)
            m_displayListBuffer = ImageBuffer::create<DynamicContentScalingAcceleratedImageBufferBackend>(m_parameters.size, m_parameters.scale, colorSpace(), pixelFormat(), RenderingPurpose::DOM, WTFMove(creationContext));
        else
            m_displayListBuffer = ImageBuffer::create<DynamicContentScalingImageBufferBackend>(m_parameters.size, m_parameters.scale, colorSpace(), pixelFormat(), RenderingPurpose::DOM, WTFMove(creationContext));
    }
#endif
}

SwapBuffersDisplayRequirement RemoteLayerBackingStore::prepareBuffers()
{
    ASSERT(!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));
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

void RemoteLayerBackingStore::paintContents()
{
    if (!m_frontBuffer.imageBuffer) {
        ASSERT(m_layer->owner()->platformCALayerDelegatesDisplay(m_layer));
        return;
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " paintContents() - has dirty region " << !hasEmptyDirtyRegion());
    if (hasEmptyDirtyRegion())
        return;

    m_lastDisplayTime = MonotonicTime::now();

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_parameters.includeDisplayList == IncludeDisplayList::Yes) {
        auto& displayListContext = m_displayListBuffer->context();

        BifurcatedGraphicsContext context(m_frontBuffer.imageBuffer->context(), displayListContext);
        drawInContext(context);
        return;
    }
#endif

    GraphicsContext& context = m_frontBuffer.imageBuffer->context();
    drawInContext(context);
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context)
{
    auto markFrontBufferNotCleared = makeScopeExit([&]() {
        m_frontBuffer.isCleared = false;
    });
    GraphicsContextStateSaver stateSaver(context);

    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the total dirty area, we'll repaint each rect separately.
    // Otherwise, repaint the entire bounding box of the dirty region.
    IntRect dirtyBounds = m_dirtyRegion.bounds();

    auto dirtyRects = m_dirtyRegion.rects();
    if (dirtyRects.size() > PlatformCALayer::webLayerMaxRectsToPaint || m_dirtyRegion.totalArea() > PlatformCALayer::webLayerWastedSpaceThreshold * dirtyBounds.width() * dirtyBounds.height()) {
        dirtyRects.clear();
        dirtyRects.append(dirtyBounds);
    }

    // FIXME: find a consistent way to scale and snap dirty and CG clip rects.
    for (const auto& rect : dirtyRects) {
        FloatRect scaledRect(rect);
        scaledRect.scale(m_parameters.scale);
        scaledRect = enclosingIntRect(scaledRect);
        scaledRect.scale(1 / m_parameters.scale);
        m_paintingRects.append(scaledRect);
    }

    IntRect layerBounds = this->layerBounds();
    if (RefPtr imageBuffer = m_backBuffer.imageBuffer; !m_dirtyRegion.contains(layerBounds) && imageBuffer) {
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

    if (m_paintingRects.size() == 1)
        context.clip(m_paintingRects[0]);
    else {
        Path clipPath;
        for (auto rect : m_paintingRects)
            clipPath.addRect(rect);
        context.clipPath(clipPath);
    }

    if (!m_parameters.isOpaque && !m_frontBuffer.isCleared && !m_layer->owner()->platformCALayerShouldPaintUsingCompositeCopy())
        context.clearRect(layerBounds);

#ifndef NDEBUG
    if (m_parameters.isOpaque)
        context.fillRect(layerBounds, SRGBA<uint8_t> { 255, 47, 146 });
#endif

    OptionSet<WebCore::GraphicsLayerPaintBehavior> paintBehavior;
    if (m_layer->context() && m_layer->context()->nextRenderingUpdateRequiresSynchronousImageDecoding())
        paintBehavior.add(GraphicsLayerPaintBehavior::ForceSynchronousImageDecode);
    
    // FIXME: This should be moved to PlatformCALayerRemote for better layering.
    switch (m_layer->layerType()) {
    case PlatformCALayer::LayerType::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer:
        m_layer->owner()->platformCALayerPaintContents(m_layer, context, dirtyBounds, paintBehavior);
        break;
    case PlatformCALayer::LayerType::LayerTypeWebLayer:
    case PlatformCALayer::LayerType::LayerTypeBackdropLayer:
        PlatformCALayer::drawLayerContents(context, m_layer, m_paintingRects, paintBehavior);
        break;
    case PlatformCALayer::LayerType::LayerTypeLayer:
    case PlatformCALayer::LayerType::LayerTypeTransformLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypeRootLayer:
    case PlatformCALayer::LayerType::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer:
    case PlatformCALayer::LayerType::LayerTypeShapeLayer:
    case PlatformCALayer::LayerType::LayerTypeScrollContainerLayer:
#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerType::LayerTypeModelLayer:
#endif
    case PlatformCALayer::LayerType::LayerTypeCustom:
    case PlatformCALayer::LayerType::LayerTypeHost:
        ASSERT_NOT_REACHED();
        break;
    };

    stateSaver.restore();

    m_dirtyRegion = { };
    m_paintingRects.clear();

    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);

    m_frontBuffer.imageBuffer->flushDrawingContextAsync();
    m_previouslyPaintedRect = dirtyBounds;

    m_frontBufferFlushers.append(m_frontBuffer.imageBuffer->createFlusher());
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_parameters.includeDisplayList == IncludeDisplayList::Yes)
        m_frontBufferFlushers.append(m_displayListBuffer->createFlusher());
#endif
}

void RemoteLayerBackingStore::enumerateRectsBeingDrawn(GraphicsContext& context, void (^block)(FloatRect))
{
    CGAffineTransform inverseTransform = CGAffineTransformInvert(context.getCTM());

    // We don't want to un-apply the flipping or contentsScale,
    // because they're not applied to repaint rects.
    inverseTransform = CGAffineTransformScale(inverseTransform, m_parameters.scale, -m_parameters.scale);
    inverseTransform = CGAffineTransformTranslate(inverseTransform, 0, -m_parameters.size.height());

    for (const auto& rect : m_paintingRects) {
        CGRect rectToDraw = CGRectApplyAffineTransform(rect, inverseTransform);
        block(rectToDraw);
    }
}

RetainPtr<id> RemoteLayerBackingStoreProperties::layerContentsBufferFromBackendHandle(ImageBufferBackendHandle&& backendHandle, LayerContentsType contentsType)
{
    RetainPtr<id> contents;
    WTF::switchOn(backendHandle,
        [&] (ShareableBitmap::Handle& handle) {
            if (auto bitmap = ShareableBitmap::create(WTFMove(handle)))
                contents = bridge_id_cast(bitmap->makeCGImageCopy());
        },
        [&] (MachSendRight& machSendRight) {
            switch (contentsType) {
            case RemoteLayerBackingStoreProperties::LayerContentsType::IOSurface: {
                auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight));
                contents = surface ? surface->asLayerContents() : nil;
                break;
            }
            case RemoteLayerBackingStoreProperties::LayerContentsType::CAMachPort:
                contents = bridge_id_cast(adoptCF(CAMachPortCreate(machSendRight.leakSendRight())));
                break;
            case RemoteLayerBackingStoreProperties::LayerContentsType::CachedIOSurface:
                auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight));
                contents = surface ? surface->asCAIOSurfaceLayerContents() : nil;
                break;
            }
        }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        , [&] (DynamicContentScalingDisplayList& handle) {
            ASSERT_NOT_REACHED();
        }
#endif
    );

    return contents;
}

void RemoteLayerBackingStoreProperties::applyBackingStoreToLayer(CALayer *layer, LayerContentsType contentsType, std::optional<WebCore::RenderingResourceIdentifier> asyncContentsIdentifier, bool replayDynamicContentScalingDisplayListsIntoBackingStore)
{
    if (asyncContentsIdentifier && m_frontBufferInfo && *asyncContentsIdentifier >= m_frontBufferInfo->resourceIdentifier)
        return;

    layer.contentsOpaque = m_isOpaque;

    RetainPtr<id> contents;
    // m_bufferHandle can be unset here if IPC with the GPU process timed out.
    if (m_contentsBuffer)
        contents = m_contentsBuffer;
    else if (m_bufferHandle)
        contents = layerContentsBufferFromBackendHandle(WTFMove(*m_bufferHandle), contentsType);

    if (!contents) {
        [layer _web_clearContents];
        return;
    }

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_displayListBufferHandle) {
        ASSERT([layer isKindOfClass:[WKCompositingLayer class]]);
        if (![layer isKindOfClass:[WKCompositingLayer class]])
            return;

        layer.drawsAsynchronously = (m_type == RemoteLayerBackingStore::Type::IOSurface);

        if (!replayDynamicContentScalingDisplayListsIntoBackingStore) {
            [layer setValue:@1 forKeyPath:WKDynamicContentScalingEnabledKey];
            [layer setValue:@1 forKeyPath:WKDynamicContentScalingBifurcationEnabledKey];
            [layer setValue:@(layer.contentsScale) forKeyPath:WKDynamicContentScalingBifurcationScaleKey];
        } else
            layer.opaque = m_isOpaque;
        [(WKCompositingLayer *)layer _setWKContents:contents.get() withDisplayList:WTFMove(std::get<DynamicContentScalingDisplayList>(*m_displayListBufferHandle)) replayForTesting:replayDynamicContentScalingDisplayListsIntoBackingStore];
        return;
    } else
        [layer _web_clearDynamicContentScalingDisplayListIfNeeded];
#else
    UNUSED_PARAM(replayDynamicContentScalingDisplayListsIntoBackingStore);
#endif

    layer.contents = contents.get();
    if ([CALayer instancesRespondToSelector:@selector(contentsDirtyRect)]) {
        if (m_paintedRect) {
            FloatRect painted = *m_paintedRect;
            painted.scale(layer.contentsScale);

            // Most of the time layer.contentsDirtyRect should be the null rect, since CA clears this on every commit,
            // but in some scenarios we don't get a CA commit for every remote layer tree transaction.
            auto existingDirtyRect = layer.contentsDirtyRect;
            if (CGRectIsNull(existingDirtyRect))
                layer.contentsDirtyRect = painted;
            else
                layer.contentsDirtyRect = CGRectUnion(existingDirtyRect, painted);
        }
    }
}

void RemoteLayerBackingStoreProperties::updateCachedBuffers(RemoteLayerTreeNode& node, LayerContentsType contentsType)
{
    ASSERT(!m_contentsBuffer);

    Vector<RemoteLayerTreeNode::CachedContentsBuffer> cachedBuffers = node.takeCachedContentsBuffers();

    if (contentsType != LayerContentsType::CachedIOSurface || !m_frontBufferInfo || !m_bufferHandle || !std::holds_alternative<MachSendRight>(*m_bufferHandle))
        return;

    cachedBuffers.removeAllMatching([&](const RemoteLayerTreeNode::CachedContentsBuffer& current) {
        if (m_frontBufferInfo && *m_frontBufferInfo == current.imageBufferInfo)
            return false;

        if (m_backBufferInfo && *m_backBufferInfo== current.imageBufferInfo)
            return false;

        if (m_secondaryBackBufferInfo && *m_secondaryBackBufferInfo == current.imageBufferInfo)
            return false;

        return true;
    });

    for (auto& current : cachedBuffers) {
        if (m_frontBufferInfo->resourceIdentifier == current.imageBufferInfo.resourceIdentifier) {
            m_contentsBuffer = current.buffer;
            break;
        }
    }

    if (!m_contentsBuffer) {
        m_contentsBuffer = layerContentsBufferFromBackendHandle(WTFMove(*m_bufferHandle), LayerContentsType::CachedIOSurface);
        cachedBuffers.append({ *m_frontBufferInfo, m_contentsBuffer });
    }

    node.setCachedContentsBuffers(WTFMove(cachedBuffers));
}

Vector<std::unique_ptr<ThreadSafeImageBufferFlusher>> RemoteLayerBackingStore::takePendingFlushers()
{
    return std::exchange(m_frontBufferFlushers, { });
}

bool RemoteLayerBackingStore::setBufferVolatile(Buffer& buffer)
{
    if (!buffer.imageBuffer || buffer.imageBuffer->volatilityState() == VolatilityState::Volatile)
        return true;

    buffer.imageBuffer->releaseGraphicsContext();
    return buffer.imageBuffer->setVolatile();
}

SetNonVolatileResult RemoteLayerBackingStore::setBufferNonVolatile(Buffer& buffer)
{
    ASSERT(!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));

    if (!buffer.imageBuffer)
        return SetNonVolatileResult::Valid; // Not really valid but the caller only checked the Empty state.

    if (buffer.imageBuffer->volatilityState() == VolatilityState::NonVolatile)
        return SetNonVolatileResult::Valid;

    return buffer.imageBuffer->setNonVolatile();
}

bool RemoteLayerBackingStore::setBufferVolatile(BufferType bufferType)
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

SetNonVolatileResult RemoteLayerBackingStore::setFrontBufferNonVolatile()
{
    if (m_parameters.type != Type::IOSurface)
        return SetNonVolatileResult::Valid;

    return setBufferNonVolatile(m_frontBuffer);
}

RefPtr<ImageBuffer> RemoteLayerBackingStore::bufferForType(BufferType bufferType) const
{
    switch (bufferType) {
    case BufferType::Front:
        return m_frontBuffer.imageBuffer;
    case BufferType::Back:
        return m_backBuffer.imageBuffer;
    case BufferType::SecondaryBack:
        return m_secondaryBackBuffer.imageBuffer;
    }
    return nullptr;
}

void RemoteLayerBackingStore::Buffer::discard()
{
    imageBuffer = nullptr;
}

TextStream& operator<<(TextStream& ts, const RemoteLayerBackingStore& backingStore)
{
    ts.dumpProperty("front buffer", backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Front));
    ts.dumpProperty("back buffer", backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Back));
    ts.dumpProperty("secondaryBack buffer", backingStore.bufferForType(RemoteLayerBackingStore::BufferType::SecondaryBack));
    ts.dumpProperty("is opaque", backingStore.isOpaque());

    return ts;
}

TextStream& operator<<(TextStream& ts, const RemoteLayerBackingStoreProperties& properties)
{
    properties.dump(ts);
    return ts;
}

TextStream& operator<<(TextStream& ts, SwapBuffersDisplayRequirement displayRequirement)
{
    switch (displayRequirement) {
    case SwapBuffersDisplayRequirement::NeedsFullDisplay: ts << "full display"; break;
    case SwapBuffersDisplayRequirement::NeedsNormalDisplay: ts << "normal display"; break;
    case SwapBuffersDisplayRequirement::NeedsNoDisplay: ts << "no display"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, BackingStoreNeedsDisplayReason reason)
{
    switch (reason) {
    case BackingStoreNeedsDisplayReason::None: ts << "none"; break;
    case BackingStoreNeedsDisplayReason::NoFrontBuffer: ts << "no front buffer"; break;
    case BackingStoreNeedsDisplayReason::FrontBufferIsVolatile: ts << "volatile front buffer"; break;
    case BackingStoreNeedsDisplayReason::FrontBufferHasNoSharingHandle: ts << "no front buffer sharing handle"; break;
    case BackingStoreNeedsDisplayReason::HasDirtyRegion: ts << "has dirty region"; break;
    }

    return ts;
}

} // namespace WebKit
