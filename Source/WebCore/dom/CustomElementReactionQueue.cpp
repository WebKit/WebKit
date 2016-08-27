/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "CustomElementReactionQueue.h"

#if ENABLE(CUSTOM_ELEMENTS)

#include "CustomElementRegistry.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Element.h"
#include "JSCustomElementInterface.h"
#include "JSDOMBinding.h"
#include <heap/Heap.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>

namespace WebCore {

class CustomElementReactionQueueItem {
public:
    enum class Type {
        ElementUpgrade,
        Connected,
        Disconnected,
        Adopted,
        AttributeChanged,
    };

    CustomElementReactionQueueItem(Type type, Element& element, JSCustomElementInterface& elementInterface)
        : m_type(type)
        , m_element(element)
        , m_interface(elementInterface)
    { }

    CustomElementReactionQueueItem(Element& element, JSCustomElementInterface& elementInterface, Document& oldDocument, Document& newDocument)
        : m_type(Type::Adopted)
        , m_element(element)
        , m_interface(elementInterface)
        , m_oldDocument(&oldDocument)
        , m_newDocument(&newDocument)
    { }

    CustomElementReactionQueueItem(Element& element, JSCustomElementInterface& elementInterface, const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
        : m_type(Type::AttributeChanged)
        , m_element(element)
        , m_interface(elementInterface)
        , m_attributeName(attributeName)
        , m_oldValue(oldValue)
        , m_newValue(newValue)
    { }

    void invoke()
    {
        switch (m_type) {
        case Type::ElementUpgrade:
            m_interface->upgradeElement(m_element.get());
            break;
        case Type::Connected:
            m_interface->invokeConnectedCallback(m_element.get());
            break;
        case Type::Disconnected:
            m_interface->invokeDisconnectedCallback(m_element.get());
            break;
        case Type::Adopted:
            m_interface->invokeAdoptedCallback(m_element.get(), *m_oldDocument, *m_newDocument);
            break;
        case Type::AttributeChanged:
            ASSERT(m_attributeName);
            m_interface->invokeAttributeChangedCallback(m_element.get(), m_attributeName.value(), m_oldValue, m_newValue);
            break;
        }
    }

private:
    Type m_type;
    Ref<Element> m_element;
    Ref<JSCustomElementInterface> m_interface;
    RefPtr<Document> m_oldDocument;
    RefPtr<Document> m_newDocument;
    Optional<QualifiedName> m_attributeName;
    AtomicString m_oldValue;
    AtomicString m_newValue;
};

CustomElementReactionQueue::CustomElementReactionQueue()
{ }

CustomElementReactionQueue::~CustomElementReactionQueue()
{
    ASSERT(m_items.isEmpty());
}

void CustomElementReactionQueue::enqueueElementUpgrade(Element& element, JSCustomElementInterface& elementInterface)
{
    if (auto* queue = CustomElementReactionStack::ensureCurrentQueue())
        queue->m_items.append({CustomElementReactionQueueItem::Type::ElementUpgrade, element, elementInterface});
}

void CustomElementReactionQueue::enqueueConnectedCallbackIfNeeded(Element& element)
{
    ASSERT(element.isCustomElement());
    auto* elementInterface = element.customElementInterface();
    ASSERT(elementInterface);
    if (!elementInterface->hasConnectedCallback())
        return;

    if (auto* queue = CustomElementReactionStack::ensureCurrentQueue())
        queue->m_items.append({CustomElementReactionQueueItem::Type::Connected, element, *elementInterface});
}

void CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(Element& element)
{
    ASSERT(element.isCustomElement());
    auto* elementInterface = element.customElementInterface();
    ASSERT(elementInterface);
    if (!elementInterface->hasDisconnectedCallback())
        return;

    if (auto* queue = CustomElementReactionStack::ensureCurrentQueue())
        queue->m_items.append({CustomElementReactionQueueItem::Type::Disconnected, element, *elementInterface});
}

void CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(Element& element, Document& oldDocument, Document& newDocument)
{
    ASSERT(element.isCustomElement());
    auto* elementInterface = element.customElementInterface();
    ASSERT(elementInterface);
    if (!elementInterface->hasAdoptedCallback())
        return;

    if (auto* queue = CustomElementReactionStack::ensureCurrentQueue())
        queue->m_items.append({element, *elementInterface, oldDocument, newDocument});
}

void CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(Element& element, const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    ASSERT(element.isCustomElement());
    auto* elementInterface = element.customElementInterface();
    ASSERT(elementInterface);
    if (!elementInterface->observesAttribute(attributeName.localName()))
        return;

    if (auto* queue = CustomElementReactionStack::ensureCurrentQueue())
        queue->m_items.append({element, *elementInterface, attributeName, oldValue, newValue});
}

void CustomElementReactionQueue::invokeAll()
{
    Vector<CustomElementReactionQueueItem> items;
    items.swap(m_items);
    for (auto& item : items)
        item.invoke();
}

CustomElementReactionQueue* CustomElementReactionStack::ensureCurrentQueue()
{
    // FIXME: This early exit indicates a bug that some DOM API is missing CEReactions
    if (!s_currentProcessingStack)
        return nullptr;

    auto*& queue = s_currentProcessingStack->m_queue;
    if (!queue) // We use a raw pointer to avoid genearing code to delete it in ~CustomElementReactionStack.
        queue = new CustomElementReactionQueue;
    return queue;
}

CustomElementReactionStack* CustomElementReactionStack::s_currentProcessingStack = nullptr;

void CustomElementReactionStack::processQueue()
{
    ASSERT(m_queue);
    m_queue->invokeAll();
    delete m_queue;
    m_queue = nullptr;
}

}

#endif
