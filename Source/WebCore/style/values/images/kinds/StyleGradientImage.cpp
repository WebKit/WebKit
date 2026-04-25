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
#include "StyleGradientImage.h"

#include "CSSGradientValue.h"
#include "GeneratedImage.h"
#include "GradientImage.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "RenderStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

GradientImage::GradientImage(Gradient&& gradient)
    : GeneratedImage { Type::GradientImage, GradientImage::isFixedSize }
    , m_gradient { WTF::move(gradient) }
    , m_knownCacheableBarringFilter { stopsAreCacheable(m_gradient) }
{
}

GradientImage::~GradientImage() = default;

bool GradientImage::operator==(const Image& other) const
{
    auto* otherGradientImage = dynamicDowncast<GradientImage>(other);
    return otherGradientImage && equals(*otherGradientImage);
}

bool GradientImage::equals(const GradientImage& other) const
{
    return m_gradient == other.m_gradient;
}

Ref<CSSValue> GradientImage::computedStyleValue(const RenderStyle& style) const
{
    return CSSGradientValue::create(toCSS(m_gradient, style));
}

bool GradientImage::isPending() const
{
    return false;
}

void GradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<WebCore::Image> GradientImage::image(const RenderElement* renderer, const FloatSize& size, const GraphicsContext&, bool isForFirstLine) const
{
    if (!renderer)
        return &WebCore::Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    CheckedRef style = isForFirstLine ? renderer->firstLineStyle() : renderer->style();

    bool cacheable = m_knownCacheableBarringFilter && style->appleColorFilter().isNone();
    if (cacheable) {
        if (auto* result = const_cast<GradientImage&>(*this).cachedImageForSize(size))
            return result;
    }

    auto gradient = createPlatformGradient(m_gradient, size, style);

    auto newImage = WebCore::GradientImage::create(WTF::move(gradient), size);
    if (cacheable)
        const_cast<GradientImage&>(*this).saveCachedImageForSize(size, newImage);
    return newImage;
}

bool GradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return isOpaque(m_gradient, renderer.style());
}

FloatSize GradientImage::fixedSize(const RenderElement&) const
{
    return { };
}

} // namespace Style
} // namespace WebCore
