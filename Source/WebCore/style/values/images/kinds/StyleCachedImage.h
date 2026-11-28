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
#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "StyleImage.h"
#include <wtf/CheckedPtr.h>
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
class SVGSVGElement;
class TreeScope;

namespace Style {

// https://svgwg.org/specs/integration/#referencing-modes
enum class SVGReferencingMode : uint8_t {
    AnimatedImageDocument = 1 << 0,
    ResourceDocument = 1 << 1,
};

class CachedImage final : public Image, private CachedImageClient {
    WTF_MAKE_TZONE_ALLOCATED(CachedImage);
public:
    NODELETE static Ref<CachedImage> create(URL&&, Ref<CSSImageValue>&&, Resolution<> = 1);
    static Ref<CachedImage> create(const URL&, const Ref<CSSImageValue>&, Resolution<> = 1);
    static Ref<CachedImage> create(WebCore::CachedImage&, WTF::URL&& authoredURL, OptionSet<SVGReferencingMode>, Resolution<> = 1);
    static Ref<CachedImage> copyOverridingResolution(CachedImage&, Resolution<>);
    virtual ~CachedImage();

    bool operator==(const Image&) const final;
    bool equals(const CachedImage&) const;

    NODELETE WebCore::CachedImage* cachedImage() const final;

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;

    bool canRender(const RenderElement*) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded(const RenderElement*) const final;
    NODELETE bool errorOccurred() const final;
    NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const final;
    ImageRenditionOptions renditionOptionsForRenderer(const RenderElement&, ImageRenditionOptions) const final;
    void didAddFirstClient() override final;
    void didRemoveLastClient() override final;
    void sendInitialNotifications(ImageClient&, const Image& shownImage) const override final;
    void addClientWaitingForAsyncDecoding(ImageClient&) override final;
    bool isClientWaitingForAsyncDecoding(const ImageClient&) const override final;
    void removeAllClientsWaitingForAsyncDecoding() override final;
    bool hasImage() const final;
    bool hasDecodedImage() const final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatRect& source, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, bool isForFirstLine) const final;
    ImageDrawResult drawTiled(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, bool isForFirstLine) const final;
    ImageDrawResult drawNinePiece(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const NinePieceGeometry&) const final;
    RefPtr<NativeImage> nativeImage(const RenderElement&, const ImageDrawDescriptor&, float resolutionScale) const final;
    bool currentFrameIsComplete(const RenderElement*) const final;
    std::optional<IntPoint> hotSpot() const final;
    ColorSpace colorSpace(const ColorSpace& defaultColorSpace = ColorSpace::SRGB()) const final;
    Resolution<> imageResolution() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool canDraw(const RenderElement&) const final;
    bool containsCurrentColor() const final { return false; }
    bool canDrawAtSize(const RenderElement&, const FloatSize&) const final;
    bool drawsSVGImage() const final;
    bool hasHDRContent() const override final;
    WTF::String accessibilityDescription() const final;
    bool isAnimated() const final;
    void startAnimation() final;
    void stopAnimation() final;
    void clearAnimationOverride() final;
    void resetAnimation() final;
    DecodingMode decodingModeForImageDraw(const RenderBoxModelObject&, const PaintInfo&) const final;
    InterpolationQuality interpolationQualityForImageDraw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const void* layer, const LayoutSize&) const final;
    FloatRect mapChangedRectToSource(const RenderElement&, ConcreteObjectSize, const FloatRect& changedRect) const final;
    bool usesDataProtocol() const final;

    URL url() const final;

    // CachedResourceClient.
    void ref() const override final { Image::ref(); }
    void deref() const override final { Image::deref(); }

    using WeakValueType = Image::WeakValueType;
    using WeakPtrImplType = Image::WeakPtrImplType;
    using Image::weakImpl;

private:
    CachedImage(URL&&, Ref<CSSImageValue>&&, Resolution<>);
    CachedImage(URL&&, Ref<CSSImageValue>&&, Resolution<>, OptionSet<SVGReferencingMode>);

    RefPtr<WebCore::Image> resolvedImage() const;
    WebCore::ImageOrientation usedOrientation(WebCore::ImageOrientation) const;
    ConcreteObjectSize concreteSizeToDrawAt(const WebCore::Image&, const RenderElement&, ConcreteObjectSize, WebCore::ImageOrientation) const;
    NaturalDimensions tileNaturalDimensions(const WebCore::Image&) const;
    Vector<CSS::ParamFunction> urlLinkParameters(const CSSParserContext&, StringView fragmentDirective) const;

    // CachedResourceClient.
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) override final;

    // CachedImageClient.
    void imageChanged(WebCore::CachedImage*, const IntRect* = nullptr) override final;
    bool allowsAnimation() const override final;
    bool canDestroyDecodedData() const override final;
    VisibleInViewportState imageFrameAvailable(WebCore::CachedImage&, ImageAnimatingState, const IntRect*) override final;
    VisibleInViewportState imageVisibleInViewport(const Document&) const override final;
    void imageContentChanged(WebCore::CachedImage&) override final;
    void scheduleRenderingUpdateForImage(WebCore::CachedImage&) override final;
    bool showsImage() const override final { return true; }

    struct ReferencedSVGResource {
        SingleThreadWeakPtr<RenderSVGResourceContainer> resource;
        SingleThreadWeakPtr<LegacyRenderSVGResourceContainer> legacyResource;

        explicit operator bool() const { return resource || legacyResource; }
    };
    ReferencedSVGResource referencedSVGResource(const RenderElement&) const;
    RefPtr<SVGSVGElement> svgImageRootElement(const RenderElement&) const;
    CheckedPtr<LegacyRenderSVGResourceContainer> uncheckedRenderSVGResource(TreeScope&, const AtomString& fragment) const;
    CheckedPtr<LegacyRenderSVGResourceContainer> uncheckedRenderSVGResource(const RenderElement*) const;
    CheckedPtr<LegacyRenderSVGResourceContainer> legacyRenderSVGResource(const RenderElement*) const;
    RenderSVGResourceContainer* renderSVGResource(const RenderElement*) const;
    bool isRenderSVGResource(const RenderElement*) const;
    ImageDrawResult drawSVGResource(GraphicsContext&, const ReferencedSVGResource&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatRect& source) const;
    ImageDrawResult drawSVGResourceAsPattern(GraphicsContext&, const ReferencedSVGResource&, const ImageDrawDescriptor&, const FloatSize&, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing) const;

    URL m_url;
    const Ref<CSSImageValue> m_cssValue;
    const OptionSet<SVGReferencingMode> m_referencingModes;
    bool m_isPending { true };
    Resolution<> m_resolution { 1 };
    mutable CachedResourceHandle<WebCore::CachedImage> m_cachedImage;
    mutable std::optional<bool> m_isRenderSVGResource;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CachedImage, isCachedImage)
