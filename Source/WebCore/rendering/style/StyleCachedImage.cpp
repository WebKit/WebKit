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

#include "CSSImageSetValue.h"
#include "CachedImage.h"
#include "RenderElement.h"

namespace WebCore {

StyleCachedImage::StyleCachedImage(CachedImage* image, float scaleFactor, CSSImageSetValue* cssValue)
    : m_image(image)
    , m_scaleFactor(scaleFactor)
    , m_cssImageSetValue(cssValue)
{
    m_isCachedImage = true;
    m_image->addClient(this);
}

StyleCachedImage::~StyleCachedImage()
{
    m_image->removeClient(this);
}

PassRefPtr<CSSValue> StyleCachedImage::cssValue() const
{
    if (m_cssImageSetValue)
        return const_cast<CSSImageSetValue*>(m_cssImageSetValue);
    return CSSPrimitiveValue::create(m_image->url(), CSSPrimitiveValue::CSS_URI);
}

bool StyleCachedImage::canRender(const RenderObject* renderer, float multiplier) const
{
    return m_image->canRender(renderer, multiplier);
}

bool StyleCachedImage::isLoaded() const
{
    return m_image->isLoaded();
}

bool StyleCachedImage::errorOccurred() const
{
    return m_image->errorOccurred();
}

FloatSize StyleCachedImage::imageSize(const RenderElement* renderer, float multiplier) const
{
    FloatSize size = m_image->imageSizeForRenderer(renderer, multiplier);
    size.scale(1 / m_scaleFactor);
    return size;
}

bool StyleCachedImage::imageHasRelativeWidth() const
{
    return m_image->imageHasRelativeWidth();
}

bool StyleCachedImage::imageHasRelativeHeight() const
{
    return m_image->imageHasRelativeHeight();
}

void StyleCachedImage::computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    m_image->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool StyleCachedImage::usesImageContainerSize() const
{
    return m_image->usesImageContainerSize();
}

void StyleCachedImage::setContainerSizeForRenderer(const RenderElement* renderer, const FloatSize& imageContainerSize, float imageContainerZoomFactor)
{
    m_image->setContainerSizeForRenderer(renderer, LayoutSize(imageContainerSize), imageContainerZoomFactor);
}

void StyleCachedImage::addClient(RenderElement* renderer)
{
    m_image->addClient(renderer);
}

void StyleCachedImage::removeClient(RenderElement* renderer)
{
    m_image->removeClient(renderer);
}

RefPtr<Image> StyleCachedImage::image(RenderElement* renderer, const FloatSize&) const
{
    return m_image->imageForRenderer(renderer);
}

float StyleCachedImage::imageScaleFactor() const
{
    return m_scaleFactor;
}

bool StyleCachedImage::knownToBeOpaque(const RenderElement* renderer) const
{
    return m_image->currentFrameKnownToBeOpaque(renderer);
}

}
