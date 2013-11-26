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

#include "config.h"
#include "CSSFilterImageValue.h"

#if ENABLE(CSS_FILTERS)

#include "CSSImageValue.h"
#include "CachedResourceLoader.h"
#include "CrossfadeGeneratedImage.h"
#include "FilterEffectRenderer.h"
#include "ImageBuffer.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include "StyleGeneratedImage.h"
#include "StyleResolver.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFilterImageValue::~CSSFilterImageValue()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(&m_filterSubimageObserver);
}

String CSSFilterImageValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("-webkit-filter(");
    result.append(m_imageValue->cssText());
    result.appendLiteral(", ");
    result.append(m_filterValue->cssText());
    result.append(')');
    return result.toString();
}

IntSize CSSFilterImageValue::fixedSize(const RenderElement* renderer)
{
    CachedResourceLoader* cachedResourceLoader = renderer->document().cachedResourceLoader();
    CachedImage* cachedImage = cachedImageForCSSValue(m_imageValue.get(), cachedResourceLoader);

    if (!cachedImage)
        return IntSize();

    return cachedImage->imageForRenderer(renderer)->size();
}

bool CSSFilterImageValue::isPending() const
{
    return CSSImageGeneratorValue::subimageIsPending(m_imageValue.get());
}

bool CSSFilterImageValue::knownToBeOpaque(const RenderElement*) const
{
    return false;
}

void CSSFilterImageValue::loadSubimages(CachedResourceLoader* cachedResourceLoader)
{
    CachedResourceHandle<CachedImage> oldCachedImage = m_cachedImage;

    m_cachedImage = CSSImageGeneratorValue::cachedImageForCSSValue(m_imageValue.get(), cachedResourceLoader);

    if (m_cachedImage != oldCachedImage) {
        if (oldCachedImage)
            oldCachedImage->removeClient(&m_filterSubimageObserver);
        if (m_cachedImage)
            m_cachedImage->addClient(&m_filterSubimageObserver);
    }

    m_filterSubimageObserver.setReady(true);
}

PassRefPtr<Image> CSSFilterImageValue::image(RenderElement* renderer, const IntSize& size)
{
    if (size.isEmpty())
        return 0;

    CachedResourceLoader* cachedResourceLoader = renderer->document().cachedResourceLoader();
    CachedImage* cachedImage = cachedImageForCSSValue(m_imageValue.get(), cachedResourceLoader);

    if (!cachedImage)
        return Image::nullImage();

    Image* image = cachedImage->imageForRenderer(renderer);

    if (!image)
        return Image::nullImage();

    // Transform Image into ImageBuffer.
    std::unique_ptr<ImageBuffer> texture = ImageBuffer::create(size);
    if (!texture)
        return Image::nullImage();
    texture->context()->drawImage(image, ColorSpaceDeviceRGB, IntPoint());

    RefPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
    filterRenderer->setSourceImage(std::move(texture));
    filterRenderer->setSourceImageRect(FloatRect(FloatPoint(), size));
    filterRenderer->setFilterRegion(FloatRect(FloatPoint(), size));
    if (!filterRenderer->build(renderer, m_filterOperations, FilterFunction))
        return Image::nullImage();
    filterRenderer->apply();

    m_generatedImage = filterRenderer->output()->copyImage();

    return m_generatedImage.release();
}

void CSSFilterImageValue::filterImageChanged(const IntRect&)
{
    for (auto it = clients().begin(), end = clients().end(); it != end; ++it)
        it->key->imageChanged(static_cast<WrappedImagePtr>(this));
}

void CSSFilterImageValue::createFilterOperations(StyleResolver* resolver)
{
    m_filterOperations.clear();
    resolver->createFilterOperations(m_filterValue.get(), m_filterOperations);
}

void CSSFilterImageValue::FilterSubimageObserverProxy::imageChanged(CachedImage*, const IntRect* rect)
{
    if (m_ready)
        m_ownerValue->filterImageChanged(*rect);
}

bool CSSFilterImageValue::hasFailedOrCanceledSubresources() const
{
    if (m_cachedImage && m_cachedImage->loadFailedOrCanceled())
        return true;
    return false;
}

bool CSSFilterImageValue::equals(const CSSFilterImageValue& other) const
{
    return equalInputImages(other) && compareCSSValuePtr(m_filterValue, other.m_filterValue);
}

bool CSSFilterImageValue::equalInputImages(const CSSFilterImageValue& other) const
{
    return compareCSSValuePtr(m_imageValue, other.m_imageValue);
}

} // namespace WebCore

#endif // ENABLE(CSS_FILTERS)
