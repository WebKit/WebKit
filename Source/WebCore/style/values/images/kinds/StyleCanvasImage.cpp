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

#include "CSSCanvasValue.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "InspectorInstrumentation.h"
#include "NinePieceGeometry.h"
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

ImageDrawResult CanvasImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr image = resolvedImage(renderer, concreteObjectSize.size(), context, isForFirstLine);
    if (!image)
        return ImageDrawResult::DidNothing;

    return drawResolved(context, renderer, *image, concreteObjectSize, destination, source, options);
}

ImageDrawResult CanvasImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr image = resolvedImage(renderer, concreteObjectSize.size(), context, isForFirstLine);
    if (!image)
        return ImageDrawResult::DidNothing;

    auto extras = drawingExtrasForRenderer(renderer);
    image->drawPattern(context, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options, &extras);
    return ImageDrawResult::DidDraw;
}

RefPtr<WebCore::Image> CanvasImage::resolvedImage(const RenderElement& renderer, FloatSize, const GraphicsContext&, bool) const
{
    ASSERT(clients().contains(const_cast<RenderElement&>(renderer)));
    RefPtr element = this->element(protect(renderer.document()));
    if (!element)
        return nullptr;
    return element->copiedImage();
}

bool CanvasImage::hasNothingToDraw(const RenderElement& renderer) const
{
    RefPtr element = this->element(protect(renderer.document()));
    if (!element)
        return true;

    return !element->copiedImage();
}

bool CanvasImage::knownToBeOpaque(const RenderElement&) const
{
    // FIXME: When CanvasRenderingContext2DSettings.alpha is implemented, this can be improved to check for it.
    return false;
}

bool CanvasImage::isOriginClean(Document& document) const
{
    RefPtr element = this->element(document);
    return !element || element->originClean();
}

NaturalDimensions CanvasImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext&) const
{
    if (auto* element = this->element(protect(renderer.document())))
        return NaturalDimensions::fixed(FloatSize { element->size() });

    return NaturalDimensions::zeroSize();
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

} // namespace WebCore::Style
