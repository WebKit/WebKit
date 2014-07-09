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

RemoteLayerBackingStore::RemoteLayerBackingStore(PlatformCALayerRemote* layer)
    : m_layer(layer)
    , m_isOpaque(false)
    , m_lastDisplayTime(std::chrono::steady_clock::time_point::min())
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

void RemoteLayerBackingStore::ensureBackingStore(FloatSize size, float scale, bool acceleratesDrawing, bool isOpaque)
{
    if (m_size == size && m_scale == scale && m_acceleratesDrawing == acceleratesDrawing && m_isOpaque == isOpaque)
        return;

    m_size = size;
    m_scale = scale;
    m_acceleratesDrawing = acceleratesDrawing;
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
#if USE(IOSURFACE)
    m_secondaryBackBuffer.discard();
#endif
}

void RemoteLayerBackingStore::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_size;
    encoder << m_scale;
    encoder << m_acceleratesDrawing;
    encoder << m_isOpaque;

#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        mach_port_t port = m_frontBuffer.surface->createMachPort();
        encoder << IPC::MachPort(port, MACH_MSG_TYPE_MOVE_SEND);
        return;
    }
#endif

    ASSERT(!m_acceleratesDrawing);

    ShareableBitmap::Handle handle;
    m_frontBuffer.bitmap->createHandle(handle);
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
        result.m_frontBuffer.surface = IOSurface::createFromMachPort(machPort.port(), ColorSpaceDeviceRGB);
        mach_port_deallocate(mach_task_self(), machPort.port());
        return true;
    }
#endif

    ASSERT(!result.m_acceleratesDrawing);

    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    result.m_frontBuffer.bitmap = ShareableBitmap::create(handle);

    return true;
}

void RemoteLayerBackingStore::setNeedsDisplay(const IntRect rect)
{
    m_dirtyRegion.unite(rect);
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    setNeedsDisplay(IntRect(IntPoint(), expandedIntSize(m_size)));
}

void RemoteLayerBackingStore::swapToValidFrontBuffer()
{
    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    IntSize expandedScaledSize = roundedIntSize(scaledSize);

#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        if (!m_backBuffer.surface || m_backBuffer.surface->isInUse()) {
            std::swap(m_backBuffer, m_secondaryBackBuffer);
            if (m_backBuffer.surface && m_backBuffer.surface->isInUse())
                m_backBuffer.discard();
        }

        std::swap(m_frontBuffer, m_backBuffer);

        if (!m_frontBuffer.surface)
            m_frontBuffer.surface = IOSurface::create(expandedScaledSize, ColorSpaceDeviceRGB);

        setBufferVolatility(BufferType::Front, false);

        return;
    }
#endif

    ASSERT(!m_acceleratesDrawing);
    std::swap(m_frontBuffer, m_backBuffer);

    if (!m_frontBuffer.bitmap)
        m_frontBuffer.bitmap = ShareableBitmap::createShareable(expandedScaledSize, m_isOpaque ? ShareableBitmap::NoFlags : ShareableBitmap::SupportsAlpha);
}

bool RemoteLayerBackingStore::display()
{
    ASSERT(!m_frontContextPendingFlush);

    m_lastDisplayTime = std::chrono::steady_clock::now();

    if (RemoteLayerTreeContext* context = m_layer->context())
        context->backingStoreWillBeDisplayed(*this);

    // Make the previous front buffer non-volatile early, so that we can dirty the whole layer if it comes back empty.
    setBufferVolatility(BufferType::Front, false);

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty())
        return false;

    IntRect layerBounds(IntPoint(), expandedIntSize(m_size));
    if (!hasFrontBuffer())
        m_dirtyRegion.unite(layerBounds);

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }

    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    IntSize expandedScaledSize = roundedIntSize(scaledSize);
    IntRect expandedScaledLayerBounds(IntPoint(), expandedScaledSize);
    bool willPaintEntireBackingStore = m_dirtyRegion.contains(layerBounds);

    swapToValidFrontBuffer();

#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        RetainPtr<CGImageRef> backImage;
        if (m_backBuffer.surface && !willPaintEntireBackingStore)
            backImage = m_backBuffer.surface->createImage();

        GraphicsContext& context = m_frontBuffer.surface->ensureGraphicsContext();

        context.scale(FloatSize(1, -1));
        context.translate(0, -expandedScaledSize.height());
        drawInContext(context, backImage.get());

        m_frontBuffer.surface->releaseGraphicsContext();
    } else
#endif
    {
        ASSERT(!m_acceleratesDrawing);
        std::unique_ptr<GraphicsContext> context = m_frontBuffer.bitmap->createGraphicsContext();

        RetainPtr<CGImageRef> backImage;
        if (m_backBuffer.bitmap && !willPaintEntireBackingStore)
            backImage = m_backBuffer.bitmap->makeCGImage();

        drawInContext(*context, backImage.get());
    }
    
    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer);
    
    return true;
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context, CGImageRef backImage)
{
    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    IntRect scaledLayerBounds(IntPoint(), roundedIntSize(scaledSize));

    if (!m_isOpaque)
        context.clearRect(scaledLayerBounds);

#ifndef NDEBUG
    if (m_isOpaque)
        context.fillRect(scaledLayerBounds, Color(255, 0, 0), ColorSpaceDeviceRGB);
#endif

    CGContextRef cgContext = context.platformContext();

    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the total dirty area, we'll repaint each rect separately.
    // Otherwise, repaint the entire bounding box of the dirty region.
    IntRect dirtyBounds = m_dirtyRegion.bounds();

    Vector<IntRect> dirtyRects = m_dirtyRegion.rects();
    if (dirtyRects.size() > PlatformCALayer::webLayerMaxRectsToPaint || m_dirtyRegion.totalArea() > PlatformCALayer::webLayerWastedSpaceThreshold * dirtyBounds.width() * dirtyBounds.height()) {
        dirtyRects.clear();
        dirtyRects.append(dirtyBounds);
    }

    // FIXME: find a consistent way to scale and snap dirty and CG clip rects.
    for (const auto& rect : dirtyRects) {
        FloatRect scaledRect(rect);
        scaledRect.scale(m_scale);
        scaledRect = enclosingIntRect(scaledRect);
        scaledRect.scale(1 / m_scale);
        m_paintingRects.append(scaledRect);
    }

    CGRect cgPaintingRects[PlatformCALayer::webLayerMaxRectsToPaint];
    for (size_t i = 0, dirtyRectCount = m_paintingRects.size(); i < dirtyRectCount; ++i) {
        FloatRect scaledPaintingRect = m_paintingRects[i];
        scaledPaintingRect.scale(m_scale);
        cgPaintingRects[i] = scaledPaintingRect;
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

    // FIXME: This should be moved to PlatformCALayerRemote for better layering.
    switch (m_layer->layerType()) {
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        m_layer->owner()->platformCALayerPaintContents(m_layer, context, dirtyBounds);
        break;
    case PlatformCALayer::LayerTypeWebLayer:
        PlatformCALayer::drawLayerContents(cgContext, m_layer, m_paintingRects);
        break;
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeTransformLayer:
    case PlatformCALayer::LayerTypeWebTiledLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeWebGLLayer:
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
        layer.contents = (id)m_frontBuffer.surface->surface();
        return;
    }
#endif

    ASSERT(!acceleratesDrawing());
    layer.contents = (id)m_frontBuffer.bitmap->makeCGImageCopy().get();
}

RetainPtr<CGContextRef> RemoteLayerBackingStore::takeFrontContextPendingFlush()
{
    return WTF::move(m_frontContextPendingFlush);
}

#if USE(IOSURFACE)
bool RemoteLayerBackingStore::setBufferVolatility(BufferType type, bool isVolatile)
{
    switch(type) {
    case BufferType::Front:
        if (m_frontBuffer.surface && m_frontBuffer.isVolatile != isVolatile) {
            if (isVolatile)
                m_frontBuffer.surface->releaseGraphicsContext();
            if (!isVolatile || !m_frontBuffer.surface->isInUse()) {
                IOSurface::SurfaceState previousState = m_frontBuffer.surface->setIsVolatile(isVolatile);
                m_frontBuffer.isVolatile = isVolatile;

                // Becoming non-volatile and the front buffer was purged, so we need to repaint.
                if (!isVolatile && (previousState == IOSurface::SurfaceState::Empty))
                    setNeedsDisplay();
            } else
                return false;
        }
        break;
    case BufferType::Back:
        if (m_backBuffer.surface && m_backBuffer.isVolatile != isVolatile) {
            if (isVolatile)
                m_backBuffer.surface->releaseGraphicsContext();
            if (!isVolatile || !m_backBuffer.surface->isInUse()) {
                m_backBuffer.surface->setIsVolatile(isVolatile);
                m_backBuffer.isVolatile = isVolatile;
            } else
                return false;
        }
        break;
    case BufferType::SecondaryBack:
        if (m_secondaryBackBuffer.surface && m_secondaryBackBuffer.isVolatile != isVolatile) {
            if (isVolatile)
                m_secondaryBackBuffer.surface->releaseGraphicsContext();
            if (!isVolatile || !m_secondaryBackBuffer.surface->isInUse()) {
                m_secondaryBackBuffer.surface->setIsVolatile(isVolatile);
                m_secondaryBackBuffer.isVolatile = isVolatile;
            } else
                return false;
        }
        break;
    }
    return true;
}
#else
bool RemoteLayerBackingStore::setBufferVolatility(BufferType, bool)
{
    return true;
}
#endif

void RemoteLayerBackingStore::Buffer::discard()
{
#if USE(IOSURFACE)
    if (surface)
        IOSurfacePool::sharedPool().addSurface(surface.get());
    surface = nullptr;
    isVolatile = false;
#endif
    bitmap = nullptr;
}

} // namespace WebKit
