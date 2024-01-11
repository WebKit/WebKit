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
#include "RenderView.h"
#include "SVGImage.h"
#include "SVGResourceImage.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"

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

LegacyRenderSVGResourceContainer* StyleCachedImage::uncheckedRenderSVGResource(TreeScope& treeScope, const AtomString& fragment) const
{
    auto renderSVGResource = ReferencedSVGResources::referencedRenderResource(treeScope, fragment);
    m_isRenderSVGResource = renderSVGResource != nullptr;
    return renderSVGResource;
}

LegacyRenderSVGResourceContainer* StyleCachedImage::uncheckedRenderSVGResource(const RenderElement* renderer) const
{
    if (!renderer)
        return nullptr;

    if (imageURL().string().find('#') == notFound) {
        m_isRenderSVGResource = false;
        return nullptr;
    }

    auto& document = renderer->document();
    auto reresolvedURL = this->reresolvedURL(document);

    if (!m_cachedImage) {
        auto fragmentIdentifier = SVGURIReference::fragmentIdentifierFromIRIString(reresolvedURL.string(), document);
        return uncheckedRenderSVGResource(renderer->treeScopeForSVGReferences(), fragmentIdentifier);
    }

    auto image = dynamicDowncast<SVGImage>(m_cachedImage->image());
    if (!image)
        return nullptr;

    auto rootElement = image->rootElement();
    if (!rootElement)
        return nullptr;

    auto fragmentIdentifier = reresolvedURL.fragmentIdentifier();
    return uncheckedRenderSVGResource(rootElement->treeScopeForSVGReferences(), fragmentIdentifier.toAtomString());
}

LegacyRenderSVGResourceContainer* StyleCachedImage::renderSVGResource(const RenderElement* renderer) const
{
    if (m_isRenderSVGResource && !*m_isRenderSVGResource)
        return nullptr;
    return uncheckedRenderSVGResource(renderer);
}

bool StyleCachedImage::isRenderSVGResource(const RenderElement* renderer) const
{
    return renderSVGResource(renderer);
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
    if (isRenderSVGResource(renderer))
        return true;
    if (!m_cachedImage)
        return false;
    return m_cachedImage->canRender(renderer, multiplier);
}

bool StyleCachedImage::isPending() const
{
    return m_isPending;
}

bool StyleCachedImage::isLoaded(const RenderElement* renderer) const
{
    if (isRenderSVGResource(renderer))
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

FloatSize StyleCachedImage::imageSize(const RenderElement* renderer, float multiplier) const
{
    if (isRenderSVGResource(renderer))
        return m_containerSize;
    if (!m_cachedImage)
        return { };
    return m_cachedImage->imageSizeForRenderer(renderer, multiplier) / m_scaleFactor;
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
    if (isRenderSVGResource(renderer)) {
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

void StyleCachedImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom)
{
    m_containerSize = containerSize;
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

RefPtr<Image> StyleCachedImage::image(const RenderElement* renderer, const FloatSize&, bool) const
{
    ASSERT(!m_isPending);

    if (auto renderSVGResource = this->renderSVGResource(renderer))
        return SVGResourceImage::create(*renderSVGResource, reresolvedURL(renderer->document()));

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

} // namespace WebCore
