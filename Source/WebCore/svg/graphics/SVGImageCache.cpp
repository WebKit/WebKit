/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "LayoutSize.h"
#include "LegacyRenderSVGRoot.h"
#include "LocalFrameView.h"
#include "SVGImage.h"
#include "SVGImageForContainer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGImageCache);

SVGImageCache::SVGImageCache(SVGImage* svgImage)
    : m_svgImage(svgImage)
{
    ASSERT(m_svgImage);
}

SVGImageCache::~SVGImageCache()
{
    m_imageForContainerMap.clear();
}

void SVGImageCache::registerContainerContext(const ImageContainerContextKey& key, ImageContainerContext&& containerContext)
{
    ASSERT(!containerContext.containerSize.isEmpty());
    ASSERT(containerContext.containerZoom);

    // SVG container has width or height less than 1 pixel.
    if (flooredIntSize(containerContext.containerSize).isEmpty())
        return;

    FloatSize containerSizeWithoutZoom(containerContext.containerSize);
    containerSizeWithoutZoom.scale(1 / containerContext.containerZoom);

    containerContext.containerSize = containerSizeWithoutZoom;

    m_imageForContainerMap.set(key, SVGImageForContainer::create(protect(m_svgImage).get(), WTF::move(containerContext)));
}

void SVGImageCache::unregisterContainerContext(const ImageContainerContextKey& key)
{
    m_imageForContainerMap.remove(key);
}

Image* SVGImageCache::findImageForKey(const ImageContainerContextKey* key) const
{
    return key ? m_imageForContainerMap.get(*key) : nullptr;
}

FloatSize SVGImageCache::imageSizeForKey(const ImageContainerContextKey* key) const
{
    SUPPRESS_UNCOUNTED_LOCAL auto* image = findImageForKey(key);
    return image ? image->size() : protect(m_svgImage)->size();
}

// FIXME: This doesn't take into account the animation timeline so animations will not
// restart on page load, nor will two animations in different pages have different timelines.
Image* SVGImageCache::imageForKey(const ImageContainerContextKey* key) const
{
    if (Image* image = findImageForKey(key)) {
        ASSERT(!image->size().isEmpty());
        return image;
    }

    return &Image::nullImage();
}

} // namespace WebCore
