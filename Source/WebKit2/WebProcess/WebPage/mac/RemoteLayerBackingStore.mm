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

#import "PlatformCALayerRemote.h"
#import "ArgumentCoders.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/WebLayer.h>

using namespace WebCore;
using namespace WebKit;

RemoteLayerBackingStore::RemoteLayerBackingStore(PlatformCALayerRemote* layer, IntSize size, float scale)
    : m_layer(layer)
    , m_size(size)
    , m_scale(scale)
{
    ASSERT(layer);
}

RemoteLayerBackingStore::RemoteLayerBackingStore()
    : m_layer(nullptr)
{
}

void RemoteLayerBackingStore::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    ShareableBitmap::Handle handle;
    m_frontBuffer->createHandle(handle);

    encoder << handle;
    encoder << m_size;
    encoder << m_scale;
}

bool RemoteLayerBackingStore::decode(CoreIPC::ArgumentDecoder& decoder, RemoteLayerBackingStore& result)
{
    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    result.m_frontBuffer = ShareableBitmap::create(handle);

    if (!decoder.decode(result.m_size))
        return false;

    if (!decoder.decode(result.m_scale))
        return false;

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

bool RemoteLayerBackingStore::display()
{
    if (!m_layer)
        return false;

    // If we previously were drawsContent=YES, and now are not, we need
    // to note that our backing store has been cleared.
    if (!m_layer->owner()->platformCALayerDrawsContent())
        return m_frontBuffer;

    if (m_dirtyRegion.isEmpty())
        return false;

    FloatSize scaledSize = m_size;
    scaledSize.scale(m_scale);

    IntRect layerBounds(IntPoint(), m_size);

    m_backBuffer = ShareableBitmap::createShareable(expandedIntSize(scaledSize), ShareableBitmap::SupportsAlpha);
    if (!m_frontBuffer)
        m_dirtyRegion.unite(layerBounds);

    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer)) {
        IntRect indicatorRect = mapToContentCoordinates(IntRect(0, 0, 52, 27));
        m_dirtyRegion.unite(indicatorRect);
    }

    std::unique_ptr<GraphicsContext> context = m_backBuffer->createGraphicsContext();

    Vector<IntRect> dirtyRects = m_dirtyRegion.rects();

    // If we have less than webLayerMaxRectsToPaint rects to paint and they cover less
    // than webLayerWastedSpaceThreshold of the area, we'll do a partial repaint.
    Vector<FloatRect, webLayerMaxRectsToPaint> rectsToPaint;
    if (dirtyRects.size() <= webLayerMaxRectsToPaint && m_dirtyRegion.totalArea() <= webLayerWastedSpaceThreshold * layerBounds.width() * layerBounds.height()) {
        // Copy over the parts of the front buffer that we're not going to repaint.
        if (m_frontBuffer) {
            Region cleanRegion(layerBounds);
            cleanRegion.subtract(m_dirtyRegion);

            RetainPtr<CGImageRef> frontImage = m_frontBuffer->makeCGImage();

            for (const auto& rect : cleanRegion.rects()) {
                FloatRect scaledRect = rect;
                scaledRect.scale(m_scale);
                context->drawNativeImage(frontImage.get(), m_frontBuffer->size(), ColorSpaceDeviceRGB, scaledRect, scaledRect);
            }
        }

        for (const auto& rect : dirtyRects)
            rectsToPaint.append(rect);
    }

    context->scale(FloatSize(m_scale, m_scale));
    drawLayerContents(context->platformContext(), m_layer, FloatRect(FloatPoint(), scaledSize), rectsToPaint, false);
    m_dirtyRegion = Region();

    m_frontBuffer = m_backBuffer;
    m_backBuffer = nullptr;

    return true;
}

#endif // USE(ACCELERATED_COMPOSITING)
