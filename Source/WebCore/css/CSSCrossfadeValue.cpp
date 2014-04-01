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
#include "CachedResourceLoader.h"
#include "CrossfadeGeneratedImage.h"
#include "ImageBuffer.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline double blendFunc(double from, double to, double progress)
{
    return blend(from, to, progress);
}

static bool subimageKnownToBeOpaque(CSSValue& value, const RenderElement* renderer)
{
    if (value.isImageValue())
        return toCSSImageValue(value).knownToBeOpaque(renderer);

    if (value.isImageGeneratorValue())
        return toCSSImageGeneratorValue(value).knownToBeOpaque(renderer);

    ASSERT_NOT_REACHED();

    return false;
}

CSSCrossfadeValue::~CSSCrossfadeValue()
{
    if (m_cachedFromImage)
        m_cachedFromImage->removeClient(&m_crossfadeSubimageObserver);
    if (m_cachedToImage)
        m_cachedToImage->removeClient(&m_crossfadeSubimageObserver);
}

String CSSCrossfadeValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("-webkit-cross-fade(");
    result.append(m_fromValue->cssText());
    result.appendLiteral(", ");
    result.append(m_toValue->cssText());
    result.appendLiteral(", ");
    result.append(m_percentageValue->cssText());
    result.append(')');
    return result.toString();
}

IntSize CSSCrossfadeValue::fixedSize(const RenderElement* renderer)
{
    float percentage = m_percentageValue->getFloatValue();
    float inversePercentage = 1 - percentage;

    CachedResourceLoader* cachedResourceLoader = renderer->document().cachedResourceLoader();
    CachedImage* cachedFromImage = cachedImageForCSSValue(m_fromValue.get(), cachedResourceLoader);
    CachedImage* cachedToImage = cachedImageForCSSValue(m_toValue.get(), cachedResourceLoader);

    if (!cachedFromImage || !cachedToImage)
        return IntSize();

    FloatSize fromImageSize = cachedFromImage->imageForRenderer(renderer)->size();
    FloatSize toImageSize = cachedToImage->imageForRenderer(renderer)->size();

    // Rounding issues can cause transitions between images of equal size to return
    // a different fixed size; avoid performing the interpolation if the images are the same size.
    if (fromImageSize == toImageSize)
        return IntSize(fromImageSize);

    return IntSize(fromImageSize.width() * inversePercentage + toImageSize.width() * percentage,
        fromImageSize.height() * inversePercentage + toImageSize.height() * percentage);
}

bool CSSCrossfadeValue::isPending() const
{
    return CSSImageGeneratorValue::subimageIsPending(m_fromValue.get())
        || CSSImageGeneratorValue::subimageIsPending(m_toValue.get());
}

bool CSSCrossfadeValue::knownToBeOpaque(const RenderElement* renderer) const
{
    return subimageKnownToBeOpaque(*m_fromValue, renderer) && subimageKnownToBeOpaque(*m_toValue, renderer);
}

void CSSCrossfadeValue::loadSubimages(CachedResourceLoader* cachedResourceLoader)
{
    CachedResourceHandle<CachedImage> oldCachedFromImage = m_cachedFromImage;
    CachedResourceHandle<CachedImage> oldCachedToImage = m_cachedToImage;

    m_cachedFromImage = CSSImageGeneratorValue::cachedImageForCSSValue(m_fromValue.get(), cachedResourceLoader);
    m_cachedToImage = CSSImageGeneratorValue::cachedImageForCSSValue(m_toValue.get(), cachedResourceLoader);

    if (m_cachedFromImage != oldCachedFromImage) {
        if (oldCachedFromImage)
            oldCachedFromImage->removeClient(&m_crossfadeSubimageObserver);
        if (m_cachedFromImage)
            m_cachedFromImage->addClient(&m_crossfadeSubimageObserver);
    }

    if (m_cachedToImage != oldCachedToImage) {
        if (oldCachedToImage)
            oldCachedToImage->removeClient(&m_crossfadeSubimageObserver);
        if (m_cachedToImage)
            m_cachedToImage->addClient(&m_crossfadeSubimageObserver);
    }

    m_crossfadeSubimageObserver.setReady(true);
}

PassRefPtr<Image> CSSCrossfadeValue::image(RenderElement* renderer, const IntSize& size)
{
    if (size.isEmpty())
        return 0;

    CachedResourceLoader* cachedResourceLoader = renderer->document().cachedResourceLoader();
    CachedImage* cachedFromImage = cachedImageForCSSValue(m_fromValue.get(), cachedResourceLoader);
    CachedImage* cachedToImage = cachedImageForCSSValue(m_toValue.get(), cachedResourceLoader);

    if (!cachedFromImage || !cachedToImage)
        return Image::nullImage();

    Image* fromImage = cachedFromImage->imageForRenderer(renderer);
    Image* toImage = cachedToImage->imageForRenderer(renderer);

    if (!fromImage || !toImage)
        return Image::nullImage();

    m_generatedImage = CrossfadeGeneratedImage::create(fromImage, toImage, m_percentageValue->getFloatValue(), fixedSize(renderer), size);

    return m_generatedImage.release();
}

void CSSCrossfadeValue::crossfadeChanged(const IntRect&)
{
    for (auto it = clients().begin(), end = clients().end(); it != end; ++it)
        it->key->imageChanged(static_cast<WrappedImagePtr>(this));
}

void CSSCrossfadeValue::CrossfadeSubimageObserverProxy::imageChanged(CachedImage*, const IntRect* rect)
{
    if (m_ready)
        m_ownerValue->crossfadeChanged(*rect);
}

bool CSSCrossfadeValue::hasFailedOrCanceledSubresources() const
{
    if (m_cachedFromImage && m_cachedFromImage->loadFailedOrCanceled())
        return true;
    if (m_cachedToImage && m_cachedToImage->loadFailedOrCanceled())
        return true;
    return false;
}

PassRefPtr<CSSCrossfadeValue> CSSCrossfadeValue::blend(const CSSCrossfadeValue& from, double progress) const
{
    ASSERT(equalInputImages(from));
    RefPtr<StyleCachedImage> toStyledImage = StyleCachedImage::create(m_cachedToImage.get());
    RefPtr<StyleCachedImage> fromStyledImage = StyleCachedImage::create(m_cachedFromImage.get());

    auto fromImageValue = CSSImageValue::create(m_cachedFromImage->url(), fromStyledImage.get());
    auto toImageValue = CSSImageValue::create(m_cachedToImage->url(), toStyledImage.get());

    RefPtr<CSSCrossfadeValue> crossfadeValue = CSSCrossfadeValue::create(std::move(fromImageValue), std::move(toImageValue));

    double fromPercentage = from.m_percentageValue->getDoubleValue();
    if (from.m_percentageValue->isPercentage())
        fromPercentage /= 100.0;
    double toPercentage = m_percentageValue->getDoubleValue();
    if (m_percentageValue->isPercentage())
        toPercentage /= 100.0;
    crossfadeValue->setPercentage(CSSPrimitiveValue::create(blendFunc(fromPercentage, toPercentage, progress), CSSPrimitiveValue::CSS_NUMBER));
    return crossfadeValue.release();
}

bool CSSCrossfadeValue::equals(const CSSCrossfadeValue& other) const
{
    return equalInputImages(other)
        && compareCSSValuePtr(m_percentageValue, other.m_percentageValue);
}


bool CSSCrossfadeValue::equalInputImages(const CSSCrossfadeValue& other) const
{
    return compareCSSValuePtr(m_fromValue, other.m_fromValue)
        && compareCSSValuePtr(m_toValue, other.m_toValue);
}

} // namespace WebCore
