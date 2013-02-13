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
#include "SVGImageForContainer.h"

namespace WebCore {

SVGImageCache::SVGImageCache(SVGImage* svgImage)
    : m_svgImage(svgImage)
{
    ASSERT(m_svgImage);
}

SVGImageCache::~SVGImageCache()
{
    m_imageForContainerMap.clear();
}

void SVGImageCache::removeClientFromCache(const CachedImageClient* client)
{
    ASSERT(client);

    if (m_imageForContainerMap.contains(client))
        m_imageForContainerMap.remove(client);
}

void SVGImageCache::setContainerSizeForRenderer(const CachedImageClient* client, const IntSize& containerSize, float containerZoom)
{
    ASSERT(client);
    ASSERT(!containerSize.isEmpty());

    FloatSize containerSizeWithoutZoom(containerSize);
    containerSizeWithoutZoom.scale(1 / containerZoom);

    ImageForContainerMap::iterator imageIt = m_imageForContainerMap.find(client);
    if (imageIt == m_imageForContainerMap.end()) {
        RefPtr<SVGImageForContainer> image = SVGImageForContainer::create(m_svgImage, containerSizeWithoutZoom, 1, containerZoom);
        m_imageForContainerMap.set(client, image);
    } else {
        imageIt->value->setSize(containerSizeWithoutZoom);
        imageIt->value->setZoom(containerZoom);
    }
}

IntSize SVGImageCache::imageSizeForRenderer(const RenderObject* renderer) const
{
    IntSize imageSize = m_svgImage->size();
    if (!renderer)
        return imageSize;

    ImageForContainerMap::const_iterator it = m_imageForContainerMap.find(renderer);
    if (it == m_imageForContainerMap.end())
        return imageSize;

    RefPtr<SVGImageForContainer> image = it->value;
    FloatSize size = image->containerSize();
    if (!size.isEmpty()) {
        size.scale(image->zoom());
        imageSize.setWidth(size.width());
        imageSize.setHeight(size.height());
    }

    return imageSize;
}

// FIXME: This doesn't take into account the animation timeline so animations will not
// restart on page load, nor will two animations in different pages have different timelines.
Image* SVGImageCache::imageForRenderer(const RenderObject* renderer)
{
    if (!renderer)
        return Image::nullImage();

    // The cache needs to know the size of the renderer before querying an image for it.
    ImageForContainerMap::iterator it = m_imageForContainerMap.find(renderer);
    if (it == m_imageForContainerMap.end())
        return Image::nullImage();

    RefPtr<SVGImageForContainer> image = it->value;
    ASSERT(!image->containerSize().isEmpty());

    // FIXME: Set the page scale in setContainerSizeForRenderer instead of looking it up here.
    Page* page = renderer->document()->page();
    image->setPageScale(page->deviceScaleFactor() * page->pageScaleFactor());

    return image.get();
}

} // namespace WebCore

#endif // ENABLE(SVG)
