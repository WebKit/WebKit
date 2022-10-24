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
#include "FilterOperations.h"
#include "StyleGeneratedImage.h"

namespace WebCore {

class StyleFilterImage final : public StyleGeneratedImage, private CachedImageClient {
public:
    static Ref<StyleFilterImage> create(RefPtr<StyleImage> image, FilterOperations filterOperations)
    {
        return adoptRef(*new StyleFilterImage(WTFMove(image), WTFMove(filterOperations)));
    }
    virtual ~StyleFilterImage();

    bool operator==(const StyleImage& other) const final;
    bool equals(const StyleFilterImage&) const;
    bool equalInputImages(const StyleFilterImage&) const;

    RefPtr<StyleImage> inputImage() const { return m_image; }
    const FilterOperations& filterOperations() const { return m_filterOperations; }

    static constexpr bool isFixedSize = true;

private:
    explicit StyleFilterImage(RefPtr<StyleImage>&&, FilterOperations&&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    // CachedImageClient.
    void imageChanged(CachedImage*, const IntRect* = nullptr) final;

    RefPtr<StyleImage> m_image;
    FilterOperations m_filterOperations;

    // FIXME: Rather than caching and tracking the input image via CachedImages, we should
    // instead use a new, StyleImage specific notification, to allow correct tracking of
    // nested images (e.g. the input image for a StyleFilterImage is a StyleCrossfadeImage
    // where one of the inputs to the StyleCrossfadeImage is a StyleCachedImage).
    CachedResourceHandle<CachedImage> m_cachedImage;
    bool m_inputImageIsReady;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleFilterImage, isFilterImage)
