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
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/Heap.h>
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

    Type type() const { return m_type; }

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

void CustomElementReactionQueue::enqueueElementUpgrade(Element& element, bool alreadyScheduledToUpgrade)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (alreadyScheduledToUpgrade) {
        ASSERT(queue.m_items.size() == 1);
        ASSERT(queue.m_items[0].type() == CustomElementReactionQueueItem::Type::ElementUpgrade);
    } else {
        queue.m_items.append({CustomElementReactionQueueItem::Type::ElementUpgrade});
        enqueueElementOnAppropriateElementQueue(element);
    }
}

void CustomElementReactionQueue::enqueueElementUpgradeIfDefined(Element& element)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
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
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasConnectedCallback())
        return;
    queue.m_items.append({CustomElementReactionQueueItem::Type::Connected});
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(Element& element)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.isDefinedCustomElement());
    if (element.document().refCount() <= 0)
        return; // Don't enqueue disconnectedCallback if the entire document is getting destructed.
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasDisconnectedCallback())
        return;
    queue.m_items.append({CustomElementReactionQueueItem::Type::Disconnected});
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(Element& element, Document& oldDocument, Document& newDocument)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasAdoptedCallback())
        return;
    queue.m_items.append({oldDocument, newDocument});
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(Element& element, const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->observesAttribute(attributeName.localName()))
        return;
    queue.m_items.append({attributeName, oldValue, newValue});
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueuePostUpgradeReactions(Element& element)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.isCustomElementUpgradeCandidate());
    if (!element.hasAttributes() && !element.isConnected())
        return;

    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();

    if (element.hasAttributes()) {
        for (auto& attribute : element.attributesIterator()) {
            if (queue.m_interface->observesAttribute(attribute.localName()))
                queue.m_items.append({attribute.name(), nullAtom(), attribute.value()});
        }
    }

    if (element.isConnected() && queue.m_interface->hasConnectedCallback())
        queue.m_items.append({CustomElementReactionQueueItem::Type::Connected});
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

inline void CustomElementReactionQueue::ElementQueue::add(Element& element)
{
    ASSERT(!m_invoking);
    // FIXME: Avoid inserting the same element multiple times.
    m_elements.append(element);
}

inline void CustomElementReactionQueue::ElementQueue::invokeAll()
{
    RELEASE_ASSERT(!m_invoking);
    SetForScope<bool> invoking(m_invoking, true);
    unsigned originalSize = m_elements.size();
    // It's possible for more elements to be enqueued if some IDL attributes were missing CEReactions.
    // Invoke callbacks slightly later here instead of crashing / ignoring those cases.
    for (unsigned i = 0; i < m_elements.size(); ++i) {
        auto& element = m_elements[i].get();
        auto* queue = element.reactionQueue();
        ASSERT(queue);
        queue->invokeAll(element);
    }
    ASSERT_UNUSED(originalSize, m_elements.size() == originalSize);
    m_elements.clear();
}

inline void CustomElementReactionQueue::ElementQueue::processQueue(JSC::ExecState* state)
{
    if (!state) {
        invokeAll();
        return;
    }

    auto& vm = state->vm();
    JSC::JSLockHolder lock(vm);

    JSC::Exception* previousException = nullptr;
    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);
        previousException = catchScope.exception();
        if (previousException)
            catchScope.clearException();
    }

    invokeAll();

    if (previousException) {
        auto throwScope = DECLARE_THROW_SCOPE(vm);
        throwException(state, throwScope, previousException);
    }
}

// https://html.spec.whatwg.org/multipage/custom-elements.html#enqueue-an-element-on-the-appropriate-element-queue
void CustomElementReactionQueue::enqueueElementOnAppropriateElementQueue(Element& element)
{
    ASSERT(element.reactionQueue());
    if (!CustomElementReactionStack::s_currentProcessingStack) {
        auto& queue = ensureBackupQueue();
        queue.add(element);
        return;
    }

    auto*& queue = CustomElementReactionStack::s_currentProcessingStack->m_queue;
    if (!queue) // We use a raw pointer to avoid genearing code to delete it in ~CustomElementReactionStack.
        queue = new ElementQueue;
    queue->add(element);
}

#if !ASSERT_DISABLED
unsigned CustomElementReactionDisallowedScope::s_customElementReactionDisallowedCount = 0;
#endif

CustomElementReactionStack* CustomElementReactionStack::s_currentProcessingStack = nullptr;

void CustomElementReactionStack::processQueue(JSC::ExecState* state)
{
    ASSERT(m_queue);
    m_queue->processQueue(state);
    delete m_queue;
    m_queue = nullptr;
}

class BackupElementQueueMicrotask final : public Microtask {
    WTF_MAKE_FAST_ALLOCATED;
private:
    Result run() final
    {
        CustomElementReactionQueue::processBackupQueue();
        return Result::Done;
    }
};

static bool s_processingBackupElementQueue = false;

CustomElementReactionQueue::ElementQueue& CustomElementReactionQueue::ensureBackupQueue()
{
    if (!s_processingBackupElementQueue) {
        s_processingBackupElementQueue = true;
        MicrotaskQueue::mainThreadQueue().append(std::make_unique<BackupElementQueueMicrotask>());
    }
    return backupElementQueue();
}

void CustomElementReactionQueue::processBackupQueue()
{
    backupElementQueue().processQueue(nullptr);
    s_processingBackupElementQueue = false;
}

CustomElementReactionQueue::ElementQueue& CustomElementReactionQueue::backupElementQueue()
{
    static NeverDestroyed<ElementQueue> queue;
    return queue.get();
}

}
