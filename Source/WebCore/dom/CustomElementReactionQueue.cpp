/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#include "ElementInlines.h"
#include "EventLoop.h"
#include "HTMLFormElement.h"
#include "JSCustomElementInterface.h"
#include "JSDOMBinding.h"
#include "WindowEventLoop.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/Heap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>

namespace WebCore {

class CustomElementReactionQueueItem {
    using AdoptedPayload = std::tuple<Ref<Document>, Ref<Document>>;
    using AttributeChangedPayload = std::tuple<QualifiedName, AtomString, AtomString>;
    using FormAssociatedPayload = RefPtr<HTMLFormElement>;
    using FormDisabledPayload = bool;
    using FormStateRestorePayload = CustomElementFormValue;
    using Payload = std::optional<std::variant<AdoptedPayload, AttributeChangedPayload, FormAssociatedPayload, FormDisabledPayload, FormStateRestorePayload>>;

public:
    enum class Type : uint8_t {
        ElementUpgrade,
        Connected,
        Disconnected,
        Adopted,
        AttributeChanged,
        FormAssociated,
        FormReset,
        FormDisabled,
        FormStateRestore,
    };

    CustomElementReactionQueueItem(Type type, Payload payload = std::nullopt)
        : m_type(type)
        , m_payload(payload)
    { }

    Type type() const { return m_type; }

    void invoke(Element& element, JSCustomElementInterface& elementInterface)
    {
        switch (m_type) {
        case Type::ElementUpgrade:
            ASSERT(!m_payload.has_value());
            elementInterface.upgradeElement(element);
            break;
        case Type::Connected:
            ASSERT(!m_payload.has_value());
            elementInterface.invokeConnectedCallback(element);
            break;
        case Type::Disconnected:
            ASSERT(!m_payload.has_value());
            elementInterface.invokeDisconnectedCallback(element);
            break;
        case Type::Adopted: {
            ASSERT(m_payload.has_value() && std::holds_alternative<AdoptedPayload>(m_payload.value()));
            auto& payload = std::get<AdoptedPayload>(m_payload.value());
            elementInterface.invokeAdoptedCallback(element, std::get<0>(payload), std::get<1>(payload));
            break;
        }
        case Type::AttributeChanged: {
            ASSERT(m_payload.has_value() && std::holds_alternative<AttributeChangedPayload>(m_payload.value()));
            auto& payload = std::get<AttributeChangedPayload>(m_payload.value());
            elementInterface.invokeAttributeChangedCallback(element, std::get<0>(payload), std::get<1>(payload), std::get<2>(payload));
            break;
        }
        case Type::FormAssociated:
            ASSERT(m_payload.has_value() && std::holds_alternative<FormAssociatedPayload>(m_payload.value()));
            elementInterface.invokeFormAssociatedCallback(element, std::get<FormAssociatedPayload>(m_payload.value()).get());
            break;
        case Type::FormReset:
            ASSERT(!m_payload.has_value());
            elementInterface.invokeFormResetCallback(element);
            break;
        case Type::FormDisabled:
            ASSERT(m_payload.has_value() && std::holds_alternative<FormDisabledPayload>(m_payload.value()));
            elementInterface.invokeFormDisabledCallback(element, std::get<FormDisabledPayload>(m_payload.value()));
            break;
        case Type::FormStateRestore:
            ASSERT(m_payload.has_value() && std::holds_alternative<FormStateRestorePayload>(m_payload.value()));
            elementInterface.invokeFormStateRestoreCallback(element, std::get<FormStateRestorePayload>(m_payload.value()));
            break;
        }
    }

private:
    Type m_type;
    Payload m_payload;
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

#if ASSERT_ENABLED
bool CustomElementReactionQueue::hasJustUpgradeReaction() const
{
    return m_items.size() == 1 && m_items[0].type() == CustomElementReactionQueueItem::Type::ElementUpgrade;
}
#endif

void CustomElementReactionQueue::enqueueElementUpgrade(Element& element, bool alreadyScheduledToUpgrade)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (alreadyScheduledToUpgrade)
        ASSERT(queue.hasJustUpgradeReaction());
    else
        queue.m_items.append(Item::Type::ElementUpgrade);
    enqueueElementOnAppropriateElementQueue(element);
}

// https://html.spec.whatwg.org/multipage/custom-elements.html#concept-try-upgrade
void CustomElementReactionQueue::tryToUpgradeElement(Element& element)
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
    queue.m_items.append(Item::Type::Connected);
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(Element& element)
{
    ASSERT(element.isDefinedCustomElement());
    if (element.document().refCount() <= 0)
        return; // Don't enqueue disconnectedCallback if the entire document is getting destructed.
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasDisconnectedCallback())
        return;
    queue.m_items.append(Item::Type::Disconnected);
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
    queue.m_items.append({ Item::Type::Adopted, std::make_tuple(Ref { oldDocument }, Ref { newDocument }) });
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(Element& element, const QualifiedName& attributeName, const AtomString& oldValue, const AtomString& newValue)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.isDefinedCustomElement());
    ASSERT(element.document().refCount() > 0);
    ASSERT(element.reactionQueue());
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->observesAttribute(attributeName.localName()))
        return;
    queue.m_items.append({ Item::Type::AttributeChanged, std::make_tuple(attributeName, oldValue, newValue) });
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueFormAssociatedCallbackIfNeeded(Element& element, HTMLFormElement* associatedForm)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    if (element.document().refCount() <= 0)
        return; // Don't enqueue formAssociatedCallback if the entire document is getting destructed.
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasFormAssociatedCallback())
        return;
    ASSERT(queue.isFormAssociated());
    queue.m_items.append({ Item::Type::FormAssociated, associatedForm });
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueFormResetCallbackIfNeeded(Element& element)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.document().refCount() > 0);
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasFormResetCallback())
        return;
    ASSERT(queue.isFormAssociated());
    queue.m_items.append(Item::Type::FormReset);
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueFormDisabledCallbackIfNeeded(Element& element, bool isDisabled)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.document().refCount() > 0);
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasFormDisabledCallback())
        return;
    ASSERT(queue.isFormAssociated());
    queue.m_items.append({ Item::Type::FormDisabled, isDisabled });
    enqueueElementOnAppropriateElementQueue(element);
}

void CustomElementReactionQueue::enqueueFormStateRestoreCallbackIfNeeded(Element& element, CustomElementFormValue&& state)
{
    ASSERT(CustomElementReactionDisallowedScope::isReactionAllowed());
    ASSERT(element.document().refCount() > 0);
    auto& queue = *element.reactionQueue();
    if (!queue.m_interface->hasFormStateRestoreCallback())
        return;
    ASSERT(queue.isFormAssociated());
    queue.m_items.append({ Item::Type::FormStateRestore, WTFMove(state) });
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
                queue.m_items.append({ Item::Type::AttributeChanged, std::make_tuple(attribute.name(), nullAtom(), attribute.value()) });
        }
    }

    if (element.isConnected() && queue.m_interface->hasConnectedCallback())
        queue.m_items.append(Item::Type::Connected);
}

bool CustomElementReactionQueue::observesStyleAttribute() const
{
    return m_interface->observesAttribute(HTMLNames::styleAttr->localName());
}

bool CustomElementReactionQueue::isElementInternalsDisabled() const
{
    return m_interface->isElementInternalsDisabled();
}

bool CustomElementReactionQueue::isFormAssociated() const
{
    return m_interface->isFormAssociated();
}

bool CustomElementReactionQueue::hasFormStateRestoreCallback() const
{
    return m_interface->hasFormStateRestoreCallback();
}

bool CustomElementReactionQueue::isElementInternalsAttached() const
{
    return m_elementInternalsAttached;
}

void CustomElementReactionQueue::setElementInternalsAttached()
{
    m_elementInternalsAttached = true;
}

void CustomElementReactionQueue::invokeAll(Element& element)
{
    while (!m_items.isEmpty()) {
        Vector<Item> items = WTFMove(m_items);
        for (auto& item : items)
            item.invoke(element, m_interface.get());
    }
}

inline void CustomElementQueue::add(Element& element)
{
    ASSERT(!m_invoking);
    // FIXME: Avoid inserting the same element multiple times.
    m_elements.append(element);
}

inline void CustomElementQueue::invokeAll()
{
    RELEASE_ASSERT(!m_invoking);
    SetForScope invoking(m_invoking, true);
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

inline void CustomElementQueue::processQueue(JSC::JSGlobalObject* state)
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
        element.document().windowEventLoop().backupElementQueue().add(element);
        return;
    }

    auto*& queue = CustomElementReactionStack::s_currentProcessingStack->m_queue;
    if (!queue) // We use a raw pointer to avoid genearing code to delete it in ~CustomElementReactionStack.
        queue = new CustomElementQueue;
    queue->add(element);
}

#if ASSERT_ENABLED
unsigned CustomElementReactionDisallowedScope::s_customElementReactionDisallowedCount = 0;
#endif

CustomElementReactionStack* CustomElementReactionStack::s_currentProcessingStack = nullptr;

void CustomElementReactionStack::processQueue(JSC::JSGlobalObject* state)
{
    ASSERT(m_queue);
    m_queue->processQueue(state);
    delete m_queue;
    m_queue = nullptr;
}

void CustomElementReactionQueue::processBackupQueue(CustomElementQueue& backupElementQueue)
{
    backupElementQueue.processQueue(nullptr);
}

}
