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
#include "StyleFilter.h"
#include "StyleGeneratedImage.h"

namespace WebCore {
namespace Style {

class FilterImage final : public GeneratedImage, private CachedImageClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FilterImage);
public:
    static Ref<FilterImage> create(RefPtr<Image> image, Filter filter)
    {
        return adoptRef(*new FilterImage(WTF::move(image), WTF::move(filter)));
    }
    virtual ~FilterImage();

    // CachedResourceClient.
    void ref() const final { GeneratedImage::ref(); }
    void deref() const final { GeneratedImage::deref(); }

    bool operator==(const Image& other) const final;
    bool equals(const FilterImage&) const;
    bool equalInputImages(const FilterImage&) const;

    RefPtr<Image> inputImage() const { return m_image; }
    const Filter& filter() const { return m_filter; }

    static constexpr bool isFixedSize = true;

private:
    explicit FilterImage(RefPtr<Image>&&, Filter&&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext& destinationContext, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    // CachedImageClient.
    void imageChanged(WebCore::CachedImage*, const IntRect* = nullptr) final;

    RefPtr<Image> m_image;
    Filter m_filter;

    // FIXME: Rather than caching and tracking the input image via WebCore::CachedImages, we should
    // instead use a new, Style::Image specific notification, to allow correct tracking of
    // nested images (e.g. the input image for a Style::FilterImage is a Style::CrossfadeImage
    // where one of the inputs to the Style::CrossfadeImage is a Style::CachedImage).
    CachedResourceHandle<WebCore::CachedImage> m_cachedImage;
    bool m_inputImageIsReady;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(FilterImage, isFilterImage)
