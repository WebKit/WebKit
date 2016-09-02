/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "CSSCursorImageValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CachedImage.h"
#include "RenderElement.h"

namespace WebCore {

StyleCachedImage::StyleCachedImage(CSSValue& cssValue)
    : m_cssValue(cssValue)
{
    ASSERT(is<CSSImageValue>(m_cssValue) || is<CSSImageSetValue>(m_cssValue) || is<CSSCursorImageValue>(m_cssValue));

    m_isCachedImage = true;
}

StyleCachedImage::~StyleCachedImage()
{
}

bool StyleCachedImage::operator==(const StyleImage& other) const
{
    if (!is<StyleCachedImage>(other))
        return false;
    auto& otherCached = downcast<StyleCachedImage>(other);
    if (&otherCached == this)
        return true;
    if (m_cssValue.ptr() == otherCached.m_cssValue.ptr())
        return true;
    if (m_cachedImage && m_cachedImage == otherCached.m_cachedImage)
        return true;
    return false;
}

CachedImage* StyleCachedImage::cachedImage() const
{
    if (!m_cachedImage) {
        if (is<CSSImageValue>(m_cssValue)) {
            auto& imageValue = downcast<CSSImageValue>(m_cssValue.get());
            m_cachedImage = imageValue.cachedImage();
        } else if (is<CSSImageSetValue>(m_cssValue)) {
            auto& imageSetValue = downcast<CSSImageSetValue>(m_cssValue.get());
            m_cachedImage = imageSetValue.cachedImage();
            m_scaleFactor = imageSetValue.bestFitScaleFactor();
        } else if (is<CSSCursorImageValue>(m_cssValue.get())) {
            auto& cursorValue = downcast<CSSCursorImageValue>(m_cssValue.get());
            m_cachedImage = cursorValue.cachedImage();
            m_scaleFactor = cursorValue.scaleFactor();
        }
    }
    return m_cachedImage.get();
}

PassRefPtr<CSSValue> StyleCachedImage::cssValue() const
{
    return const_cast<CSSValue*>(m_cssValue.ptr());
}

bool StyleCachedImage::canRender(const RenderObject* renderer, float multiplier) const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->canRender(renderer, multiplier);
}

bool StyleCachedImage::isPending() const
{
    return !m_cachedImage;
}

bool StyleCachedImage::isLoaded() const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->isLoaded();
}

bool StyleCachedImage::errorOccurred() const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->errorOccurred();
}

FloatSize StyleCachedImage::imageSize(const RenderElement* renderer, float multiplier) const
{
    auto* image = cachedImage();
    if (!image)
        return { };
    FloatSize size = image->imageSizeForRenderer(renderer, multiplier);
    size.scale(1 / m_scaleFactor);
    return size;
}

bool StyleCachedImage::imageHasRelativeWidth() const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->imageHasRelativeWidth();
}

bool StyleCachedImage::imageHasRelativeHeight() const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->imageHasRelativeHeight();
}

void StyleCachedImage::computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    auto* image = cachedImage();
    if (!image)
        return;
    image->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool StyleCachedImage::usesImageContainerSize() const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->usesImageContainerSize();
}

void StyleCachedImage::setContainerSizeForRenderer(const RenderElement* renderer, const FloatSize& imageContainerSize, float imageContainerZoomFactor)
{
    auto* image = cachedImage();
    if (!image)
        return;
    image->setContainerSizeForRenderer(renderer, LayoutSize(imageContainerSize), imageContainerZoomFactor);
}

void StyleCachedImage::addClient(RenderElement* renderer)
{
    auto* image = cachedImage();
    if (!image)
        return;
    image->addClient(renderer);
}

void StyleCachedImage::removeClient(RenderElement* renderer)
{
    auto* image = cachedImage();
    if (!image)
        return;
    image->removeClient(renderer);
}

RefPtr<Image> StyleCachedImage::image(RenderElement* renderer, const FloatSize&) const
{
    auto* image = cachedImage();
    if (!image)
        return nullptr;
    return image->imageForRenderer(renderer);
}

float StyleCachedImage::imageScaleFactor() const
{
    return m_scaleFactor;
}

bool StyleCachedImage::knownToBeOpaque(const RenderElement* renderer) const
{
    auto* image = cachedImage();
    if (!image)
        return false;
    return image->currentFrameKnownToBeOpaque(renderer);
}

}
