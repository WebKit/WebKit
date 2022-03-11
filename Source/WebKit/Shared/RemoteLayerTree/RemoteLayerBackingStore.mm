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
#import "PlatformRemoteImageBufferProxy.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeLayers.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/BifurcatedGraphicsContext.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
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

void RemoteLayerBackingStore::ensureBackingStore(Type type, WebCore::FloatSize size, float scale, bool deepColor, bool isOpaque, IncludeDisplayList includeDisplayList)
{
    if (m_type == type && m_size == size && m_scale == scale && m_deepColor == deepColor && m_isOpaque == isOpaque && m_includeDisplayList == includeDisplayList)
        return;

    m_type = type;
    m_size = size;
    m_scale = scale;
    m_deepColor = deepColor;
    m_isOpaque = isOpaque;
    m_includeDisplayList = includeDisplayList;

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
}

void RemoteLayerBackingStore::encode(IPC::Encoder& encoder) const
{
    auto handleFromBuffer = [](WebCore::ImageBuffer& buffer) -> std::optional<ImageBufferBackendHandle> {
        auto* backend = buffer.ensureBackendCreated();
        if (!backend)
            return std::nullopt;

        auto* sharing = backend->toBackendSharing();
        if (is<ImageBufferBackendHandleSharing>(sharing))
            return downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();

        return std::nullopt;
    };

    encoder << m_type;
    encoder << m_size;
    encoder << m_scale;
    encoder << m_isOpaque;
    encoder << m_includeDisplayList;

    // FIXME: For simplicity this should be moved to the end of display() once the buffer handles can be created once
    // and stored in m_bufferHandle. http://webkit.org/b/234169
    std::optional<ImageBufferBackendHandle> handle;
    if (m_contentsBufferHandle) {
        ASSERT(m_type == Type::IOSurface);
        handle = m_contentsBufferHandle;
    } else if (m_frontBuffer.imageBuffer)
        handle = handleFromBuffer(*m_frontBuffer.imageBuffer);

    encoder << handle;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    std::optional<ImageBufferBackendHandle> displayListHandle;
    if (m_frontBuffer.displayListImageBuffer)
        displayListHandle = handleFromBuffer(*m_frontBuffer.displayListImageBuffer);

    encoder << displayListHandle;
#endif
}

bool RemoteLayerBackingStore::decode(IPC::Decoder& decoder, RemoteLayerBackingStore& result)
{
    if (!decoder.decode(result.m_type))
        return false;

    if (!decoder.decode(result.m_size))
        return false;

    if (!decoder.decode(result.m_scale))
        return false;

    if (!decoder.decode(result.m_isOpaque))
        return false;

    if (!decoder.decode(result.m_includeDisplayList))
        return false;

    if (!decoder.decode(result.m_bufferHandle))
        return false;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (!decoder.decode(result.m_displayListBufferHandle))
        return false;
#endif

    return true;
}

void RemoteLayerBackingStore::setNeedsDisplay(const WebCore::IntRect rect)
{
    m_dirtyRegion.unite(rect);
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    setNeedsDisplay(WebCore::IntRect(WebCore::IntPoint(), WebCore::expandedIntSize(m_size)));
}

WebCore::PixelFormat RemoteLayerBackingStore::pixelFormat() const
{
#if HAVE(IOSURFACE_RGB10)
    if (m_type == Type::IOSurface && m_deepColor)
        return m_isOpaque ? WebCore::PixelFormat::RGB10 : WebCore::PixelFormat::RGB10A8;
#endif

    return WebCore::PixelFormat::BGRA8;
}

unsigned RemoteLayerBackingStore::bytesPerPixel() const
{
    switch (pixelFormat()) {
    case WebCore::PixelFormat::RGBA8: return 4;
    case WebCore::PixelFormat::BGRA8: return 4;
    case WebCore::PixelFormat::RGB10: return 4;
    case WebCore::PixelFormat::RGB10A8: return 5;
    }
    return 4;
}

WebCore::SetNonVolatileResult RemoteLayerBackingStore::swapToValidFrontBuffer()
{
    ASSERT(!WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM));

    // Sometimes, we can get two swaps ahead of the render server.
    // If we're using shared IOSurfaces, we must wait to modify
    // a surface until it no longer has outstanding clients.
    if (m_type == Type::IOSurface) {
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

// Called after buffer swapping in the GPU process.
void RemoteLayerBackingStore::applySwappedBuffers(RefPtr<WebCore::ImageBuffer>&& front, RefPtr<WebCore::ImageBuffer>&& back, RefPtr<WebCore::ImageBuffer>&& secondaryBack)
{
    ASSERT(WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM));

    m_frontBuffer.imageBuffer = WTFMove(front);
    m_backBuffer.imageBuffer = WTFMove(back);
    m_secondaryBackBuffer.imageBuffer = WTFMove(secondaryBack);
}

void RemoteLayerBackingStore::swapBuffers()
{
    m_contentsBufferHandle = std::nullopt;

    auto* collection = backingStoreCollection();
    if (!collection)
        return;

    auto result = collection->swapToValidFrontBuffer(*this);
    if (result == WebCore::SetNonVolatileResult::Empty)
        setNeedsDisplay();
    
    if (m_frontBuffer.imageBuffer)
        return;

    m_frontBuffer.imageBuffer = collection->allocateBufferForBackingStore(*this);

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_includeDisplayList == IncludeDisplayList::Yes)
        m_frontBuffer.displayListImageBuffer = WebCore::ConcreteImageBuffer<CGDisplayListImageBufferBackend>::create(m_size, m_scale, WebCore::DestinationColorSpace::SRGB(), pixelFormat(), nullptr);
#endif
}

bool RemoteLayerBackingStore::supportsPartialRepaint() const
{
    // FIXME: Find a way to support partial repaint for backing store that
    // includes a display list without allowing unbounded memory growth.
    return m_includeDisplayList == IncludeDisplayList::No;
}

void RemoteLayerBackingStore::setContents(WTF::MachSendRight&& contents)
{
    m_contentsBufferHandle = WTFMove(contents);
    m_dirtyRegion = { };
    m_paintingRects.clear();
}

bool RemoteLayerBackingStore::display()
{
    ASSERT(!m_frontBufferFlushers.size());

    m_lastDisplayTime = MonotonicTime::now();

    auto* collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    bool needToEncodeBackingStore = collection->backingStoreWillBeDisplayed(*this);

    auto& layerOwner = *m_layer->owner();
    if (layerOwner.platformCALayerDelegatesDisplay(m_layer)) {
        // This can call back to setContents(), setting m_contentsBufferHandle.
        layerOwner.platformCALayerLayerDisplay(m_layer);
        layerOwner.platformCALayerLayerDidDisplay(m_layer);
        return true;
    }

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerBackingStore::display()");

    // Make the previous front buffer non-volatile early, so that we can dirty the whole layer if it comes back empty.
    if (collection->makeFrontBufferNonVolatile(*this) == WebCore::SetNonVolatileResult::Empty)
        setNeedsDisplay();

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty()) {
        LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << " no dirty region");
        return needToEncodeBackingStore;
    }

    if (!hasFrontBuffer() || !supportsPartialRepaint())
        setNeedsDisplay();

    if (layerOwner.platformCALayerShowRepaintCounter(m_layer)) {
        WebCore::IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }

    swapBuffers();

    paintContents();
    return true;
}

void RemoteLayerBackingStore::paintContents()
{
    if (!m_frontBuffer.imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty())
        return;

    if (m_includeDisplayList == IncludeDisplayList::Yes) {
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        auto& displayListContext = m_frontBuffer.displayListImageBuffer->context();

        // FIXME: Remove this when <rdar://problem/80487697> is fixed.
        static std::optional<bool> needsMissingFlipWorkaround;
        WebCore::GraphicsContextStateSaver workaroundStateSaver(displayListContext, false);
        if (!needsMissingFlipWorkaround) {
            id defaultValue = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitNeedsWorkaroundFor80487697"];
            needsMissingFlipWorkaround = defaultValue ? [defaultValue boolValue] : true;
        }
        if (needsMissingFlipWorkaround.value()) {
            workaroundStateSaver.save();
            displayListContext.scale(WebCore::FloatSize(1, -1));
            displayListContext.translate(0, -m_size.height());
        }

        WebCore::BifurcatedGraphicsContext context(m_frontBuffer.imageBuffer->context(), displayListContext);
#else
        WebCore::GraphicsContext& context = m_frontBuffer.imageBuffer->context();
#endif
        drawInContext(context);
    } else {
        WebCore::GraphicsContext& context = m_frontBuffer.imageBuffer->context();
        drawInContext(context);    
    }

    m_dirtyRegion = { };
    m_paintingRects.clear();

    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);

    m_frontBuffer.imageBuffer->flushDrawingContextAsync();

    m_frontBufferFlushers.append(m_frontBuffer.imageBuffer->createFlusher());
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_includeDisplayList == IncludeDisplayList::Yes)
        m_frontBufferFlushers.append(m_frontBuffer.displayListImageBuffer->createFlusher());
#endif
}

void RemoteLayerBackingStore::drawInContext(WebCore::GraphicsContext& context)
{
    WebCore::GraphicsContextStateSaver stateSaver(context);

    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the total dirty area, we'll repaint each rect separately.
    // Otherwise, repaint the entire bounding box of the dirty region.
    WebCore::IntRect dirtyBounds = m_dirtyRegion.bounds();

    auto dirtyRects = m_dirtyRegion.rects();
    if (dirtyRects.size() > WebCore::PlatformCALayer::webLayerMaxRectsToPaint || m_dirtyRegion.totalArea() > WebCore::PlatformCALayer::webLayerWastedSpaceThreshold * dirtyBounds.width() * dirtyBounds.height()) {
        dirtyRects.clear();
        dirtyRects.append(dirtyBounds);
    }

    // FIXME: find a consistent way to scale and snap dirty and CG clip rects.
    for (const auto& rect : dirtyRects) {
        WebCore::FloatRect scaledRect(rect);
        scaledRect.scale(m_scale);
        scaledRect = WebCore::enclosingIntRect(scaledRect);
        scaledRect.scale(1 / m_scale);
        m_paintingRects.append(scaledRect);
    }

    WebCore::IntRect layerBounds(WebCore::IntPoint(), WebCore::expandedIntSize(m_size));
    if (!m_dirtyRegion.contains(layerBounds)) {
        ASSERT(m_backBuffer.imageBuffer);
        context.drawImageBuffer(*m_backBuffer.imageBuffer, { 0, 0 }, { WebCore::CompositeOperator::Copy });
    }

    if (m_paintingRects.size() == 1)
        context.clip(m_paintingRects[0]);
    else {
        WebCore::Path clipPath;
        for (auto rect : m_paintingRects)
            clipPath.addRect(rect);
        context.clipPath(clipPath);
    }

    if (!m_isOpaque)
        context.clearRect(layerBounds);

#ifndef NDEBUG
    if (m_isOpaque)
        context.fillRect(layerBounds, WebCore::SRGBA<uint8_t> { 255, 47, 146 });
#endif

    // FIXME: Clarify that WebCore::GraphicsLayerPaintSnapshotting is just about image decoding.
    auto flags = m_layer->context() && m_layer->context()->nextRenderingUpdateRequiresSynchronousImageDecoding() ? WebCore::GraphicsLayerPaintSnapshotting : WebCore::GraphicsLayerPaintNormal;
    
    // FIXME: This should be moved to PlatformCALayerRemote for better layering.
    switch (m_layer->layerType()) {
    case WebCore::PlatformCALayer::LayerTypeSimpleLayer:
    case WebCore::PlatformCALayer::LayerTypeTiledBackingTileLayer:
        m_layer->owner()->platformCALayerPaintContents(m_layer, context, dirtyBounds, flags);
        break;
    case WebCore::PlatformCALayer::LayerTypeWebLayer:
    case WebCore::PlatformCALayer::LayerTypeBackdropLayer:
        WebCore::PlatformCALayer::drawLayerContents(context, m_layer, m_paintingRects, flags);
        break;
    case WebCore::PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
    case WebCore::PlatformCALayer::LayerTypeLightSystemBackdropLayer:
        // FIXME: These have a more complicated layer hierarchy. We need to paint into
        // a child layer in order to see the rendered results.
        WebCore::PlatformCALayer::drawLayerContents(context, m_layer, m_paintingRects, flags);
        break;
    case WebCore::PlatformCALayer::LayerTypeLayer:
    case WebCore::PlatformCALayer::LayerTypeTransformLayer:
    case WebCore::PlatformCALayer::LayerTypeTiledBackingLayer:
    case WebCore::PlatformCALayer::LayerTypePageTiledBackingLayer:
    case WebCore::PlatformCALayer::LayerTypeRootLayer:
    case WebCore::PlatformCALayer::LayerTypeAVPlayerLayer:
    case WebCore::PlatformCALayer::LayerTypeContentsProvidedLayer:
    case WebCore::PlatformCALayer::LayerTypeShapeLayer:
    case WebCore::PlatformCALayer::LayerTypeScrollContainerLayer:
#if ENABLE(MODEL_ELEMENT)
    case WebCore::PlatformCALayer::LayerTypeModelLayer:
#endif
    case WebCore::PlatformCALayer::LayerTypeCustom:
        ASSERT_NOT_REACHED();
        break;
    };
}

void RemoteLayerBackingStore::enumerateRectsBeingDrawn(WebCore::GraphicsContext& context, void (^block)(WebCore::FloatRect))
{
    CGAffineTransform inverseTransform = CGAffineTransformInvert(context.getCTM());

    // We don't want to un-apply the flipping or contentsScale,
    // because they're not applied to repaint rects.
    inverseTransform = CGAffineTransformScale(inverseTransform, m_scale, -m_scale);
    inverseTransform = CGAffineTransformTranslate(inverseTransform, 0, -m_size.height());

    for (const auto& rect : m_paintingRects) {
        CGRect rectToDraw = CGRectApplyAffineTransform(rect, inverseTransform);
        block(rectToDraw);
    }
}

void RemoteLayerBackingStore::applyBackingStoreToLayer(CALayer *layer, LayerContentsType contentsType, bool replayCGDisplayListsIntoBackingStore)
{
    ASSERT(m_bufferHandle);
    layer.contentsOpaque = m_isOpaque;

    RetainPtr<id> contents;
    WTF::switchOn(*m_bufferHandle,
        [&] (ShareableBitmap::Handle& handle) {
            ASSERT(m_type == Type::Bitmap);
            contents = bridge_id_cast(ShareableBitmap::create(handle)->makeCGImageCopy());
        },
        [&] (MachSendRight& machSendRight) {
            ASSERT(m_type == Type::IOSurface);
            switch (contentsType) {
            case RemoteLayerBackingStore::LayerContentsType::IOSurface: {
                auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight), WebCore::DestinationColorSpace::SRGB());
                contents = surface ? surface->asLayerContents() : nil;
                break;
            }
            case RemoteLayerBackingStore::LayerContentsType::CAMachPort:
                contents = bridge_id_cast(adoptCF(CAMachPortCreate(machSendRight.leakSendRight())));
                break;
            }
        }
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        , [&] (IPC::SharedBufferCopy& buffer) {
            ASSERT_NOT_REACHED();
        }
#endif
    );

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_displayListBufferHandle) {
        ASSERT([layer isKindOfClass:[WKCompositingLayer class]]);
        if (![layer isKindOfClass:[WKCompositingLayer class]])
            return;

        if (!replayCGDisplayListsIntoBackingStore) {
            [layer setValue:@1 forKeyPath:WKCGDisplayListEnabledKey];
            [layer setValue:@1 forKeyPath:WKCGDisplayListBifurcationEnabledKey];
        } else
            layer.opaque = m_isOpaque;
        auto data = std::get<IPC::SharedBufferCopy>(*m_displayListBufferHandle).buffer()->createCFData();
        [(WKCompositingLayer *)layer _setWKContents:contents.get() withDisplayList:data.get() replayForTesting:replayCGDisplayListsIntoBackingStore];
        return;
    }
#else
    UNUSED_PARAM(replayCGDisplayListsIntoBackingStore);
#endif

    layer.contents = contents.get();
}

Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> RemoteLayerBackingStore::takePendingFlushers()
{
    return std::exchange(m_frontBufferFlushers, { });
}

bool RemoteLayerBackingStore::setBufferVolatile(Buffer& buffer)
{
    if (!buffer.imageBuffer || buffer.imageBuffer->volatilityState() == WebCore::VolatilityState::Volatile)
        return true;

    buffer.imageBuffer->releaseGraphicsContext();
    return buffer.imageBuffer->setVolatile();
}

WebCore::SetNonVolatileResult RemoteLayerBackingStore::setBufferNonVolatile(Buffer& buffer)
{
    ASSERT(!WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM));

    if (!buffer.imageBuffer)
        return WebCore::SetNonVolatileResult::Valid; // Not really valid but the caller only checked the Empty state.

    if (buffer.imageBuffer->volatilityState() == WebCore::VolatilityState::NonVolatile)
        return WebCore::SetNonVolatileResult::Valid;

    return buffer.imageBuffer->setNonVolatile();
}

bool RemoteLayerBackingStore::setBufferVolatile(BufferType bufferType)
{
    if (m_type != Type::IOSurface)
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

WebCore::SetNonVolatileResult RemoteLayerBackingStore::setFrontBufferNonVolatile()
{
    if (m_type != Type::IOSurface)
        return WebCore::SetNonVolatileResult::Valid;

    return setBufferNonVolatile(m_frontBuffer);
}

RefPtr<WebCore::ImageBuffer> RemoteLayerBackingStore::bufferForType(BufferType bufferType) const
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
    if (imageBuffer)
        imageBuffer->releaseBufferToPool();
    imageBuffer = nullptr;
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    displayListImageBuffer = nullptr;
#endif
}

} // namespace WebKit
