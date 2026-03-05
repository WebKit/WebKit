/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleCrossfadeImage.h"

#include "AnimationUtilities.h"
#include "CSSCrossfadeValue.h"
#include "CSSValuePool.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CrossfadeGeneratedImage.h"
#include "RenderElement.h"
#include "SVGImageForContainer.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

CrossfadeImage::CrossfadeImage(RefPtr<Image>&& from, RefPtr<Image>&& to, double percentage, bool isPrefixed)
    : GeneratedImage { Type::CrossfadeImage, CrossfadeImage::isFixedSize }
    , m_from { WTF::move(from) }
    , m_to { WTF::move(to) }
    , m_percentage { percentage }
    , m_isPrefixed { isPrefixed }
    , m_inputImagesAreReady { false }
{
}

CrossfadeImage::~CrossfadeImage()
{
    if (m_cachedFromImage)
        m_cachedFromImage->removeClient(*this);
    if (m_cachedToImage)
        m_cachedToImage->removeClient(*this);
}

bool CrossfadeImage::operator==(const Image& other) const
{
    auto* otherCrossfadeImage = dynamicDowncast<CrossfadeImage>(other);
    return otherCrossfadeImage && equals(*otherCrossfadeImage);
}

bool CrossfadeImage::equals(const CrossfadeImage& other) const
{
    return equalInputImages(other) && m_percentage == other.m_percentage;
}

bool CrossfadeImage::equalInputImages(const CrossfadeImage& other) const
{
    return arePointingToEqualData(m_from, other.m_from) && arePointingToEqualData(m_to, other.m_to);
}

RefPtr<CrossfadeImage> CrossfadeImage::blend(const CrossfadeImage& from, const BlendingContext& context) const
{
    ASSERT(equalInputImages(from));

    if (!m_cachedToImage || !m_cachedFromImage)
        return nullptr;

    auto newPercentage = WebCore::blend(from.m_percentage, m_percentage, context);
    return CrossfadeImage::create(m_from, m_to, newPercentage, from.m_isPrefixed && m_isPrefixed);
}

Ref<CSSValue> CrossfadeImage::computedStyleValue(const RenderStyle& style) const
{
    auto fromComputedValue = m_from ? m_from->computedStyleValue(style) : upcast<CSSValue>(CSSPrimitiveValue::create(CSSValueNone));
    auto toComputedValue = m_to ? m_to->computedStyleValue(style) : upcast<CSSValue>(CSSPrimitiveValue::create(CSSValueNone));
    return CSSCrossfadeValue::create(WTF::move(fromComputedValue), WTF::move(toComputedValue), CSSPrimitiveValue::create(m_percentage), m_isPrefixed);
}

bool CrossfadeImage::isPending() const
{
    if (m_from && m_from->isPending())
        return true;
    if (m_to && m_to->isPending())
        return true;
    return false;
}

void CrossfadeImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    auto oldCachedFromImage = m_cachedFromImage;
    auto oldCachedToImage = m_cachedToImage;

    if (m_from) {
        if (m_from->isPending())
            m_from->load(loader, options);
        m_cachedFromImage = m_from->cachedImage();
    } else
        m_cachedFromImage = nullptr;

    if (m_to) {
        if (m_to->isPending())
            m_to->load(loader, options);
        m_cachedToImage = m_to->cachedImage();
    } else
        m_cachedToImage = nullptr;

    if (m_cachedFromImage != oldCachedFromImage) {
        if (oldCachedFromImage)
            oldCachedFromImage->removeClient(*this);
        if (m_cachedFromImage)
            m_cachedFromImage->addClient(*this);
    }

    if (m_cachedToImage != oldCachedToImage) {
        if (oldCachedToImage)
            oldCachedToImage->removeClient(*this);
        if (m_cachedToImage)
            m_cachedToImage->addClient(*this);
    }

    m_inputImagesAreReady = true;
}

RefPtr<WebCore::Image> CrossfadeImage::image(const RenderElement* renderer, const FloatSize& size, const GraphicsContext& destinationContext, bool isForFirstLine) const
{
    if (!renderer)
        return &WebCore::Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    if (!m_from || !m_to)
        return &WebCore::Image::nullImage();

    auto fromImage = m_from->image(renderer, size, destinationContext, isForFirstLine);
    auto toImage = m_to->image(renderer, size, destinationContext, isForFirstLine);

    if (!fromImage || !toImage)
        return &WebCore::Image::nullImage();

    RefPtr protectedFromImage = fromImage;
    RefPtr protectedToImage = toImage;

    if (RefPtr fromSVGImage = dynamicDowncast<SVGImage>(protectedFromImage)) {
        auto fromURL = m_cachedFromImage ? m_cachedFromImage->url() : WTF::URL();
        protectedFromImage = SVGImageForContainer::create(fromSVGImage.get(), size, 1, fromURL);
    }
    if (RefPtr toSVGImage = dynamicDowncast<SVGImage>(protectedToImage)) {
        auto toURL = m_cachedToImage ? m_cachedToImage->url() : WTF::URL();
        protectedToImage = SVGImageForContainer::create(toSVGImage.get(), size, 1, toURL);
    }

    return CrossfadeGeneratedImage::create(*protectedFromImage, *protectedToImage, m_percentage, fixedSize(*renderer), size);
}

bool CrossfadeImage::currentFrameIsComplete(const RenderElement* renderer) const
{
    if (m_from && !m_from->currentFrameIsComplete(renderer))
        return false;
    if (m_to && !m_to->currentFrameIsComplete(renderer))
        return false;
    return true;
}

bool CrossfadeImage::knownToBeOpaque(const RenderElement& renderer) const
{
    if (m_from && !m_from->knownToBeOpaque(renderer))
        return false;
    if (m_to && !m_to->knownToBeOpaque(renderer))
        return false;
    return true;
}

FloatSize CrossfadeImage::fixedSize(const RenderElement& renderer) const
{
    if (!m_from || !m_to)
        return { };

    auto fromImageSize = m_from->imageSize(&renderer, 1);
    auto toImageSize = m_to->imageSize(&renderer, 1);

    // Rounding issues can cause transitions between images of equal size to return
    // a different fixed size; avoid performing the interpolation if the images are the same size.
    if (fromImageSize == toImageSize)
        return fromImageSize;

    float percentage = m_percentage;
    float inversePercentage = 1 - percentage;

    return fromImageSize * inversePercentage + toImageSize * percentage;
}

void CrossfadeImage::imageChanged(WebCore::CachedImage*, const IntRect*)
{
    if (!m_inputImagesAreReady)
        return;
    for (auto entry : clients()) {
        CheckedRef client = entry.key;
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

} // namespace Style
} // namespace WebCore
