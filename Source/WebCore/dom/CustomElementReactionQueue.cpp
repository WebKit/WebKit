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

#include "CustomElementRegistry.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "JSCustomElementInterface.h"
#include "JSDOMBinding.h"
#include "Microtasks.h"
#include <heap/Heap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>

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

    CustomElementReactionQueueItem(Type type)
        : m_type(type)
    { }

    CustomElementReactionQueueItem(Document& oldDocument, Document& newDocument)
        : m_type(Type::Adopted)
        , m_oldDocument(&oldDocument)
        , m_newDocument(&newDocument)
    { }

    CustomElementReactionQueueItem(const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
        : m_type(Type::AttributeChanged)
        , m_attributeName(attributeName)
        , m_oldValue(oldValue)
        , m_newValue(newValue)
    { }

    void invoke(Element& element, JSCustomElementInterface& elementInterface)
    {
        switch (m_type) {
        case Type::ElementUpgrade:
            elementInterface.upgradeElement(element);
            break;
        case Type::Connected:
            elementInterface.invokeConnectedCallback(element);
            break;
        case Type::Disconnected:
            elementInterface.invokeDisconnectedCallback(element);
            break;
        case Type::Adopted:
            elementInterface.invokeAdoptedCallback(element, *m_oldDocument, *m_newDocument);
            break;
        case Type::AttributeChanged:
            ASSERT(m_attributeName);
            elementInterface.invokeAttributeChangedCallback(element, m_attributeName.value(), m_oldValue, m_newValue);
            break;
        }
    }

private:
    Type m_type;
    RefPtr<Document> m_oldDocument;
    RefPtr<Document> m_newDocument;
    std::optional<QualifiedName> m_attributeName;
    AtomicString m_oldValue;
    AtomicString m_newValue;
};

CustomElementReactionQueue::CustomElementReactionQueue(JSCustomElementInterface& elementInterface)
    : m_interface(elementInterface)
{ }

CustomElementReactionQueue::~CustomElementReactionQueue()
{
    ASSERT(m_items.isEmpty());
}

void CustomElementReactionQueue::clear()
{
    m_items.clear();
}

void CustomElementReactionQueue::enqueueElementUpgrade(Element& element)
{
    auto& queue = CustomElementReactionStack::ensureCurrentQueue(element);
    queue.m_items.append({CustomElementReactionQueueItem::Type::ElementUpgrade});
}

void CustomElementReactionQueue::enqueueElementUpgradeIfDefined(Element& element)
{
    ASSERT(element.isConnected());
    ASSERT(element.isCustomElementUpgradeCandidate());
    auto* window = element.document().domWindow();
    if (!window)
        return;

    auto* registry = window->customElementRegistry();
    if (!registry)
        return;

    auto* elementInterface = registry->findInterface(element);
    if (!elementInterface)
        return;

    element.enqueueToUpgrade(*elementInterface);
}

void CustomElementReactionQueue::enqueueConnectedCallbackIfNeeded(Element& element)
{
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    auto& queue = CustomElementReactionStack::ensureCurrentQueue(element);
    if (queue.m_interface->hasConnectedCallback())
        queue.m_items.append({CustomElementReactionQueueItem::Type::Connected});
}

void CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(Element& element)
{
    ASSERT(element.isDefinedCustomElement());
    if (element.document().refCount() <= 0)
        return; // Don't enqueue disconnectedCallback if the entire document is getting destructed.
    auto& queue = CustomElementReactionStack::ensureCurrentQueue(element);
    if (queue.m_interface->hasDisconnectedCallback())
        queue.m_items.append({CustomElementReactionQueueItem::Type::Disconnected});
}

void CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(Element& element, Document& oldDocument, Document& newDocument)
{
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    auto& queue = CustomElementReactionStack::ensureCurrentQueue(element);
    if (queue.m_interface->hasAdoptedCallback())
        queue.m_items.append({oldDocument, newDocument});
}

void CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(Element& element, const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    auto& queue = CustomElementReactionStack::ensureCurrentQueue(element);
    if (queue.m_interface->observesAttribute(attributeName.localName()))
        queue.m_items.append({attributeName, oldValue, newValue});
}

void CustomElementReactionQueue::enqueuePostUpgradeReactions(Element& element)
{
    ASSERT(element.isCustomElementUpgradeCandidate());
    if (!element.hasAttributes() && !element.isConnected())
        return;

    auto* queue = element.reactionQueue();
    ASSERT(queue);

    if (element.hasAttributes()) {
        for (auto& attribute : element.attributesIterator()) {
            if (queue->m_interface->observesAttribute(attribute.localName()))
                queue->m_items.append({attribute.name(), nullAtom(), attribute.value()});
        }
    }

    if (element.isConnected() && queue->m_interface->hasConnectedCallback())
        queue->m_items.append({CustomElementReactionQueueItem::Type::Connected});
}

bool CustomElementReactionQueue::observesStyleAttribute() const
{
    return m_interface->observesAttribute(HTMLNames::styleAttr->localName());
}

void CustomElementReactionQueue::invokeAll(Element& element)
{
    while (!m_items.isEmpty()) {
        Vector<CustomElementReactionQueueItem> items = WTFMove(m_items);
        for (auto& item : items)
            item.invoke(element, m_interface.get());
    }
}

inline void CustomElementReactionStack::ElementQueue::add(Element& element)
{
    RELEASE_ASSERT(!m_invoking);
    // FIXME: Avoid inserting the same element multiple times.
    m_elements.append(element);
}

inline void CustomElementReactionStack::ElementQueue::invokeAll()
{
    RELEASE_ASSERT(!m_invoking);
    SetForScope<bool> invoking(m_invoking, true);
    Vector<Ref<Element>> elements;
    elements.swap(m_elements);
    RELEASE_ASSERT(m_elements.isEmpty());
    for (auto& element : elements) {
        auto* queue = element->reactionQueue();
        ASSERT(queue);
        queue->invokeAll(element.get());
    }
    RELEASE_ASSERT(m_elements.isEmpty());
}

CustomElementReactionQueue& CustomElementReactionStack::ensureCurrentQueue(Element& element)
{
    ASSERT(element.reactionQueue());
    if (!s_currentProcessingStack) {
        auto& queue = CustomElementReactionStack::ensureBackupQueue();
        queue.add(element);
        return *element.reactionQueue();
    }

    auto*& queue = s_currentProcessingStack->m_queue;
    if (!queue) // We use a raw pointer to avoid genearing code to delete it in ~CustomElementReactionStack.
        queue = new ElementQueue;
    queue->add(element);
    return *element.reactionQueue();
}

CustomElementReactionStack* CustomElementReactionStack::s_currentProcessingStack = nullptr;

void CustomElementReactionStack::processQueue()
{
    ASSERT(m_queue);
    m_queue->invokeAll();
    delete m_queue;
    m_queue = nullptr;
}

class BackupElementQueueMicrotask final : public Microtask {
    WTF_MAKE_FAST_ALLOCATED;
private:
    Result run() final
    {
        CustomElementReactionStack::processBackupQueue();
        return Result::Done;
    }
};

static bool s_processingBackupElementQueue = false;

CustomElementReactionStack::ElementQueue& CustomElementReactionStack::ensureBackupQueue()
{
    if (!s_processingBackupElementQueue) {
        s_processingBackupElementQueue = true;
        MicrotaskQueue::mainThreadQueue().append(std::make_unique<BackupElementQueueMicrotask>());
    }
    return backupElementQueue();
}

void CustomElementReactionStack::processBackupQueue()
{
    backupElementQueue().invokeAll();
    s_processingBackupElementQueue = false;
}

CustomElementReactionStack::ElementQueue& CustomElementReactionStack::backupElementQueue()
{
    static NeverDestroyed<ElementQueue> queue;
    return queue.get();
}

}
