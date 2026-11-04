/*
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
#include "StyleMeshGradientImage.h"

#include "CSSMeshGradientValue.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "MeshGradient.h"
#include "RenderElement.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

MeshGradientImage::MeshGradientImage(Gradient&& gradient)
    : GeneratedImage { Type::MeshGradientImage }
    , m_gradient { WTF::move(gradient) }
    , m_containsCurrentColor { Style::containsCurrentColor(m_gradient.parameters) }
{
}

MeshGradientImage::~MeshGradientImage() = default;

bool MeshGradientImage::operator==(const Image& other) const
{
    auto* otherMeshGradientImage = dynamicDowncast<MeshGradientImage>(other);
    return otherMeshGradientImage && equals(*otherMeshGradientImage);
}

bool MeshGradientImage::equals(const MeshGradientImage& other) const
{
    return m_gradient == other.m_gradient;
}

Ref<CSSValue> MeshGradientImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSMeshGradientValue::create(toCSS(m_gradient, style));
}

Ref<DeprecatedCSSOMValue> MeshGradientImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool MeshGradientImage::isPending() const
{
    return false;
}

void MeshGradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

Ref<WebCore::MeshGradient> MeshGradientImage::gradientForSize(const RenderElement& renderer, FloatSize size, bool isForFirstLine, CachedGradient*& cached) const
{
    cached = nullptr;

    CheckedRef style = isForFirstLine ? renderer.firstLineStyle() : renderer.style();

    // A gradient that uses currentColor can differ for each renderer, so it is not cached.
    if (m_containsCurrentColor)
        return createPlatformMeshGradient(m_gradient.parameters, size, style);

    cached = &m_gradients.ensure(size, style->appleColorFilter(), [&] {
        return createPlatformMeshGradient(m_gradient.parameters, size, style);
    });
    return cached->value();
}

ImageDrawResult MeshGradientImage::draw(GraphicsContext& context, const RenderElement& renderer, const ImageDrawDescriptor& descriptor, const FloatRect& destination, const FloatRect& source, bool isForFirstLine) const
{
    auto size = descriptor.concreteObjectSize.size() * descriptor.concreteObjectSize.zoom();
    if (size.isEmpty() || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);

    return drawIntoDestination(context, descriptor, destination, source, [&](GraphicsContext& context) {
        context.drawMeshGradient(gradient, FloatRect { { }, size });
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult MeshGradientImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, const ImageDrawDescriptor& descriptor, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, bool isForFirstLine) const
{
    auto size = descriptor.concreteObjectSize.size() * descriptor.concreteObjectSize.zoom();
    if (size.isEmpty() || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    CachedGradient* cached = nullptr;
    Ref gradient = gradientForSize(renderer, size, isForFirstLine, cached);

    auto scaleFactor = context.scaleFactor();

    RefPtr buffer = cached ? cached->patternBuffer(descriptor, size, scaleFactor) : nullptr;
    if (!buffer) {
        RefPtr newBuffer = createPatternBuffer(context, descriptor, size, [&](GraphicsContext& context) {
            context.drawMeshGradient(gradient, FloatRect { { }, size });
        });
        if (!newBuffer)
            return ImageDrawResult::DidNothing;

        buffer = newBuffer.copyRef();
        if (cached)
            cached->setPatternBuffer(newBuffer.releaseNonNull(), descriptor, size, scaleFactor);
    }

    return drawPatternBuffer(context, *buffer, descriptor, destination, tile, patternTransform, phase, spacing);
}

bool MeshGradientImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

bool MeshGradientImage::containsCurrentColor() const
{
    return m_containsCurrentColor;
}

} // namespace Style
} // namespace WebCore
