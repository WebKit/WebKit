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

#include "StyleGeneratedImage.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

struct BlendingContext;

namespace Style {

class CrossfadeImage final : public GeneratedImage, private ImageClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CrossfadeImage);
public:
    using Progress = NumberOrPercentageResolvedToNumber<CSS::ClosedUnitRangeClampBoth, CSS::ClosedPercentageRangeClampBoth>;

    static Ref<CrossfadeImage> create(RefPtr<Image> from, RefPtr<Image> to, Progress progress, bool isPrefixed)
    {
        return adoptRef(*new CrossfadeImage(WTF::move(from), WTF::move(to), progress, isPrefixed));
    }
    virtual ~CrossfadeImage();

    // ImageClient.
    void ref() const final { GeneratedImage::ref(); }
    void deref() const final { GeneratedImage::deref(); }

    RefPtr<CrossfadeImage> blend(const CrossfadeImage&, const BlendingContext&) const;

    bool operator==(const Image&) const final;
    bool equals(const CrossfadeImage&) const;
    bool equalInputImages(const CrossfadeImage&) const;

    static constexpr bool isFixedSize = true;

private:
    explicit CrossfadeImage(RefPtr<Image>&&, RefPtr<Image>&&, Progress, bool);

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;
    bool isPending() const final;
    bool isLoading() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext& destinationContext, bool isForFirstLine) const final;
    bool currentFrameIsComplete(const RenderElement&) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void registerContainerContext(const ImageContainerContextKey&, ImageContainerContext&&) final;
    bool contains(const Image&) const final;
    void didAddClient(ImageClient&) final { }
    void didRemoveClient(ImageClient&) final { }

    // ImageClient.
    void imageChanged(const Image&, const IntRect*) const final;
    void notifyFinished(const CachedImage&) const final;
    bool allowsAnimation(const CachedImage&) const final;
    bool canDestroyDecodedData(const CachedImage&) const final;
    bool useSystemDarkAppearance(const CachedImage&) const final;
    VisibleInViewportState imageFrameAvailable(const CachedImage&, ImageAnimatingState, const IntRect*) const final;
    VisibleInViewportState imageVisibleInViewport(const CachedImage&, const Document&) const final;
    void didRemoveCachedImageClient(const CachedImage&) const final;
    void imageContentChanged(const CachedImage&) const final;
    void scheduleRenderingUpdateForImage(const CachedImage&) const final;
    bool isRendererClient() const final;

    RefPtr<Image> m_from;
    RefPtr<Image> m_to;
    Progress m_progress;
    bool m_isPrefixed;
    bool m_inputImagesAreReady;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CrossfadeImage, isCrossfadeImage)
