/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSLinkParameter.h"
#include "CachedImage.h"
#include "CachedResourceHandle.h"
#include "StyleImage.h"
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CSSValue;
class CSSImageValue;
class CachedImage;
class Document;
class LegacyRenderSVGResourceContainer;
class RenderElement;
class RenderSVGResourceContainer;
class TreeScope;

namespace Style {

// https://svgwg.org/specs/integration/#referencing-modes
enum class SVGReferencingMode : uint8_t {
    AnimatedImageDocument = 1 << 0,
    ResourceDocument = 1 << 1,
};

class CachedImage final : public Image {
    WTF_MAKE_TZONE_ALLOCATED(CachedImage);
public:
    static Ref<CachedImage> LEGACY_NODELETE create(URL&&, Ref<CSSImageValue>&&, float scaleFactor = 1);
    static Ref<CachedImage> create(const URL&, const Ref<CSSImageValue>&, float scaleFactor = 1);
    static Ref<CachedImage> create(WebCore::CachedImage&, WTF::URL&& authoredURL, OptionSet<SVGReferencingMode>, float scaleFactor = 1);
    static Ref<CachedImage> copyOverridingScaleFactor(CachedImage&, float scaleFactor);
    virtual ~CachedImage();

    bool operator==(const Image&) const final;
    bool equals(const CachedImage&) const;

    WebCore::CachedImage* LEGACY_NODELETE cachedImage() const final;

    WrappedImagePtr data() const final { return m_cachedImage.get(); }

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;

    bool canRender(const RenderElement*) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded(const RenderElement*) const final;
    bool errorOccurred() const final;
    NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const final;
    ImageDrawingExtras drawingExtrasForRenderer(const RenderElement&) const final;
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    bool hasImage() const final;
    bool hasDecodedImage() const final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawTiled(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawNinePiece(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const NinePieceGeometry&, ImagePaintingOptions) const final;
    RefPtr<NativeImage> nativeImage(const RenderElement&, ConcreteObjectSize, float resolutionScale, ImagePaintingOptions) const final;
    bool currentFrameIsComplete(const RenderElement*) const final;
    std::optional<IntPoint> hotSpot() const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool canDraw(const RenderElement&) const final;
    bool containsCurrentColor() const final { return false; }
    bool canDrawAtSize(const RenderElement&, const FloatSize&) const final;
    bool drawsSVGImage() const final;
    WTF::String accessibilityDescription() const final;
    bool isAnimated() const final;
    void stopAnimation() final;
    void resetAnimation() final;
    DecodingMode decodingModeForImageDraw(const RenderBoxModelObject&, const PaintInfo&) const final;
    InterpolationQuality interpolationQualityForImageDraw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const void* layer, const LayoutSize&) const final;
    FloatRect mapChangedRectToSource(const RenderElement&, ConcreteObjectSize, const FloatRect& changedRect) const final;
    bool usesDataProtocol() const final;

    URL url() const final;

private:
    CachedImage(URL&&, Ref<CSSImageValue>&&, float);
    CachedImage(URL&&, Ref<CSSImageValue>&&, float, OptionSet<SVGReferencingMode>);

    RefPtr<WebCore::Image> resolvedImage() const;
    ConcreteObjectSize concreteSizeToDrawAt(const WebCore::Image&, const RenderElement&, ConcreteObjectSize) const;
    NaturalDimensions tileNaturalDimensions(const WebCore::Image&) const;
    Vector<CSS::ParamFunction> urlLinkParameters(const CSSParserContext&, StringView fragment) const;

    struct ReferencedSVGResource {
        SingleThreadWeakPtr<RenderSVGResourceContainer> resource;
        SingleThreadWeakPtr<LegacyRenderSVGResourceContainer> legacyResource;

        explicit operator bool() const { return resource || legacyResource; }
    };
    ReferencedSVGResource referencedSVGResource(const RenderElement&) const;
    LegacyRenderSVGResourceContainer* uncheckedRenderSVGResource(TreeScope&, const AtomString& fragment) const;
    LegacyRenderSVGResourceContainer* uncheckedRenderSVGResource(const RenderElement*) const;
    LegacyRenderSVGResourceContainer* legacyRenderSVGResource(const RenderElement*) const;
    RenderSVGResourceContainer* renderSVGResource(const RenderElement*) const;
    bool isRenderSVGResource(const RenderElement*) const;
    ImageDrawResult drawSVGResource(GraphicsContext&, const ReferencedSVGResource&, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions) const;
    ImageDrawResult drawSVGResourceAsPattern(GraphicsContext&, const ReferencedSVGResource&, const FloatSize&, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions) const;

    URL m_url;
    const Ref<CSSImageValue> m_cssValue;
    const OptionSet<SVGReferencingMode> m_referencingModes;
    bool m_isPending { true };
    mutable float m_scaleFactor { 1 };
    mutable CachedResourceHandle<WebCore::CachedImage> m_cachedImage;
    mutable std::optional<bool> m_isRenderSVGResource;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CachedImage, isCachedImage)
