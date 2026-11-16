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
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>


namespace WebCore {

struct AbortSignalRegistration {
    WeakPtr<AbortSignal, WeakPtrImplWithEventTargetData> signal;
    uint32_t abortAlgorithmIdentifier { 0 };
};

// Few listeners have a signal, so RegisteredEventListener only has a bit for it, and the signal is kept here.
static Lock abortSignalRegistrationsLock;
NODELETE static HashMap<const RegisteredEventListener*, AbortSignalRegistration>& abortSignalRegistrations() WTF_REQUIRES_LOCK(abortSignalRegistrationsLock)
{
    static NeverDestroyed<HashMap<const RegisteredEventListener*, AbortSignalRegistration>> registrations;
    return registrations;
}

void RegisteredEventListener::setAbortSignal(AbortSignal& signal, uint32_t abortAlgorithmIdentifier)
{
    ASSERT(!m_hasAbortSignal);
    m_hasAbortSignal = true;
    Locker locker { abortSignalRegistrationsLock };
    abortSignalRegistrations().set(this, AbortSignalRegistration { signal, abortAlgorithmIdentifier });
}

// Without this, the abort algorithm would stay with the signal until it aborts, and keep a signal returned by
// AbortSignal.any() alive while its source signals are.
void RegisteredEventListener::removeAbortAlgorithm(const EventTarget* ownerBeingDestroyed)
{
    ASSERT(m_hasAbortSignal);
    m_hasAbortSignal = false;
    AbortSignalRegistration registration;
    {
        Locker locker { abortSignalRegistrationsLock };
        registration = abortSignalRegistrations().take(this);
    }

    // A listener registered on its own signal is torn down after that signal's abort algorithms were destroyed.
    if (ownerBeingDestroyed && static_cast<const EventTarget*>(registration.signal.get()) == ownerBeingDestroyed)
        return;

    if (RefPtr signal = registration.signal)
        signal->removeAlgorithm(registration.abortAlgorithmIdentifier);
}

EventListenerMap::EventListenerMap() = default;

bool EventListenerMap::containsCapturing(const AtomString& eventType) const
{
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

    for (auto& entry : m_entries) {
        for (auto& listener : entry.second)
            listener->markAsRemoved();
    }

    m_entries.clear();
}

void EventListenerMap::clearEntriesForTearDown(const EventTarget& owner)
{
    releaseAssertOrSetThreadUID();

    for (auto& entry : m_entries) {
        for (auto& listener : entry.second) {
            if (listener->hasAbortSignal()) [[unlikely]]
                listener->removeAbortAlgorithm(&owner);
        }
    }

    m_entries.clear();
}

Vector<AtomString> EventListenerMap::eventTypes() const
{
    return m_entries.map([](auto& entry) {
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

    auto* listeners = find(eventType);
    ASSERT(listeners);
    size_t index = findListener(*listeners, oldListener, useCapture);
    ASSERT(index != notFound);
    auto& registeredListener = listeners->at(index);
    auto existingOptions = registeredListener->options();
    registeredListener->markAsRemoved();
    registeredListener = RegisteredEventListener::create(WTF::move(newListener), existingOptions);
}

RefPtr<RegisteredEventListener> EventListenerMap::add(const AtomString& eventType, Ref<EventListener>&& listener, const RegisteredEventListener::Options& options)
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    if (auto* listeners = find(eventType)) {
        if (findListener(*listeners, listener, options.capture) != notFound)
            return nullptr; // Duplicate listener.
        listeners->append(RegisteredEventListener::create(WTF::move(listener), options));
        return listeners->last().copyRef();
    }

    m_entries.append({ eventType, EventListenerVector { RegisteredEventListener::create(WTF::move(listener), options) } });
    return m_entries.last().second.last().copyRef();
}

static bool removeListenerFromVector(EventListenerVector& listeners, EventListener& listener, bool useCapture)
{
    size_t indexOfRemovedListener = findListener(listeners, listener, useCapture);
    if (indexOfRemovedListener == notFound) [[unlikely]]
        return false;

    protect(listeners[indexOfRemovedListener])->markAsRemoved();
    listeners.removeAt(indexOfRemovedListener);
    return true;
}

bool EventListenerMap::remove(const AtomString& eventType, EventListener& listener, bool useCapture)
{
    releaseAssertOrSetThreadUID();
    Locker locker { m_lock };

    for (unsigned i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].first == eventType) {
            bool wasRemoved = removeListenerFromVector(m_entries[i].second, listener, useCapture);
            if (m_entries[i].second.isEmpty())
                m_entries.removeAt(i);
            return wasRemoved;
        }
    }

    return false;
}

EventListenerVector* EventListenerMap::find(const AtomString& eventType)
{
    for (auto& entry : m_entries) {
        if (entry.first == eventType)
            return &entry.second;
    }

    return nullptr;
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

    for (unsigned i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].first == eventType) {
            removeFirstListenerCreatedFromMarkup(m_entries[i].second);
            if (m_entries[i].second.isEmpty())
                m_entries.removeAt(i);
            return;
        }
    }
}

static void copyListenersNotCreatedFromMarkupToTarget(const AtomString& eventType, EventListenerVector& listenerVector, EventTarget* target)
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
    for (auto& entry : m_entries)
        copyListenersNotCreatedFromMarkupToTarget(entry.first, entry.second, target);
}

} // namespace WebCore
