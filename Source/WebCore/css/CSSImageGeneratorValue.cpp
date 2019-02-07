/*
 * Copyright (C) 2008, 2011, 2012, 2013 Apple Inc.  All rights reserved.
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
#include "CSSImageGeneratorValue.h"

#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSFilterImageValue.h"
#include "CSSGradientValue.h"
#include "CSSImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "GeneratedImage.h"
#include "HTMLCanvasElement.h"
#include "InspectorInstrumentation.h"
#include "RenderElement.h"

namespace WebCore {

static const Seconds timeToKeepCachedGeneratedImages { 3_s };

class CSSImageGeneratorValue::CachedGeneratedImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CachedGeneratedImage(CSSImageGeneratorValue&, FloatSize, GeneratedImage&);
    GeneratedImage& image() const { return m_image; }
    void puntEvictionTimer() { m_evictionTimer.restart(); }

private:
    void evictionTimerFired();

    CSSImageGeneratorValue& m_owner;
    const FloatSize m_size;
    const Ref<GeneratedImage> m_image;
    DeferrableOneShotTimer m_evictionTimer;
};

CSSImageGeneratorValue::CSSImageGeneratorValue(ClassType classType)
    : CSSValue(classType)
{
}

CSSImageGeneratorValue::~CSSImageGeneratorValue() = default;

void CSSImageGeneratorValue::addClient(RenderElement& renderer)
{
    if (m_clients.isEmpty())
        ref();

    m_clients.add(&renderer);

    if (is<CSSCanvasValue>(this)) {
        if (HTMLCanvasElement* canvasElement = downcast<CSSCanvasValue>(this)->element())
            InspectorInstrumentation::didChangeCSSCanvasClientNodes(*canvasElement);
    }
}

void CSSImageGeneratorValue::removeClient(RenderElement& renderer)
{
    ASSERT(m_clients.contains(&renderer));
    if (!m_clients.remove(&renderer))
        return;

    if (is<CSSCanvasValue>(this)) {
        if (HTMLCanvasElement* canvasElement = downcast<CSSCanvasValue>(this)->element())
            InspectorInstrumentation::didChangeCSSCanvasClientNodes(*canvasElement);
    }

    if (m_clients.isEmpty())
        deref();
}

GeneratedImage* CSSImageGeneratorValue::cachedImageForSize(FloatSize size)
{
    if (size.isEmpty())
        return nullptr;

    auto* cachedGeneratedImage = m_images.get(size);
    if (!cachedGeneratedImage)
        return nullptr;

    cachedGeneratedImage->puntEvictionTimer();
    return &cachedGeneratedImage->image();
}

void CSSImageGeneratorValue::saveCachedImageForSize(FloatSize size, GeneratedImage& image)
{
    ASSERT(!m_images.contains(size));
    m_images.add(size, std::make_unique<CachedGeneratedImage>(*this, size, image));
}

void CSSImageGeneratorValue::evictCachedGeneratedImage(FloatSize size)
{
    ASSERT(m_images.contains(size));
    m_images.remove(size);
}

inline CSSImageGeneratorValue::CachedGeneratedImage::CachedGeneratedImage(CSSImageGeneratorValue& owner, FloatSize size, GeneratedImage& image)
    : m_owner(owner)
    , m_size(size)
    , m_image(image)
    , m_evictionTimer(*this, &CSSImageGeneratorValue::CachedGeneratedImage::evictionTimerFired, timeToKeepCachedGeneratedImages)
{
    m_evictionTimer.restart();
}

void CSSImageGeneratorValue::CachedGeneratedImage::evictionTimerFired()
{
    // NOTE: This is essentially a "delete this", the object is no longer valid after this line.
    m_owner.evictCachedGeneratedImage(m_size);
}

RefPtr<Image> CSSImageGeneratorValue::image(RenderElement& renderer, const FloatSize& size)
{
    switch (classType()) {
    case CanvasClass:
        return downcast<CSSCanvasValue>(*this).image(&renderer, size);
    case NamedImageClass:
        return downcast<CSSNamedImageValue>(*this).image(&renderer, size);
    case CrossfadeClass:
        return downcast<CSSCrossfadeValue>(*this).image(renderer, size);
    case FilterImageClass:
        return downcast<CSSFilterImageValue>(*this).image(&renderer, size);
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).image(renderer, size);
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).image(renderer, size);
    case ConicGradientClass:
        return downcast<CSSConicGradientValue>(*this).image(renderer, size);
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return downcast<CSSPaintImageValue>(*this).image(renderer, size);
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

bool CSSImageGeneratorValue::isFixedSize() const
{
    switch (classType()) {
    case CanvasClass:
        return downcast<CSSCanvasValue>(*this).isFixedSize();
    case NamedImageClass:
        return downcast<CSSNamedImageValue>(*this).isFixedSize();
    case CrossfadeClass:
        return downcast<CSSCrossfadeValue>(*this).isFixedSize();
    case FilterImageClass:
        return downcast<CSSFilterImageValue>(*this).isFixedSize();
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).isFixedSize();
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).isFixedSize();
    case ConicGradientClass:
        return downcast<CSSConicGradientValue>(*this).isFixedSize();
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return downcast<CSSPaintImageValue>(*this).isFixedSize();
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

FloatSize CSSImageGeneratorValue::fixedSize(const RenderElement& renderer)
{
    switch (classType()) {
    case CanvasClass:
        return downcast<CSSCanvasValue>(*this).fixedSize(&renderer);
    case CrossfadeClass:
        return downcast<CSSCrossfadeValue>(*this).fixedSize(renderer);
    case FilterImageClass:
        return downcast<CSSFilterImageValue>(*this).fixedSize(&renderer);
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).fixedSize(renderer);
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).fixedSize(renderer);
    case ConicGradientClass:
        return downcast<CSSConicGradientValue>(*this).fixedSize(renderer);
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return downcast<CSSPaintImageValue>(*this).fixedSize(renderer);
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    return FloatSize();
}

bool CSSImageGeneratorValue::isPending() const
{
    switch (classType()) {
    case CrossfadeClass:
        return downcast<CSSCrossfadeValue>(*this).isPending();
    case CanvasClass:
        return downcast<CSSCanvasValue>(*this).isPending();
    case NamedImageClass:
        return downcast<CSSNamedImageValue>(*this).isPending();
    case FilterImageClass:
        return downcast<CSSFilterImageValue>(*this).isPending();
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).isPending();
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).isPending();
    case ConicGradientClass:
        return downcast<CSSConicGradientValue>(*this).isPending();
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return downcast<CSSPaintImageValue>(*this).isPending();
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

bool CSSImageGeneratorValue::knownToBeOpaque(const RenderElement& renderer) const
{
    switch (classType()) {
    case CrossfadeClass:
        return downcast<CSSCrossfadeValue>(*this).knownToBeOpaque(renderer);
    case CanvasClass:
        return false;
    case NamedImageClass:
        return false;
    case FilterImageClass:
        return downcast<CSSFilterImageValue>(*this).knownToBeOpaque(renderer);
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).knownToBeOpaque(renderer);
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).knownToBeOpaque(renderer);
    case ConicGradientClass:
        return downcast<CSSConicGradientValue>(*this).knownToBeOpaque(renderer);
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return downcast<CSSPaintImageValue>(*this).knownToBeOpaque(renderer);
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void CSSImageGeneratorValue::loadSubimages(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    switch (classType()) {
    case CrossfadeClass:
        downcast<CSSCrossfadeValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
    case CanvasClass:
        downcast<CSSCanvasValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
    case FilterImageClass:
        downcast<CSSFilterImageValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
    case LinearGradientClass:
        downcast<CSSLinearGradientValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
    case RadialGradientClass:
        downcast<CSSRadialGradientValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
    case ConicGradientClass:
        downcast<CSSConicGradientValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        downcast<CSSPaintImageValue>(*this).loadSubimages(cachedResourceLoader, options);
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
    }
}

bool CSSImageGeneratorValue::subimageIsPending(const CSSValue& value)
{
    if (is<CSSImageValue>(value))
        return downcast<CSSImageValue>(value).isPending();
    
    if (is<CSSImageGeneratorValue>(value))
        return downcast<CSSImageGeneratorValue>(value).isPending();

    if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

CachedImage* CSSImageGeneratorValue::cachedImageForCSSValue(CSSValue& value, CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (is<CSSImageValue>(value)) {
        auto& imageValue = downcast<CSSImageValue>(value);
        return imageValue.loadImage(cachedResourceLoader, options);
    }
    
    if (is<CSSImageGeneratorValue>(value)) {
        downcast<CSSImageGeneratorValue>(value).loadSubimages(cachedResourceLoader, options);
        // FIXME: Handle CSSImageGeneratorValue (and thus cross-fades with gradients and canvas).
        return nullptr;
    }

    if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone)
        return nullptr;

    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore
