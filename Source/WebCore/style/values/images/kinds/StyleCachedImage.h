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

#include "CachedImage.h"
#include "CachedResourceHandle.h"
#include "StyleImage.h"
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

class CachedImage final : public Image, public CachedImageClient {
    WTF_MAKE_TZONE_ALLOCATED(CachedImage);
public:
    static Ref<CachedImage> create(URL&&, Ref<CSSImageValue>&&, float scaleFactor = 1);
    static Ref<CachedImage> create(const URL&, const Ref<CSSImageValue>&, float scaleFactor = 1);
    static Ref<CachedImage> create(WebCore::CachedImage&, float scaleFactor = 1);
    static Ref<CachedImage> copyOverridingScaleFactor(CachedImage&, float scaleFactor);
    virtual ~CachedImage();

    // CachedImageClient.
    void ref() const final { Image::ref(); }
    void deref() const final { Image::deref(); }

    bool operator==(const Image&) const final;
    bool equals(const CachedImage&) const;

    WebCore::CachedImage* NODELETE cachedImage() const final;

    WrappedImagePtr data() const final { return m_cachedImage.get(); }

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;

    bool canRender(const RenderElement*, float multiplier) const final;
    bool isPending() const final;
    bool isLoading() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded(const RenderElement*) const final;
    bool errorOccurred() const final;
    FloatSize imageSize(const RenderElement*, float multiplier, ImageSizeType = ImageSizeType::Used) const final;
    bool imageHasRelativeWidth() const final;
    bool imageHasRelativeHeight() const final;
    bool imageHasNaturalAspectRatio() const final;
    bool hasHDRContent() const final;
    void computeIntrinsicDimensions(const RenderElement*, float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool usesImageContainerSize() const final;
    void registerContainerContext(const ImageContainerContextKey&, ImageContainerContext&&) final;
    bool hasImage() const final;
    RefPtr<WebCore::Image> image() const final;
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext& destinationContext, bool isForFirstLine) const final;
    bool currentFrameIsComplete(const RenderElement&) const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool usesDataProtocol() const final;
    void didAddClient(ImageClient&) final { }
    void didRemoveClient(ImageClient&) final { }

    URL url() const final;

private:
    CachedImage(URL&&, Ref<CSSImageValue>&&, float);

    LegacyRenderSVGResourceContainer* uncheckedRenderSVGResource(TreeScope&, const AtomString& fragment) const;
    LegacyRenderSVGResourceContainer* uncheckedRenderSVGResource(const RenderElement*) const;
    LegacyRenderSVGResourceContainer* legacyRenderSVGResource(const RenderElement*) const;
    RenderSVGResourceContainer* renderSVGResource(const RenderElement*) const;
    bool isRenderSVGResource(const RenderElement*) const;

    // CachedImageClient.
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;
    void imageChanged(WebCore::CachedImage&, const IntRect* = nullptr) final;
    bool allowsAnimation() const final;
    bool canDestroyDecodedData() const final;
    bool useSystemDarkAppearance() const final;
    VisibleInViewportState imageFrameAvailable(WebCore::CachedImage&, ImageAnimatingState, const IntRect*) final;
    VisibleInViewportState imageVisibleInViewport(WebCore::CachedImage&, const Document&) const final;
    void didRemoveCachedImageClient(WebCore::CachedImage&) final;
    void imageContentChanged(WebCore::CachedImage&) final;
    void scheduleRenderingUpdateForImage(WebCore::CachedImage&) final;
    bool isRendererClient() const final;

    URL m_url;
    const Ref<CSSImageValue> m_cssValue;
    bool m_isPending { true };
    mutable float m_scaleFactor { 1 };
    mutable CachedResourceHandle<WebCore::CachedImage> m_cachedImage;
    mutable std::optional<bool> m_isRenderSVGResource;
    FloatSize m_containerSize;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CachedImage, isCachedImage)
