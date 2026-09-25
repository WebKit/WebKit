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

#include "BitmapImage.h"
#include "CSSImageValue.h"
#include "CachedImage.h"
#include "ContainerNodeInlines.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "LegacyRenderSVGImage.h"
#include "LegacyRenderSVGResourceMasker.h"
#include "NativeImage.h"
#include "NinePieceGeometry.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderImageResource.h"
#include "RenderObjectInlines.h"
#include "RenderSVGImage.h"
#include "RenderSVGResourceMasker.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGImageElement.h"
#include "SVGMaskElement.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "CSSParserContext.h"
#include "CSSPropertyParserConsumer+LinkParameters.h"
#include "StyleLinkParameters.h"
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

Ref<CachedImage> CachedImage::create(WebCore::CachedImage& cachedImage, WTF::URL&& authoredURL, float scaleFactor)
{
    Ref result = CachedImage::create(URL { .resolved = cachedImage.url(), .modifiers = { } }, CSSImageValue::create(cachedImage), scaleFactor);
    result->m_authoredURL = WTF::move(authoredURL);
    return result;
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

Vector<CSS::ParamFunction> CachedImage::urlLinkParameters(const CSSParserContext& context, StringView fragment) const
{
    auto parameters = CSSPropertyParserHelpers::parseLinkParametersFromFragment(fragment, context);
    for (auto& parameter : m_url.modifiers.linkParameters)
        parameters.append(parameter);
    return parameters;
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

LegacyRenderSVGResourceContainer* CachedImage::uncheckedLegacyRenderSVGResource(TreeScope& treeScope, const AtomString& fragment) const
{
    CheckedPtr renderSVGResource = ReferencedSVGResources::referencedRenderResource(treeScope, fragment);
    m_resourceType = renderSVGResource ? ResourceType::LegacySVGResource : ResourceType::ResolvedImage;
    return renderSVGResource.unsafeGet();
}

LegacyRenderSVGResourceContainer* CachedImage::uncheckedLegacyRenderSVGResource(const RenderElement& renderer) const
{
    if (!m_url.resolved.string().contains('#')) {
        m_resourceType = ResourceType::ResolvedImage;
        return nullptr;
    }

    if (!m_cachedImage) {
        auto fragmentIdentifier = SVGURIReference::fragmentIdentifierFromIRIString(m_url, protect(renderer.document()));
        return uncheckedLegacyRenderSVGResource(protect(renderer.treeScopeForSVGReferences()), fragmentIdentifier);
    }

    RefPtr image = dynamicDowncast<SVGImage>(protect(m_cachedImage)->image());
    if (!image)
        return nullptr;

    auto rootElement = image->rootElement();
    if (!rootElement)
        return nullptr;

    return uncheckedLegacyRenderSVGResource(protect(rootElement->treeScopeForSVGReferences()), m_url.resolved.fragmentIdentifier().toAtomString());
}

LegacyRenderSVGResourceContainer* CachedImage::legacyRenderSVGResource(const RenderElement& renderer) const
{
    if (m_resourceType == ResourceType::SVGResource || m_resourceType == ResourceType::ResolvedImage)
        return nullptr;
    return uncheckedLegacyRenderSVGResource(renderer);
}

RenderSVGResourceContainer* CachedImage::renderSVGResource(const RenderElement& renderer) const
{
    if (m_resourceType == ResourceType::LegacySVGResource || m_resourceType == ResourceType::ResolvedImage)
        return nullptr;

    if (!m_url.resolved.string().contains('#')) {
        m_resourceType = ResourceType::ResolvedImage;
        return nullptr;
    }

    if (!m_cachedImage) {
        if (RefPtr referencedMaskElement = ReferencedSVGResources::referencedMaskElement(protect(renderer.treeScopeForSVGReferences()), *this)) {
            if (auto* referencedMaskerRenderer = dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer())) {
                m_resourceType = ResourceType::SVGResource;
                return referencedMaskerRenderer;
            }
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

    CheckedPtr masker = dynamicDowncast<RenderSVGResourceMasker>(referencedMaskElement->renderer());
    if (masker)
        m_resourceType = ResourceType::SVGResource;
    return masker.unsafeGet();
}

bool CachedImage::isRenderSVGResource(const RenderElement& renderer) const
{
    return renderSVGResource(renderer) || legacyRenderSVGResource(renderer);
}

void CachedImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    ASSERT(m_isPending);
    m_isPending = false;
    m_cachedImage = m_cssValue->loadImage(loader, options);
}

bool CachedImage::hasDecodedImage() const
{
    return m_cachedImage && !m_cachedImage->errorOccurred() && m_cachedImage->hasImage();
}

RefPtr<NativeImage> CachedImage::nativeImage(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const ColorSpace& colorSpace) const
{
    if (referencedSVGResource(renderer))
        return Image::nativeImage(renderer, concreteObjectSize, colorSpace);

    RefPtr image = m_cachedImage ? protect(m_cachedImage)->image() : nullptr;
    if (!image)
        return nullptr;

    auto extras = drawingExtrasForRenderer(renderer);
    return image->nativeImage(concreteObjectSize, colorSpace, &extras);
}

std::optional<IntPoint> CachedImage::hotSpot() const
{
    RefPtr image = m_cachedImage ? protect(m_cachedImage)->image() : nullptr;
    return image ? image->hotSpot() : std::nullopt;
}

bool CachedImage::isOpaqueBitmap() const
{
    if (!hasDecodedImage())
        return false;

    RefPtr bitmapImage = dynamicDowncast<BitmapImage>(m_cachedImage->image());
    if (!bitmapImage)
        return false;

    RefPtr nativeImage = bitmapImage->nativeImage();
    return nativeImage && !nativeImage->hasAlpha();
}

bool CachedImage::isTransparentBitmap() const
{
    if (!hasDecodedImage())
        return false;

    RefPtr bitmapImage = dynamicDowncast<BitmapImage>(m_cachedImage->image());
    if (!bitmapImage)
        return false;

    RefPtr nativeImage = bitmapImage->nativeImage();
    return nativeImage && nativeImage->hasAlpha();
}

bool CachedImage::isSVGImage() const
{
    if (!hasDecodedImage())
        return false;

    RefPtr image = m_cachedImage->image();
    return image && image->isSVGImage();
}

ConcreteObjectSize CachedImage::concreteSizeToDrawAt(const RenderElement& renderer, ConcreteObjectSize laidOut) const
{
    auto box = laidOut.size() * laidOut.zoom();
    auto zoom = renderer.style().usedZoom();

    RefPtr image = resolvedImage();
    if (!image)
        return ConcreteObjectSize::fixed(box / zoom, zoom);
    if (!image->isSVGImage()) {
        auto naturalDimensions = image->naturalDimensions();
        if (naturalDimensions.width && naturalDimensions.height)
            return ConcreteObjectSize::fixed({ *naturalDimensions.width, *naturalDimensions.height });
    }
    return ConcreteObjectSize::fixed(box / zoom, zoom);
}

Ref<WebCore::Image> CachedImage::decodedImage() const
{
    RefPtr image = m_cachedImage ? m_cachedImage->image() : nullptr;
    if (!image)
        return WebCore::Image::nullImage();
    return image.releaseNonNull();
}

bool CachedImage::hasNothingToDraw(const RenderElement&) const
{
    if (m_isPending)
        return true;

    RefPtr image = m_cachedImage ? m_cachedImage->image() : nullptr;
    return !image || image->hasNothingToDraw();
}

WebCore::CachedImage* CachedImage::resource() const
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

bool CachedImage::canRender(const RenderElement& renderer, float) const
{
    if (isRenderSVGResource(renderer))
        return true;
    if (!m_cachedImage)
        return false;
    return protect(m_cachedImage)->canRender();
}

bool CachedImage::isPending() const
{
    return m_isPending;
}

bool CachedImage::isLoaded(const RenderElement& renderer) const
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

WTF::String CachedImage::accessibilityDescription() const
{
    return decodedImage()->accessibilityDescription();
}

bool CachedImage::isAnimated() const
{
    return decodedImage()->isAnimated();
}

void CachedImage::stopAnimation()
{
    decodedImage()->stopAnimation();
}

void CachedImage::resetAnimation()
{
    decodedImage()->resetAnimation();
}

NaturalDimensions CachedImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext&) const
{
    if (isRenderSVGResource(renderer))
        return NaturalDimensions::none();

    if (!m_cachedImage)
        return NaturalDimensions::none();

    return protect(m_cachedImage)->naturalDimensions(orientationForRenderer(renderer));
}

ImageDrawingExtras CachedImage::drawingExtrasForRenderer(const RenderElement& renderer, const WTF::URL& url) const
{
    auto viewURL = !url.isNull() ? url : (m_authoredURL.isNull() ? m_url.resolved : m_authoredURL);
    auto linkParameters = linkParametersForResource(renderer.style().linkParameters(), urlLinkParameters(protect(renderer.document())->cssParserContext(), viewURL.fragmentIdentifier()));
    return { WTF::move(viewURL), WTF::move(linkParameters), ignoreRootPreserveAspectRatio(renderer) };
}

ImageDrawingExtras::IgnoreRootPreserveAspectRatio CachedImage::ignoreRootPreserveAspectRatio(const RenderElement& renderer) const
{
    if (!is<SVGImageElement>(renderer.element()))
        return ImageDrawingExtras::IgnoreRootPreserveAspectRatio::No;

    CheckedPtr imageResource = [&] -> const RenderImageResource* {
        if (auto* svgImageRenderer = dynamicDowncast<RenderSVGImage>(renderer))
            return &svgImageRenderer->imageResource();
        if (auto* legacySVGImageRenderer = dynamicDowncast<LegacyRenderSVGImage>(renderer))
            return &legacySVGImageRenderer->imageResource();
        return nullptr;
    }();
    if (!imageResource || imageResource->styleImage() != this)
        return ImageDrawingExtras::IgnoreRootPreserveAspectRatio::No;

    RefPtr image = m_cachedImage ? protect(m_cachedImage)->image() : nullptr;
    if (!image || !image->isSVGImage())
        return ImageDrawingExtras::IgnoreRootPreserveAspectRatio::No;

    return ImageDrawingExtras::IgnoreRootPreserveAspectRatio::Yes;
}

void CachedImage::addClient(RenderElement& renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    protect(m_cachedImage)->addClient(protect(renderer.cachedImageClient()));
}

void CachedImage::removeClient(RenderElement& renderer)
{
    ASSERT(!m_isPending);
    if (!m_cachedImage)
        return;
    protect(m_cachedImage)->removeClient(protect(renderer.cachedImageClient()));
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

CachedImage::ReferencedSVGResource CachedImage::referencedSVGResource(const RenderElement& renderer) const
{
    if (CheckedPtr resource = renderSVGResource(renderer))
        return { resource.get(), nullptr };

    return { nullptr, legacyRenderSVGResource(renderer) };
}

ImageDrawResult CachedImage::drawSVGResource(GraphicsContext& context, const ReferencedSVGResource& referenced, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options) const
{
    if (CheckedPtr masker = dynamicDowncast<RenderSVGResourceMasker>(referenced.resource.get())) {
        if (masker->drawContentIntoContext(context, destination, source, options))
            return ImageDrawResult::DidDraw;
    }

    if (CheckedPtr masker = dynamicDowncast<LegacyRenderSVGResourceMasker>(referenced.legacyResource.get())) {
        if (masker->drawContentIntoContext(context, destination, source, options))
            return ImageDrawResult::DidDraw;
    }

    return ImageDrawResult::DidNothing;
}

ImageDrawResult CachedImage::drawSVGResourceAsPattern(GraphicsContext& context, const ReferencedSVGResource& referenced, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options) const
{
    RefPtr imageBuffer = context.createImageBuffer(concreteObjectSize.size());
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    auto imageRect = FloatRect { { }, concreteObjectSize.size() };
    auto result = drawSVGResource(imageBuffer->context(), referenced, imageRect, imageRect, options);

    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);
    return result;
}

ImageDrawResult CachedImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize laidOut, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool) const
{
    auto box = laidOut.size() * laidOut.zoom();
    auto sourceInOwnSpace = source;
    if (RefPtr image = resolvedImage(); image && !image->isSVGImage() && !box.isEmpty()) {
        auto naturalDimensions = image->naturalDimensions(orientationForRenderer(renderer));
        if (naturalDimensions.width && naturalDimensions.height) {
            auto mapX = [&](float x) {
                return narrowPrecisionToFloat(static_cast<double>(x) * *naturalDimensions.width / box.width());
            };
            auto mapY = [&](float y) {
                return narrowPrecisionToFloat(static_cast<double>(y) * *naturalDimensions.height / box.height());
            };
            auto minX = mapX(source.x());
            auto minY = mapY(source.y());
            sourceInOwnSpace = { minX, minY, mapX(source.maxX()) - minX, mapY(source.maxY()) - minY };
        }
    }

    return drawInOwnSpace(context, renderer, concreteSizeToDrawAt(renderer, laidOut), destination, sourceInOwnSpace, options);
}

ImageDrawResult CachedImage::drawInOwnSpace(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options) const
{
    if (auto referenced = referencedSVGResource(renderer))
        return drawSVGResource(context, referenced, destination, source, options);

    RefPtr image = resolvedImage();
    if (!image)
        return ImageDrawResult::DidNothing;

    if (!allowsOrientationOverride())
        options = { options, WebCore::ImageOrientation::Orientation::FromImage };

    return drawResolved(context, renderer, *image, concreteObjectSize, destination, source, options);
}

ImageDrawResult CachedImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize laidOut, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool) const
{
    auto concreteObjectSize = concreteSizeToDrawAt(renderer, laidOut);
    if (auto referenced = referencedSVGResource(renderer))
        return drawSVGResourceAsPattern(context, referenced, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options);

    RefPtr image = resolvedImage();
    if (!image)
        return ImageDrawResult::DidNothing;

    auto extras = drawingExtrasForRenderer(renderer);
    image->drawPattern(context, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options, &extras);

    image->startAnimation();

    return ImageDrawResult::DidDraw;
}

ImageDrawResult CachedImage::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize laidOut, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    if (tileSize.isEmpty())
        return ImageDrawResult::DidNothing;

    return drawTiledUsing(context, tileNaturalDimensions(renderer), [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return drawInOwnSpace(context, renderer, tileConcreteObjectSize, destination, source, options);
    }, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawAsPattern(context, renderer, tileConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, isForFirstLine);
    }, concreteSizeToDrawAt(renderer, laidOut), destination, phase, tileSize, spacing, options);
}

ImageDrawResult CachedImage::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize laidOut, const NinePieceGeometry& geometry, ImagePaintingOptions options) const
{
    return drawNinePieceUsing(context, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        return drawInOwnSpace(context, renderer, pieceConcreteObjectSize, destination, source, options);
    }, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        return drawAsPattern(context, renderer, pieceConcreteObjectSize, destination, tile, patternTransform, phase, spacing, options, false);
    }, concreteSizeToDrawAt(renderer, laidOut), geometry);
}

NaturalDimensions CachedImage::tileNaturalDimensions(const RenderElement& renderer) const
{
    if (referencedSVGResource(renderer))
        return NaturalDimensions::none();

    RefPtr image = resolvedImage();
    if (!image)
        return NaturalDimensions::none();

    if (image->isSVGImage())
        return NaturalDimensions::none();

    return image->naturalDimensions();
}

bool CachedImage::allowsOrientationOverride() const
{
    return !m_cachedImage || m_cachedImage->allowsOrientationOverride();
}

WebCore::ImageOrientation CachedImage::orientationForRenderer(const RenderElement& renderer) const
{
    if (!allowsOrientationOverride())
        return WebCore::ImageOrientation::Orientation::FromImage;
    return renderer.imageOrientation();
}

RefPtr<WebCore::Image> CachedImage::resolvedImage() const
{
    ASSERT(!m_isPending);

    if (!m_cachedImage)
        return nullptr;

    return protect(m_cachedImage)->image();
}

bool CachedImage::currentFrameIsComplete() const
{
    return m_cachedImage && protect(m_cachedImage)->currentFrameIsComplete();
}

float CachedImage::imageScaleFactor() const
{
    return m_scaleFactor;
}

bool CachedImage::knownToBeOpaque(const RenderElement&) const
{
    return m_cachedImage && protect(m_cachedImage)->currentFrameKnownToBeOpaque();
}

bool CachedImage::isOriginClean(Document& document) const
{
    if (!m_cachedImage)
        return true;
    return protect(m_cachedImage)->isOriginClean(&document.securityOrigin());
}

bool CachedImage::usesDataProtocol() const
{
    return m_url.resolved.protocolIsData();
}

} // namespace Style
} // namespace WebCore
