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
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "StyleAppleColorFilter.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

auto GradientImage::cachedGradientsForTesting() const -> Vector<CachedGradientForTesting>
{
    Vector<CachedGradientForTesting> result;
    for (auto& [size, entries] : m_gradients.entries()) {
        for (auto& entry : entries) {
            auto& patternBuffer = entry->patternBufferForTesting();
            result.append({
                size.width(),
                size.height(),
                !entry->colorFilter().isNone(),
                patternBuffer ? patternBuffer->renderingResourceIdentifier().toUInt64() : 0,
            });
        }
    }
    std::ranges::stable_sort(result, [](auto& a, auto& b) {
        return std::pair { a.width, a.height } < std::pair { b.width, b.height };
    });
    return result;
}

GradientImage::GradientImage(Gradient&& gradient)
    : GeneratedImage { Type::GradientImage }
    , m_gradient { WTF::move(gradient) }
    , m_stopsAreCacheable { stopsAreCacheable(m_gradient) }
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

Ref<CSSValue> GradientImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSGradientValue::create(toCSS(m_gradient, style));
}

Ref<DeprecatedCSSOMValue> GradientImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool GradientImage::isPending() const
{
    return false;
}

void GradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

Ref<WebCore::Gradient> GradientImage::gradientForSize(const RenderElement& renderer, FloatSize size, bool isForFirstLine, CachedGradient*& cached) const
{
    cached = nullptr;

    CheckedRef style = isForFirstLine ? renderer.firstLineStyle() : renderer.style();

    if (!m_stopsAreCacheable)
        return createPlatformGradient(m_gradient, size, style);

    cached = &m_gradients.ensure(size, style->appleColorFilter(), [&] {
        return createPlatformGradient(m_gradient, size, style);
    });
    return cached->value();
}

ImageDrawResult GradientImage::draw(GraphicsContext& context, const RenderElement& renderer, const ImageDrawDescriptor& descriptor, const FloatRect& destination, const FloatRect& source, bool isForFirstLine) const
{
    auto size = descriptor.concreteObjectSize.size() * descriptor.concreteObjectSize.zoom();
    if (size.isEmpty())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);

    return drawIntoDestination(context, descriptor, destination, source, [&](GraphicsContext& context) {
        context.fillRect(FloatRect { { }, size }, gradient);
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult GradientImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, const ImageDrawDescriptor& descriptor, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, bool isForFirstLine) const
{
    auto size = descriptor.concreteObjectSize.size() * descriptor.concreteObjectSize.zoom();
    if (size.isEmpty() || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);

    // Allow the generator to provide visually-equivalent tiling parameters for better performance.
    auto adjustedSize = size;
    auto adjustedTile = tile;
    gradient->adjustParametersForTiledDrawing(adjustedSize, adjustedTile, spacing);

    auto scaleFactor = context.scaleFactor();

    RefPtr buffer = cached ? cached->patternBuffer(descriptor, adjustedSize, scaleFactor) : nullptr;
    if (!buffer) {
        RefPtr newBuffer = createPatternBuffer(context, descriptor, adjustedSize, [&](GraphicsContext& context) {
            context.fillRect(FloatRect { { }, adjustedSize }, gradient);
        });
        if (!newBuffer)
            return ImageDrawResult::DidDraw;

        buffer = newBuffer.copyRef();
        if (cached)
            cached->setPatternBuffer(newBuffer.releaseNonNull(), descriptor, adjustedSize, scaleFactor);
    }

    return drawPatternBuffer(context, *buffer, descriptor, destination, adjustedTile, patternTransform, phase, spacing);
}

bool GradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return isOpaque(m_gradient, renderer.style());
}

bool GradientImage::containsCurrentColor() const
{
    return Style::containsCurrentColor(m_gradient);
}

} // namespace Style
} // namespace WebCore
