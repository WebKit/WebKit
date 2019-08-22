/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "CSSCrossfadeValue.h"

#include "AnimationUtilities.h"
#include "CSSImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CrossfadeGeneratedImage.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline double blendFunc(double from, double to, double progress)
{
    return blend(from, to, progress);
}

static bool subimageKnownToBeOpaque(const CSSValue& value, const RenderElement& renderer)
{
    if (is<CSSImageValue>(value))
        return downcast<CSSImageValue>(value).knownToBeOpaque(renderer);

    if (is<CSSImageGeneratorValue>(value))
        return downcast<CSSImageGeneratorValue>(value).knownToBeOpaque(renderer);

    ASSERT_NOT_REACHED();

    return false;
}

inline CSSCrossfadeValue::SubimageObserver::SubimageObserver(CSSCrossfadeValue& owner)
    : m_owner(owner)
{
}

void CSSCrossfadeValue::SubimageObserver::imageChanged(CachedImage*, const IntRect*)
{
    m_owner.crossfadeChanged();
}

inline CSSCrossfadeValue::CSSCrossfadeValue(Ref<CSSValue>&& fromValue, Ref<CSSValue>&& toValue, Ref<CSSPrimitiveValue>&& percentageValue, bool prefixed)
    : CSSImageGeneratorValue(CrossfadeClass)
    , m_fromValue(WTFMove(fromValue))
    , m_toValue(WTFMove(toValue))
    , m_percentageValue(WTFMove(percentageValue))
    , m_subimageObserver(*this)
    , m_isPrefixed(prefixed)
{
}

Ref<CSSCrossfadeValue> CSSCrossfadeValue::create(Ref<CSSValue>&& fromValue, Ref<CSSValue>&& toValue, Ref<CSSPrimitiveValue>&& percentageValue, bool prefixed)
{
    return adoptRef(*new CSSCrossfadeValue(WTFMove(fromValue), WTFMove(toValue), WTFMove(percentageValue), prefixed));
}

CSSCrossfadeValue::~CSSCrossfadeValue()
{
    if (m_cachedFromImage)
        m_cachedFromImage->removeClient(m_subimageObserver);
    if (m_cachedToImage)
        m_cachedToImage->removeClient(m_subimageObserver);
}

String CSSCrossfadeValue::customCSSText() const
{
    return makeString(m_isPrefixed ? "-webkit-" : "", "cross-fade(", m_fromValue->cssText(), ", ", m_toValue->cssText(), ", ", m_percentageValue->cssText(), ')');
}

FloatSize CSSCrossfadeValue::fixedSize(const RenderElement& renderer)
{
    float percentage = m_percentageValue->floatValue();
    float inversePercentage = 1 - percentage;

    // FIXME: Skip Content Security Policy check when cross fade is applied to an element in a user agent shadow tree.
    // See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
    auto options = CachedResourceLoader::defaultCachedResourceOptions();

    auto& cachedResourceLoader = renderer.document().cachedResourceLoader();
    auto* cachedFromImage = cachedImageForCSSValue(m_fromValue, cachedResourceLoader, options);
    auto* cachedToImage = cachedImageForCSSValue(m_toValue, cachedResourceLoader, options);

    if (!cachedFromImage || !cachedToImage)
        return FloatSize();

    FloatSize fromImageSize = cachedFromImage->imageForRenderer(&renderer)->size();
    FloatSize toImageSize = cachedToImage->imageForRenderer(&renderer)->size();

    // Rounding issues can cause transitions between images of equal size to return
    // a different fixed size; avoid performing the interpolation if the images are the same size.
    if (fromImageSize == toImageSize)
        return fromImageSize;

    return fromImageSize * inversePercentage + toImageSize * percentage;
}

bool CSSCrossfadeValue::isPending() const
{
    return CSSImageGeneratorValue::subimageIsPending(m_fromValue)
        || CSSImageGeneratorValue::subimageIsPending(m_toValue);
}

bool CSSCrossfadeValue::knownToBeOpaque(const RenderElement& renderer) const
{
    return subimageKnownToBeOpaque(m_fromValue, renderer)
        && subimageKnownToBeOpaque(m_toValue, renderer);
}

void CSSCrossfadeValue::loadSubimages(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    auto oldCachedFromImage = m_cachedFromImage;
    auto oldCachedToImage = m_cachedToImage;

    m_cachedFromImage = CSSImageGeneratorValue::cachedImageForCSSValue(m_fromValue, cachedResourceLoader, options);
    m_cachedToImage = CSSImageGeneratorValue::cachedImageForCSSValue(m_toValue, cachedResourceLoader, options);

    if (m_cachedFromImage != oldCachedFromImage) {
        if (oldCachedFromImage)
            oldCachedFromImage->removeClient(m_subimageObserver);
        if (m_cachedFromImage)
            m_cachedFromImage->addClient(m_subimageObserver);
    }

    if (m_cachedToImage != oldCachedToImage) {
        if (oldCachedToImage)
            oldCachedToImage->removeClient(m_subimageObserver);
        if (m_cachedToImage)
            m_cachedToImage->addClient(m_subimageObserver);
    }

    // FIXME: Unclear why this boolean adds any value; for now keeping it around to avoid changing semantics.
    m_subimagesAreReady = true;
}

Image* CSSCrossfadeValue::image(RenderElement& renderer, const FloatSize& size)
{
    if (size.isEmpty())
        return nullptr;

    // FIXME: Skip Content Security Policy check when cross fade is applied to an element in a user agent shadow tree.
    // See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
    auto options = CachedResourceLoader::defaultCachedResourceOptions();

    auto& cachedResourceLoader = renderer.document().cachedResourceLoader();
    auto* cachedFromImage = cachedImageForCSSValue(m_fromValue, cachedResourceLoader, options);
    auto* cachedToImage = cachedImageForCSSValue(m_toValue, cachedResourceLoader, options);

    if (!cachedFromImage || !cachedToImage)
        return &Image::nullImage();

    auto* fromImage = cachedFromImage->imageForRenderer(&renderer);
    auto* toImage = cachedToImage->imageForRenderer(&renderer);

    if (!fromImage || !toImage)
        return &Image::nullImage();

    m_generatedImage = CrossfadeGeneratedImage::create(*fromImage, *toImage, m_percentageValue->floatValue(), fixedSize(renderer), size);
    return m_generatedImage.get();
}

inline void CSSCrossfadeValue::crossfadeChanged()
{
    if (!m_subimagesAreReady)
        return;
    for (auto& client : clients())
        client.key->imageChanged(this);
}

bool CSSCrossfadeValue::traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const
{
    if (m_cachedFromImage && handler(*m_cachedFromImage))
        return true;
    if (m_cachedToImage && handler(*m_cachedToImage))
        return true;
    return false;
}

RefPtr<CSSCrossfadeValue> CSSCrossfadeValue::blend(const CSSCrossfadeValue& from, double progress) const
{
    ASSERT(equalInputImages(from));

    if (!m_cachedToImage || !m_cachedFromImage)
        return nullptr;

    auto fromImageValue = CSSImageValue::create(*m_cachedFromImage);
    auto toImageValue = CSSImageValue::create(*m_cachedToImage);

    double fromPercentage = from.m_percentageValue->doubleValue();
    if (from.m_percentageValue->isPercentage())
        fromPercentage /= 100.0;
    double toPercentage = m_percentageValue->doubleValue();
    if (m_percentageValue->isPercentage())
        toPercentage /= 100.0;
    auto percentageValue = CSSPrimitiveValue::create(blendFunc(fromPercentage, toPercentage, progress), CSSPrimitiveValue::CSS_NUMBER);

    return CSSCrossfadeValue::create(WTFMove(fromImageValue), WTFMove(toImageValue), WTFMove(percentageValue), from.isPrefixed() && isPrefixed());
}

bool CSSCrossfadeValue::equals(const CSSCrossfadeValue& other) const
{
    return equalInputImages(other) && compareCSSValue(m_percentageValue, other.m_percentageValue);
}


bool CSSCrossfadeValue::equalInputImages(const CSSCrossfadeValue& other) const
{
    return compareCSSValue(m_fromValue, other.m_fromValue) && compareCSSValue(m_toValue, other.m_toValue);
}

} // namespace WebCore
