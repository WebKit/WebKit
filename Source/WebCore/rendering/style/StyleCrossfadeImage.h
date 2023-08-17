/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

class StyleCrossfadeImage final : public StyleGeneratedImage, private CachedImageClient {
public:
    static Ref<StyleCrossfadeImage> create(RefPtr<StyleImage> from, RefPtr<StyleImage> to, double percentage, bool isPrefixed)
    {
        return adoptRef(*new StyleCrossfadeImage(WTFMove(from), WTFMove(to), percentage, isPrefixed));
    }
    virtual ~StyleCrossfadeImage();

    RefPtr<StyleCrossfadeImage> blend(const StyleCrossfadeImage&, const BlendingContext&) const;

    bool operator==(const StyleImage&) const final;
    bool equals(const StyleCrossfadeImage&) const;
    bool equalInputImages(const StyleCrossfadeImage&) const;

    static constexpr bool isFixedSize = true;

private:
    explicit StyleCrossfadeImage(RefPtr<StyleImage>&&, RefPtr<StyleImage>&&, double, bool);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    // CachedImageClient.
    void imageChanged(CachedImage*, const IntRect*) final;

    RefPtr<StyleImage> m_from;
    RefPtr<StyleImage> m_to;
    double m_percentage;
    bool m_isPrefixed;

    // FIXME: Rather than caching and tracking the input image via CachedImages, we should
    // instead use a new, StyleImage specific notification, to allow correct tracking of
    // nested images (e.g. one of the input images for a StyleCrossfadeImage is a StyleFilterImage
    // where its input image is a StyleCachedImage).
    CachedResourceHandle<CachedImage> m_cachedFromImage;
    CachedResourceHandle<CachedImage> m_cachedToImage;
    bool m_inputImagesAreReady;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleCrossfadeImage, isCrossfadeImage)
