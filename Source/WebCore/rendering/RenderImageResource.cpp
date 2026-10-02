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
#include "Font.h"
#include "FontCascadeInlines.h"
#include "NullGraphicsContext.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderObjectDocument.h"
#include "ReplacedElementIntrinsicSizing.h"
#include "StyleCachedImage.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleImageDrawingExtras.h"
#include "StyleInvalidImage.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#include "MultiRepresentationHEICMetrics.h"
#endif

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
        protect(m_styleImage)->addClient(renderer);
}

void RenderImageResource::willBeDestroyed()
{
    RefPtr cachedImage = this->cachedImage();
    Ref image = this->image();
    if (m_styleImage && m_renderer)
        protect(m_styleImage)->removeClient(*m_renderer);
    if (image->isAnimated() && cachedImage && !cachedImage->hasRendererClients())
        image->stopAnimation();
}

void RenderImageResource::clearCachedImage()
{
    if (!m_styleImage)
        return;

    if (m_renderer)
        protect(m_styleImage)->removeClient(*m_renderer);

    m_styleImage = nullptr;
}

void RenderImageResource::setCachedImage(CachedImage* newImage)
{
    RefPtr existingCachedImage = this->cachedImage();
    if (existingCachedImage == newImage)
        return;

    if (m_styleImage && m_renderer) {
        RefPtr styleImage = m_styleImage;
        styleImage->removeClient(*m_renderer);
    }

    if (!m_renderer) {
        // removeClient may have destroyed the renderer.
        // FIXME: Document under what circumstances this can happen.
        return;
    }

    if (!newImage)
        m_styleImage = nullptr;
    else {
        m_styleImage = Style::CachedImage::create(*newImage);

        RefPtr styleImage = m_styleImage;
        styleImage->addClient(*m_renderer);

        if (styleImage->errorOccurred())
            m_renderer->imageChanged(styleImage->cachedImage());
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
    if (!m_styleImage)
        return Image::nullImage();

    Ref styleImage = *m_styleImage;
    if (styleImage->isPending())
        return Image::nullImage();

    RefPtr image = styleImage->image(m_renderer.get(), size, NullGraphicsContext());
    if (!image)
        return Image::nullImage();

    return image.releaseNonNull();
}

bool RenderImageResource::currentFrameIsComplete() const
{
    if (!m_styleImage)
        return false;
    return protect(m_styleImage)->currentFrameIsComplete(m_renderer.get());
}

Style::ImageDrawingExtras RenderImageResource::drawingExtras(const URL& url) const
{
    if (!m_styleImage || !m_renderer)
        return { };
    return protect(m_styleImage)->drawingExtrasForRenderer(*m_renderer, url);
}

LayoutSize RenderImageResource::imageSize(float multiplier, CachedImage::SizeType type) const
{
    if (!m_styleImage)
        return { };
    return LayoutSize(protect(m_styleImage)->imageSize(m_renderer.get(), multiplier, type));
}

bool RenderImageResource::hasDecodedImage() const
{
    return m_styleImage && protect(m_styleImage)->hasDecodedImage();
}

NaturalDimensions RenderImageResource::naturalDimensions() const
{
    if (!m_renderer)
        return NaturalDimensions::none();

#if ENABLE(MULTI_REPRESENTATION_HEIC)
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(m_renderer.get()); renderImage && renderImage->isMultiRepresentationHEIC())
        return NaturalDimensions::fixed(renderImage->style().fontCascade().primaryFont().metricsForMultiRepresentationHEIC().size());
#endif

    if (!m_styleImage)
        return NaturalDimensions::none();
    return protect(m_styleImage)->naturalDimensions(*m_renderer, ReplacedElementIntrinsicSizing { });
}

std::optional<FloatSize> RenderImageResource::usedImageSize(FloatSize containerSize) const
{
    if (!hasDecodedImage() || !m_renderer)
        return std::nullopt;

    auto naturalDimensions = this->naturalDimensions();
    if (naturalDimensions.width && naturalDimensions.height)
        return FloatSize { *naturalDimensions.width, *naturalDimensions.height };

    if (containerSize.isEmpty())
        return std::nullopt;

    if (auto usedZoom = m_renderer->style().usedZoom(); usedZoom != 1)
        containerSize.scale(1 / usedZoom);
    return containerSize;
}

float RenderImageResource::density() const
{
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(m_renderer.get()))
        return renderImage->imageDevicePixelRatio();
    return 1;
}

LayoutSize RenderImageResource::intrinsicSize(float multiplier) const
{
    if (!hasDecodedImage())
        return { };

    auto size = ReplacedElementIntrinsicSizing { density() }.resolve(naturalDimensions()).size();
    size.scale(multiplier);
    return LayoutSize(size / protect(m_styleImage)->imageScaleFactor());
}

} // namespace WebCore
