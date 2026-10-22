/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CSSNormalizedMixPercentages.h"
#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "StyleCrossfade.h"
#include "StyleGeneratedImage.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

struct BlendingContext;

namespace Style {

class CrossfadeImage final : public GeneratedImage, private CachedImageClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CrossfadeImage);
public:
    static Ref<CrossfadeImage> create(CrossfadeFunction&&);
    static Ref<CrossfadeImage> create(WebkitCrossfadeFunction&&);
    virtual ~CrossfadeImage();

    // CachedResourceClient.
    void ref() const final { GeneratedImage::ref(); }
    void deref() const final { GeneratedImage::deref(); }

    RefPtr<CrossfadeImage> blend(const CrossfadeImage&, const BlendingContext&) const;

    bool operator==(const Image&) const final;
    bool equals(const CrossfadeImage&) const;
    bool equalInputImages(const CrossfadeImage&) const;

private:
    explicit CrossfadeImage(CrossfadeFunction&&);
    explicit CrossfadeImage(WebkitCrossfadeFunction&&);

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions, bool isForFirstLine) const final;
    bool currentFrameIsComplete(const RenderElement*) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool containsCurrentColor() const final;
    bool canDrawAtSize(const RenderElement&, const FloatSize&) const final;
    InterpolationQuality interpolationQualityForImageDraw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const void* layer, const LayoutSize&) const final;
    NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    // CachedImageClient.
    void imageChanged(WebCore::CachedImage*, const IntRect*) final;

    void drawCrossfade(GraphicsContext&, const RenderElement&, ConcreteObjectSize, bool isForFirstLine) const;
    void normalizePercentages();

    decltype(auto) withInputs(NOESCAPE auto&&) const;

    Variant<CrossfadeFunction, WebkitCrossfadeFunction> m_function;
    CSS::NormalizedMixPercentages<Vector<double, 2>> m_normalizedPercentages;
    // FIXME: Rather than caching and tracking the input image via WebCore::CachedImages, we should
    // instead use a new, Style::Image specific notification, to allow correct tracking of
    // nested images (e.g. one of the input images for a Style::CrossfadeImage is a Style::FilterImage
    // where its input image is a Style::CachedImage).
    Vector<CachedResourceHandle<WebCore::CachedImage>> m_cachedImages;
    bool m_inputImagesAreReady;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CrossfadeImage, isCrossfadeImage)
