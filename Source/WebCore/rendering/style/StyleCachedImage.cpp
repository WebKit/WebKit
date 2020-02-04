/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
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
#include "StyleCachedImage.h"

#include "CSSImageValue.h"
#include "CachedImage.h"
#include "RenderElement.h"
#include "RenderView.h"

namespace WebCore {

Ref<StyleCachedImage> StyleCachedImage::create(CSSImageValue& cssValue, float scaleFactor)
{
    return adoptRef(*new StyleCachedImage(cssValue, scaleFactor));
}

StyleCachedImage::StyleCachedImage(CSSImageValue& cssValue, float scaleFactor)
    : m_cssValue(cssValue)
    , m_scaleFactor(scaleFactor)
{
    m_isCachedImage = true;
    m_cachedImage = m_cssValue->cachedImage();
    if (m_cachedImage)
        m_isPending = false;
}

StyleCachedImage::~StyleCachedImage() = default;

bool StyleCachedImage::operator==(const StyleImage& other) const
{
    if (!is<StyleCachedImage>(other))
        return false;
    auto& otherCached = downcast<StyleCachedImage>(other);
    if (&otherCached == this)
        return true;
    if (m_scaleFactor != otherCached.m_scaleFactor)
        return false;
    if (m_cssValue.ptr() == otherCached.m_cssValue.ptr())
        return true;
    if (m_cachedImage && m_cachedImage == otherCached.m_cachedImage)
        return true;
    return false;
}

URL StyleCachedImage::imageURL()
{
    return m_cssValue->url();
}

void StyleCachedImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    ASSERT(m_isPending);
    m_isPending = false;
    m_cachedImage = m_cssValue->loadImage(loader, options);
}

CachedImage* StyleCachedImage::cachedImage() const
{
    return m_cachedImage.get();
}

Ref<CSSValue> StyleCachedImage::cssValue() const
{
    return m_cssValue.copyRef();
}

bool StyleCachedImage::canRender(const RenderElement* renderer, float multiplier) const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->canRender(renderer, multiplier);
}

bool StyleCachedImage::isPending() const
{
    return m_isPending;
}

bool StyleCachedImage::isLoaded() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->isLoaded();
}

bool StyleCachedImage::errorOccurred() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->errorOccurred();
}

FloatSize StyleCachedImage::imageSize(const RenderElement* renderer, float multiplier) const
{
    if (!m_cachedImage)
        return { };
    FloatSize size = m_cachedImage->imageSizeForRenderer(renderer, multiplier);
    size.scale(1 / m_scaleFactor);
    return size;
}

bool StyleCachedImage::imageHasRelativeWidth() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->imageHasRelativeWidth();
}

bool StyleCachedImage::imageHasRelativeHeight() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->imageHasRelativeHeight();
}

void StyleCachedImage::computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (!m_cachedImage)
        return;
    m_cachedImage->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool StyleCachedImage::usesImageContainerSize() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->usesImageContainerSize();
}

void StyleCachedImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom)
{
    if (!m_cachedImage)
        return;
    m_cachedImage->setContainerContextForClient(renderer, LayoutSize(containerSize), containerZoom, imageURL());
}

void StyleCachedImage::addClient(RenderElement* renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    ASSERT(renderer);
    m_cachedImage->addClient(*renderer);
}

void StyleCachedImage::removeClient(RenderElement* renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    ASSERT(renderer);

    m_cachedImage->removeClient(*renderer);
}

RefPtr<Image> StyleCachedImage::image(RenderElement* renderer, const FloatSize&) const
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return nullptr;
    return m_cachedImage->imageForRenderer(renderer);
}

float StyleCachedImage::imageScaleFactor() const
{
    return m_scaleFactor;
}

bool StyleCachedImage::knownToBeOpaque(const RenderElement* renderer) const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->currentFrameKnownToBeOpaque(renderer);
}

}
