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
#include "GeneratedImage.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"

namespace WebCore {

static const double timeToKeepCachedGeneratedImagesInSeconds = 3;

CSSImageGeneratorValue::CSSImageGeneratorValue(ClassType classType)
    : CSSValue(classType)
{
}

CSSImageGeneratorValue::~CSSImageGeneratorValue()
{
}

void CSSImageGeneratorValue::addClient(RenderElement* renderer)
{
    ASSERT(renderer);
    if (m_clients.isEmpty())
        ref();
    m_clients.add(renderer);
}

void CSSImageGeneratorValue::removeClient(RenderElement* renderer)
{
    ASSERT(renderer);
    ASSERT(m_clients.contains(renderer));
    if (m_clients.remove(renderer) && m_clients.isEmpty())
        deref();
}

GeneratedImage* CSSImageGeneratorValue::cachedImageForSize(IntSize size)
{
    if (size.isEmpty())
        return nullptr;

    CachedGeneratedImage* cachedGeneratedImage = m_images.get(size);
    if (!cachedGeneratedImage)
        return nullptr;

    cachedGeneratedImage->puntEvictionTimer();
    return cachedGeneratedImage->image();
}

void CSSImageGeneratorValue::saveCachedImageForSize(IntSize size, PassRefPtr<GeneratedImage> image)
{
    ASSERT(!m_images.contains(size));
    m_images.add(size, std::make_unique<CachedGeneratedImage>(*this, size, image));
}

void CSSImageGeneratorValue::evictCachedGeneratedImage(IntSize size)
{
    ASSERT(m_images.contains(size));
    m_images.remove(size);
}

CSSImageGeneratorValue::CachedGeneratedImage::CachedGeneratedImage(CSSImageGeneratorValue& owner, IntSize size, PassRefPtr<GeneratedImage> image)
    : m_owner(owner)
    , m_size(size)
    , m_image(image)
    , m_evictionTimer(this, &CSSImageGeneratorValue::CachedGeneratedImage::evictionTimerFired, timeToKeepCachedGeneratedImagesInSeconds)
{
    m_evictionTimer.restart();
}

void CSSImageGeneratorValue::CachedGeneratedImage::evictionTimerFired(DeferrableOneShotTimer<CachedGeneratedImage>&)
{
    // NOTE: This is essentially a "delete this", the object is no longer valid after this line.
    m_owner.evictCachedGeneratedImage(m_size);
}

PassRefPtr<Image> CSSImageGeneratorValue::image(RenderElement* renderer, const IntSize& size)
{
    switch (classType()) {
    case CanvasClass:
        return toCSSCanvasValue(this)->image(renderer, size);
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->image(renderer, size);
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        return toCSSFilterImageValue(this)->image(renderer, size);
#endif
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->image(renderer, size);
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->image(renderer, size);
    default:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

bool CSSImageGeneratorValue::isFixedSize() const
{
    switch (classType()) {
    case CanvasClass:
        return toCSSCanvasValue(this)->isFixedSize();
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->isFixedSize();
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        return toCSSFilterImageValue(this)->isFixedSize();
#endif
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->isFixedSize();
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->isFixedSize();
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

IntSize CSSImageGeneratorValue::fixedSize(const RenderElement* renderer)
{
    switch (classType()) {
    case CanvasClass:
        return toCSSCanvasValue(this)->fixedSize(renderer);
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->fixedSize(renderer);
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        return toCSSFilterImageValue(this)->fixedSize(renderer);
#endif
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->fixedSize(renderer);
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->fixedSize(renderer);
    default:
        ASSERT_NOT_REACHED();
    }
    return IntSize();
}

bool CSSImageGeneratorValue::isPending() const
{
    switch (classType()) {
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->isPending();
    case CanvasClass:
        return toCSSCanvasValue(this)->isPending();
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        return toCSSFilterImageValue(this)->isPending();
#endif
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->isPending();
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->isPending();
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

bool CSSImageGeneratorValue::knownToBeOpaque(const RenderElement* renderer) const
{
    switch (classType()) {
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->knownToBeOpaque(renderer);
    case CanvasClass:
        return false;
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        return toCSSFilterImageValue(this)->knownToBeOpaque(renderer);
#endif
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->knownToBeOpaque(renderer);
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->knownToBeOpaque(renderer);
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void CSSImageGeneratorValue::loadSubimages(CachedResourceLoader* cachedResourceLoader)
{
    switch (classType()) {
    case CrossfadeClass:
        toCSSCrossfadeValue(this)->loadSubimages(cachedResourceLoader);
        break;
    case CanvasClass:
        toCSSCanvasValue(this)->loadSubimages(cachedResourceLoader);
        break;
#if ENABLE(CSS_FILTERS)
    case FilterImageClass:
        toCSSFilterImageValue(this)->loadSubimages(cachedResourceLoader);
        break;
#endif
    case LinearGradientClass:
        toCSSLinearGradientValue(this)->loadSubimages(cachedResourceLoader);
        break;
    case RadialGradientClass:
        toCSSRadialGradientValue(this)->loadSubimages(cachedResourceLoader);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool CSSImageGeneratorValue::subimageIsPending(CSSValue* value)
{
    if (value->isImageValue())
        return toCSSImageValue(value)->cachedOrPendingImage()->isPendingImage();
    
    if (value->isImageGeneratorValue())
        return toCSSImageGeneratorValue(value)->isPending();

    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueNone)
        return false;

    ASSERT_NOT_REACHED();
    
    return false;
}

CachedImage* CSSImageGeneratorValue::cachedImageForCSSValue(CSSValue* value, CachedResourceLoader* cachedResourceLoader)
{
    if (!value)
        return nullptr;

    if (value->isImageValue()) {
        StyleCachedImage* styleCachedImage = toCSSImageValue(value)->cachedImage(cachedResourceLoader);
        if (!styleCachedImage)
            return nullptr;

        return styleCachedImage->cachedImage();
    }
    
    if (value->isImageGeneratorValue()) {
        toCSSImageGeneratorValue(value)->loadSubimages(cachedResourceLoader);
        // FIXME: Handle CSSImageGeneratorValue (and thus cross-fades with gradients and canvas).
        return nullptr;
    }

    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueNone)
        return nullptr;

    ASSERT_NOT_REACHED();
    
    return nullptr;
}
} // namespace WebCore
