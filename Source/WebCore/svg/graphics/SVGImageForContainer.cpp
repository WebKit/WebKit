/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "SVGImageForContainer.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "Image.h"

namespace WebCore {


SVGImageForContainer::SVGImageForContainer(SVGImage* image, const FloatSize& containerSize, float containerZoom, const URL& initialFragmentURL)
    : m_image(image)
    , m_containerSize(containerSize)
    , m_containerZoom(containerZoom)
    , m_initialFragmentURL(initialFragmentURL)
{
}

FloatSize SVGImageForContainer::size(ImageOrientation) const
{
    FloatSize scaledContainerSize(m_containerSize);
    scaledContainerSize.scale(m_containerZoom);
    return FloatSize(roundedIntSize(scaledContainerSize));
}

ImageDrawResult SVGImageForContainer::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    return m_image->drawForContainer(context, m_containerSize, m_containerZoom, m_initialFragmentURL, dstRect, srcRect, options);
}

void SVGImageForContainer::drawPattern(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
    const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    m_image->drawPatternForContainer(context, m_containerSize, m_containerZoom, m_initialFragmentURL, srcRect, patternTransform, phase, spacing, dstRect, options);
}

RefPtr<NativeImage> SVGImageForContainer::nativeImageForCurrentFrame()
{
    return m_image->nativeImageForCurrentFrame();
}

} // namespace WebCore
