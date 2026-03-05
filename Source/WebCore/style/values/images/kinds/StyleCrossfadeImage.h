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

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "StyleGeneratedImage.h"

namespace WebCore {

struct BlendingContext;

namespace Style {

class CrossfadeImage final : public GeneratedImage, private CachedImageClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CrossfadeImage);
public:
    static Ref<CrossfadeImage> create(RefPtr<Image> from, RefPtr<Image> to, double percentage, bool isPrefixed)
    {
        return adoptRef(*new CrossfadeImage(WTF::move(from), WTF::move(to), percentage, isPrefixed));
    }
    virtual ~CrossfadeImage();

    // CachedResourceClient.
    void ref() const final { GeneratedImage::ref(); }
    void deref() const final { GeneratedImage::deref(); }

    RefPtr<CrossfadeImage> blend(const CrossfadeImage&, const BlendingContext&) const;

    bool operator==(const Image&) const final;
    bool equals(const CrossfadeImage&) const;
    bool equalInputImages(const CrossfadeImage&) const;

    static constexpr bool isFixedSize = true;

private:
    explicit CrossfadeImage(RefPtr<Image>&&, RefPtr<Image>&&, double, bool);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext& destinationContext, bool isForFirstLine) const final;
    bool currentFrameIsComplete(const RenderElement*) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    // CachedImageClient.
    void imageChanged(WebCore::CachedImage*, const IntRect*) final;

    RefPtr<Image> m_from;
    RefPtr<Image> m_to;
    double m_percentage;
    bool m_isPrefixed;

    // FIXME: Rather than caching and tracking the input image via WebCore::CachedImages, we should
    // instead use a new, Style::Image specific notification, to allow correct tracking of
    // nested images (e.g. one of the input images for a Style::CrossfadeImage is a Style::FilterImage
    // where its input image is a Style::CachedImage).
    CachedResourceHandle<WebCore::CachedImage> m_cachedFromImage;
    CachedResourceHandle<WebCore::CachedImage> m_cachedToImage;
    bool m_inputImagesAreReady;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CrossfadeImage, isCrossfadeImage)
