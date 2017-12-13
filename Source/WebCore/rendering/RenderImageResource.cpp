/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 *
 */

#include "config.h"
#include "RenderImageResource.h"

#include "CachedImage.h"
#include "Image.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderImageResourceStyleImage.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderImageResource);

RenderImageResource::RenderImageResource()
{
}

void RenderImageResource::initialize(RenderElement& renderer, CachedImage* styleCachedImage)
{
    ASSERT(!m_renderer);
    ASSERT(!m_cachedImage);
    m_renderer = makeWeakPtr(renderer);
    m_cachedImage = styleCachedImage;
    m_cachedImageRemoveClientIsNeeded = !styleCachedImage;
}

void RenderImageResource::shutdown()
{
    image()->stopAnimation();
    setCachedImage(nullptr);
}

void RenderImageResource::setCachedImage(CachedImage* newImage)
{
    if (m_cachedImage == newImage)
        return;

    ASSERT(m_renderer);
    if (m_cachedImage && m_cachedImageRemoveClientIsNeeded)
        m_cachedImage->removeClient(*renderer());
    m_cachedImage = newImage;
    m_cachedImageRemoveClientIsNeeded = true;
    if (!m_cachedImage)
        return;

    m_cachedImage->addClient(*renderer());
    if (m_cachedImage->errorOccurred())
        renderer()->imageChanged(m_cachedImage.get());
}

void RenderImageResource::resetAnimation()
{
    if (!m_cachedImage)
        return;

    ASSERT(m_renderer);
    image()->resetAnimation();

    if (!renderer()->needsLayout())
        renderer()->repaint();
}

RefPtr<Image> RenderImageResource::image(const IntSize&) const
{
    if (!m_cachedImage)
        return &Image::nullImage();
    if (auto image = m_cachedImage->imageForRenderer(renderer()))
        return image;
    return &Image::nullImage();
}

void RenderImageResource::setContainerContext(const IntSize& imageContainerSize, const URL& imageURL)
{
    if (!m_cachedImage)
        return;
    ASSERT(m_renderer);
    m_cachedImage->setContainerContextForClient(*renderer(), imageContainerSize, renderer()->style().effectiveZoom(), imageURL);
}

LayoutSize RenderImageResource::imageSize(float multiplier, CachedImage::SizeType type) const
{
    if (!m_cachedImage)
        return LayoutSize();
    LayoutSize size = m_cachedImage->imageSizeForRenderer(renderer(), multiplier, type);
    if (is<RenderImage>(renderer()))
        size.scale(downcast<RenderImage>(*renderer()).imageDevicePixelRatio());
    return size;
}

} // namespace WebCore
