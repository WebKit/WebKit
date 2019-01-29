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

#include "Event.h"
#include "EventTarget.h"
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>


namespace WebCore {

#ifndef NDEBUG
void EventListenerMap::assertNoActiveIterators() const
{
    ASSERT(!m_activeIteratorCount);
}
#endif

EventListenerMap::EventListenerMap()
{
}

bool EventListenerMap::containsCapturing(const AtomicString& eventType) const
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

bool EventListenerMap::containsActive(const AtomicString& eventType) const
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
    auto locker = holdLock(m_lock);
    
    assertNoActiveIterators();

    for (auto& entry : m_entries) {
        for (auto& listener : *entry.second)
            listener->markAsRemoved();
    }

    m_entries.clear();
}

Vector<AtomicString> EventListenerMap::eventTypes() const
{
    Vector<AtomicString> types;
    types.reserveInitialCapacity(m_entries.size());

    for (auto& entry : m_entries)
        types.uncheckedAppend(entry.first);

    return types;
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

void EventListenerMap::replace(const AtomicString& eventType, EventListener& oldListener, Ref<EventListener>&& newListener, const RegisteredEventListener::Options& options)
{
    auto locker = holdLock(m_lock);
    
    assertNoActiveIterators();

    auto* listeners = find(eventType);
    ASSERT(listeners);
    size_t index = findListener(*listeners, oldListener, options.capture);
    ASSERT(index != notFound);
    auto& registeredListener = listeners->at(index);
    registeredListener->markAsRemoved();
    registeredListener = RegisteredEventListener::create(WTFMove(newListener), options);
}

bool EventListenerMap::add(const AtomicString& eventType, Ref<EventListener>&& listener, const RegisteredEventListener::Options& options)
{
    auto locker = holdLock(m_lock);
    
    assertNoActiveIterators();

    if (auto* listeners = find(eventType)) {
        if (findListener(*listeners, listener, options.capture) != notFound)
            return false; // Duplicate listener.
        listeners->append(RegisteredEventListener::create(WTFMove(listener), options));
        return true;
    }

    auto listeners = std::make_unique<EventListenerVector>();
    listeners->uncheckedAppend(RegisteredEventListener::create(WTFMove(listener), options));
    m_entries.append({ eventType, WTFMove(listeners) });
    return true;
}

static bool removeListenerFromVector(EventListenerVector& listeners, EventListener& listener, bool useCapture)
{
    size_t indexOfRemovedListener = findListener(listeners, listener, useCapture);
    if (UNLIKELY(indexOfRemovedListener == notFound))
        return false;

    listeners[indexOfRemovedListener]->markAsRemoved();
    listeners.remove(indexOfRemovedListener);
    return true;
}

bool EventListenerMap::remove(const AtomicString& eventType, EventListener& listener, bool useCapture)
{
    auto locker = holdLock(m_lock);
    
    assertNoActiveIterators();

    for (unsigned i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].first == eventType) {
            bool wasRemoved = removeListenerFromVector(*m_entries[i].second, listener, useCapture);
            if (m_entries[i].second->isEmpty())
                m_entries.remove(i);
            return wasRemoved;
        }
    }

    return false;
}

EventListenerVector* EventListenerMap::find(const AtomicString& eventType) const
{
    for (auto& entry : m_entries) {
        if (entry.first == eventType)
            return entry.second.get();
    }

    return nullptr;
}

static void removeFirstListenerCreatedFromMarkup(EventListenerVector& listenerVector)
{
    bool foundListener = listenerVector.removeFirstMatching([] (const auto& registeredListener) {
        if (registeredListener->callback().wasCreatedFromMarkup()) {
            registeredListener->markAsRemoved();
            return true;
        }
        return false;
    });
    ASSERT_UNUSED(foundListener, foundListener);
}

void EventListenerMap::removeFirstEventListenerCreatedFromMarkup(const AtomicString& eventType)
{
    auto locker = holdLock(m_lock);
    
    assertNoActiveIterators();

    for (unsigned i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].first == eventType) {
            removeFirstListenerCreatedFromMarkup(*m_entries[i].second);
            if (m_entries[i].second->isEmpty())
                m_entries.remove(i);
            return;
        }
    }
}

static void copyListenersNotCreatedFromMarkupToTarget(const AtomicString& eventType, EventListenerVector& listenerVector, EventTarget* target)
{
    for (auto& registeredListener : listenerVector) {
        // Event listeners created from markup have already been transfered to the shadow tree during cloning.
        if (registeredListener->callback().wasCreatedFromMarkup())
            continue;
        target->addEventListener(eventType, registeredListener->callback(), registeredListener->useCapture());
    }
}

void EventListenerMap::copyEventListenersNotCreatedFromMarkupToTarget(EventTarget* target)
{
    for (auto& entry : m_entries)
        copyListenersNotCreatedFromMarkupToTarget(entry.first, *entry.second, target);
}

EventListenerIterator::EventListenerIterator(EventTarget* target)
{
    ASSERT(target);
    EventTargetData* data = target->eventTargetData();

    if (!data)
        return;

    m_map = &data->eventListenerMap;

#ifndef NDEBUG
    m_map->m_activeIteratorCount++;
#endif
}

EventListenerIterator::EventListenerIterator(EventListenerMap* map)
{
    m_map = map;

#ifndef NDEBUG
    m_map->m_activeIteratorCount++;
#endif
}

#ifndef NDEBUG
EventListenerIterator::~EventListenerIterator()
{
    if (m_map)
        m_map->m_activeIteratorCount--;
}
#endif

EventListener* EventListenerIterator::nextListener()
{
    if (!m_map)
        return nullptr;

    for (; m_entryIndex < m_map->m_entries.size(); ++m_entryIndex) {
        EventListenerVector& listeners = *m_map->m_entries[m_entryIndex].second;
        if (m_index < listeners.size())
            return &listeners[m_index++]->callback();
        m_index = 0;
    }

    return nullptr;
}

} // namespace WebCore
