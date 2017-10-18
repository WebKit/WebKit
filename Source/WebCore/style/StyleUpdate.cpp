/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleUpdate.h"

#include "ComposedTreeAncestorIterator.h"
#include "Document.h"
#include "Element.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "Text.h"

namespace WebCore {
namespace Style {

Update::Update(Document& document)
    : m_document(document)
{
}

const ElementUpdates* Update::elementUpdates(const Element& element) const
{
    auto it = m_elements.find(&element);
    if (it == m_elements.end())
        return nullptr;
    return &it->value;
}

ElementUpdates* Update::elementUpdates(const Element& element)
{
    auto it = m_elements.find(&element);
    if (it == m_elements.end())
        return nullptr;
    return &it->value;
}

const TextUpdate* Update::textUpdate(const Text& text) const
{
    auto it = m_texts.find(&text);
    if (it == m_texts.end())
        return nullptr;
    return &it->value;
}

const RenderStyle* Update::elementStyle(const Element& element) const
{
    if (auto* updates = elementUpdates(element))
        return updates->update.style.get();
    auto* renderer = element.renderer();
    if (!renderer)
        return nullptr;
    return &renderer->style();
}

RenderStyle* Update::elementStyle(const Element& element)
{
    if (auto* updates = elementUpdates(element))
        return updates->update.style.get();
    auto* renderer = element.renderer();
    if (!renderer)
        return nullptr;
    return &renderer->mutableStyle();
}

void Update::addElement(Element& element, Element* parent, ElementUpdates&& elementUpdates)
{
    ASSERT(!m_elements.contains(&element));
    ASSERT(composedTreeAncestors(element).first() == parent);

    addPossibleRoot(parent);
    m_elements.add(&element, WTFMove(elementUpdates));
}

void Update::addText(Text& text, Element* parent, TextUpdate&& textUpdate)
{
    ASSERT(!m_texts.contains(&text));
    ASSERT(composedTreeAncestors(text).first() == parent);

    addPossibleRoot(parent);
    m_texts.add(&text, WTFMove(textUpdate));
}

void Update::addText(Text& text, TextUpdate&& textUpdate)
{
    addText(text, composedTreeAncestors(text).first(), WTFMove(textUpdate));
}

void Update::addPossibleRoot(Element* element)
{
    if (!element) {
        m_roots.add(&m_document);
        return;
    }
    if (m_elements.contains(element))
        return;
    m_roots.add(element);
}

}
}
