 /*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#import "MachPort.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PlatformCALayerClient.h>
#import <WebCore/WebLayer.h>
#import <mach/mach_port.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

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

void RemoteLayerBackingStore::ensureBackingStore(WebCore::FloatSize size, float scale, bool acceleratesDrawing, bool deepColor, bool isOpaque)
{
    if (m_size == size && m_scale == scale && m_deepColor == deepColor && m_acceleratesDrawing == acceleratesDrawing && m_isOpaque == isOpaque)
        return;

    m_size = size;
    m_scale = scale;
    m_acceleratesDrawing = acceleratesDrawing;
    m_deepColor = deepColor;
    m_isOpaque = isOpaque;

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
}

void RemoteLayerBackingStore::encode(IPC::Encoder& encoder) const
{
    encoder << m_size;
    encoder << m_scale;
    encoder << m_acceleratesDrawing;
    encoder << m_isOpaque;

    if (m_acceleratesDrawing) {
        if (m_frontBuffer.surface)
            encoder << m_frontBuffer.surface->createSendRight();
        else
            encoder << WTF::MachSendRight();
        return;
    }

    ASSERT(!m_acceleratesDrawing);

    ShareableBitmap::Handle handle;
    m_frontBuffer.bitmap->createHandle(handle);
    encoder << handle;
}

bool RemoteLayerBackingStore::decode(IPC::Decoder& decoder, RemoteLayerBackingStore& result)
{
    if (!decoder.decode(result.m_size))
        return false;

    if (!decoder.decode(result.m_scale))
        return false;

    if (!decoder.decode(result.m_acceleratesDrawing))
        return false;

    if (!decoder.decode(result.m_isOpaque))
        return false;

    if (result.m_acceleratesDrawing) {
        MachSendRight sendRight;
        if (!decoder.decode(sendRight))
            return false;
        result.m_frontBufferSendRight = WTFMove(sendRight);
        return true;
    }

    ASSERT(!result.m_acceleratesDrawing);

    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    result.m_frontBuffer.bitmap = ShareableBitmap::create(handle);

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

WebCore::IntSize RemoteLayerBackingStore::backingStoreSize() const
{
    WebCore::FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    return roundedIntSize(scaledSize);
}

unsigned RemoteLayerBackingStore::bytesPerPixel() const
{
    switch (surfaceBufferFormat()) {
    case WebCore::IOSurface::Format::RGBA: return 4;
    case WebCore::IOSurface::Format::YUV422: return 2;
#if HAVE(IOSURFACE_RGB10)
    case WebCore::IOSurface::Format::RGB10: return 4;
    case WebCore::IOSurface::Format::RGB10A8: return 5;
#endif
    }
    return 4;
}

void RemoteLayerBackingStore::swapToValidFrontBuffer()
{
    WebCore::IntSize expandedScaledSize = backingStoreSize();

    if (m_acceleratesDrawing) {
        if (!m_backBuffer.surface || m_backBuffer.surface->isInUse()) {
            std::swap(m_backBuffer, m_secondaryBackBuffer);
            if (m_backBuffer.surface && m_backBuffer.surface->isInUse())
                m_backBuffer.discard();
        }

        std::swap(m_frontBuffer, m_backBuffer);

        if (!m_frontBuffer.surface)
            m_frontBuffer.surface = WebCore::IOSurface::create(expandedScaledSize, WebCore::sRGBColorSpaceRef(), surfaceBufferFormat());

        setBufferVolatility(BufferType::Front, false);
        return;
    }

    ASSERT(!m_acceleratesDrawing);
    std::swap(m_frontBuffer, m_backBuffer);

    if (!m_frontBuffer.bitmap) {
        ShareableBitmap::Configuration bitmapConfiguration;
        bitmapConfiguration.isOpaque = m_isOpaque;
        m_frontBuffer.bitmap = ShareableBitmap::createShareable(expandedScaledSize, bitmapConfiguration);
    }
}

bool RemoteLayerBackingStore::display()
{
    ASSERT(!m_frontContextPendingFlush);

    m_lastDisplayTime = MonotonicTime::now();

    bool needToEncodeBackingStore = false;
    if (RemoteLayerTreeContext* context = m_layer->context())
        needToEncodeBackingStore = context->backingStoreWillBeDisplayed(*this);

    // Make the previous front buffer non-volatile early, so that we can dirty the whole layer if it comes back empty.
    setBufferVolatility(BufferType::Front, false);

    WebCore::IntSize expandedScaledSize = backingStoreSize();

    if (m_dirtyRegion.isEmpty() || expandedScaledSize.isEmpty())
        return needToEncodeBackingStore;

    WebCore::IntRect layerBounds(WebCore::IntPoint(), WebCore::expandedIntSize(m_size));
    if (!hasFrontBuffer())
        m_dirtyRegion.unite(layerBounds);

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        WebCore::IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }

    WebCore::IntRect expandedScaledLayerBounds(WebCore::IntPoint(), expandedScaledSize);
    bool willPaintEntireBackingStore = m_dirtyRegion.contains(layerBounds);

    swapToValidFrontBuffer();

    if (m_acceleratesDrawing) {
        RetainPtr<CGImageRef> backImage;
        if (m_backBuffer.surface && !willPaintEntireBackingStore)
            backImage = m_backBuffer.surface->createImage();

        if (m_frontBuffer.surface) {
            WebCore::GraphicsContext& context = m_frontBuffer.surface->ensureGraphicsContext();

            context.scale(WebCore::FloatSize(1, -1));
            context.translate(0, -expandedScaledSize.height());
            drawInContext(context, backImage.get());

            m_frontBuffer.surface->releaseGraphicsContext();
        }
    } else {
        ASSERT(!m_acceleratesDrawing);
        std::unique_ptr<WebCore::GraphicsContext> context = m_frontBuffer.bitmap->createGraphicsContext();

        RetainPtr<CGImageRef> backImage;
        if (m_backBuffer.bitmap && !willPaintEntireBackingStore)
            backImage = m_backBuffer.bitmap->makeCGImage();

        if (context)
            drawInContext(*context, backImage.get());
    }
    
    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);
    
    return true;
}

void RemoteLayerBackingStore::drawInContext(WebCore::GraphicsContext& context, CGImageRef backImage)
{
    WebCore::FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    WebCore::IntRect scaledLayerBounds(WebCore::IntPoint(), WebCore::roundedIntSize(scaledSize));

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

    CGRect cgPaintingRects[WebCore::PlatformCALayer::webLayerMaxRectsToPaint];
    for (size_t i = 0, dirtyRectCount = m_paintingRects.size(); i < dirtyRectCount; ++i) {
        WebCore::FloatRect scaledPaintingRect = m_paintingRects[i];
        scaledPaintingRect.scale(m_scale);
        cgPaintingRects[i] = scaledPaintingRect;
    }

    CGContextRef cgContext = context.platformContext();

    if (backImage) {
        WebCore::CGContextStateSaver stateSaver(cgContext);
        CGContextSetBlendMode(cgContext, kCGBlendModeCopy);
        CGContextTranslateCTM(cgContext, 0, scaledLayerBounds.height());
        CGContextScaleCTM(cgContext, 1, -1);
        CGContextDrawImage(cgContext, scaledLayerBounds, backImage);
    }

    CGContextClipToRects(cgContext, cgPaintingRects, m_paintingRects.size());

    if (!m_isOpaque)
        context.clearRect(scaledLayerBounds);

#ifndef NDEBUG
    if (m_isOpaque)
        context.fillRect(scaledLayerBounds, WebCore::makeSimpleColor(255, 47, 146));
#endif

    context.scale(m_scale);
    
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
    case WebCore::PlatformCALayer::LayerTypeEditableImageLayer:
    case WebCore::PlatformCALayer::LayerTypeCustom:
        ASSERT_NOT_REACHED();
        break;
    };

    m_dirtyRegion = WebCore::Region();
    m_paintingRects.clear();

    m_frontContextPendingFlush = context.platformContext();
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

void RemoteLayerBackingStore::applyBackingStoreToLayer(CALayer *layer, LayerContentsType contentsType)
{
    layer.contentsOpaque = m_isOpaque;

    if (acceleratesDrawing()) {
        switch (contentsType) {
        case LayerContentsType::IOSurface:
            if (!m_frontBuffer.surface) {
                ASSERT(m_frontBufferSendRight);
                m_frontBuffer.surface = WebCore::IOSurface::createFromSendRight(WTFMove(m_frontBufferSendRight), WebCore::sRGBColorSpaceRef());
            }
            layer.contents = m_frontBuffer.surface ? m_frontBuffer.surface->asLayerContents() : nil;
            break;
        case LayerContentsType::CAMachPort:
            ASSERT(m_frontBufferSendRight);
            layer.contents = (__bridge id)adoptCF(CAMachPortCreate(m_frontBufferSendRight.leakSendRight())).get();
            break;
        }
        return;
    }

    ASSERT(!acceleratesDrawing());
    layer.contents = (__bridge id)m_frontBuffer.bitmap->makeCGImageCopy().get();
}

RetainPtr<CGContextRef> RemoteLayerBackingStore::takeFrontContextPendingFlush()
{
    return WTFMove(m_frontContextPendingFlush);
}

bool RemoteLayerBackingStore::setBufferVolatility(BufferType type, bool isVolatile)
{
    // Return value is true if we succeeded in making volatile.
    auto makeVolatile = [] (Buffer& buffer) -> bool {
        if (!buffer.surface || buffer.isVolatile)
            return true;

        buffer.surface->releaseGraphicsContext();

        if (!buffer.surface->isInUse()) {
            buffer.surface->setIsVolatile(true);
            buffer.isVolatile = true;
            return true;
        }
    
        return false;
    };

    // Return value is true if we need to repaint.
    auto makeNonVolatile = [] (Buffer& buffer) -> bool {
        if (!buffer.surface || !buffer.isVolatile)
            return false;

        auto previousState = buffer.surface->setIsVolatile(false);
        buffer.isVolatile = false;

        return previousState == WebCore::IOSurface::SurfaceState::Empty;
    };

    switch (type) {
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
    if (surface)
        WebCore::IOSurface::moveToPool(WTFMove(surface));
    isVolatile = false;
    bitmap = nullptr;
}

WebCore::IOSurface::Format RemoteLayerBackingStore::surfaceBufferFormat() const
{
#if HAVE(IOSURFACE_RGB10)
    if (m_deepColor)
        return m_isOpaque ? WebCore::IOSurface::Format::RGB10 : WebCore::IOSurface::Format::RGB10A8;
#endif

    return WebCore::IOSurface::Format::RGBA;
}

} // namespace WebKit
