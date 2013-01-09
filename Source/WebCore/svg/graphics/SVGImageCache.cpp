/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGImageCache.h"

#if ENABLE(SVG)
#include "CachedImage.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Page.h"
#include "RenderSVGRoot.h"
#include "SVGImage.h"

namespace WebCore {

static const int timeToKeepCachedBitmapsAfterLastUseInSeconds = 30;

SVGImageCache::SVGImageCache(SVGImage* svgImage)
    : m_svgImage(svgImage)
    , m_redrawTimer(this, &SVGImageCache::redrawTimerFired)
    , m_cacheClearTimer(this, &SVGImageCache::cacheClearTimerFired, timeToKeepCachedBitmapsAfterLastUseInSeconds)
{
    ASSERT(m_svgImage);
}

SVGImageCache::~SVGImageCache()
{
    m_sizeAndScalesMap.clear();
    clearBitmapCache();
}

void SVGImageCache::clearBitmapCache()
{
    ImageDataMap::iterator end = m_imageDataMap.end();
    for (ImageDataMap::iterator it = m_imageDataMap.begin(); it != end; ++it) {
        // Checks if the client (it->key) is still valid. The client should remove itself from this
        // cache before its end of life, otherwise the following ASSERT will crash on pure virtual
        // function call or a general crash.
        ASSERT(it->key->resourceClientType() == CachedImageClient::expectedType());
        delete it->value.buffer;
    }

    m_imageDataMap.clear();
}

void SVGImageCache::removeClientFromCache(const CachedImageClient* client)
{
    ASSERT(client);
    m_sizeAndScalesMap.remove(client);

    ImageDataMap::iterator it = m_imageDataMap.find(client);
    if (it == m_imageDataMap.end())
        return;

    delete it->value.buffer;
    m_imageDataMap.remove(it);
}

void SVGImageCache::setRequestedSizeAndScales(const CachedImageClient* client, const SizeAndScales& sizeAndScales)
{
    ASSERT(client);
    ASSERT(!sizeAndScales.size.isEmpty());
    m_sizeAndScalesMap.set(client, sizeAndScales);
}

SVGImageCache::SizeAndScales SVGImageCache::requestedSizeAndScales(const CachedImageClient* client) const
{
    if (!client)
        return SizeAndScales();
    SizeAndScalesMap::const_iterator it = m_sizeAndScalesMap.find(client);
    if (it == m_sizeAndScalesMap.end())
        return SizeAndScales();
    return it->value;
}

void SVGImageCache::imageContentChanged()
{
    ImageDataMap::iterator end = m_imageDataMap.end();
    for (ImageDataMap::iterator it = m_imageDataMap.begin(); it != end; ++it)
        it->value.imageNeedsUpdate = true;

    // Always redraw on a timer because this method may be invoked from destructors of things we are intending to draw.
    if (!m_redrawTimer.isActive())
        m_redrawTimer.startOneShot(0);
}

void SVGImageCache::redraw()
{
    ImageDataMap::iterator end = m_imageDataMap.end();
    for (ImageDataMap::iterator it = m_imageDataMap.begin(); it != end; ++it) {
        ImageData& data = it->value;
        if (!data.imageNeedsUpdate)
            continue;
        // If the content changed we redraw using our existing ImageBuffer.
        ASSERT(data.buffer);
        ASSERT(data.image);
        m_svgImage->drawSVGToImageBuffer(data.buffer, data.sizeAndScales.size, data.sizeAndScales.zoom, data.sizeAndScales.scale, SVGImage::ClearImageBuffer);
        data.image = data.buffer->copyImage(CopyBackingStore);
        data.imageNeedsUpdate = false;
    }
    ASSERT(m_svgImage->imageObserver());
    m_svgImage->imageObserver()->animationAdvanced(m_svgImage);
}

void SVGImageCache::redrawTimerFired(Timer<SVGImageCache>*)
{
    // We have no guarantee that the frame does not require layout when the timer fired.
    // So be sure to check again in case it is still not safe to run redraw.
    FrameView* frameView = m_svgImage->frameView();
    if (frameView && (frameView->needsLayout() || frameView->isInLayout())) {
        if (!m_redrawTimer.isActive())
            m_redrawTimer.startOneShot(0);
    } else
       redraw();
}

void SVGImageCache::cacheClearTimerFired(DeferrableOneShotTimer<SVGImageCache>*)
{
    clearBitmapCache();
}

Image* SVGImageCache::lookupOrCreateBitmapImageForRenderer(const RenderObject* renderer)
{
    if (!renderer)
        return Image::nullImage();

    const CachedImageClient* client = renderer;

    // The cache needs to know the size of the renderer before querying an image for it.
    SizeAndScalesMap::iterator sizeIt = m_sizeAndScalesMap.find(renderer);
    if (sizeIt == m_sizeAndScalesMap.end())
        return Image::nullImage();

    IntSize size = sizeIt->value.size;
    float zoom = sizeIt->value.zoom;
    float scale = sizeIt->value.scale;

    // FIXME (85335): This needs to take CSS transform scale into account as well.
    Page* page = renderer->document()->page();
    if (!scale)
        scale = page->deviceScaleFactor() * page->pageScaleFactor();

    ASSERT(!size.isEmpty());

    // (Re)schedule the oneshot timer to throw out all the cached ImageBuffers if they remain unused for too long.
    m_cacheClearTimer.restart();

    // Lookup image for client in cache and eventually update it.
    ImageDataMap::iterator it = m_imageDataMap.find(client);
    if (it != m_imageDataMap.end()) {
        ImageData& data = it->value;

        // Common case: image size & zoom remained the same.
        if (data.sizeAndScales.size == size && data.sizeAndScales.zoom == zoom && data.sizeAndScales.scale == scale)
            return data.image.get();

        // If the image size for the client changed, we have to delete the buffer, remove the item from the cache and recreate it.
        delete data.buffer;
        m_imageDataMap.remove(it);
    }

    FloatSize scaledSize(size);
    scaledSize.scale(scale);

    // Create and cache new image and image buffer at requested size.
    OwnPtr<ImageBuffer> newBuffer = ImageBuffer::create(expandedIntSize(scaledSize), 1);
    if (!newBuffer)
        return Image::nullImage();

    m_svgImage->drawSVGToImageBuffer(newBuffer.get(), size, zoom, scale, SVGImage::DontClearImageBuffer);

    RefPtr<Image> newImage = newBuffer->copyImage(CopyBackingStore);
    Image* newImagePtr = newImage.get();
    ASSERT(newImagePtr);

    m_imageDataMap.add(client, ImageData(newBuffer.leakPtr(), newImage.release(), SizeAndScales(size, zoom, scale)));
    return newImagePtr;
}

} // namespace WebCore

#endif // ENABLE(SVG)
