/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include "NullGraphicsContext.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleCachedImage.h"
#include "StyleInvalidImage.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderImageResource);

RenderImageResource::RenderImageResource() = default;

RenderImageResource::RenderImageResource(Style::Image* styleImage)
    : m_styleImage { styleImage }
{
}

RenderImageResource::~RenderImageResource() = default;

void RenderImageResource::initialize(RenderElement& renderer)
{
    ASSERT(!m_renderer);

    m_renderer = renderer;
    if (m_styleImage)
        m_styleImage->addClient(renderer);
}

void RenderImageResource::willBeDestroyed()
{
    image()->stopAnimation();
    if (m_styleImage && m_renderer)
        m_styleImage->removeClient(*m_renderer);
}

void RenderImageResource::clearCachedImage()
{
    if (!m_styleImage)
        return;

    if (m_renderer)
        m_styleImage->removeClient(*m_renderer);

    m_styleImage = nullptr;
}

void RenderImageResource::setCachedImage(CachedResourceHandle<CachedImage>&& newImage)
{
    auto existingCachedImage = this->cachedImage();
    if (existingCachedImage == newImage)
        return;

    if (m_styleImage && m_renderer)
        m_styleImage->removeClient(*m_renderer);

    if (!m_renderer) {
        // removeClient may have destroyed the renderer.
        // FIXME: Document under what circumstances this can happen.
        return;
    }

    if (!newImage)
        m_styleImage = nullptr;
    else {
        m_styleImage = Style::CachedImage::create(*newImage);

        m_styleImage->addClient(*m_renderer);

        if (m_styleImage->errorOccurred())
            m_renderer->imageChanged(m_styleImage->cachedImage());
    }
}

void RenderImageResource::resetAnimation()
{
    if (!m_styleImage)
        return;

    image()->resetAnimation();

    if (m_renderer && !m_renderer->needsLayout())
        m_renderer->repaint();
}

Ref<Image> RenderImageResource::image(const IntSize& size) const
{
    // Generated content may trigger calls to image() while we're still pending, don't assert but gracefully exit.
    if (!m_styleImage || m_styleImage->isPending())
        return Image::nullImage();
    if (RefPtr image = m_styleImage->image(m_renderer.get(), size, NullGraphicsContext()))
        return image.releaseNonNull();
    return Image::nullImage();
}

bool RenderImageResource::currentFrameIsComplete() const
{
    if (!m_styleImage)
        return false;
    return m_styleImage->currentFrameIsComplete(m_renderer.get());
}

void RenderImageResource::setContainerContext(const IntSize& imageContainerSize, const URL& url)
{
    if (!m_styleImage || !m_renderer)
        return;
    m_styleImage->setContainerContextForRenderer(*m_renderer, imageContainerSize, m_renderer->style().usedZoom(), url);
}

LayoutSize RenderImageResource::imageSize(float multiplier, CachedImage::SizeType type) const
{
    if (!m_styleImage)
        return { };
    auto size = LayoutSize(m_styleImage->imageSize(m_renderer.get(), multiplier, type));
    if (auto* renderImage = dynamicDowncast<RenderImage>(m_renderer.get()))
        size.scale(renderImage->imageDevicePixelRatio());
    return size;
}

} // namespace WebCore
