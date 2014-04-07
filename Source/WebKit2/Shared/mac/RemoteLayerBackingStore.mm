 /*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import <WebCore/IOSurfacePool.h>
#import <WebCore/WebLayer.h>

#if USE(IOSURFACE)
#import <mach/mach_port.h>
#endif

#if __has_include(<QuartzCore/CALayerPrivate.h>)
#import <QuartzCore/CALayerPrivate.h>
#endif

@interface CALayer (Details)
@property BOOL contentsOpaque;
@end

using namespace WebCore;

namespace WebKit {

RemoteLayerBackingStore::RemoteLayerBackingStore(RemoteLayerTreeContext* context)
    : m_layer(nullptr)
    , m_isOpaque(false)
    , m_volatility(RemoteLayerBackingStore::Volatility::NonVolatile)
    , m_context(context)
    , m_lastDisplayTime(std::chrono::steady_clock::time_point::min())
{
    if (m_context)
        m_context->backingStoreWasCreated(this);
}

RemoteLayerBackingStore::~RemoteLayerBackingStore()
{
    clearBackingStore();

    if (m_context)
        m_context->backingStoreWillBeDestroyed(this);
}

void RemoteLayerBackingStore::ensureBackingStore(PlatformCALayerRemote* layer, IntSize size, float scale, bool acceleratesDrawing, bool isOpaque)
{
    if (m_layer == layer && m_size == size && m_scale == scale && m_acceleratesDrawing == acceleratesDrawing && m_isOpaque == isOpaque)
        return;

    m_layer = layer;
    m_size = size;
    m_scale = scale;
    m_acceleratesDrawing = acceleratesDrawing;
    m_isOpaque = isOpaque;

    clearBackingStore();
}

void RemoteLayerBackingStore::clearBackingStore()
{
#if USE(IOSURFACE)
    if (m_frontSurface)
        IOSurfacePool::sharedPool().addSurface(m_frontSurface.get());
    if (m_backSurface)
        IOSurfacePool::sharedPool().addSurface(m_backSurface.get());

    m_frontSurface = nullptr;
    m_backSurface = nullptr;
#endif
    m_frontBuffer = nullptr;
    m_backBuffer = nullptr;
}

void RemoteLayerBackingStore::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_size;
    encoder << m_scale;
    encoder << m_acceleratesDrawing;
    encoder << m_isOpaque;

#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        mach_port_t port = m_frontSurface->createMachPort();
        encoder << IPC::MachPort(port, MACH_MSG_TYPE_MOVE_SEND);
        return;
    }
#endif

    ASSERT(!m_acceleratesDrawing);

    ShareableBitmap::Handle handle;
    m_frontBuffer->createHandle(handle);
    encoder << handle;
}

bool RemoteLayerBackingStore::decode(IPC::ArgumentDecoder& decoder, RemoteLayerBackingStore& result)
{
    if (!decoder.decode(result.m_size))
        return false;

    if (!decoder.decode(result.m_scale))
        return false;

    if (!decoder.decode(result.m_acceleratesDrawing))
        return false;

    if (!decoder.decode(result.m_isOpaque))
        return false;

#if USE(IOSURFACE)
    if (result.m_acceleratesDrawing) {
        IPC::MachPort machPort;
        if (!decoder.decode(machPort))
            return false;
        result.m_frontSurface = IOSurface::createFromMachPort(machPort.port(), ColorSpaceDeviceRGB);
        mach_port_deallocate(mach_task_self(), machPort.port());
        return true;
    }
#endif

    ASSERT(!result.m_acceleratesDrawing);

    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    result.m_frontBuffer = ShareableBitmap::create(handle);

    return true;
}

void RemoteLayerBackingStore::setNeedsDisplay(const IntRect rect)
{
    m_dirtyRegion.unite(rect);
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    setNeedsDisplay(IntRect(IntPoint(), m_size));
}

bool RemoteLayerBackingStore::display()
{
    ASSERT(!m_frontContextPendingFlush);

    m_lastDisplayTime = std::chrono::steady_clock::now();

    setVolatility(Volatility::NonVolatile);

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty())
        return false;

    IntRect layerBounds(IntPoint(), m_size);
    if (!hasFrontBuffer())
        m_dirtyRegion.unite(layerBounds);

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }

    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    IntSize expandedScaledSize = expandedIntSize(scaledSize);
    IntRect expandedScaledLayerBounds(IntPoint(), expandedScaledSize);

    bool willPaintEntireBackingStore = m_dirtyRegion.contains(layerBounds);
#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        std::swap(m_frontSurface, m_backSurface);

        if (!m_frontSurface || m_frontSurface->isInUse()) {
            if (m_frontSurface)
                IOSurfacePool::sharedPool().addSurface(m_frontSurface.get());
            m_frontSurface = IOSurface::create(expandedScaledSize, ColorSpaceDeviceRGB);
        }

        RetainPtr<CGImageRef> backImage;
        if (m_backSurface && !willPaintEntireBackingStore)
            backImage = m_backSurface->createImage();

        GraphicsContext& context = m_frontSurface->ensureGraphicsContext();

        if (!m_isOpaque)
            context.clearRect(expandedScaledLayerBounds);

#ifndef NDEBUG
        if (m_isOpaque)
            context.fillRect(expandedScaledLayerBounds, Color(255, 0, 0), ColorSpaceDeviceRGB);
#endif

        context.scale(FloatSize(1, -1));
        context.translate(0, -expandedScaledSize.height());
        drawInContext(context, backImage.get());

        m_frontSurface->clearGraphicsContext();

        return true;
    }
#endif

    ASSERT(!m_acceleratesDrawing);

    std::swap(m_frontBuffer, m_backBuffer);
    if (!m_frontBuffer)
        m_frontBuffer = ShareableBitmap::createShareable(expandedScaledSize, m_isOpaque ? ShareableBitmap::NoFlags : ShareableBitmap::SupportsAlpha);
    std::unique_ptr<GraphicsContext> context = m_frontBuffer->createGraphicsContext();

    RetainPtr<CGImageRef> backImage;
    if (m_backBuffer && !willPaintEntireBackingStore)
        backImage = m_backBuffer->makeCGImage();

    drawInContext(*context, backImage.get());
    
    return true;
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context, CGImageRef backImage)
{
    IntRect layerBounds(IntPoint(), m_size);
    IntRect scaledLayerBounds(IntPoint(), expandedIntSize(m_size * m_scale));

    CGContextRef cgContext = context.platformContext();

    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the total dirty area, we'll repaint each rect separately.
    // Otherwise, repaint the entire bounding box of the dirty region.
    IntRect dirtyBounds = m_dirtyRegion.bounds();

    Vector<IntRect> dirtyRects = m_dirtyRegion.rects();
    if (dirtyRects.size() > webLayerMaxRectsToPaint || m_dirtyRegion.totalArea() > webLayerWastedSpaceThreshold * dirtyBounds.width() * dirtyBounds.height()) {
        dirtyRects.clear();
        dirtyRects.append(dirtyBounds);
    }

    for (const auto& rect : dirtyRects) {
        FloatRect scaledRect(rect);
        scaledRect.scale(m_scale, m_scale);
        scaledRect = enclosingIntRect(scaledRect);
        scaledRect.scale(1 / m_scale, 1 / m_scale);
        m_paintingRects.append(scaledRect);
    }

    CGRect cgPaintingRects[webLayerMaxRectsToPaint];
    for (size_t i = 0, dirtyRectCount = m_paintingRects.size(); i < dirtyRectCount; ++i) {
        FloatRect scaledPaintingRect = m_paintingRects[i];
        scaledPaintingRect.scale(m_scale);
        cgPaintingRects[i] = enclosingIntRect(scaledPaintingRect);
    }

    if (backImage) {
        CGContextSaveGState(cgContext);
        CGContextSetBlendMode(cgContext, kCGBlendModeCopy);

        CGContextAddRect(cgContext, CGRectInfinite);
        CGContextAddRects(cgContext, cgPaintingRects, m_paintingRects.size());
        CGContextEOClip(cgContext);

        CGContextTranslateCTM(cgContext, 0, scaledLayerBounds.height());
        CGContextScaleCTM(cgContext, 1, -1);
        CGContextDrawImage(cgContext, scaledLayerBounds, backImage);
        CGContextRestoreGState(cgContext);
    }

    CGContextClipToRects(cgContext, cgPaintingRects, m_paintingRects.size());

    context.scale(FloatSize(m_scale, m_scale));

    switch (m_layer->layerType()) {
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        m_layer->owner()->platformCALayerPaintContents(m_layer, context, dirtyBounds);
        break;
    case PlatformCALayer::LayerTypeWebLayer:
        drawLayerContents(cgContext, m_layer, m_paintingRects);
        break;
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeTransformLayer:
    case PlatformCALayer::LayerTypeWebTiledLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeCustom:
        ASSERT_NOT_REACHED();
        break;
    };

    m_dirtyRegion = Region();
    m_paintingRects.clear();

    m_frontContextPendingFlush = context.platformContext();
}

void RemoteLayerBackingStore::enumerateRectsBeingDrawn(CGContextRef context, void (^block)(CGRect))
{
    CGAffineTransform inverseTransform = CGAffineTransformInvert(CGContextGetCTM(context));

    // We don't want to un-apply the flipping or contentsScale,
    // because they're not applied to repaint rects.
    inverseTransform = CGAffineTransformScale(inverseTransform, m_scale, -m_scale);
    inverseTransform = CGAffineTransformTranslate(inverseTransform, 0, -m_size.height());

    for (const auto& rect : m_paintingRects) {
        CGRect rectToDraw = CGRectApplyAffineTransform(rect, inverseTransform);
        block(rectToDraw);
    }
}

void RemoteLayerBackingStore::applyBackingStoreToLayer(CALayer *layer)
{
    layer.contentsOpaque = m_isOpaque;

#if USE(IOSURFACE)
    if (acceleratesDrawing()) {
        layer.contents = (id)m_frontSurface->surface();
        return;
    }
#endif

    ASSERT(!acceleratesDrawing());
    layer.contents = (id)m_frontBuffer->makeCGImageCopy().get();
}

void RemoteLayerBackingStore::flush()
{
    if (m_frontContextPendingFlush) {
        CGContextFlush(m_frontContextPendingFlush.get());
        m_frontContextPendingFlush = nullptr;
    }
}

#if USE(IOSURFACE)
bool RemoteLayerBackingStore::setVolatility(Volatility volatility)
{
    if (m_volatility == volatility)
        return true;

    bool wantsVolatileFrontBuffer = volatility == Volatility::AllBuffersVolatile;
    bool wantsVolatileBackBuffer = volatility == Volatility::AllBuffersVolatile || volatility == Volatility::BackBufferVolatile;

    // If either surface is in-use and would become volatile, don't make any changes.
    if (wantsVolatileFrontBuffer && m_frontSurface && m_frontSurface->isInUse())
        return false;
    if (wantsVolatileBackBuffer && m_backSurface && m_backSurface->isInUse())
        return false;

    if (m_frontSurface) {
        IOSurface::SurfaceState previousState = m_frontSurface->setIsVolatile(wantsVolatileFrontBuffer);

        // Becoming non-volatile and the front buffer was purged, so we need to repaint.
        if (!wantsVolatileFrontBuffer && (previousState == IOSurface::SurfaceState::Empty))
            setNeedsDisplay();
    }

    if (m_backSurface)
        m_backSurface->setIsVolatile(wantsVolatileBackBuffer);

    m_volatility = volatility;

    return true;
}
#else
bool RemoteLayerBackingStore::setVolatility(Volatility)
{
    return true;
}
#endif

} // namespace WebKit
