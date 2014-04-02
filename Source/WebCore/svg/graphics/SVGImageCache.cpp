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

#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "LayoutSize.h"
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

    m_imageForContainerMap.remove(client);
}

void SVGImageCache::setContainerSizeForRenderer(const CachedImageClient* client, const LayoutSize& containerSize, float containerZoom)
{
    ASSERT(client);
    ASSERT(!containerSize.isEmpty());
    ASSERT(containerZoom);

    FloatSize containerSizeWithoutZoom(containerSize);
    containerSizeWithoutZoom.scale(1 / containerZoom);

    m_imageForContainerMap.set(client, SVGImageForContainer::create(m_svgImage, containerSizeWithoutZoom, containerZoom));
}

FloatSize SVGImageCache::imageSizeForRenderer(const RenderObject* renderer) const
{
    FloatSize imageSize = m_svgImage->size();
    if (!renderer)
        return imageSize;

    ImageForContainerMap::const_iterator it = m_imageForContainerMap.find(renderer);
    if (it == m_imageForContainerMap.end())
        return imageSize;

    RefPtr<SVGImageForContainer> imageForContainer = it->value;
    ASSERT(!imageForContainer->size().isEmpty());
    return imageForContainer->size();
}

// FIXME: This doesn't take into account the animation timeline so animations will not
// restart on page load, nor will two animations in different pages have different timelines.
Image* SVGImageCache::imageForRenderer(const RenderObject* renderer)
{
    if (!renderer)
        return Image::nullImage();

    ImageForContainerMap::iterator it = m_imageForContainerMap.find(renderer);
    if (it == m_imageForContainerMap.end())
        return Image::nullImage();

    RefPtr<SVGImageForContainer> imageForContainer = it->value;
    
    Node* node = renderer->node();
    if (node && isHTMLImageElement(node)) {
        const AtomicString& urlString = toHTMLImageElement(node)->imageSourceURL();
        URL url = node->document().completeURL(urlString);
        imageForContainer->setURL(url);
    }
        
    ASSERT(!imageForContainer->size().isEmpty());
    return imageForContainer.get();
}

} // namespace WebCore
