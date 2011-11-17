/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "CSSImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CrossfadeGeneratedImage.h"
#include "ImageBuffer.h"
#include "RenderObject.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"

namespace WebCore {

static bool subimageIsPending(CSSValue* value)
{
    if (value->isImageValue())
        return static_cast<CSSImageValue*>(value)->cachedOrPendingImage()->isPendingImage();
    
    if (value->isImageGeneratorValue())
        return static_cast<CSSImageGeneratorValue*>(value)->isPending();
    
    ASSERT_NOT_REACHED();
    
    return false;
}

static void loadSubimage(CSSValue* value, CachedResourceLoader* cachedResourceLoader)
{
    if (value->isImageValue()) {
        static_cast<CSSImageValue*>(value)->cachedImage(cachedResourceLoader);
        return;
    }
    
    if (value->isImageGeneratorValue()) {
        static_cast<CSSImageGeneratorValue*>(value)->loadSubimages(cachedResourceLoader);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

static CachedImage* cachedImageForCSSValue(CSSValue* value, const RenderObject* renderer)
{
    CachedResourceLoader* cachedResourceLoader = renderer->document()->cachedResourceLoader();
    
    if (value->isImageValue())
        return static_cast<CSSImageValue*>(value)->cachedImage(cachedResourceLoader)->cachedImage();
    
    if (value->isImageGeneratorValue()) {
        // FIXME: Handle CSSImageGeneratorValue (and thus cross-fades with gradients and canvas).
        return 0;
    }
    
    ASSERT_NOT_REACHED();
    
    return 0;
}

String CSSCrossfadeValue::customCssText() const
{
    String result = "-webkit-cross-fade(";
    result += m_fromImage->cssText() + ", ";
    result += m_toImage->cssText() + ", ";
    result += m_percentage->cssText();
    result += ")";
    return result;
}

IntSize CSSCrossfadeValue::fixedSize(const RenderObject* renderer)
{
    float percentage = m_percentage->getFloatValue();
    float inversePercentage = 1 - percentage;

    CachedImage* fromImage = cachedImageForCSSValue(m_fromImage.get(), renderer);
    CachedImage* toImage = cachedImageForCSSValue(m_toImage.get(), renderer);

    if (!fromImage || !toImage)
        return IntSize();

    IntSize fromImageSize = fromImage->image()->size();
    IntSize toImageSize = toImage->image()->size();

    return IntSize(fromImageSize.width() * inversePercentage + toImageSize.width() * percentage,
        fromImageSize.height() * inversePercentage + toImageSize.height() * percentage);
}

bool CSSCrossfadeValue::isPending() const
{
    return subimageIsPending(m_fromImage.get()) || subimageIsPending(m_toImage.get());
}

void CSSCrossfadeValue::loadSubimages(CachedResourceLoader* cachedResourceLoader)
{
    loadSubimage(m_fromImage.get(), cachedResourceLoader);
    loadSubimage(m_toImage.get(), cachedResourceLoader);
}

PassRefPtr<Image> CSSCrossfadeValue::image(RenderObject* renderer, const IntSize& size)
{
    if (size.isEmpty())
        return 0;

    CachedImage* fromImage = cachedImageForCSSValue(m_fromImage.get(), renderer);
    CachedImage* toImage = cachedImageForCSSValue(m_toImage.get(), renderer);

    if (!fromImage || !toImage)
        return Image::nullImage();

    m_generatedImage = CrossfadeGeneratedImage::create(fromImage, toImage, m_percentage->getFloatValue(), &m_crossfadeObserver, fixedSize(renderer), size);

    return m_generatedImage.get();
}

void CSSCrossfadeValue::crossfadeChanged(const IntRect& rect)
{
    UNUSED_PARAM(rect);

    RenderObjectSizeCountMap::const_iterator end = clients().end();
    for (RenderObjectSizeCountMap::const_iterator curr = clients().begin(); curr != end; ++curr) {
        RenderObject* client = const_cast<RenderObject*>(curr->first);
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

} // namespace WebCore
