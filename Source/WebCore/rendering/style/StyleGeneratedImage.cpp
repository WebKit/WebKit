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
#include "StyleGeneratedImage.h"

#include "CSSImageGeneratorValue.h"
#include "RenderElement.h"
#include "StyleResolver.h"

namespace WebCore {
    
StyleGeneratedImage::StyleGeneratedImage(Ref<CSSImageGeneratorValue>&& value)
    : m_imageGeneratorValue(WTFMove(value))
    , m_fixedSize(m_imageGeneratorValue->isFixedSize())
{
    m_isGeneratedImage = true;
}

bool StyleGeneratedImage::operator==(const StyleImage& other) const
{
    if (is<StyleGeneratedImage>(other))
        return arePointingToEqualData(m_imageGeneratorValue.ptr(), downcast<StyleGeneratedImage>(other).m_imageGeneratorValue.ptr());
    return false;
}

Ref<CSSValue> StyleGeneratedImage::cssValue() const
{
    return m_imageGeneratorValue.copyRef();
}

bool StyleGeneratedImage::isPending() const
{
    return m_imageGeneratorValue->isPending();
}

void StyleGeneratedImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    m_imageGeneratorValue->loadSubimages(loader, options);
}

FloatSize StyleGeneratedImage::imageSize(const RenderElement* renderer, float multiplier) const
{
    if (!m_fixedSize)
        return m_containerSize;

    if (!renderer)
        return { };

    FloatSize fixedSize = m_imageGeneratorValue->fixedSize(*renderer);
    if (multiplier == 1.0f)
        return fixedSize;

    float width = fixedSize.width() * multiplier;
    float height = fixedSize.height() * multiplier;

    // Don't let images that have a width/height >= 1 shrink below 1 device pixel when zoomed.
    float deviceScaleFactor = renderer->document().deviceScaleFactor();
    if (fixedSize.width() > 0)
        width = std::max<float>(1 / deviceScaleFactor, width);
    if (fixedSize.height() > 0)
        height = std::max<float>(1 / deviceScaleFactor, height);

    return { width, height };
}

void StyleGeneratedImage::computeIntrinsicDimensions(const RenderElement* renderer, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    // At a zoom level of 1 the image is guaranteed to have a device pixel size.
    FloatSize size = floorSizeToDevicePixels(LayoutSize(imageSize(renderer, 1)), renderer ? renderer->document().deviceScaleFactor() : 1);
    intrinsicWidth = Length(size.width(), LengthType::Fixed);
    intrinsicHeight = Length(size.height(), LengthType::Fixed);
    intrinsicRatio = size;
}

void StyleGeneratedImage::addClient(RenderElement& renderer)
{
    m_imageGeneratorValue->addClient(renderer);
}

void StyleGeneratedImage::removeClient(RenderElement& renderer)
{
    m_imageGeneratorValue->removeClient(renderer);
}

bool StyleGeneratedImage::hasClient(RenderElement& renderer) const
{
    return m_imageGeneratorValue->clients().contains(&renderer);
}

RefPtr<Image> StyleGeneratedImage::image(RenderElement* renderer, const FloatSize& size) const
{
    return renderer ? m_imageGeneratorValue->image(*renderer, size) : &Image::nullImage();
}

bool StyleGeneratedImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_imageGeneratorValue->knownToBeOpaque(renderer);
}

}
