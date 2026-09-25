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
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

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

class CachedImage final : public Image {
    WTF_MAKE_TZONE_ALLOCATED(CachedImage);
public:
    static Ref<CachedImage> create(URL&&, Ref<CSSImageValue>&&, float scaleFactor = 1);
    static Ref<CachedImage> create(const URL&, const Ref<CSSImageValue>&, float scaleFactor = 1);
    static Ref<CachedImage> create(WebCore::CachedImage&, WTF::URL&& authoredURL, float scaleFactor = 1);
    static Ref<CachedImage> copyOverridingScaleFactor(CachedImage&, float scaleFactor);
    virtual ~CachedImage();

    bool operator==(const Image&) const final;
    bool equals(const CachedImage&) const;

    const CachedImage* cachedImage() const final { return m_cachedImage ? this : nullptr; }

    WebCore::CachedImage* NODELETE resource() const;

    bool hasDecodedImage() const final;
    RefPtr<NativeImage> nativeImage(const RenderElement&, ConcreteObjectSize, const ColorSpace&) const final;
    std::optional<IntPoint> hotSpot() const final;

    bool isOpaqueBitmap() const;

    bool isTransparentBitmap() const;

    bool isSVGImage() const final;
    bool hasNothingToDraw(const RenderElement&) const final;

    WrappedImagePtr data() const final { return m_cachedImage.get(); }

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;

    bool canRender(const RenderElement&, float multiplier) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded(const RenderElement&) const final;
    bool errorOccurred() const final;
    NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const final;
    WTF::String accessibilityDescription() const final;

    bool isAnimated() const final;
    void stopAnimation() final;
    void resetAnimation() final;

    ImageDrawingExtras::IgnoreRootPreserveAspectRatio ignoreRootPreserveAspectRatio(const RenderElement&) const;
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    bool hasImage() const final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawTiled(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawNinePiece(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const NinePieceGeometry&, ImagePaintingOptions) const final;

    ImageDrawResult drawInOwnSpace(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions) const;

    ConcreteObjectSize concreteSizeToDrawAt(const RenderElement&, ConcreteObjectSize laidOut) const;

    Ref<WebCore::Image> decodedImage() const;
    bool currentFrameIsComplete() const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool isOriginClean(Document&) const final;
    bool usesDataProtocol() const final;

    URL url() const final;

private:
    NaturalDimensions tileNaturalDimensions(const RenderElement&) const;
    ImageDrawingExtras drawingExtrasForRenderer(const RenderElement&, const WTF::URL& = WTF::URL()) const final;
    RefPtr<WebCore::Image> resolvedImage() const;
    WebCore::ImageOrientation orientationForRenderer(const RenderElement&) const;
    bool allowsOrientationOverride() const;

    struct ReferencedSVGResource {
        SingleThreadWeakPtr<RenderSVGResourceContainer> resource;
        SingleThreadWeakPtr<LegacyRenderSVGResourceContainer> legacyResource;

        explicit operator bool() const { return resource || legacyResource; }
    };
    ReferencedSVGResource referencedSVGResource(const RenderElement&) const;
    ImageDrawResult drawSVGResource(GraphicsContext&, const ReferencedSVGResource&, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions) const;
    ImageDrawResult drawSVGResourceAsPattern(GraphicsContext&, const ReferencedSVGResource&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions) const;

    CachedImage(URL&&, Ref<CSSImageValue>&&, float);

    Vector<CSS::ParamFunction> urlLinkParameters(const CSSParserContext&, StringView fragment) const;

    LegacyRenderSVGResourceContainer* uncheckedLegacyRenderSVGResource(TreeScope&, const AtomString& fragment) const;
    LegacyRenderSVGResourceContainer* uncheckedLegacyRenderSVGResource(const RenderElement&) const;
    LegacyRenderSVGResourceContainer* legacyRenderSVGResource(const RenderElement&) const;
    RenderSVGResourceContainer* renderSVGResource(const RenderElement&) const;
    bool isRenderSVGResource(const RenderElement&) const;

    URL m_url;
    WTF::URL m_authoredURL;
    const Ref<CSSImageValue> m_cssValue;
    bool m_isPending { true };
    mutable float m_scaleFactor { 1 };
    mutable CachedResourceHandle<WebCore::CachedImage> m_cachedImage;
    enum class ResourceType : uint8_t { Unknown, SVGResource, LegacySVGResource, ResolvedImage };
    mutable ResourceType m_resourceType { ResourceType::Unknown };
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CachedImage, isCachedImage)
