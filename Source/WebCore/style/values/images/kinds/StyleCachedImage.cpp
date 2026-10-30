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
#include "ImageQualityController.h"
#include "LegacyRenderSVGResourceMasker.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderObjectInlines.h"
#include "RenderSVGResourceMasker.h"
#include "RenderView.h"
#include "SVGImage.h"
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

Ref<CachedImage> CachedImage::create(WebCore::CachedImage& cachedImage, WTF::URL&& authoredURL, OptionSet<SVGReferencingMode> referencingModes, float scaleFactor)
{
    auto url = authoredURL.isNull() ? cachedImage.url() : WTF::move(authoredURL);
    return adoptRef(*new CachedImage(URL { .resolved = WTF::move(url), .modifiers = { } }, CSSImageValue::create(cachedImage), scaleFactor, referencingModes));
}

Ref<CachedImage> CachedImage::copyOverridingScaleFactor(CachedImage& other, float scaleFactor)
{
    if (other.m_scaleFactor == scaleFactor)
        return other;
    return adoptRef(*new CachedImage(URL { other.m_url }, other.m_cssValue.copyRef(), scaleFactor, other.m_referencingModes));
}

CachedImage::CachedImage(URL&& url, Ref<CSSImageValue>&& cssValue, float scaleFactor)
    : CachedImage { WTF::move(url), WTF::move(cssValue), scaleFactor, { SVGReferencingMode::AnimatedImageDocument, SVGReferencingMode::ResourceDocument } }
{
}

CachedImage::CachedImage(URL&& url, Ref<CSSImageValue>&& cssValue, float scaleFactor, OptionSet<SVGReferencingMode> referencingModes)
    : Image { Type::CachedImage }
    , m_url { WTF::move(url) }
    , m_cssValue { WTF::move(cssValue) }
    , m_referencingModes { referencingModes }
    , m_scaleFactor { scaleFactor }
{
    m_cachedImage = m_cssValue->cachedImage();
    if (m_cachedImage)
        m_isPending = false;
}

Vector<CSS::ParamFunction> CachedImage::urlLinkParameters(const CSSParserContext& context, StringView fragmentDirective) const
{
    auto parameters = CSSPropertyParserHelpers::parseLinkParametersFromFragmentDirective(fragmentDirective, context);
    parameters.appendRange(m_url.modifiers.linkParameters);
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

LegacyRenderSVGResourceContainer* CachedImage::uncheckedRenderSVGResource(TreeScope& treeScope, const AtomString& fragment) const
{
    CheckedPtr renderSVGResource = ReferencedSVGResources::referencedRenderResource(treeScope, fragment);
    m_isRenderSVGResource = !!renderSVGResource;
    return renderSVGResource.unsafeGet();
}

LegacyRenderSVGResourceContainer* CachedImage::uncheckedRenderSVGResource(const RenderElement* renderer) const
{
    if (!renderer)
        return nullptr;

    if (!m_referencingModes.contains(SVGReferencingMode::ResourceDocument) || !m_url.resolved.string().contains('#')) {
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

    if (!m_referencingModes.contains(SVGReferencingMode::ResourceDocument) || !m_url.resolved.string().contains('#'))
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

bool CachedImage::canRender(const RenderElement* renderer) const
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

NaturalDimensions CachedImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext&) const
{
    if (isRenderSVGResource(&renderer))
        return NaturalDimensions::none();

    RefPtr cachedImage = m_cachedImage;
    if (!cachedImage)
        return NaturalDimensions::none();

    return cachedImage->naturalDimensions(usedOrientation(renderer.imageOrientation()));
}

WebCore::ImageOrientation CachedImage::usedOrientation(WebCore::ImageOrientation orientation) const
{
    return (m_cachedImage && protect(m_cachedImage)->allowsOrientationOverride()) ? orientation : WebCore::ImageOrientation::Orientation::FromImage;
}

ImageDrawingExtras CachedImage::drawingExtrasForRenderer(const RenderElement& renderer) const
{
    auto fragmentURL = m_url.resolved;
    auto fragmentDirective = fragmentURL.consumeFragmentDirective();
    return { WTF::move(fragmentURL), linkParametersForResource(renderer.style().linkParameters(), urlLinkParameters(protect(renderer.document())->cssParserContext(), fragmentDirective)) };
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

bool CachedImage::hasDecodedImage() const
{
    return m_cachedImage && !m_cachedImage->errorOccurred() && m_cachedImage->hasImage();
}

RefPtr<WebCore::Image> CachedImage::resolvedImage() const
{
    ASSERT(!m_isPending);

    if (!m_cachedImage)
        return nullptr;

    return protect(m_cachedImage)->image();
}

CachedImage::ReferencedSVGResource CachedImage::referencedSVGResource(const RenderElement& renderer) const
{
    if (CheckedPtr resource = renderSVGResource(&renderer))
        return { resource.get(), nullptr };

    return { nullptr, legacyRenderSVGResource(&renderer) };
}

ImageDrawResult CachedImage::drawSVGResource(GraphicsContext& context, const ReferencedSVGResource& referenced, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options) const
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

ImageDrawResult CachedImage::drawSVGResourceAsPattern(GraphicsContext& context, const ReferencedSVGResource& referenced, const FloatSize& size, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options) const
{
    RefPtr imageBuffer = context.createImageBuffer(size);
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    auto imageRect = FloatRect { { }, size };
    auto result = drawSVGResource(imageBuffer->context(), referenced, imageRect, imageRect, options);

    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);
    return result;
}

static std::optional<FloatSize> fixedContentSize(const WebCore::Image& image, WebCore::ImageOrientation orientation = WebCore::ImageOrientation::Orientation::FromImage)
{
    if (image.usesConcreteObjectSizeAsViewport())
        return std::nullopt;

    auto naturalDimensions = image.naturalDimensions(orientation);
    if (!naturalDimensions.width || !naturalDimensions.height)
        return std::nullopt;
    return FloatSize { *naturalDimensions.width, *naturalDimensions.height };
}

static ConcreteObjectSize LEGACY_NODELETE viewportSize(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize)
{
    auto zoom = renderer.style().usedZoom();
    return ConcreteObjectSize::fixed(concreteObjectSize.size() * concreteObjectSize.zoom() / zoom, zoom);
}

ConcreteObjectSize CachedImage::concreteSizeToDrawAt(const WebCore::Image& image, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, WebCore::ImageOrientation orientation) const
{
    if (auto size = fixedContentSize(image, orientation))
        return ConcreteObjectSize::fixed(*size);
    return viewportSize(renderer, concreteObjectSize);
}

NaturalDimensions CachedImage::tileNaturalDimensions(const WebCore::Image& image) const
{
    if (image.usesConcreteObjectSizeAsViewport())
        return NaturalDimensions::none();
    return image.naturalDimensions();
}

ImageDrawResult CachedImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, bool) const
{
    if (isPending())
        return ImageDrawResult::DidNothing;

    if (auto referenced = referencedSVGResource(renderer))
        return drawSVGResource(context, referenced, destination, source, options);

    RefPtr image = resolvedImage();
    if (!image || !image->hasSomethingToDraw())
        return ImageDrawResult::DidNothing;

    options = { options, usedOrientation(options.orientation()) };
    if (auto size = fixedContentSize(*image, options.orientation()))
        return drawResolved(context, renderer, *image, ConcreteObjectSize::fixed(*size), destination, mapSourceToSize(source, concreteObjectSize, *size), options);
    return drawResolved(context, renderer, *image, viewportSize(renderer, concreteObjectSize), destination, source, options);
}

ImageDrawResult CachedImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, bool) const
{
    if (context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    if (auto referenced = referencedSVGResource(renderer))
        return drawSVGResourceAsPattern(context, referenced, concreteObjectSize.size() * concreteObjectSize.zoom(), destination, tile, patternTransform, phase, spacing, options);

    RefPtr image = resolvedImage();
    if (!image)
        return ImageDrawResult::DidNothing;

    options = { options, usedOrientation(options.orientation()) };
    return drawResolvedAsPattern(context, renderer, *image, concreteSizeToDrawAt(*image, renderer, concreteObjectSize, options.orientation()), destination, tile, patternTransform, phase, spacing, options);
}

ImageDrawResult CachedImage::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImageDrawingOptions options, bool) const
{
    if (context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    if (auto referenced = referencedSVGResource(renderer)) {
        return drawTiledUsing(context, NaturalDimensions::none(), [&](GraphicsContext& context, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
            return drawSVGResource(context, referenced, destination, source, options);
        }, [&](GraphicsContext& context, ConcreteObjectSize tileConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
            return drawSVGResourceAsPattern(context, referenced, tileConcreteObjectSize.size(), destination, tile, patternTransform, phase, spacing, options);
        }, ConcreteObjectSize::fixed(tileSize), destination, phase, tileSize, spacing, options);
    }

    RefPtr image = resolvedImage();
    if (!image)
        return ImageDrawResult::DidNothing;

    options = { options, usedOrientation(options.orientation()) };
    return drawResolvedTiled(context, renderer, *image, tileNaturalDimensions(*image), concreteSizeToDrawAt(*image, renderer, concreteObjectSize, options.orientation()), destination, phase, tileSize, spacing, options);
}

ImageDrawResult CachedImage::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImageDrawingOptions options) const
{
    if (context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    if (auto referenced = referencedSVGResource(renderer)) {
        return drawNinePieceUsing(context, [&](GraphicsContext& context, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
            return drawSVGResource(context, referenced, destination, source, options);
        }, [&](GraphicsContext& context, ConcreteObjectSize pieceConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
            return drawSVGResourceAsPattern(context, referenced, pieceConcreteObjectSize.size(), destination, tile, patternTransform, phase, spacing, { options.compositeOperator(), options.interpolationQuality() });
        }, ConcreteObjectSize::fixed(concreteObjectSize.size() * concreteObjectSize.zoom()), geometry);
    }

    RefPtr image = resolvedImage();
    if (!image)
        return ImageDrawResult::DidNothing;

    options = { options, usedOrientation(options.orientation()) };
    return drawResolvedNinePiece(context, renderer, *image, concreteSizeToDrawAt(*image, renderer, concreteObjectSize, options.orientation()), geometry, options);
}

bool CachedImage::currentFrameIsComplete(const RenderElement*) const
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

bool CachedImage::canDraw(const RenderElement& renderer) const
{
    if (m_isPending)
        return false;
    if (renderSVGResource(&renderer) || legacyRenderSVGResource(&renderer))
        return true;
    RefPtr image = m_cachedImage ? protect(m_cachedImage)->image() : nullptr;
    return image && image->hasSomethingToDraw();
}

bool CachedImage::canDrawAtSize(const RenderElement& renderer, const FloatSize& size) const
{
    return !size.isEmpty() && canDraw(renderer);
}

RefPtr<NativeImage> CachedImage::nativeImage(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, float resolutionScale, ImageDrawingOptions options) const
{
    if (!referencedSVGResource(renderer)) {
        if (RefPtr bitmapImage = dynamicDowncast<BitmapImage>(resolvedImage()))
            return bitmapImage->nativeImage();
    }
    return Image::nativeImage(renderer, concreteObjectSize, resolutionScale, options);
}

std::optional<IntPoint> CachedImage::hotSpot() const
{
    RefPtr image = resolvedImage();
    return image ? image->hotSpot() : std::nullopt;
}

bool CachedImage::drawsSVGImage() const
{
    RefPtr image = m_cachedImage ? protect(m_cachedImage)->image() : nullptr;
    return image && image->drawsSVGImage();
}

WTF::String CachedImage::accessibilityDescription() const
{
    return m_cachedImage ? protect(m_cachedImage)->accessibilityDescription() : WTF::String();
}

bool CachedImage::isAnimated() const
{
    return m_cachedImage ? protect(m_cachedImage)->isAnimated() : false;
}

void CachedImage::stopAnimation()
{
    if (m_cachedImage)
        protect(m_cachedImage)->stopAnimation();
}

void CachedImage::resetAnimation()
{
    if (m_cachedImage)
        protect(m_cachedImage)->resetAnimation();
}

DecodingMode CachedImage::decodingModeForImageDraw(const RenderBoxModelObject& renderer, const PaintInfo& paintInfo) const
{
    if (!m_cachedImage || renderSVGResource(&renderer) || legacyRenderSVGResource(&renderer))
        return Image::decodingModeForImageDraw(renderer, paintInfo);
    return renderer.decodingModeForImageDraw(*protect(protect(m_cachedImage)->image()), paintInfo);
}

InterpolationQuality CachedImage::interpolationQualityForImageDraw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const void* layer, const LayoutSize& size) const
{
    if (!m_cachedImage || renderSVGResource(&renderer) || legacyRenderSVGResource(&renderer))
        return Image::interpolationQualityForImageDraw(context, renderer, concreteObjectSize, layer, size);
    auto contentSize = fixedContentSize(*protect(protect(m_cachedImage)->image()));
    if (!contentSize)
        return InterpolationQuality::Default;
    return ImageQualityController::chooseInterpolationQuality(context, renderer, IntSize(*contentSize), layer, size);
}

FloatRect CachedImage::mapChangedRectToSource(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& changedRect) const
{
    if (isPending())
        return Image::mapChangedRectToSource(renderer, concreteObjectSize, changedRect);

    RefPtr image = resolvedImage();
    if (!image)
        return Image::mapChangedRectToSource(renderer, concreteObjectSize, changedRect);

    if (auto size = fixedContentSize(*image, usedOrientation(renderer.imageOrientation())))
        return mapSizeToSource(changedRect, *size, concreteObjectSize);
    return mapSizeToSource(changedRect, viewportSize(renderer, concreteObjectSize).size(), concreteObjectSize);
}

bool CachedImage::usesDataProtocol() const
{
    return m_url.resolved.protocolIsData();
}

} // namespace Style
} // namespace WebCore
