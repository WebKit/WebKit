/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderSVGResourceMasker.h"
#include "RenderView.h"
#include "SVGImage.h"
#include "SVGMaskElement.h"
#include "SVGResourceImage.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"

namespace WebCore {

Ref<StyleCachedImage> StyleCachedImage::create(Ref<CSSImageValue> cssValue, float scaleFactor)
{
    return adoptRef(*new StyleCachedImage(WTFMove(cssValue), scaleFactor));
}

Ref<StyleCachedImage> StyleCachedImage::create(CachedImage& cachedImage, float scaleFactor)
{
    return adoptRef(*new StyleCachedImage(cachedImage, scaleFactor));
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

StyleCachedImage::StyleCachedImage(CachedImage& cachedImage, float scaleFactor)
    : StyleCachedImage { CSSImageValue::create(cachedImage), scaleFactor }
{
}

StyleCachedImage::~StyleCachedImage() = default;

Ref<StyleImage> StyleCachedImage::copyOverridingScaleFactor(float newScaleFactor)
{
    if (m_scaleFactor == newScaleFactor)
        return *this;
    return StyleCachedImage::create(m_cssValue.copyRef(), newScaleFactor);
}

bool StyleCachedImage::operator==(const StyleImage& other) const
{
    auto* otherCachedImage = dynamicDowncast<StyleCachedImage>(other);
    return otherCachedImage && equals(*otherCachedImage);
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
    if (isRenderSVGResource())
        return true;
    if (!m_cachedImage)
        return false;
    return m_cachedImage->canRender(renderer, multiplier);
}

bool StyleCachedImage::isPending() const
{
    return m_isPending;
}

bool StyleCachedImage::isLoaded(const RenderElement*) const
{
    if (isRenderSVGResource())
        return true;
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

FloatSize StyleCachedImage::imageSize(const RenderElement* renderer, float multiplier, CachedImage::SizeType sizeType) const
{
    if (isRenderSVGResource())
        return m_containerSize;
    if (!m_cachedImage)
        return { };
    return m_cachedImage->imageSizeForRenderer(renderer, multiplier, sizeType) / m_scaleFactor;
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

void StyleCachedImage::computeIntrinsicDimensions(const RenderElement* renderer, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    // In case of an SVG resource, we should return the container size.
    if (isRenderSVGResource()) {
        FloatSize size = floorSizeToDevicePixels(LayoutSize(m_containerSize), renderer ? renderer->document().deviceScaleFactor() : 1);
        intrinsicWidth = Length(size.width(), LengthType::Fixed);
        intrinsicHeight = Length(size.height(), LengthType::Fixed);
        intrinsicRatio = size;
        return;
    }

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

void StyleCachedImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom, const URL& url)
{
    m_containerSize = containerSize;
    if (!m_cachedImage)
        return;
    m_cachedImage->setContainerContextForClient(renderer, LayoutSize(containerSize), containerZoom, !url.isNull() ? url : imageURL());
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

RefPtr<Image> StyleCachedImage::image(const RenderElement* renderer, const FloatSize&, bool) const
{
    ASSERT(!m_isPending);

    if (auto renderSVGResourceRoot = rootForSVGResource(); renderer && renderSVGResourceRoot) {
        if (auto renderSVGResource = this->renderSVGResource(*renderSVGResourceRoot))
            return SVGResourceImage::create(*renderSVGResource, reresolvedURL(renderer->document()));

        if (auto renderSVGResource = this->legacyRenderSVGResource(*renderSVGResourceRoot))
            return SVGResourceImage::create(*renderSVGResource, reresolvedURL(renderer->document()));
    }

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

// MARK: - Internal

RefPtr<SVGSVGElement> StyleCachedImage::rootForSVGResource() const
{
    if (m_isRenderSVGResource == IsRenderSVGResource::No)
        return nullptr;

    if (!m_cachedImage)
        return nullptr;

    if (imageURL().string().find('#') == notFound) {
        m_isRenderSVGResource = IsRenderSVGResource::No;
        return nullptr;
    }

    auto image = dynamicDowncast<SVGImage>(m_cachedImage->image());
    if (!image)
        return nullptr;

    return image->rootElement();
}

LegacyRenderSVGResourceContainer* StyleCachedImage::legacyRenderSVGResource() const
{
    auto root = rootForSVGResource();
    if (!root)
        return nullptr;
    return legacyRenderSVGResource(*root);
}

LegacyRenderSVGResourceContainer* StyleCachedImage::legacyRenderSVGResource(const SVGSVGElement& root) const
{
    auto renderSVGResource = ReferencedSVGResources::referencedRenderResource(root.treeScopeForSVGReferences(), imageURL().fragmentIdentifier().toAtomString());
    if (!renderSVGResource) {
        m_isRenderSVGResource = IsRenderSVGResource::No;
        return nullptr;
    }

    m_isRenderSVGResource = IsRenderSVGResource::Yes;
    return renderSVGResource;
}

RenderSVGResourceContainer* StyleCachedImage::renderSVGResource() const
{
    auto root = rootForSVGResource();
    if (!root)
        return nullptr;
    return renderSVGResource(*root);
}

RenderSVGResourceContainer* StyleCachedImage::renderSVGResource(const SVGSVGElement& root) const
{
    RefPtr referencedMaskElement = ReferencedSVGResources::referencedMaskElement(root.treeScopeForSVGReferences(), *this);
    if (!referencedMaskElement)
        return nullptr;

    auto* referencedMaskerRenderer = dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer());
    if (!referencedMaskerRenderer)
        return nullptr;

    m_isRenderSVGResource = IsRenderSVGResource::Yes;
    return referencedMaskerRenderer;
}

bool StyleCachedImage::isRenderSVGResource() const
{
    auto root = rootForSVGResource();
    if (!root)
        return false;
    return renderSVGResource(*root) || legacyRenderSVGResource(*root);
}

} // namespace WebCore
