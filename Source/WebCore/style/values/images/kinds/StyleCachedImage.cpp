/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include "StyleCachedImage.h"

#include "CSSImageValue.h"
#include "CachedImage.h"
#include "ContainerNodeInlines.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "RenderSVGResourceMasker.h"
#include "RenderView.h"
#include "SVGImage.h"
#include "SVGMaskElement.h"
#include "SVGResourceImage.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CachedImage);

Ref<CachedImage> CachedImage::create(URL&& url, Ref<CSSImageValue>&& cssValue, float scaleFactor)
{
    return adoptRef(*new CachedImage(WTF::move(url), WTF::move(cssValue), scaleFactor));
}

Ref<CachedImage> CachedImage::create(const URL& url, const Ref<CSSImageValue>& cssValue, float scaleFactor)
{
    return CachedImage::create(URL { url }, cssValue.copyRef(), scaleFactor);
}

Ref<CachedImage> CachedImage::create(WebCore::CachedImage& cachedImage, float scaleFactor)
{
    return CachedImage::create(URL { .resolved = cachedImage.url(), .modifiers = { } }, CSSImageValue::create(cachedImage), scaleFactor);
}

Ref<CachedImage> CachedImage::copyOverridingScaleFactor(CachedImage& other, float scaleFactor)
{
    if (other.m_scaleFactor == scaleFactor)
        return other;
    return CachedImage::create(other.m_url, other.m_cssValue, scaleFactor);
}

CachedImage::CachedImage(URL&& url, Ref<CSSImageValue>&& cssValue, float scaleFactor)
    : Image { Type::CachedImage }
    , m_url { WTF::move(url) }
    , m_cssValue { WTF::move(cssValue) }
    , m_scaleFactor { scaleFactor }
{
    m_cachedImage = m_cssValue->cachedImage();
    if (m_cachedImage)
        m_isPending = false;
}

CachedImage::~CachedImage() = default;

bool CachedImage::operator==(const Image& other) const
{
    auto* otherCachedImage = dynamicDowncast<CachedImage>(other);
    return otherCachedImage && equals(*otherCachedImage);
}

bool CachedImage::equals(const CachedImage& other) const
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

URL CachedImage::url() const
{
    return m_url;
}

LegacyRenderSVGResourceContainer* CachedImage::uncheckedRenderSVGResource(TreeScope& treeScope, const AtomString& fragment) const
{
    auto renderSVGResource = ReferencedSVGResources::referencedRenderResource(treeScope, fragment);
    m_isRenderSVGResource = renderSVGResource != nullptr;
    return renderSVGResource;
}

LegacyRenderSVGResourceContainer* CachedImage::uncheckedRenderSVGResource(const RenderElement* renderer) const
{
    if (!renderer)
        return nullptr;

    if (!m_url.resolved.string().contains('#')) {
        m_isRenderSVGResource = false;
        return nullptr;
    }

    if (!m_cachedImage) {
        auto fragmentIdentifier = SVGURIReference::fragmentIdentifierFromIRIString(m_url, protect(renderer->document()));
        return uncheckedRenderSVGResource(renderer->treeScopeForSVGReferences(), fragmentIdentifier);
    }

    RefPtr image = dynamicDowncast<SVGImage>(m_cachedImage->image());
    if (!image)
        return nullptr;

    auto rootElement = image->rootElement();
    if (!rootElement)
        return nullptr;

    return uncheckedRenderSVGResource(rootElement->treeScopeForSVGReferences(), m_url.resolved.fragmentIdentifier().toAtomString());
}

LegacyRenderSVGResourceContainer* CachedImage::legacyRenderSVGResource(const RenderElement* renderer) const
{
    if (m_isRenderSVGResource && !*m_isRenderSVGResource)
        return nullptr;
    return uncheckedRenderSVGResource(renderer);
}

RenderSVGResourceContainer* CachedImage::renderSVGResource(const RenderElement* renderer) const
{
    if (m_isRenderSVGResource)
        return nullptr;

    if (!renderer)
        return nullptr;

    if (!m_url.resolved.string().contains('#'))
        return nullptr;

    if (!m_cachedImage) {
        if (RefPtr referencedMaskElement = ReferencedSVGResources::referencedMaskElement(renderer->treeScopeForSVGReferences(), *this)) {
            if (auto* referencedMaskerRenderer = dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer()))
                return referencedMaskerRenderer;
        }
        return nullptr;
    }

    RefPtr image = dynamicDowncast<SVGImage>(m_cachedImage->image());
    if (!image)
        return nullptr;

    auto rootElement = image->rootElement();
    if (!rootElement)
        return nullptr;

    auto referencedMaskElement = ReferencedSVGResources::referencedMaskElement(rootElement->treeScopeForSVGReferences(), m_url.resolved.fragmentIdentifier().toAtomString());
    if (!referencedMaskElement)
        return nullptr;

    return dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer());
}

bool CachedImage::isRenderSVGResource(const RenderElement* renderer) const
{
    return renderSVGResource(renderer) || legacyRenderSVGResource(renderer);
}

void CachedImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    ASSERT(m_isPending);
    m_isPending = false;
    m_cachedImage = m_cssValue->loadImage(loader, options);
}

WebCore::CachedImage* CachedImage::cachedImage() const
{
    return m_cachedImage.get();
}

Ref<CSSValue> CachedImage::computedStyleValue(const RenderStyle& style) const
{
    return m_cssValue->copyForComputedStyle(toCSS(m_url, style));
}

bool CachedImage::canRender(const RenderElement* renderer, float multiplier) const
{
    if (isRenderSVGResource(renderer))
        return true;
    if (!m_cachedImage)
        return false;
    return m_cachedImage->canRender(renderer, multiplier);
}

bool CachedImage::isPending() const
{
    return m_isPending;
}

bool CachedImage::isLoaded(const RenderElement* renderer) const
{
    if (isRenderSVGResource(renderer))
        return true;
    if (!m_cachedImage)
        return false;
    return m_cachedImage->isLoaded();
}

bool CachedImage::errorOccurred() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->errorOccurred();
}

FloatSize CachedImage::imageSize(const RenderElement* renderer, float multiplier, WebCore::CachedImage::SizeType sizeType) const
{
    if (isRenderSVGResource(renderer))
        return m_containerSize;
    if (!m_cachedImage)
        return { };
    return m_cachedImage->imageSizeForRenderer(renderer, multiplier, sizeType) / m_scaleFactor;
}

bool CachedImage::imageHasRelativeWidth() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->imageHasRelativeWidth();
}

bool CachedImage::imageHasRelativeHeight() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->imageHasRelativeHeight();
}

void CachedImage::computeIntrinsicDimensions(const RenderElement* renderer, float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio)
{
    // In case of an SVG resource, we should return the container size.
    if (isRenderSVGResource(renderer)) {
        FloatSize size = floorSizeToDevicePixels(LayoutSize(m_containerSize), renderer ? protect(renderer->document())->deviceScaleFactor() : 1);
        intrinsicWidth = size.width();
        intrinsicHeight = size.height();
        intrinsicRatio = size;
        return;
    }

    if (!m_cachedImage)
        return;

    m_cachedImage->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool CachedImage::usesImageContainerSize() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->usesImageContainerSize();
}

void CachedImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom, const WTF::URL& url)
{
    m_containerSize = containerSize;
    if (!m_cachedImage)
        return;
    m_cachedImage->setContainerContextForClient(protect(renderer.cachedImageClient()), LayoutSize(containerSize), containerZoom, !url.isNull() ? url : m_url.resolved);
}

void CachedImage::addClient(RenderElement& renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    m_cachedImage->addClient(renderer.cachedImageClient());
}

void CachedImage::removeClient(RenderElement& renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    m_cachedImage->removeClient(renderer.cachedImageClient());
}

bool CachedImage::hasClient(RenderElement& renderer) const
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return false;
    return m_cachedImage->hasClient(renderer.cachedImageClient());
}

bool CachedImage::hasImage() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->hasImage();
}

RefPtr<WebCore::Image> CachedImage::image(const RenderElement* renderer, const FloatSize&, const GraphicsContext&, bool) const
{
    ASSERT(!m_isPending);

    if (CheckedPtr renderSVGResource = this->renderSVGResource(renderer))
        return SVGResourceImage::create(*renderSVGResource, m_url);

    if (auto renderSVGResource = this->legacyRenderSVGResource(renderer))
        return SVGResourceImage::create(*renderSVGResource, m_url);

    if (!m_cachedImage)
        return nullptr;

    return m_cachedImage->imageForRenderer(renderer);
}

bool CachedImage::currentFrameIsComplete(const RenderElement* renderer) const
{
    return m_cachedImage && m_cachedImage->currentFrameIsComplete(renderer);
}

float CachedImage::imageScaleFactor() const
{
    return m_scaleFactor;
}

bool CachedImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_cachedImage && m_cachedImage->currentFrameKnownToBeOpaque(&renderer);
}

bool CachedImage::usesDataProtocol() const
{
    return m_url.resolved.protocolIsData();
}

} // namespace Style
} // namespace WebCore
