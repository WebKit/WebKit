/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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
#include "CSSCanvasValue.h"

#include "RenderElement.h"

namespace WebCore {

CSSCanvasValue::~CSSCanvasValue()
{
    if (m_element)
        m_element->removeObserver(m_canvasObserver);
}

String CSSCanvasValue::customCSSText() const
{
    return makeString("-webkit-canvas(", m_name, ')');
}

void CSSCanvasValue::canvasChanged(HTMLCanvasElement&, const FloatRect& changedRect)
{
    IntRect imageChangeRect = enclosingIntRect(changedRect);
    for (auto it = clients().begin(), end = clients().end(); it != end; ++it)
        it->key->imageChanged(static_cast<WrappedImagePtr>(this), &imageChangeRect);
}

void CSSCanvasValue::canvasResized(HTMLCanvasElement&)
{
    for (auto it = clients().begin(), end = clients().end(); it != end; ++it)
        it->key->imageChanged(static_cast<WrappedImagePtr>(this));
}

void CSSCanvasValue::canvasDestroyed(HTMLCanvasElement& element)
{
    ASSERT_UNUSED(&element, &element == m_element);
    m_element = nullptr;
}

FloatSize CSSCanvasValue::fixedSize(const RenderElement* renderer)
{
    if (HTMLCanvasElement* elt = element(renderer->document()))
        return FloatSize(elt->width(), elt->height());
    return FloatSize();
}

HTMLCanvasElement* CSSCanvasValue::element(Document& document)
{
     if (!m_element) {
        m_element = document.getCSSCanvasElement(m_name);
        if (!m_element)
            return nullptr;
        m_element->addObserver(m_canvasObserver);
    }
    return m_element;
}

RefPtr<Image> CSSCanvasValue::image(RenderElement* renderer, const FloatSize& /*size*/)
{
    ASSERT(clients().contains(renderer));
    HTMLCanvasElement* element = this->element(renderer->document());
    if (!element || !element->buffer())
        return nullptr;
    return element->copiedImage();
}

bool CSSCanvasValue::equals(const CSSCanvasValue& other) const
{
    return m_name == other.m_name;
}

} // namespace WebCore
