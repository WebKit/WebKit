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
#include "StyleNamedImage.h"

#include "CSSNamedImageValue.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "NinePieceGeometry.h"
#include "Theme.h"

namespace WebCore {
namespace Style {

NamedImage::NamedImage(CustomIdent&& name)
    : GeneratedImage { Type::NamedImage }
    , m_name { WTF::move(name) }
{
}

NamedImage::~NamedImage() = default;

bool NamedImage::operator==(const Image& other) const
{
    auto* otherNamedImage = dynamicDowncast<NamedImage>(other);
    return otherNamedImage && equals(*otherNamedImage);
}

bool NamedImage::equals(const NamedImage& other) const
{
    return m_name == other.m_name;
}

Ref<CSSValue> NamedImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSNamedImageValue::create(toCSS(m_name, style));
}

Ref<DeprecatedCSSOMValue> NamedImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool NamedImage::isPending() const
{
    return false;
}

void NamedImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

static ImageDrawResult drawNamedImage(GraphicsContext& context, const WTF::String& name, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options)
{
    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    context.clip(destination);
    context.translate(destination.location());
    if (destination.size() != source.size())
        context.scale(FloatSize(destination.width() / source.width(), destination.height() / source.height()));
    context.translate(-source.location());

    Theme::singleton().drawNamedImage(name, context, destination.size());
    return ImageDrawResult::DidDraw;
}

ImageDrawResult NamedImage::draw(GraphicsContext& context, const RenderElement&, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool) const
{
    if (concreteObjectSize.size().isEmpty())
        return ImageDrawResult::DidNothing;

    return drawNamedImage(context, m_name.value, destination, source, options);
}

ImageDrawResult NamedImage::drawAsPattern(GraphicsContext& context, const RenderElement&, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool) const
{
    auto imageBuffer = context.createAlignedImageBuffer(concreteObjectSize.size());
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    Theme::singleton().drawNamedImage(m_name.value, imageBuffer->context(), concreteObjectSize.size());
    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);
    return ImageDrawResult::DidDraw;
}

bool NamedImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

} // namespace Style
} // namespace WebCore
