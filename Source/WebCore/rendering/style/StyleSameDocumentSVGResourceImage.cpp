/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleSameDocumentSVGResourceImage.h"

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

Ref<StyleSameDocumentSVGResourceImage> StyleSameDocumentSVGResourceImage::create(Ref<CSSImageValue> cssValue)
{
    return adoptRef(*new StyleSameDocumentSVGResourceImage(WTFMove(cssValue)));
}

StyleSameDocumentSVGResourceImage::StyleSameDocumentSVGResourceImage(Ref<CSSImageValue>&& cssValue)
    : StyleImage { Type::SameDocumentSVGResourceImage }
    , m_cssValue { WTFMove(cssValue) }
{
}

StyleSameDocumentSVGResourceImage::~StyleSameDocumentSVGResourceImage() = default;

bool StyleSameDocumentSVGResourceImage::operator==(const StyleImage& other) const
{
    auto* otherSameDocumentSVGResourceImage = dynamicDowncast<StyleSameDocumentSVGResourceImage>(other);
    return otherSameDocumentSVGResourceImage && equals(*otherSameDocumentSVGResourceImage);
}

bool StyleSameDocumentSVGResourceImage::equals(const StyleSameDocumentSVGResourceImage& other) const
{
    if (&other == this)
        return true;
    if (m_cssValue.ptr() == other.m_cssValue.ptr() || m_cssValue->equals(other.m_cssValue.get()))
        return true;
    return false;
}

URL StyleSameDocumentSVGResourceImage::imageURL() const
{
    return m_cssValue->imageURL();
}

URL StyleSameDocumentSVGResourceImage::reresolvedURL(const Document& document) const
{
    return m_cssValue->reresolvedURL(document);
}

void StyleSameDocumentSVGResourceImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

Ref<CSSValue> StyleSameDocumentSVGResourceImage::computedStyleValue(const RenderStyle&) const
{
    return m_cssValue.copyRef();
}

bool StyleSameDocumentSVGResourceImage::canRender(const RenderElement* renderer, float) const
{
    return isRenderSVGResource(renderer);
}

bool StyleSameDocumentSVGResourceImage::isPending() const
{
    return false;
}

bool StyleSameDocumentSVGResourceImage::isLoaded(const RenderElement* renderer) const
{
    // FIXME: This should probably just always return true, but for now, we are matching the old behavior in StyleCachedImage.
    return isRenderSVGResource(renderer);
}

FloatSize StyleSameDocumentSVGResourceImage::imageSize(const RenderElement*, float, CachedImage::SizeType) const
{
    return m_containerSize;
}

bool StyleSameDocumentSVGResourceImage::imageHasRelativeWidth() const
{
    return false;
}

bool StyleSameDocumentSVGResourceImage::imageHasRelativeHeight() const
{
    return false;
}

void StyleSameDocumentSVGResourceImage::computeIntrinsicDimensions(const RenderElement* renderer, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    // In case of an SVG resource, we should return the container size.
    FloatSize size = floorSizeToDevicePixels(LayoutSize(m_containerSize), renderer ? renderer->document().deviceScaleFactor() : 1);
    intrinsicWidth = Length(size.width(), LengthType::Fixed);
    intrinsicHeight = Length(size.height(), LengthType::Fixed);
    intrinsicRatio = size;
}

bool StyleSameDocumentSVGResourceImage::usesImageContainerSize() const
{
    return false;
}

void StyleSameDocumentSVGResourceImage::setContainerContextForRenderer(const RenderElement&, const FloatSize& containerSize, float, const URL&)
{
    m_containerSize = containerSize;
}

void StyleSameDocumentSVGResourceImage::addClient(RenderElement&)
{
}

void StyleSameDocumentSVGResourceImage::removeClient(RenderElement&)
{
}

bool StyleSameDocumentSVGResourceImage::hasClient(RenderElement&) const
{
    return false;
}

RefPtr<Image> StyleSameDocumentSVGResourceImage::image(const RenderElement* renderer, const FloatSize&, bool) const
{
    if (!renderer)
        return nullptr;

    if (auto renderSVGResource = this->renderSVGResource(*renderer))
        return SVGResourceImage::create(*renderSVGResource, reresolvedURL(renderer->document()));

    if (auto renderSVGResource = this->legacyRenderSVGResource(*renderer))
        return SVGResourceImage::create(*renderSVGResource, reresolvedURL(renderer->document()));

    return nullptr;
}

bool StyleSameDocumentSVGResourceImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

// MARK: - Internal

LegacyRenderSVGResourceContainer* StyleSameDocumentSVGResourceImage::legacyRenderSVGResource(const RenderElement& renderer) const
{
    auto reresolvedURL = this->reresolvedURL(renderer.document());
    auto fragmentIdentifier = SVGURIReference::fragmentIdentifierFromIRIString(reresolvedURL.string(), renderer.document());
    return ReferencedSVGResources::referencedRenderResource(renderer.treeScopeForSVGReferences(), fragmentIdentifier);
}

RenderSVGResourceContainer* StyleSameDocumentSVGResourceImage::renderSVGResource(const RenderElement& renderer) const
{
    RefPtr referencedMaskElement = ReferencedSVGResources::referencedMaskElement(renderer.treeScopeForSVGReferences(), *this);
    if (!referencedMaskElement)
        return nullptr;
    return dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer());
}

bool StyleSameDocumentSVGResourceImage::isRenderSVGResource(const RenderElement* renderer) const
{
    if (!renderer)
        return false;
    return renderSVGResource(*renderer) || legacyRenderSVGResource(*renderer);
}

} // namespace WebCore
