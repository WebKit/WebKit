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
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderObjectInlines.h"
#include "RenderSVGResourceMasker.h"
#include "RenderView.h"
#include "SVGImage.h"
#include "SVGMaskElement.h"
#include "SVGResourceImage.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "StyleComputedStyle+GettersInlines.h"
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
    if (m_cachedImage && m_cachedImage == other.m_cachedImage
        && m_url.resolved.fragmentIdentifier() == other.m_url.resolved.fragmentIdentifier())
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
        return uncheckedRenderSVGResource(protect(renderer->treeScopeForSVGReferences()), fragmentIdentifier);
    }

    RefPtr image = dynamicDowncast<SVGImage>(protect(m_cachedImage)->image());
    if (!image)
        return nullptr;

    auto rootElement = image->rootElement();
    if (!rootElement)
        return nullptr;

    return uncheckedRenderSVGResource(protect(rootElement->treeScopeForSVGReferences()), m_url.resolved.fragmentIdentifier().toAtomString());
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
        if (RefPtr referencedMaskElement = ReferencedSVGResources::referencedMaskElement(protect(renderer->treeScopeForSVGReferences()), *this)) {
            if (auto* referencedMaskerRenderer = dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer()))
                return referencedMaskerRenderer;
        }
        return nullptr;
    }

    RefPtr image = dynamicDowncast<SVGImage>(protect(m_cachedImage)->image());
    if (!image)
        return nullptr;

    auto rootElement = image->rootElement();
    if (!rootElement)
        return nullptr;

    auto referencedMaskElement = ReferencedSVGResources::referencedMaskElement(protect(rootElement->treeScopeForSVGReferences()), m_url.resolved.fragmentIdentifier().toAtomString());
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

Ref<CSSValue> CachedImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return m_cssValue->copyForComputedStyle(toCSS(m_url, style));
}

Ref<DeprecatedCSSOMValue> CachedImage::computedStyleDeprecatedCSSOMValue(CSSValuePool& pool, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    // We expose CachedImage as just the URI primitive values in the DeprecatedCSSOM to maintain existing behavior.
    return createDeprecatedCSSOMValue(pool, style, owner, m_url);
}

bool CachedImage::canRender(const RenderElement* renderer, float multiplier) const
{
    if (isRenderSVGResource(renderer))
        return true;
    if (!m_cachedImage)
        return false;
    return protect(m_cachedImage)->canRender(renderer, multiplier);
}

bool CachedImage::isPending() const
{
    return m_isPending;
}

bool CachedImage::isLoading() const
{
    return m_cachedImage && m_cachedImage->isLoaded();
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

FloatSize CachedImage::imageSize(const RenderElement* renderer, float multiplier, ImageSizeType sizeType) const
{
    if (isRenderSVGResource(renderer))
        return m_containerSize;
    if (!m_cachedImage)
        return { };
    float density = 1.0f;
    if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer))
        density = renderImage->imageDevicePixelRatio();
    return protect(m_cachedImage)->imageSizeForRenderer(renderer, multiplier, sizeType, density) / m_scaleFactor;
}

bool CachedImage::imageHasRelativeWidth() const
{
    if (!m_cachedImage)
        return false;
    return protect(m_cachedImage)->imageHasRelativeWidth();
}

bool CachedImage::imageHasRelativeHeight() const
{
    if (!m_cachedImage)
        return false;
    return protect(m_cachedImage)->imageHasRelativeHeight();
}

bool CachedImage::imageHasNaturalAspectRatio() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->imageHasNaturalAspectRatio();
}

bool CachedImage::hasHDRContent() const
{
    return m_cachedImage && m_cachedImage->hasHDRContent();
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

    protect(m_cachedImage)->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool CachedImage::usesImageContainerSize() const
{
    if (!m_cachedImage)
        return false;
    return protect(m_cachedImage)->usesImageContainerSize();
}

void CachedImage::registerContainerContext(const ImageContainerContextKey& key, ImageContainerContext&& containerContext)
{
    m_containerSize = containerContext.containerSize;
    if (!m_cachedImage)
        return;

    if (containerContext.imageURL.isNull())
        containerContext.imageURL = m_url.resolved;

    protect(m_cachedImage)->registerContainerContext(key, WTF::move(containerContext));
}

bool CachedImage::hasImage() const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->hasImage();
}

RefPtr<WebCore::Image> CachedImage::image() const
{
    if (!m_cachedImage)
        return nullptr;
    return protect(m_cachedImage)->image();
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

    return protect(m_cachedImage)->imageForRenderer(renderer);
}

bool CachedImage::currentFrameIsComplete(const RenderElement& renderer) const
{
    return m_cachedImage && protect(m_cachedImage)->currentFrameIsCompleteForKey(renderer.imageContainerContextKey());
}

float CachedImage::imageScaleFactor() const
{
    return m_scaleFactor;
}

bool CachedImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_cachedImage && protect(m_cachedImage)->currentFrameKnownToBeOpaqueForKey(renderer.imageContainerContextKey());
}

bool CachedImage::usesDataProtocol() const
{
    return m_url.resolved.protocolIsData();
}

// MARK: - CachedImageClient

void CachedImage::notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        client->notifyFinished(*this);
    }
}

void CachedImage::imageChanged(WebCore::CachedImage&, const IntRect* changeRect)
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        client->imageChanged(*this, changeRect);
    }
}

bool CachedImage::allowsAnimation() const
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (client->allowsAnimation(*this))
            return true;
    }
    return false;
}

bool CachedImage::canDestroyDecodedData() const
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (!client->canDestroyDecodedData(*this))
            return false;
    }
    return false;
}

bool CachedImage::useSystemDarkAppearance() const
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (client->useSystemDarkAppearance(*this))
            return true;
    }
    return false;
}

VisibleInViewportState CachedImage::imageFrameAvailable(WebCore::CachedImage& cachedImage, ImageAnimatingState animatingState, const IntRect* changeRect)
{
    if (&cachedImage != m_cachedImage)
        return VisibleInViewportState::No;

    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (client->imageFrameAvailable(*this, animatingState, changeRect) == VisibleInViewportState::Yes)
            return VisibleInViewportState::Yes;
    }
    return VisibleInViewportState::No;
}

VisibleInViewportState CachedImage::imageVisibleInViewport(WebCore::CachedImage& cachedImage, const Document& document) const
{
    if (&cachedImage != m_cachedImage)
        return VisibleInViewportState::No;

    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (client->imageVisibleInViewport(*this, document) == VisibleInViewportState::Yes)
            return VisibleInViewportState::Yes;
    }
    return VisibleInViewportState::No;
}

void CachedImage::didRemoveCachedImageClient(WebCore::CachedImage& cachedImage)
{
    if (&cachedImage != m_cachedImage)
        return;

    for (auto entry : m_clients) {
        Ref client = entry.key;
        client->didRemoveCachedImageClient(*this);
    }
}

void CachedImage::imageContentChanged(WebCore::CachedImage& cachedImage)
{
    if (&cachedImage != m_cachedImage)
        return;

    for (auto entry : m_clients) {
        Ref client = entry.key;
        client->imageContentChanged(*this);
    }
}

void CachedImage::scheduleRenderingUpdateForImage(WebCore::CachedImage& cachedImage)
{
    if (&cachedImage != m_cachedImage)
        return;

    for (auto entry : m_clients) {
        Ref client = entry.key;
        client->scheduleRenderingUpdateForImage(*this);
    }
}

bool CachedImage::isRendererClient() const
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (client->isRendererClient())
            return true;
    }
    return false;
}

} // namespace Style
} // namespace WebCore
