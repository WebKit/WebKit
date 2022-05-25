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
#import "MachPort.h"
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
#import <WebCore/Model.h>
#import <WebCore/PlatformCALayerClient.h>
#import <WebCore/WebLayer.h>
#import <mach/mach_port.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
#import <WebKitAdditions/CGDisplayListImageBufferAdditions.h>
#endif

namespace WebKit {

RemoteLayerBackingStore::RemoteLayerBackingStore(PlatformCALayerRemote* layer)
    : m_layer(layer)
    , m_isOpaque(false)
    , m_lastDisplayTime(-MonotonicTime::infinity())
{
    if (!m_layer)
        return;
    if (RemoteLayerTreeContext* context = m_layer->context())
        context->backingStoreWasCreated(*this);
}

RemoteLayerBackingStore::~RemoteLayerBackingStore()
{
    clearBackingStore();

    if (!m_layer)
        return;

    if (RemoteLayerTreeContext* context = m_layer->context())
        context->backingStoreWillBeDestroyed(*this);
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
    } else if (m_frontBuffer.imageBuffer) {
        switch (m_type) {
        case Type::IOSurface:
            if (auto* backend = m_frontBuffer.imageBuffer->ensureBackendCreated()) {
                if (m_frontBuffer.imageBuffer->canMapBackingStore())
                    handle = static_cast<AcceleratedImageBufferShareableMappedBackend&>(*backend).createImageBufferBackendHandle();
                else
                    handle = static_cast<AcceleratedImageBufferShareableBackend&>(*backend).createImageBufferBackendHandle();
            }
            break;
        case Type::Bitmap:
            if (auto* backend = m_frontBuffer.imageBuffer->ensureBackendCreated())
                handle = static_cast<UnacceleratedImageBufferShareableBackend&>(*backend).createImageBufferBackendHandle();
            break;
        }
    }

    encoder << handle;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    std::optional<ImageBufferBackendHandle> displayListHandle;
    if (m_frontBuffer.displayListImageBuffer) {
        if (auto* backend = m_frontBuffer.displayListImageBuffer->ensureBackendCreated())
            displayListHandle = static_cast<CGDisplayListImageBufferBackend&>(*backend).createImageBufferBackendHandle();
    }

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

void RemoteLayerBackingStore::swapToValidFrontBuffer()
{
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

    if (m_frontBuffer.imageBuffer) {
        setBufferVolatility(BufferType::Front, false);
        return;
    }

    bool shouldUseRemoteRendering = WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM);

    switch (m_type) {
    case Type::IOSurface:
        if (shouldUseRemoteRendering)
            m_frontBuffer.imageBuffer = m_layer->context()->ensureRemoteRenderingBackendProxy().createImageBuffer(m_size, WebCore::RenderingMode::Accelerated, m_scale, WebCore::DestinationColorSpace::SRGB(), pixelFormat());
        else
            m_frontBuffer.imageBuffer = WebCore::ConcreteImageBuffer<AcceleratedImageBufferShareableMappedBackend>::create(m_size, m_scale, WebCore::DestinationColorSpace::SRGB(), pixelFormat(), nullptr);
        break;
    case Type::Bitmap:
        if (shouldUseRemoteRendering)
            m_frontBuffer.imageBuffer = m_layer->context()->ensureRemoteRenderingBackendProxy().createImageBuffer(m_size, WebCore::RenderingMode::Unaccelerated, m_scale, WebCore::DestinationColorSpace::SRGB(), pixelFormat());
        else
            m_frontBuffer.imageBuffer = WebCore::ConcreteImageBuffer<UnacceleratedImageBufferShareableBackend>::create(m_size, m_scale, WebCore::DestinationColorSpace::SRGB(), pixelFormat(), nullptr);
        break;
    }

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_includeDisplayList == IncludeDisplayList::Yes)
        m_frontBuffer.displayListImageBuffer = WebCore::ConcreteImageBuffer<CGDisplayListImageBufferBackend>::create(m_size, m_scale, WebCore::DestinationColorSpace::SRGB(), pixelFormat(), nullptr);
#endif
}

bool RemoteLayerBackingStore::supportsPartialRepaint()
{
    // FIXME: Find a way to support partial repaint for backing store that
    // includes a display list without allowing unbounded memory growth.
    return m_includeDisplayList == IncludeDisplayList::No;
}

void RemoteLayerBackingStore::setContents(WTF::MachSendRight&& contents)
{
    m_contentsBufferHandle = WTFMove(contents);
    m_dirtyRegion = WebCore::Region();
    m_paintingRects.clear();
}

bool RemoteLayerBackingStore::display()
{
    ASSERT(!m_frontBufferFlushers.size());

    m_lastDisplayTime = MonotonicTime::now();

    bool needToEncodeBackingStore = false;
    if (RemoteLayerTreeContext* context = m_layer->context())
        needToEncodeBackingStore = context->backingStoreWillBeDisplayed(*this);

    if (m_layer->owner()->platformCALayerDelegatesDisplay(m_layer)) {
        m_layer->owner()->platformCALayerLayerDisplay(m_layer);
        m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);
        return true;
    }

    // Make the previous front buffer non-volatile early, so that we can dirty the whole layer if it comes back empty.
    setBufferVolatility(BufferType::Front, false);

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty())
        return needToEncodeBackingStore;

    WebCore::IntRect layerBounds(WebCore::IntPoint(), WebCore::expandedIntSize(m_size));
    if (!hasFrontBuffer() || !supportsPartialRepaint())
        m_dirtyRegion.unite(layerBounds);

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        WebCore::IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }

    swapToValidFrontBuffer();
    if (!m_frontBuffer.imageBuffer)
        return true;


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

    m_dirtyRegion = WebCore::Region();
    m_paintingRects.clear();

    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);

    m_frontBuffer.imageBuffer->flushDrawingContextAsync();

    m_frontBufferFlushers.append(m_frontBuffer.imageBuffer->createFlusher());
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    if (m_includeDisplayList == IncludeDisplayList::Yes)
        m_frontBufferFlushers.append(m_frontBuffer.displayListImageBuffer->createFlusher());
#endif

    return true;
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
        , [&] (IPC::SharedBufferReference& buffer) {
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
        }
        auto data = std::get<IPC::SharedBufferReference>(*m_displayListBufferHandle).buffer()->createCFData();
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

bool RemoteLayerBackingStore::setBufferVolatility(BufferType bufferType, bool isVolatile)
{
    if (m_type != Type::IOSurface)
        return true;

    // Return value is true if we succeeded in making volatile.
    auto makeVolatile = [] (Buffer& buffer) -> bool {
        if (!buffer.imageBuffer || buffer.isVolatile)
            return true;

        buffer.imageBuffer->releaseGraphicsContext();

        if (!buffer.imageBuffer->isInUse()) {
            buffer.imageBuffer->setVolatile(true);
            buffer.isVolatile = true;
            return true;
        }
    
        return false;
    };

    // Return value is true if we need to repaint.
    auto makeNonVolatile = [] (Buffer& buffer) -> bool {
        if (!buffer.imageBuffer || !buffer.isVolatile)
            return false;

        auto previousState = buffer.imageBuffer->setVolatile(false);
        buffer.isVolatile = false;

        return previousState == WebCore::VolatilityState::Empty;
    };

    switch (bufferType) {
    case BufferType::Front:
        if (isVolatile)
            return makeVolatile(m_frontBuffer);
        
        // Becoming non-volatile and the front buffer was purged, so we need to repaint.
        if (makeNonVolatile(m_frontBuffer))
            setNeedsDisplay();
        break;
    case BufferType::Back:
        if (isVolatile)
            return makeVolatile(m_backBuffer);
    
        makeNonVolatile(m_backBuffer);
        break;
    case BufferType::SecondaryBack:
        if (isVolatile)
            return makeVolatile(m_secondaryBackBuffer);
    
        makeNonVolatile(m_secondaryBackBuffer);
        break;
    }
    return true;
}

void RemoteLayerBackingStore::Buffer::discard()
{
    isVolatile = false;
    if (imageBuffer)
        imageBuffer->releaseBufferToPool();
    imageBuffer = nullptr;
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    displayListImageBuffer = nullptr;
#endif
}

} // namespace WebKit
