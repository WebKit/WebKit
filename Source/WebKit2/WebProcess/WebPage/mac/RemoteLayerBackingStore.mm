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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if USE(ACCELERATED_COMPOSITING)

#import "ArgumentCoders.h"
#import "MachPort.h"
#import "PlatformCALayerRemote.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/WebLayer.h>

#ifdef __has_include
#if __has_include(<ApplicationServices/ApplicationServicesPriv.h>)
#import <ApplicationServices/ApplicationServicesPriv.h>
#endif
#endif

extern "C" {
CGContextRef CGIOSurfaceContextCreate(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo);
CGImageRef CGIOSurfaceContextCreateImage(CGContextRef);
}

using namespace WebCore;
using namespace WebKit;

RemoteLayerBackingStore::RemoteLayerBackingStore()
    : m_layer(nullptr)
{
}

void RemoteLayerBackingStore::ensureBackingStore(PlatformCALayerRemote* layer, IntSize size, float scale, bool acceleratesDrawing)
{
    if (m_layer == layer
        && m_size == size
        && m_scale == scale
        && m_acceleratesDrawing == acceleratesDrawing)
        return;

    m_layer = layer;
    m_size = size;
    m_scale = scale;
    m_acceleratesDrawing = acceleratesDrawing;

    m_frontSurface = nullptr;
    m_frontBuffer = nullptr;
}

void RemoteLayerBackingStore::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder << m_size;
    encoder << m_scale;
    encoder << m_acceleratesDrawing;

    if (m_acceleratesDrawing) {
        mach_port_t port = IOSurfaceCreateMachPort(m_frontSurface.get());
        encoder << CoreIPC::MachPort(port, MACH_MSG_TYPE_MOVE_SEND);
    } else {
        ShareableBitmap::Handle handle;
        m_frontBuffer->createHandle(handle);
        encoder << handle;
    }
}

bool RemoteLayerBackingStore::decode(CoreIPC::ArgumentDecoder& decoder, RemoteLayerBackingStore& result)
{
    if (!decoder.decode(result.m_size))
        return false;

    if (!decoder.decode(result.m_scale))
        return false;

    if (!decoder.decode(result.m_acceleratesDrawing))
        return false;

    if (result.m_acceleratesDrawing) {
        CoreIPC::MachPort machPort;
        if (!decoder.decode(machPort))
            return false;
        result.m_frontSurface = adoptCF(IOSurfaceLookupFromMachPort(machPort.port()));
        mach_port_deallocate(mach_task_self(), machPort.port());
    } else {
        ShareableBitmap::Handle handle;
        if (!decoder.decode(handle))
            return false;
        result.m_frontBuffer = ShareableBitmap::create(handle);
    }

    return true;
}

IntRect RemoteLayerBackingStore::mapToContentCoordinates(const IntRect rect) const
{
    IntRect flippedRect = rect;
    if (m_layer->owner()->platformCALayerContentsOrientation() == GraphicsLayer::CompositingCoordinatesBottomUp)
        flippedRect.setY(m_size.height() - rect.y() - rect.height());
    return flippedRect;
}

void RemoteLayerBackingStore::setNeedsDisplay(const IntRect rect)
{
    IntRect flippedRect = mapToContentCoordinates(rect);
    m_dirtyRegion.unite(flippedRect);
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    setNeedsDisplay(IntRect(IntPoint(), m_size));
}

static RetainPtr<CGContextRef> createIOSurfaceContext(IOSurfaceRef surface, IntSize size, CGColorSpaceRef colorSpace)
{
    if (!surface)
        return nullptr;

    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 32;
    RetainPtr<CGContextRef> ctx = adoptCF(CGIOSurfaceContextCreate(surface, size.width(), size.height(), bitsPerComponent, bitsPerPixel, colorSpace, bitmapInfo));
    if (!ctx)
        return nullptr;

    return ctx;
}

static RetainPtr<IOSurfaceRef> createIOSurface(IntSize size)
{
    unsigned pixelFormat = 'BGRA';
    unsigned bytesPerElement = 4;
    int width = size.width();
    int height = size.height();

    unsigned long bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
    ASSERT(bytesPerRow);

    unsigned long allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    ASSERT(allocSize);

    RetainPtr<NSDictionary> dict = @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
        (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
        (id)kIOSurfaceAllocSize: @(allocSize)
    };

    return adoptCF(IOSurfaceCreate((CFDictionaryRef)dict.get()));
}

RetainPtr<CGImageRef> RemoteLayerBackingStore::image() const
{
    if (!m_frontBuffer || m_acceleratesDrawing)
        return nullptr;

    // FIXME: Do we need Copy?
    return m_frontBuffer->makeCGImageCopy();
}

bool RemoteLayerBackingStore::display()
{
    if (!m_layer)
        return false;

    // If we previously were drawsContent=YES, and now are not, we need
    // to note that our backing store has been cleared.
    if (!m_layer->owner() || !m_layer->owner()->platformCALayerDrawsContent()) {
        bool previouslyDrewContents = hasFrontBuffer();

        m_frontBuffer = nullptr;
        m_frontSurface = nullptr;

        return previouslyDrewContents;
    }

    if (!hasFrontBuffer())
        m_dirtyRegion.unite(IntRect(IntPoint(), m_size));

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty())
        return false;

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        IntRect indicatorRect = mapToContentCoordinates(IntRect(0, 0, 52, 27));
        m_dirtyRegion.unite(indicatorRect);
    }

    std::unique_ptr<GraphicsContext> context;

    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    IntSize expandedScaledSize = expandedIntSize(scaledSize);

    RefPtr<ShareableBitmap> backBuffer;
    RetainPtr<IOSurfaceRef> backSurface;

    if (m_acceleratesDrawing) {
        backSurface = createIOSurface(expandedScaledSize);
        RetainPtr<CGContextRef> cgContext = createIOSurfaceContext(backSurface.get(), expandedScaledSize, sRGBColorSpaceRef());
        context = std::make_unique<GraphicsContext>(cgContext.get());
        CGContextClearRect(cgContext.get(), CGRectMake(0, 0, expandedScaledSize.width(), expandedScaledSize.height()));
        context->scale(FloatSize(1, -1));
        context->translate(0, -expandedScaledSize.height());
    } else {
        backBuffer = ShareableBitmap::createShareable(expandedIntSize(scaledSize), ShareableBitmap::SupportsAlpha);
        context = backBuffer->createGraphicsContext();
    }

    drawInContext(*context);

    if (m_acceleratesDrawing)
        m_frontSurface = backSurface;
    else
        m_frontBuffer = backBuffer;

    return true;
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context)
{
    RetainPtr<CGImageRef> frontImage;
    RetainPtr<CGContextRef> frontContext;

    if (m_acceleratesDrawing) {
        frontContext = createIOSurfaceContext(m_frontSurface.get(), expandedIntSize(m_size * m_scale), sRGBColorSpaceRef());
        frontImage = adoptCF(CGIOSurfaceContextCreateImage(frontContext.get()));
    } else
        frontImage = image();

    Vector<IntRect> dirtyRects = m_dirtyRegion.rects();

    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the area, we'll do a partial repaint.

    Vector<FloatRect, webLayerMaxRectsToPaint> rectsToPaint;
    if (dirtyRects.size() <= webLayerMaxRectsToPaint && m_dirtyRegion.totalArea() <= webLayerWastedSpaceThreshold * m_size.width() * m_size.height()) {
        // Copy over the parts of the front buffer that we're not going to repaint.
        if (frontImage) {
            Region cleanRegion(IntRect(IntPoint(), m_size));
            cleanRegion.subtract(m_dirtyRegion);

            for (const auto& rect : cleanRegion.rects()) {
                FloatRect scaledRect = rect;
                scaledRect.scale(m_scale);
                FloatSize imageSize(CGImageGetWidth(frontImage.get()), CGImageGetHeight(frontImage.get()));
                context.drawNativeImage(frontImage.get(), imageSize, ColorSpaceDeviceRGB, scaledRect, scaledRect);
            }
        }

        for (const auto& rect : dirtyRects)
            rectsToPaint.append(rect);
    }

    context.scale(FloatSize(m_scale, m_scale));

    switch (m_layer->layerType()) {
        case PlatformCALayer::LayerTypeSimpleLayer:
        case PlatformCALayer::LayerTypeTiledBackingTileLayer:
            if (rectsToPaint.isEmpty())
                rectsToPaint.append(IntRect(IntPoint(), m_size));
            for (const auto& rect : rectsToPaint)
                m_layer->owner()->platformCALayerPaintContents(m_layer, context, enclosingIntRect(rect));
            break;
        case PlatformCALayer::LayerTypeWebLayer:
            drawLayerContents(context.platformContext(), m_layer, rectsToPaint);
            break;
        case PlatformCALayer::LayerTypeLayer:
        case PlatformCALayer::LayerTypeTransformLayer:
        case PlatformCALayer::LayerTypeWebTiledLayer:
        case PlatformCALayer::LayerTypeTiledBackingLayer:
        case PlatformCALayer::LayerTypePageTiledBackingLayer:
        case PlatformCALayer::LayerTypeRootLayer:
        case PlatformCALayer::LayerTypeAVPlayerLayer:
        case PlatformCALayer::LayerTypeCustom:
            break;
    };

    m_dirtyRegion = Region();

    CGContextFlush(context.platformContext());

    // If our frontImage is derived from an IOSurface, we need to
    // destroy the image before the CGContext it's derived from,
    // so that the context doesn't make a CPU copy of the surface data.
    frontImage = nullptr;
    frontContext = nullptr;
}

#endif // USE(ACCELERATED_COMPOSITING)
