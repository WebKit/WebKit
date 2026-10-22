/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StyleColorImage.h"

#include "CSSColorImageValue.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderElement.h"
#include "StyleColorResolver.h"

namespace WebCore {
namespace Style {

ColorImage::ColorImage(Color&& color)
    : GeneratedImage { Type::ColorImage }
    , m_color { WTF::move(color) }
{
}

ColorImage::~ColorImage() = default;

bool ColorImage::operator==(const Image& other) const
{
    auto* otherColorImage = dynamicDowncast<ColorImage>(other);
    return otherColorImage && equals(*otherColorImage);
}

bool ColorImage::equals(const ColorImage& other) const
{
    return m_color == other.m_color;
}

Ref<CSSValue> ColorImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSColorImageValue::create(toCSS(m_color, style));
}

Ref<DeprecatedCSSOMValue> ColorImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool ColorImage::isPending() const
{
    return false;
}

void ColorImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

ImageDrawResult ColorImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect&, ImageDrawingOptions options, bool) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (size.isEmpty())
        return ImageDrawResult::DidNothing;

    WebCore::Image::fillWithSolidColor(context, destination, resolvedColor(renderer), options.compositeOperator());
    return ImageDrawResult::DidDraw;
}

ImageDrawResult ColorImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, bool) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (size.isEmpty() || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    auto color = resolvedColor(renderer);

    if (spacing.isZero()) {
        WebCore::Image::fillWithSolidColor(context, destination, color, options.compositeOperator());
        return ImageDrawResult::DidDraw;
    }

    // FIXME: Add support to GraphicsContext for drawing patterns without requiring an ImageBuffer or NativeImage, using a callback or recorded DisplayList instead.
    RefPtr imageBuffer = context.createAlignedImageBuffer(size);
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    imageBuffer->context().fillRect(FloatRect { { }, size }, color);
    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);

    return ImageDrawResult::DidDraw;
}

bool ColorImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return resolvedColor(renderer).isOpaque();
}

bool ColorImage::containsCurrentColor() const
{
    return m_color.containsCurrentColor();
}

WebCore::Color ColorImage::resolvedColor(const RenderElement& renderer) const
{
    return ColorResolver { renderer.style() }.colorResolvingCurrentColorApplyingColorFilter(m_color);
}

} // namespace Style
} // namespace WebCore
