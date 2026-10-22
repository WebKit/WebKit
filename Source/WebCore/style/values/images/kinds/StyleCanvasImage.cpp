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
#include "StyleCanvasImage.h"

#include "BitmapImage.h"
#include "CSSCanvasValue.h"
#include "DeprecatedCSSOMValue.h"
#include "HTMLCanvasElement.h"
#include "ImageQualityController.h"
#include "InspectorInstrumentation.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore::Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CanvasImage);

CanvasImage::CanvasImage(CustomIdent&& name)
    : GeneratedImage { Type::CanvasImage }
    , m_name { WTF::move(name) }
{
}

CanvasImage::~CanvasImage()
{
    if (m_element)
        protect(m_element.get())->removeObserver(*this);
}

bool CanvasImage::operator==(const Image& other) const
{
    auto* otherCanvasImage = dynamicDowncast<CanvasImage>(other);
    return otherCanvasImage && equals(*otherCanvasImage);
}

bool CanvasImage::equals(const CanvasImage& other) const
{
    return m_name == other.m_name;
}

Ref<CSSValue> CanvasImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    return CSSCanvasValue::create(toCSS(m_name, style));
}

Ref<DeprecatedCSSOMValue> CanvasImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool CanvasImage::isPending() const
{
    return false;
}

void CanvasImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

ImageDrawResult CanvasImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, bool) const
{
    if (isPending())
        return ImageDrawResult::DidNothing;

    RefPtr image = resolvedImage(renderer);
    if (!image || !image->hasSomethingToDraw())
        return ImageDrawResult::DidNothing;

    return drawResolved(context, renderer, *image, ConcreteObjectSize::fixed(image->size()), destination, mapSourceToSize(source, concreteObjectSize, image->size(options.orientation())), options);
}

ImageDrawResult CanvasImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, bool) const
{
    RefPtr image = resolvedImage(renderer);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawResolvedAsPattern(context, renderer, *image, ConcreteObjectSize::fixed(image->size()), destination, tile, patternTransform, phase, spacing, options);
}

ImageDrawResult CanvasImage::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImageDrawingOptions options, bool) const
{
    RefPtr image = resolvedImage(renderer);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawResolvedTiled(context, renderer, *image, NaturalDimensions::fixed(image->size()), ConcreteObjectSize::fixed(image->size()), destination, phase, tileSize, spacing, options);
}

ImageDrawResult CanvasImage::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const NinePieceGeometry& geometry, ImageDrawingOptions options) const
{
    RefPtr image = resolvedImage(renderer);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawResolvedNinePiece(context, renderer, *image, ConcreteObjectSize::fixed(image->size()), geometry, options);
}

bool CanvasImage::knownToBeOpaque(const RenderElement&) const
{
    // FIXME: When CanvasRenderingContext2DSettings.alpha is implemented, this can be improved to check for it.
    return false;
}

bool CanvasImage::canDraw(const RenderElement& renderer) const
{
    RefPtr image = resolvedImage(renderer);
    return image && image->hasSomethingToDraw();
}

bool CanvasImage::canDrawAtSize(const RenderElement& renderer, const FloatSize& size) const
{
    return !size.isEmpty() && canDraw(renderer);
}

InterpolationQuality CanvasImage::interpolationQualityForImageDraw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const void* layer, const LayoutSize& size) const
{
    RefPtr image = resolvedImage(renderer);
    if (!image)
        return Image::interpolationQualityForImageDraw(context, renderer, concreteObjectSize, layer, size);
    return ImageQualityController::chooseInterpolationQuality(context, renderer, IntSize(image->size()), layer, size);
}

FloatRect CanvasImage::mapChangedRectToSource(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& changedRect) const
{
    RefPtr element = this->element(protect(renderer.document()));
    if (!element)
        return Image::mapChangedRectToSource(renderer, concreteObjectSize, changedRect);
    return mapSizeToSource(changedRect, FloatSize { element->size() }, concreteObjectSize);
}

NaturalDimensions CanvasImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext&) const
{
    if (auto* element = this->element(protect(renderer.document())))
        return NaturalDimensions::fixed(FloatSize { element->size() });
    return NaturalDimensions::zero();
}

void CanvasImage::didAddClient(RenderElement& renderer)
{
    if (RefPtr element = this->element(protect(renderer.document())))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*element);
}

void CanvasImage::didRemoveClient(RenderElement& renderer)
{
    if (RefPtr element = this->element(protect(renderer.document())))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*element);
}

void CanvasImage::canvasContentsWillChange(CanvasBase& canvasBase, const FloatRect& changingRect)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));

    auto imageChangeRect = enclosingIntRect(changingRect);
    for (auto entry : clients()) {
        auto& client = entry.key;
        client.imageChanged(static_cast<WrappedImagePtr>(this), &imageChangeRect);
    }
}

void CanvasImage::canvasResized(CanvasBase& canvasBase)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));

    for (auto entry : clients()) {
        auto& client = entry.key;
        client.imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

void CanvasImage::canvasDestroyed(CanvasBase& canvasBase)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));
    m_element = nullptr;
}

HTMLCanvasElement* CanvasImage::element(Document& document) const
{
    if (!m_element) {
        m_element = document.getCSSCanvasElement(m_name.value);
        protect(m_element.get())->addObserver(const_cast<CanvasImage&>(*this));
    }
    return m_element.get();
}

RefPtr<BitmapImage> CanvasImage::resolvedImage(const RenderElement& renderer) const
{
    ASSERT(clients().contains(const_cast<RenderElement&>(renderer)));
    RefPtr element = this->element(protect(renderer.document()));
    if (!element)
        return nullptr;
    return element->copiedImage();
}

} // namespace WebCore::Style
