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
#include "HTMLCanvasElement.h"
#include "InspectorInstrumentation.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore::Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CanvasImage);

CanvasImage::CanvasImage(String&& name)
    : GeneratedImage { Type::CanvasImage, CanvasImage::isFixedSize }
    , m_name { WTF::move(name) }
{
}

CanvasImage::~CanvasImage()
{
    if (m_element)
        m_element->removeObserver(*this);
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

Ref<CSSValue> CanvasImage::computedStyleValue(const RenderStyle&) const
{
    return CSSCanvasValue::create(m_name);
}

bool CanvasImage::isPending() const
{
    return false;
}

void CanvasImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<WebCore::Image> CanvasImage::image(const RenderElement* renderer, const FloatSize&, const GraphicsContext&, bool) const
{
    if (!renderer)
        return &WebCore::Image::nullImage();

    ASSERT(clients().contains(const_cast<RenderElement&>(*renderer)));
    RefPtr element = this->element(renderer->document());
    if (!element)
        return nullptr;
    return element->copiedImage();
}

bool CanvasImage::knownToBeOpaque(const RenderElement&) const
{
    // FIXME: When CanvasRenderingContext2DSettings.alpha is implemented, this can be improved to check for it.
    return false;
}

FloatSize CanvasImage::fixedSize(const RenderElement& renderer) const
{
    if (auto* element = this->element(renderer.document()))
        return FloatSize { element->size() };
    return { };
}

void CanvasImage::didAddClient(RenderElement& renderer)
{
    if (RefPtr element = this->element(renderer.document()))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*element);
}

void CanvasImage::didRemoveClient(RenderElement& renderer)
{
    if (RefPtr element = this->element(renderer.document()))
        InspectorInstrumentation::didChangeCSSCanvasClientNodes(*element);
}

void CanvasImage::canvasChanged(CanvasBase& canvasBase, const FloatRect& changedRect)
{
    ASSERT_UNUSED(canvasBase, is<HTMLCanvasElement>(canvasBase));
    ASSERT_UNUSED(canvasBase, m_element == &downcast<HTMLCanvasElement>(canvasBase));

    auto imageChangeRect = enclosingIntRect(changedRect);
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
        m_element = document.getCSSCanvasElement(m_name);
        m_element->addObserver(const_cast<CanvasImage&>(*this));
    }
    return m_element.get();
}

} // namespace WebCore::Style
