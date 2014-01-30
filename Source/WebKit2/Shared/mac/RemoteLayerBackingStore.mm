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

#import "ArgumentCoders.h"
#import "MachPort.h"
#import "PlatformCALayerRemote.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/WebLayer.h>

#if USE(IOSURFACE)
#import <mach/mach_port.h>
#endif

#if defined(__has_include) && __has_include(<ApplicationServices/ApplicationServicesPriv.h>)
#import <ApplicationServices/ApplicationServicesPriv.h>
#endif

extern "C" {
#if USE(IOSURFACE)
CGContextRef CGIOSurfaceContextCreate(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo);
#endif
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
    if (m_layer == layer && m_size == size && m_scale == scale && m_acceleratesDrawing == acceleratesDrawing)
        return;

    m_layer = layer;
    m_size = size;
    m_scale = scale;
    m_acceleratesDrawing = acceleratesDrawing;

#if USE(IOSURFACE)
    m_frontSurface = nullptr;
#endif
    m_frontBuffer = nullptr;
}

void RemoteLayerBackingStore::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_size;
    encoder << m_scale;
    encoder << m_acceleratesDrawing;

#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        mach_port_t port = IOSurfaceCreateMachPort(m_frontSurface.get());
        encoder << IPC::MachPort(port, MACH_MSG_TYPE_MOVE_SEND);
        return;
    }
#else
    ASSERT(!m_acceleratesDrawing);
#endif

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

#if USE(IOSURFACE)
    if (result.m_acceleratesDrawing) {
        IPC::MachPort machPort;
        if (!decoder.decode(machPort))
            return false;
        result.m_frontSurface = adoptCF(IOSurfaceLookupFromMachPort(machPort.port()));
        mach_port_deallocate(mach_task_self(), machPort.port());
        return true;
    }
#else
    ASSERT(!result.m_acceleratesDrawing);
#endif

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

#if USE(IOSURFACE)
static RetainPtr<CGContextRef> createIOSurfaceContext(IOSurfaceRef surface, IntSize size, CGColorSpaceRef colorSpace)
{
    if (!surface)
        return nullptr;

    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 32;
    return adoptCF(CGIOSurfaceContextCreate(surface, size.width(), size.height(), bitsPerComponent, bitsPerPixel, colorSpace, bitmapInfo));
}

static RetainPtr<IOSurfaceRef> createIOSurface(IntSize size)
{
    unsigned pixelFormat = 'BGRA';
    unsigned bytesPerElement = 4;
    int width = size.width();
    int height = size.height();

    size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
    ASSERT(bytesPerRow);

    size_t allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    ASSERT(allocSize);

    NSDictionary *dict = @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
        (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
        (id)kIOSurfaceAllocSize: @(allocSize)
    };

    return adoptCF(IOSurfaceCreate((CFDictionaryRef)dict));
}
#endif // USE(IOSURFACE)

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
#if USE(IOSURFACE)
        m_frontSurface = nullptr;
#endif

        return previouslyDrewContents;
    }

    if (m_dirtyRegion.isEmpty() || m_size.isEmpty())
        return false;

    if (!hasFrontBuffer())
        m_dirtyRegion.unite(IntRect(IntPoint(), m_size));

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }

    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);
    IntSize expandedScaledSize = expandedIntSize(scaledSize);

#if USE(IOSURFACE)
    if (m_acceleratesDrawing) {
        RetainPtr<IOSurfaceRef> backSurface = createIOSurface(expandedScaledSize);
        RetainPtr<CGContextRef> cgContext = createIOSurfaceContext(backSurface.get(), expandedScaledSize, sRGBColorSpaceRef());
        GraphicsContext context(cgContext.get());
        context.clearRect(FloatRect(FloatPoint(), expandedScaledSize));
        context.scale(FloatSize(1, -1));
        context.translate(0, -expandedScaledSize.height());

        RetainPtr<CGContextRef> frontContext;
        RetainPtr<CGImageRef> frontImage;
        if (m_frontSurface) {
            frontContext = createIOSurfaceContext(m_frontSurface.get(), expandedIntSize(m_size * m_scale), sRGBColorSpaceRef());
            frontImage = adoptCF(CGIOSurfaceContextCreateImage(frontContext.get()));
        }

        drawInContext(context, frontImage.get());
        m_frontSurface = backSurface;

        // If our frontImage is derived from an IOSurface, we need to
        // destroy the image before the CGContext it's derived from,
        // so that the context doesn't make a CPU copy of the surface data.
        if (frontImage) {
            frontImage = nullptr;
            ASSERT(frontContext);
        }

        return true;
    }
#else
    ASSERT(!m_acceleratesDrawing);
#endif

    RetainPtr<CGImageRef> frontImage = image();
    m_frontBuffer = ShareableBitmap::createShareable(expandedScaledSize, ShareableBitmap::SupportsAlpha);
    std::unique_ptr<GraphicsContext> context = m_frontBuffer->createGraphicsContext();
    drawInContext(*context, frontImage.get());
    
    return true;
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context, CGImageRef frontImage)
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

    if (frontImage) {
        CGContextSaveGState(cgContext);
        CGContextSetBlendMode(cgContext, kCGBlendModeCopy);

        CGContextAddRect(cgContext, CGRectInfinite);
        CGContextAddRects(cgContext, cgPaintingRects, m_paintingRects.size());
        CGContextEOClip(cgContext);

        CGContextTranslateCTM(cgContext, 0, scaledLayerBounds.height());
        CGContextScaleCTM(cgContext, 1, -1);
        CGContextDrawImage(cgContext, scaledLayerBounds, frontImage);
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

    CGContextFlush(context.platformContext());
}

void RemoteLayerBackingStore::enumerateRectsBeingDrawn(CGContextRef context, void (^block)(CGRect))
{
    CGAffineTransform inverseTransform = CGAffineTransformInvert(CGContextGetCTM(context));

    // We don't want to un-apply the flipping or contentsScale,
    // because they're not applied to repaint rects.
    inverseTransform = CGAffineTransformScale(inverseTransform, m_scale, -m_scale);
    inverseTransform = CGAffineTransformTranslate(inverseTransform, 0, -m_size.height());

    for (auto rect : m_paintingRects) {
        CGRect rectToDraw = CGRectApplyAffineTransform(rect, inverseTransform);
        block(rectToDraw);
    }
}
