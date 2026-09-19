/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 *
 */

#include "config.h"
#include "EventListenerMap.h"

#include "AbortSignal.h"
#include "Event.h"
#include "EventTarget.h"
#include "JSEventListener.h"
#include <JavaScriptCore/Heap.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>


namespace WebCore {

EventListenerMap::EventListenerMap() = default;

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
void EventListenerMap::assertIsOwnerThreadOrGCThreadWithWorldStopped() const
{
#if PLATFORM(IOS_FAMILY)
    if (WebThreadIsEnabled())
        return;
#endif
    ASSERT_WITH_SECURITY_IMPLICATION(!m_threadUID || m_threadUID == currentThreadID() || JSC::currentThreadIsCollectingWithWorldStopped());
}
#endif

bool EventListenerMap::containsCapturing(const AtomString& eventType) const
{
    assertIsOwnerThreadOrGCThreadWithWorldStopped();
    auto* listeners = find(eventType);
    if (!listeners)
        return false;

    for (auto& eventListener : *listeners) {
        if (eventListener->useCapture())
            return true;
    }
    return false;
}

bool EventListenerMap::containsActive(const AtomString& eventType) const
{
    assertIsOwnerThreadOrGCThreadWithWorldStopped();
    auto* listeners = find(eventType);
    if (!listeners)
        return false;

    for (auto& eventListener : *listeners) {
        if (!eventListener->isPassive())
            return true;
    }
    return false;
}

void EventListenerMap::clear()
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    for (auto& entry : entriesForMutation()) {
        for (auto& listener : entry.second)
            listener->markAsRemoved();
    }

    entriesForMutation().clear();
}

Vector<AtomString> EventListenerMap::eventTypes() const
{
    assertIsOwnerThread();
    return entries().map([](auto& entry) {
        return entry.first;
    });
}

static inline size_t findListener(const EventListenerVector& listeners, EventListener& listener, bool useCapture)
{
    for (size_t i = 0; i < listeners.size(); ++i) {
        auto& registeredListener = listeners[i];
        if (registeredListener->callback() == listener && registeredListener->useCapture() == useCapture)
            return i;
    }
    return notFound;
}

void EventListenerMap::replacePreservingOptions(const AtomString& eventType, EventListener& oldListener, Ref<EventListener>&& newListener, bool useCapture)
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    auto* listeners = findForMutation(eventType);
    ASSERT(listeners);
    size_t index = findListener(*listeners, oldListener, useCapture);
    ASSERT(index != notFound);
    auto& registeredListener = listeners->at(index);
    auto existingOptions = registeredListener->options();
    registeredListener->markAsRemoved();
    registeredListener = RegisteredEventListener::create(WTF::move(newListener), existingOptions);
}

bool EventListenerMap::add(const AtomString& eventType, Ref<EventListener>&& listener, const RegisteredEventListener::Options& options)
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    if (auto* listeners = findForMutation(eventType)) {
        if (findListener(*listeners, listener, options.capture) != notFound)
            return false; // Duplicate listener.
        listeners->append(RegisteredEventListener::create(WTF::move(listener), options));
        return true;
    }

    entriesForMutation().append({ eventType, EventListenerVector { RegisteredEventListener::create(WTF::move(listener), options) } });
    return true;
}

static bool removeListenerFromVector(EventListenerVector& listeners, EventListener& listener, bool useCapture)
{
    size_t indexOfRemovedListener = findListener(listeners, listener, useCapture);
    if (indexOfRemovedListener == notFound) [[unlikely]]
        return false;

    listeners[indexOfRemovedListener]->markAsRemoved();
    listeners.removeAt(indexOfRemovedListener);
    return true;
}

bool EventListenerMap::remove(const AtomString& eventType, EventListener& listener, bool useCapture)
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    auto& entries = entriesForMutation();
    for (unsigned i = 0; i < entries.size(); ++i) {
        if (entries[i].first == eventType) {
            bool wasRemoved = removeListenerFromVector(entries[i].second, listener, useCapture);
            if (entries[i].second.isEmpty())
                entries.removeAt(i);
            return wasRemoved;
        }
    }

    return false;
}

const EventListenerVector* EventListenerMap::find(const AtomString& eventType) const
{
    for (auto& entry : entries()) {
        if (entry.first == eventType)
            return &entry.second;
    }

    return nullptr;
}

EventListenerVector* EventListenerMap::findForMutation(const AtomString& eventType)
{
    // Safe to drop the const: this requires m_lock exclusively, so no other thread can be reading.
    return const_cast<EventListenerVector*>(find(eventType));
}

static void removeFirstListenerCreatedFromMarkup(EventListenerVector& listenerVector)
{
    bool foundListener = listenerVector.removeFirstMatching([] (const auto& registeredListener) {
        if (JSEventListener::wasCreatedFromMarkup(registeredListener->callback())) {
            registeredListener->markAsRemoved();
            return true;
        }
        return false;
    });
    ASSERT_UNUSED(foundListener, foundListener);
}

void EventListenerMap::removeFirstEventListenerCreatedFromMarkup(const AtomString& eventType)
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    auto& entries = entriesForMutation();
    for (unsigned i = 0; i < entries.size(); ++i) {
        if (entries[i].first == eventType) {
            removeFirstListenerCreatedFromMarkup(entries[i].second);
            if (entries[i].second.isEmpty())
                entries.removeAt(i);
            return;
        }
    }
}

static void copyListenersNotCreatedFromMarkupToTarget(const AtomString& eventType, const EventListenerVector& listenerVector, EventTarget* target)
{
    for (auto& registeredListener : listenerVector) {
        // Event listeners created from markup have already been transfered to the shadow tree during cloning.
        if (JSEventListener::wasCreatedFromMarkup(registeredListener->callback()))
            continue;
        target->addEventListener(eventType, registeredListener->callback(), { { registeredListener->useCapture() }, registeredListener->isPassive(), registeredListener->isOnce(), nullptr, false });
    }
}

void EventListenerMap::copyEventListenersNotCreatedFromMarkupToTarget(EventTarget* target)
{
    assertIsOwnerThread();
    for (auto& entry : entries())
        copyListenersNotCreatedFromMarkupToTarget(entry.first, entry.second, target);
}

} // namespace WebCore
