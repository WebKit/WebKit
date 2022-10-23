/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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
#include "StyleMultiImage.h"

#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSFilterImageValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "RenderElement.h"
#include "RenderView.h"
#include "StyleCachedImage.h"
#include "StyleCanvasImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleFilterImage.h"
#include "StyleGradientImage.h"
#include "StyleNamedImage.h"
#include "StylePaintImage.h"

namespace WebCore {

StyleMultiImage::StyleMultiImage(Type type)
    : StyleImage { type }
{
}

StyleMultiImage::~StyleMultiImage() = default;

bool StyleMultiImage::equals(const StyleMultiImage& other) const
{
    return (!m_isPending && !other.m_isPending && m_selectedImage.get() == other.m_selectedImage.get());
}

void StyleMultiImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    ASSERT(m_isPending);
    ASSERT(loader.document());

    m_isPending = false;

    auto bestFitImage = selectBestFitImage(*loader.document());
    ASSERT(is<StyleCachedImage>(bestFitImage.image) || is<StyleGeneratedImage>(bestFitImage.image));

    if (is<StyleGeneratedImage>(bestFitImage.image)) {
        m_selectedImage = bestFitImage.image;
        m_selectedImage->load(loader, options);
        return;
    }
    
    if (is<StyleCachedImage>(bestFitImage.image)) {
        if (downcast<StyleCachedImage>(*bestFitImage.image).imageScaleFactor() == bestFitImage.scaleFactor)
            m_selectedImage = bestFitImage.image;
        else
            m_selectedImage = StyleCachedImage::copyOverridingScaleFactor(downcast<StyleCachedImage>(*bestFitImage.image), bestFitImage.scaleFactor);

        if (m_selectedImage->isPending())
            m_selectedImage->load(loader, options);
        return;
    }
}

CachedImage* StyleMultiImage::cachedImage() const
{
    if (!m_selectedImage)
        return nullptr;
    return m_selectedImage->cachedImage();
}

WrappedImagePtr StyleMultiImage::data() const
{
    if (!m_selectedImage)
        return nullptr;
    return m_selectedImage->data();
}

bool StyleMultiImage::canRender(const RenderElement* renderer, float multiplier) const
{
    return m_selectedImage && m_selectedImage->canRender(renderer, multiplier);
}

bool StyleMultiImage::isPending() const
{
    return m_isPending;
}

bool StyleMultiImage::isLoaded() const
{
    return m_selectedImage && m_selectedImage->isLoaded();
}

bool StyleMultiImage::errorOccurred() const
{
    return m_selectedImage && m_selectedImage->errorOccurred();
}

FloatSize StyleMultiImage::imageSize(const RenderElement* renderer, float multiplier) const
{
    if (!m_selectedImage)
        return { };
    return m_selectedImage->imageSize(renderer, multiplier);
}

bool StyleMultiImage::imageHasRelativeWidth() const
{
    return m_selectedImage && m_selectedImage->imageHasRelativeWidth();
}

bool StyleMultiImage::imageHasRelativeHeight() const
{
    return m_selectedImage && m_selectedImage->imageHasRelativeHeight();
}

void StyleMultiImage::computeIntrinsicDimensions(const RenderElement* element, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->computeIntrinsicDimensions(element, intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool StyleMultiImage::usesImageContainerSize() const
{
    return m_selectedImage && m_selectedImage->usesImageContainerSize();
}

void StyleMultiImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->setContainerContextForRenderer(renderer, containerSize, containerZoom);
}

void StyleMultiImage::addClient(RenderElement& renderer)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->addClient(renderer);
}

void StyleMultiImage::removeClient(RenderElement& renderer)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->removeClient(renderer);
}

bool StyleMultiImage::hasClient(RenderElement& renderer) const
{
    if (!m_selectedImage)
        return false;
    return m_selectedImage->hasClient(renderer);
}

RefPtr<Image> StyleMultiImage::image(const RenderElement* renderer, const FloatSize& size) const
{
    if (!m_selectedImage)
        return nullptr;
    return m_selectedImage->image(renderer, size);
}

float StyleMultiImage::imageScaleFactor() const
{
    if (!m_selectedImage)
        return 1;
    return m_selectedImage->imageScaleFactor();
}

bool StyleMultiImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_selectedImage && m_selectedImage->knownToBeOpaque(renderer);
}

} // namespace WebCore
