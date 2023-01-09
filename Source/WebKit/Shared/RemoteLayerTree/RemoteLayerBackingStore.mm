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
#import "CGDisplayListImageBufferBackend.h"
#import "Logging.h"
#import "PlatformCALayerRemote.h"
#import "PlatformImageBufferShareableBackend.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeLayers.h"
#import "ShareableBitmap.h"
#import "SwapBuffersDisplayRequirement.h"
#import "WebCoreArgumentCoders.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/BifurcatedGraphicsContext.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ImageBuffer.h>
#import <WebCore/PlatformCALayerClient.h>
#import <WebCore/WebLayer.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/TextStream.h>

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
#import <WebKitAdditions/CGDisplayListImageBufferAdditions.h>
#endif

namespace WebKit {

using namespace WebCore;

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
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    m_displayListBuffer = nullptr;
#endif
}

void RemoteLayerBackingStore::encode(IPC::Encoder& encoder) const
{
    auto handleFromBuffer = [](ImageBuffer& buffer) -> std::optional<ImageBufferBackendHandle> {
        auto* backend = buffer.ensureBackendCreated();
        if (!backend)
            return std::nullopt;

        auto* sharing = backend->toBackendSharing();
        if (is<ImageBufferBackendHandleSharing>(sharing))
            return downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();

        return std::nullopt;
    };

    encoder << m_parameters;

    // FIXME: For simplicity this should be moved to the end of display() once the buffer handles can be created once
    // and stored in m_bufferHandle. http://webkit.org/b/234169
    std::optional<ImageBufferBackendHandle> handle;
    if (m_contentsBufferHandle) {
        ASSERT(m_parameters.type == Type::IOSurface);
        handle = m_contentsBufferHandle;
    } else if (m_frontBuffer.imageBuffer)
        handle = handleFromBuffer(*m_frontBuffer.imageBuffer);

    encoder << handle;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    std::optional<ImageBufferBackendHandle> displayListHandle;
    if (m_displayListBuffer)
        displayListHandle = handleFromBuffer(*m_displayListBuffer);

    encoder << displayListHandle;
#endif
}

bool RemoteLayerBackingStore::decode(IPC::Decoder& decoder, RemoteLayerBackingStore& result)
{
    if (!decoder.decode(result.m_parameters))
        return false;

    if (!decoder.decode(result.m_bufferHandle))
        return false;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (!decoder.decode(result.m_displayListBufferHandle))
        return false;
#endif

    return true;
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
    m_dirtyRegion.unite(rect);
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    setNeedsDisplay(IntRect(IntPoint(), expandedIntSize(m_parameters.size)));
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

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_displayListBuffer)
        m_displayListBuffer->clearContents();
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
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    // FIXME: Find a way to support partial repaint for backing store that
    // includes a display list without allowing unbounded memory growth.
    return m_parameters.includeDisplayList == IncludeDisplayList::No;
#else
    return true;
#endif
}

void RemoteLayerBackingStore::setContents(WTF::MachSendRight&& contents)
{
    m_contentsBufferHandle = WTFMove(contents);
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
        LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " needsDisplay() - delegates display");
        return true;
    }

    bool needsDisplay = collection->backingStoreNeedsDisplay(*this);
    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " size " << size() << " needsDisplay() - needsDisplay " << needsDisplay);
    return needsDisplay;
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

    ASSERT(collection->backingStoreNeedsDisplay(*this));

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " prepareToDisplay()");

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

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (!m_displayListBuffer && m_parameters.includeDisplayList == IncludeDisplayList::Yes) {
        ImageBufferCreationContext creationContext;
        creationContext.useCGDisplayListImageCache = m_parameters.useCGDisplayListImageCache;
        // FIXME: This should use colorSpace(), not hardcode sRGB.
        m_displayListBuffer = ImageBuffer::create<CGDisplayListImageBufferBackend>(m_parameters.size, m_parameters.scale, DestinationColorSpace::SRGB(), pixelFormat(), RenderingPurpose::DOM, WTFMove(creationContext));
    }
#endif
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, SwapBuffersDisplayRequirement result)
{
    switch (result) {
    case SwapBuffersDisplayRequirement::NeedsFullDisplay: ts << "full display"; break;
    case SwapBuffersDisplayRequirement::NeedsNormalDisplay: ts << "normal display"; break;
    case SwapBuffersDisplayRequirement::NeedsNoDisplay: ts << "no display"; break;
    }
    return ts;
}
#endif

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

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " prepareBuffers() - " << displayRequirement);
    return displayRequirement;
}

void RemoteLayerBackingStore::paintContents()
{
    if (!m_frontBuffer.imageBuffer) {
        ASSERT(m_layer->owner()->platformCALayerDelegatesDisplay(m_layer));
        return;
    }

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " paintContents() - has dirty region " << !hasEmptyDirtyRegion());
    if (hasEmptyDirtyRegion())
        return;

    m_lastDisplayTime = MonotonicTime::now();

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_parameters.includeDisplayList == IncludeDisplayList::Yes) {
        BifurcatedGraphicsContext context(m_frontBuffer.imageBuffer->context(), m_displayListBuffer->context());
        drawInContext(context);
        return;
    }
#endif

    GraphicsContext& context = m_frontBuffer.imageBuffer->context();
    drawInContext(context);
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context)
{
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

    IntRect layerBounds(IntPoint(), expandedIntSize(m_parameters.size));
    if (!m_dirtyRegion.contains(layerBounds) && m_backBuffer.imageBuffer)
        context.drawImageBuffer(*m_backBuffer.imageBuffer, { 0, 0 }, { CompositeOperator::Copy });

    if (m_paintingRects.size() == 1)
        context.clip(m_paintingRects[0]);
    else {
        Path clipPath;
        for (auto rect : m_paintingRects)
            clipPath.addRect(rect);
        context.clipPath(clipPath);
    }

    if (!m_parameters.isOpaque)
        context.clearRect(layerBounds);

#ifndef NDEBUG
    if (m_parameters.isOpaque)
        context.fillRect(layerBounds, SRGBA<uint8_t> { 255, 47, 146 });
#endif

    // FIXME: Clarify that GraphicsLayerPaintSnapshotting is just about image decoding.
    auto flags = m_layer->context() && m_layer->context()->nextRenderingUpdateRequiresSynchronousImageDecoding() ? GraphicsLayerPaintSnapshotting : GraphicsLayerPaintNormal;
    
    // FIXME: This should be moved to PlatformCALayerRemote for better layering.
    switch (m_layer->layerType()) {
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        m_layer->owner()->platformCALayerPaintContents(m_layer, context, dirtyBounds, flags);
        break;
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeBackdropLayer:
        PlatformCALayer::drawLayerContents(context, m_layer, m_paintingRects, flags);
        break;
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeTransformLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
    case PlatformCALayer::LayerTypeShapeLayer:
    case PlatformCALayer::LayerTypeScrollContainerLayer:
#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerTypeModelLayer:
#endif
    case PlatformCALayer::LayerTypeCustom:
        ASSERT_NOT_REACHED();
        break;
    };

    stateSaver.restore();

    m_dirtyRegion = { };
    m_paintingRects.clear();

    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);

    m_frontBuffer.imageBuffer->flushDrawingContextAsync();

    m_frontBufferFlushers.append(m_frontBuffer.imageBuffer->createFlusher());
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
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

RetainPtr<id> RemoteLayerBackingStore::layerContentsBufferFromBackendHandle(ImageBufferBackendHandle&& backendHandle, LayerContentsType contentsType)
{
    RetainPtr<id> contents;
    WTF::switchOn(backendHandle,
        [&] (ShareableBitmapHandle& handle) {
            if (auto bitmap = ShareableBitmap::create(handle))
                contents = bridge_id_cast(bitmap->makeCGImageCopy());
        },
        [&] (MachSendRight& machSendRight) {
            switch (contentsType) {
            case RemoteLayerBackingStore::LayerContentsType::IOSurface: {
                auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight));
                contents = surface ? surface->asLayerContents() : nil;
                break;
            }
            case RemoteLayerBackingStore::LayerContentsType::CAMachPort:
                contents = bridge_id_cast(adoptCF(CAMachPortCreate(machSendRight.leakSendRight())));
                break;
            }
        }
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        , [&] (CGDisplayList& handle) {
            ASSERT_NOT_REACHED();
        }
#endif
    );

    return contents;
}

void RemoteLayerBackingStore::applyBackingStoreToLayer(CALayer *layer, LayerContentsType contentsType, bool replayCGDisplayListsIntoBackingStore)
{
    layer.contentsOpaque = m_parameters.isOpaque;

    RetainPtr<id> contents;
    // m_bufferHandle can be unset here if IPC with the GPU process timed out.
    if (m_bufferHandle)
        contents = layerContentsBufferFromBackendHandle(WTFMove(*m_bufferHandle), contentsType);

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_displayListBufferHandle) {
        ASSERT([layer isKindOfClass:[WKCompositingLayer class]]);
        if (![layer isKindOfClass:[WKCompositingLayer class]])
            return;

        if (!replayCGDisplayListsIntoBackingStore) {
            [layer setValue:@1 forKeyPath:WKCGDisplayListEnabledKey];
            [layer setValue:@1 forKeyPath:WKCGDisplayListBifurcationEnabledKey];
        } else
            layer.opaque = m_parameters.isOpaque;
        [(WKCompositingLayer *)layer _setWKContents:contents.get() withDisplayList:WTFMove(std::get<CGDisplayList>(*m_displayListBufferHandle)) replayForTesting:replayCGDisplayListsIntoBackingStore];
        return;
    }
#else
    UNUSED_PARAM(replayCGDisplayListsIntoBackingStore);
#endif

    layer.contents = contents.get();
}

Vector<std::unique_ptr<ThreadSafeImageBufferFlusher>> RemoteLayerBackingStore::takePendingFlushers()
{
    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " takePendingFlushers()");
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

void RemoteLayerBackingStore::Parameters::encode(IPC::Encoder& encoder) const
{
    encoder << type;
    encoder << size;
    encoder << scale;
    encoder << deepColor;
    encoder << isOpaque;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    encoder << includeDisplayList;
    encoder << useCGDisplayListImageCache;
#endif
}

bool RemoteLayerBackingStore::Parameters::decode(IPC::Decoder& decoder, RemoteLayerBackingStore::Parameters& result)
{
    if (!decoder.decode(result.type))
        return false;

    if (!decoder.decode(result.size))
        return false;

    if (!decoder.decode(result.scale))
        return false;

    if (!decoder.decode(result.deepColor))
        return false;

    if (!decoder.decode(result.isOpaque))
        return false;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (!decoder.decode(result.includeDisplayList))
        return false;

    if (!decoder.decode(result.useCGDisplayListImageCache))
        return false;
#endif

    return true;
}

} // namespace WebKit
