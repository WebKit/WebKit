/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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
#include "ImageBySizeCache.h"

#include "Image.h"
#include "IntSize.h"
#include "RenderObject.h"

namespace WebCore {

ImageBySizeCache::ImageBySizeCache()
{
}

void ImageBySizeCache::addClient(const RenderObject* renderer, const IntSize& size)
{
    ASSERT(renderer);
    if (!size.isEmpty())
        m_sizes.add(size);
    
    RenderObjectSizeCountMap::iterator it = m_clients.find(renderer);
    if (it == m_clients.end())
        m_clients.add(renderer, SizeAndCount(size, 1));
    else {
        SizeAndCount& sizeCount = it->second;
        ++sizeCount.count;
    }
}

void ImageBySizeCache::removeClient(const RenderObject* renderer)
{
    ASSERT(renderer);
    RenderObjectSizeCountMap::iterator it = m_clients.find(renderer);
    ASSERT(it != m_clients.end());

    IntSize removedImageSize;
    SizeAndCount& sizeCount = it->second;
    IntSize size = sizeCount.size;
    if (!size.isEmpty()) {
        m_sizes.remove(size);
        if (!m_sizes.contains(size))
            m_images.remove(size);
    }
    
    if (!--sizeCount.count)
        m_clients.remove(renderer);
}

Image* ImageBySizeCache::getImage(const RenderObject* renderer, const IntSize& size)
{
    RenderObjectSizeCountMap::iterator it = m_clients.find(renderer);
    if (it != m_clients.end()) {
        SizeAndCount& sizeCount = it->second;
        IntSize oldSize = sizeCount.size;
        if (oldSize != size) {
            removeClient(renderer);
            addClient(renderer, size);
        }
    }

    // Don't generate an image for empty sizes.
    if (size.isEmpty())
        return 0;

    // Look up the image in our cache.
    return m_images.get(size).get();
}

void ImageBySizeCache::putImage(const IntSize& size, PassRefPtr<Image> image)
{
    m_images.add(size, image);
}

} // namespace WebCore
