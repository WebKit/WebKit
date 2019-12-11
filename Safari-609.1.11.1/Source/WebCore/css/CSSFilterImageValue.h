/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "CSSImageGeneratorValue.h"
#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "FilterOperations.h"
#include "Image.h"
#include <wtf/Function.h>

namespace WebCore {

class CachedImage;
class FilterSubimageObserverProxy;
class RenderElement;
class Document;

namespace Style {
class BuilderState;
}

class CSSFilterImageValue final : public CSSImageGeneratorValue {
    friend class FilterSubimageObserverProxy;
public:
    static Ref<CSSFilterImageValue> create(Ref<CSSValue>&& imageValue, Ref<CSSValue>&& filterValue)
    {
        return adoptRef(*new CSSFilterImageValue(WTFMove(imageValue), WTFMove(filterValue)));
    }

    ~CSSFilterImageValue();

    String customCSSText() const;

    RefPtr<Image> image(RenderElement&, const FloatSize&);
    bool isFixedSize() const { return true; }
    FloatSize fixedSize(const RenderElement&);

    bool isPending() const;
    bool knownToBeOpaque(const RenderElement&) const;

    void loadSubimages(CachedResourceLoader&, const ResourceLoaderOptions&);

    bool traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const;

    bool equals(const CSSFilterImageValue&) const;

    bool equalInputImages(const CSSFilterImageValue&) const;

    void createFilterOperations(Style::BuilderState&);

    const FilterOperations& filterOperations() const { return m_filterOperations; }
    void setFilterOperations(const FilterOperations& filterOperations)
    {
        m_filterOperations = filterOperations;
    }
    CachedImage* cachedImage() const { return m_cachedImage.get(); }

private:
    CSSFilterImageValue(Ref<CSSValue>&& imageValue, Ref<CSSValue>&& filterValue)
        : CSSImageGeneratorValue(FilterImageClass)
        , m_imageValue(WTFMove(imageValue))
        , m_filterValue(WTFMove(filterValue))
        , m_filterSubimageObserver(this)
    {
    }

    class FilterSubimageObserverProxy final : public CachedImageClient {
    public:
        FilterSubimageObserverProxy(CSSFilterImageValue* ownerValue)
            : m_ownerValue(ownerValue)
            , m_ready(false)
        {
        }

        virtual ~FilterSubimageObserverProxy() = default;
        void imageChanged(CachedImage*, const IntRect* = nullptr) final;
        void setReady(bool ready) { m_ready = ready; }
    private:
        CSSFilterImageValue* m_ownerValue;
        bool m_ready;
    };

    void filterImageChanged(const IntRect&);

    Ref<CSSValue> m_imageValue;
    Ref<CSSValue> m_filterValue;

    FilterOperations m_filterOperations;

    CachedResourceHandle<CachedImage> m_cachedImage;

    FilterSubimageObserverProxy m_filterSubimageObserver;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSFilterImageValue, isFilterImageValue())
