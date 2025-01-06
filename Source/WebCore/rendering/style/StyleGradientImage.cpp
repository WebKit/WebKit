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

#include "config.h"
#include "StyleGradientImage.h"

#include "CSSGradientValue.h"
#include "GeneratedImage.h"
#include "GradientImage.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {

StyleGradientImage::StyleGradientImage(Style::Gradient&& gradient)
    : StyleGeneratedImage { Type::GradientImage, StyleGradientImage::isFixedSize }
    , m_gradient { WTFMove(gradient) }
    , m_knownCacheableBarringFilter { Style::stopsAreCacheable(m_gradient) }
{
}

StyleGradientImage::~StyleGradientImage() = default;

bool StyleGradientImage::operator==(const StyleImage& other) const
{
    auto* otherGradientImage = dynamicDowncast<StyleGradientImage>(other);
    return otherGradientImage && equals(*otherGradientImage);
}

bool StyleGradientImage::equals(const StyleGradientImage& other) const
{
    return m_gradient == other.m_gradient;
}

Ref<CSSValue> StyleGradientImage::computedStyleValue(const RenderStyle& style) const
{
    return CSSGradientValue::create(Style::toCSS(m_gradient, style));
}

bool StyleGradientImage::isPending() const
{
    return false;
}

void StyleGradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<Image> StyleGradientImage::image(const RenderElement* renderer, const FloatSize& size, bool isForFirstLine) const
{
    if (!renderer)
        return &Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    auto& style = isForFirstLine ? renderer->firstLineStyle() : renderer->style();

    bool cacheable = m_knownCacheableBarringFilter && !style.hasAppleColorFilter();
    if (cacheable) {
        if (!clients().contains(const_cast<RenderElement&>(*renderer)))
            return nullptr;
        if (auto* result = const_cast<StyleGradientImage&>(*this).cachedImageForSize(size))
            return result;
    }

    auto gradient = Style::createPlatformGradient(m_gradient, size, style);

    auto newImage = GradientImage::create(WTFMove(gradient), size);
    if (cacheable)
        const_cast<StyleGradientImage&>(*this).saveCachedImageForSize(size, newImage);
    return newImage;
}

bool StyleGradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return Style::isOpaque(m_gradient, renderer.style());
}

FloatSize StyleGradientImage::fixedSize(const RenderElement&) const
{
    return { };
}

} // namespace WebCore
