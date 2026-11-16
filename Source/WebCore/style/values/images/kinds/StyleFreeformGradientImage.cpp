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
#include "StyleFreeformGradientImage.h"

#include "CSSFreeformGradientValue.h"
#include "DeprecatedCSSOMValue.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

FreeformGradientImage::FreeformGradientImage(Gradient&& gradient)
    : GeneratedImage { Type::FreeformGradientImage }
    , m_gradient { WTF::move(gradient) }
    , m_containsCurrentColor { Style::containsCurrentColor(m_gradient.parameters) }
{
}

FreeformGradientImage::~FreeformGradientImage() = default;

bool FreeformGradientImage::operator==(const Image& other) const
{
    auto* otherFreeformGradientImage = dynamicDowncast<FreeformGradientImage>(other);
    return otherFreeformGradientImage && equals(*otherFreeformGradientImage);
}

bool FreeformGradientImage::equals(const FreeformGradientImage& other) const
{
    return m_gradient == other.m_gradient;
}

Ref<CSSValue> FreeformGradientImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSFreeformGradientValue::create(toCSS(m_gradient, style));
}

Ref<DeprecatedCSSOMValue> FreeformGradientImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool FreeformGradientImage::isPending() const
{
    return false;
}

void FreeformGradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

ImageDrawResult FreeformGradientImage::draw(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect&, const FloatRect&, bool) const
{
    // FIXME: Draw freeform-gradient().
    return ImageDrawResult::DidNothing;
}

ImageDrawResult FreeformGradientImage::drawAsPattern(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, bool) const
{
    // FIXME: Draw freeform-gradient().
    return ImageDrawResult::DidNothing;
}

bool FreeformGradientImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

bool FreeformGradientImage::containsCurrentColor() const
{
    return m_containsCurrentColor;
}

} // namespace Style
} // namespace WebCore
