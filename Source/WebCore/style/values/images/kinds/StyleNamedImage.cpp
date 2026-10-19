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
#include "ImageBuffer.h"
#include "RenderElement.h"
#include "Theme.h"

#if ENABLE(AX_CUSTOM_COLOR_MODE)
#include <WebKitAdditions/AXCustomColorModeController.h>
#endif

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

Ref<CSSValue> NamedImage::computedStyleValue(const Style::ComputedStyle& style, PropertyValueType) const
{
    return CSSNamedImageValue::create(toCSS(m_name, style));
}

Ref<DeprecatedCSSOMValue> NamedImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style, PropertyValueType::Resolved)->createDeprecatedCSSOMWrapper(owner);
}

bool NamedImage::isPending() const
{
    return false;
}

void NamedImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

static WTF::String LEGACY_NODELETE adjustNamedImage(const RenderElement& renderer, const AtomString& name)
{
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    return AXCustomColorModeController::adjustNamedImage(renderer, name);
#else
    UNUSED_PARAM(renderer);
    return name;
#endif
}

ImageDrawResult NamedImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (size.isEmpty())
        return ImageDrawResult::DidNothing;

    return drawIntoDestination(context, destination, source, options, [&](GraphicsContext& context) {
        Theme::singleton().drawNamedImage(adjustNamedImage(renderer, m_name.value), context, destination.size());
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult NamedImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool) const
{
    auto size = concreteObjectSize.size() * concreteObjectSize.zoom();
    if (size.isEmpty() || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    RefPtr imageBuffer = context.createAlignedImageBuffer(size);
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    Theme::singleton().drawNamedImage(adjustNamedImage(renderer, m_name.value), imageBuffer->context(), size);
    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);

    return ImageDrawResult::DidDraw;
}

bool NamedImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

} // namespace Style
} // namespace WebCore
