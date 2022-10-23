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
#include "StyleCanvasImage.h"

#include "CSSCanvasValue.h"
#include "HTMLCanvasElement.h"
#include "InspectorInstrumentation.h"
#include "RenderElement.h"

namespace WebCore {

StyleCanvasImage::StyleCanvasImage(String&& name)
    : StyleGeneratedImage { Type::CanvasImage, StyleCanvasImage::isFixedSize }
    , m_name { WTFMove(name) }
    , m_element { nullptr }
{
}

StyleCanvasImage::~StyleCanvasImage()
{
    if (m_element)
        m_element->removeObserver(*this);
}

bool StyleCanvasImage::operator==(const StyleImage& other) const
{
    return is<StyleCanvasImage>(other) && equals(downcast<StyleCanvasImage>(other));
}

bool StyleCanvasImage::equals(const StyleCanvasImage& other) const
{
    return m_name == other.m_name;
}

Ref<CSSValue> StyleCanvasImage::computedStyleValue(const RenderStyle&) const
{
    return CSSCanvasValue::create(m_name);
}

bool StyleCanvasImage::isPending() const
{
    return false;
}

void StyleCanvasImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<Image> StyleCanvasImage::image(const RenderElement* renderer, const FloatSize&) const
{
    if (!renderer)
        return &Image::nullImage();

    ASSERT(clients().contains(const_cast<RenderElement*>(renderer)));
    auto* element = this->element(renderer->document());
    if (!element || !element->buffer())
        return nullptr;
    return element->copiedImage();
}

bool StyleCanvasImage::knownToBeOpaque(const RenderElement&) const
{
    // FIXME: When CanvasRenderingContext2DSettings.alpha is implemented, this can be improved to check for it.
    return false;
}

FloatSize StyleCanvasImage::fixedSize(const RenderElement& renderer) const
{
    if (auto* element = this->element(renderer.document()))
        return FloatSize { element->size() };
    return { };
}

void StyleCanvasImage::didAddClient(RenderElement& renderer)
{
    if (auto* element = this->element(renderer.document()))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*element);
}

void StyleCanvasImage::didRemoveClient(RenderElement& renderer)
{
    if (auto* element = this->element(renderer.document()))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*element);
}

void StyleCanvasImage::canvasChanged(CanvasBase& canvasBase, const std::optional<FloatRect>& changedRect)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));

    if (!changedRect)
        return;

    auto imageChangeRect = enclosingIntRect(changedRect.value());
    for (auto& client : clients().values())
        client->imageChanged(static_cast<WrappedImagePtr>(this), &imageChangeRect);
}

void StyleCanvasImage::canvasResized(CanvasBase& canvasBase)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));

    for (auto& client : clients().values())
        client->imageChanged(static_cast<WrappedImagePtr>(this));
}

void StyleCanvasImage::canvasDestroyed(CanvasBase& canvasBase)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));
    m_element = nullptr;
}

HTMLCanvasElement* StyleCanvasImage::element(Document& document) const
{
    if (!m_element) {
        m_element = document.getCSSCanvasElement(m_name);
        if (!m_element)
            return nullptr;
        m_element->addObserver(const_cast<StyleCanvasImage&>(*this));
    }
    return m_element;
}

} // namespace WebCore
