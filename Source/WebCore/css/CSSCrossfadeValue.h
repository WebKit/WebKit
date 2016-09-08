/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSCrossfadeValue_h
#define CSSCrossfadeValue_h

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "CSSImageGeneratorValue.h"
#include "CSSPrimitiveValue.h"
#include "Image.h"
#include "ImageObserver.h"

namespace WebCore {

class CachedImage;
class CrossfadeSubimageObserverProxy;
class RenderElement;
class Document;

class CSSCrossfadeValue final : public CSSImageGeneratorValue {
    friend class CrossfadeSubimageObserverProxy;
public:
    static Ref<CSSCrossfadeValue> create(Ref<CSSValue>&& fromValue, Ref<CSSValue>&& toValue, Ref<CSSPrimitiveValue>&& percentageValue, bool prefixed = false)
    {
        return adoptRef(*new CSSCrossfadeValue(WTFMove(fromValue), WTFMove(toValue), WTFMove(percentageValue), prefixed));
    }

    ~CSSCrossfadeValue();

    String customCSSText() const;

    RefPtr<Image> image(RenderElement*, const FloatSize&);
    bool isFixedSize() const { return true; }
    FloatSize fixedSize(const RenderElement*);

    bool isPrefixed() const { return m_isPrefixed; }
    bool isPending() const;
    bool knownToBeOpaque(const RenderElement*) const;

    void loadSubimages(CachedResourceLoader&, const ResourceLoaderOptions&);

    bool traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const;

    RefPtr<CSSCrossfadeValue> blend(const CSSCrossfadeValue&, double) const;

    bool equals(const CSSCrossfadeValue&) const;

    bool equalInputImages(const CSSCrossfadeValue&) const;

private:
    CSSCrossfadeValue(Ref<CSSValue>&& fromValue, Ref<CSSValue>&& toValue, Ref<CSSPrimitiveValue>&& percentageValue, bool prefixed)
        : CSSImageGeneratorValue(CrossfadeClass)
        , m_fromValue(WTFMove(fromValue))
        , m_toValue(WTFMove(toValue))
        , m_percentageValue(WTFMove(percentageValue))
        , m_crossfadeSubimageObserver(this)
        , m_isPrefixed(prefixed)
    {
    }

    class CrossfadeSubimageObserverProxy final : public CachedImageClient {
    public:
        CrossfadeSubimageObserverProxy(CSSCrossfadeValue* ownerValue)
            : m_ownerValue(ownerValue)
            , m_ready(false)
        {
        }

        virtual ~CrossfadeSubimageObserverProxy() { }
        void imageChanged(CachedImage*, const IntRect* = nullptr) final;
        void setReady(bool ready) { m_ready = ready; }
    private:
        CSSCrossfadeValue* m_ownerValue;
        bool m_ready;
    };

    void crossfadeChanged(const IntRect&);

    Ref<CSSValue> m_fromValue;
    Ref<CSSValue> m_toValue;
    Ref<CSSPrimitiveValue> m_percentageValue;

    CachedResourceHandle<CachedImage> m_cachedFromImage;
    CachedResourceHandle<CachedImage> m_cachedToImage;

    RefPtr<Image> m_generatedImage;

    CrossfadeSubimageObserverProxy m_crossfadeSubimageObserver;
    bool m_isPrefixed { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCrossfadeValue, isCrossfadeValue())

#endif // CSSCrossfadeValue_h
