/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSVariableData.h"
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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MultiImage);

MultiImage::MultiImage(Type type)
    : Image { type }
{
}

MultiImage::~MultiImage() = default;

bool MultiImage::equals(const MultiImage& other) const
{
    return !m_isPending && !other.m_isPending && arePointingToEqualData(m_selectedImage, other.m_selectedImage);
}

void MultiImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    ASSERT(m_isPending);
    ASSERT(loader.document());

    m_isPending = false;

    auto bestFitImage = selectBestFitImage(*loader.document());

    ASSERT(is<CachedImage>(bestFitImage.image) || is<GeneratedImage>(bestFitImage.image));

    if (is<GeneratedImage>(bestFitImage.image)) {
        m_selectedImage = bestFitImage.image;
        m_selectedImage->load(loader, options);
        return;
    }

    if (RefPtr styleCachedImage = dynamicDowncast<CachedImage>(bestFitImage.image)) {
        if (styleCachedImage->imageScaleFactor() == bestFitImage.scaleFactor)
            m_selectedImage = WTF::move(styleCachedImage);
        else
            m_selectedImage = CachedImage::copyOverridingScaleFactor(*styleCachedImage, bestFitImage.scaleFactor);

        if (m_selectedImage->isPending())
            m_selectedImage->load(loader, options);
        return;
    }
}

WebCore::CachedImage* MultiImage::cachedImage() const
{
    if (!m_selectedImage)
        return nullptr;
    return m_selectedImage->cachedImage();
}

WrappedImagePtr MultiImage::data() const
{
    if (!m_selectedImage)
        return nullptr;
    return m_selectedImage->data();
}

bool MultiImage::canRender(const RenderElement* renderer, float multiplier) const
{
    return m_selectedImage && m_selectedImage->canRender(renderer, multiplier);
}

bool MultiImage::isLoaded(const RenderElement* renderer) const
{
    return m_selectedImage && m_selectedImage->isLoaded(renderer);
}

bool MultiImage::errorOccurred() const
{
    return m_selectedImage && m_selectedImage->errorOccurred();
}

FloatSize MultiImage::imageSize(const RenderElement* renderer, float multiplier, WebCore::CachedImage::SizeType sizeType) const
{
    if (!m_selectedImage)
        return { };
    return m_selectedImage->imageSize(renderer, multiplier, sizeType);
}

bool MultiImage::imageHasRelativeWidth() const
{
    return m_selectedImage && m_selectedImage->imageHasRelativeWidth();
}

bool MultiImage::imageHasRelativeHeight() const
{
    return m_selectedImage && m_selectedImage->imageHasRelativeHeight();
}

void MultiImage::computeIntrinsicDimensions(const RenderElement* element, float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->computeIntrinsicDimensions(element, intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool MultiImage::usesImageContainerSize() const
{
    return m_selectedImage && m_selectedImage->usesImageContainerSize();
}

void MultiImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom, const WTF::URL& url)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->setContainerContextForRenderer(renderer, containerSize, containerZoom, url);
}

void MultiImage::addClient(RenderElement& renderer)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->addClient(renderer);
}

void MultiImage::removeClient(RenderElement& renderer)
{
    if (!m_selectedImage)
        return;
    m_selectedImage->removeClient(renderer);
}

bool MultiImage::hasClient(RenderElement& renderer) const
{
    if (!m_selectedImage)
        return false;
    return m_selectedImage->hasClient(renderer);
}

RefPtr<WebCore::Image> MultiImage::image(const RenderElement* renderer, const FloatSize& size, const GraphicsContext& destinationContext, bool isForFirstLine) const
{
    if (!m_selectedImage)
        return nullptr;
    return m_selectedImage->image(renderer, size, destinationContext, isForFirstLine);
}

bool MultiImage::currentFrameIsComplete(const RenderElement* renderer) const
{
    return m_selectedImage && m_selectedImage->currentFrameIsComplete(renderer);
}

float MultiImage::imageScaleFactor() const
{
    if (!m_selectedImage)
        return 1;
    return m_selectedImage->imageScaleFactor();
}

bool MultiImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_selectedImage && m_selectedImage->knownToBeOpaque(renderer);
}

} // namespace Style
} // namespace WebCore
