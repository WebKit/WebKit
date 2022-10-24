/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

Ref<StyleCachedImage> StyleCachedImage::create(Ref<CSSImageValue> cssValue, float scaleFactor)
{
    return adoptRef(*new StyleCachedImage(WTFMove(cssValue), scaleFactor));
}

Ref<StyleCachedImage> StyleCachedImage::copyOverridingScaleFactor(StyleCachedImage& other, float scaleFactor)
{
    if (other.m_scaleFactor == scaleFactor)
        return other;
    return StyleCachedImage::create(other.m_cssValue, scaleFactor);
}

StyleCachedImage::StyleCachedImage(Ref<CSSImageValue>&& cssValue, float scaleFactor)
    : StyleImage { Type::CachedImage }
    , m_cssValue { WTFMove(cssValue) }
    , m_scaleFactor { scaleFactor }
{
    m_cachedImage = m_cssValue->cachedImage();
    if (m_cachedImage)
        m_isPending = false;
}

StyleCachedImage::~StyleCachedImage() = default;

bool StyleCachedImage::operator==(const StyleImage& other) const
{
    return is<StyleCachedImage>(other) && equals(downcast<StyleCachedImage>(other));
}

bool StyleCachedImage::equals(const StyleCachedImage& other) const
{
    if (&other == this)
        return true;
    if (m_scaleFactor != other.m_scaleFactor)
        return false;
    if (m_cssValue.ptr() == other.m_cssValue.ptr() || m_cssValue->equals(other.m_cssValue.get()))
        return true;
    if (m_cachedImage && m_cachedImage == other.m_cachedImage)
        return true;
    return false;
}

URL StyleCachedImage::imageURL() const
{
    return m_cssValue->imageURL();
}

URL StyleCachedImage::reresolvedURL(const Document& document) const
{
    return m_cssValue->reresolvedURL(document);
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

Ref<CSSValue> StyleCachedImage::computedStyleValue(const RenderStyle&) const
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

void StyleCachedImage::addClient(RenderElement& renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    m_cachedImage->addClient(renderer);
}

void StyleCachedImage::removeClient(RenderElement& renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    m_cachedImage->removeClient(renderer);
}

bool StyleCachedImage::hasClient(RenderElement& renderer) const
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return false;
    return m_cachedImage->hasClient(renderer);
}

bool StyleCachedImage::hasImage() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->hasImage();
}

RefPtr<Image> StyleCachedImage::image(const RenderElement* renderer, const FloatSize&) const
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

bool StyleCachedImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_cachedImage && m_cachedImage->currentFrameKnownToBeOpaque(&renderer);
}

bool StyleCachedImage::usesDataProtocol() const
{
    return m_cssValue->imageURL().protocolIsData();
}

}
