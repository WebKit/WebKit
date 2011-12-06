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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "EventException.h"
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {

EventListenerMap::EventListenerMap()
#ifndef NDEBUG
    : m_activeIteratorCount(0)
#endif
{
}

EventListenerMap::~EventListenerMap()
{
    clear();
}

bool EventListenerMap::isEmpty() const
{
    if (m_hashMap)
        return m_hashMap->isEmpty();
    return !m_singleEventListenerVector;
}

bool EventListenerMap::contains(const AtomicString& eventType) const
{
    if (m_hashMap)
        return m_hashMap->contains(eventType);
    return m_singleEventListenerType == eventType;
}

void EventListenerMap::clear()
{
    ASSERT(!m_activeIteratorCount);

    if (m_hashMap)
        m_hashMap.clear();
    else {
        m_singleEventListenerType = nullAtom;
        m_singleEventListenerVector.clear();
    }
}

Vector<AtomicString> EventListenerMap::eventTypes() const
{
    Vector<AtomicString> types;

    if (m_hashMap) {
        EventListenerHashMap::iterator it = m_hashMap->begin();
        EventListenerHashMap::iterator end = m_hashMap->end();
        for (; it != end; ++it)
            types.append(it->first);
    } else if (m_singleEventListenerVector)
        types.append(m_singleEventListenerType);

    return types;
}

static bool addListenerToVector(EventListenerVector* vector, PassRefPtr<EventListener> listener, bool useCapture)
{
    RegisteredEventListener registeredListener(listener, useCapture);

    if (vector->find(registeredListener) != notFound)
        return false; // Duplicate listener.

    vector->append(registeredListener);
    return true;
}

bool EventListenerMap::add(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    ASSERT(!m_activeIteratorCount);

    if (m_singleEventListenerVector && m_singleEventListenerType != eventType) {
        // We already have a single (first) listener vector, and this event is not
        // of that type, so create the hash map and move the first listener vector there.
        ASSERT(!m_hashMap);
        m_hashMap = adoptPtr(new EventListenerHashMap);
        m_hashMap->add(m_singleEventListenerType, m_singleEventListenerVector.release());
        m_singleEventListenerType = nullAtom;
    }

    if (m_hashMap) {
        pair<EventListenerHashMap::iterator, bool> result = m_hashMap->add(eventType, nullptr);
        if (result.second)
            result.first->second = adoptPtr(new EventListenerVector);

        return addListenerToVector(result.first->second.get(), listener, useCapture);
    }

    if (!m_singleEventListenerVector) {
        m_singleEventListenerType = eventType;
        m_singleEventListenerVector = adoptPtr(new EventListenerVector);
    }

    ASSERT(m_singleEventListenerType == eventType);
    return addListenerToVector(m_singleEventListenerVector.get(), listener, useCapture);
}

static bool removeListenerFromVector(EventListenerVector* listenerVector, EventListener* listener, bool useCapture, size_t& indexOfRemovedListener)
{
    RegisteredEventListener registeredListener(listener, useCapture);
    indexOfRemovedListener = listenerVector->find(registeredListener);
    if (indexOfRemovedListener == notFound)
        return false;
    listenerVector->remove(indexOfRemovedListener);
    return true;
}

bool EventListenerMap::remove(const AtomicString& eventType, EventListener* listener, bool useCapture, size_t& indexOfRemovedListener)
{
    ASSERT(!m_activeIteratorCount);

    if (!m_hashMap) {
        if (m_singleEventListenerType != eventType)
            return false;
        bool wasRemoved = removeListenerFromVector(m_singleEventListenerVector.get(), listener, useCapture, indexOfRemovedListener);
        if (m_singleEventListenerVector->isEmpty()) {
            m_singleEventListenerVector.clear();
            m_singleEventListenerType = nullAtom;
        }
        return wasRemoved;
    }

    EventListenerHashMap::iterator it = m_hashMap->find(eventType);
    if (it == m_hashMap->end())
        return false;

    bool wasRemoved = removeListenerFromVector(it->second.get(), listener, useCapture, indexOfRemovedListener);
    if (it->second->isEmpty())
        m_hashMap->remove(it);
    return wasRemoved;
}

EventListenerVector* EventListenerMap::find(const AtomicString& eventType)
{
    ASSERT(!m_activeIteratorCount);

    if (m_hashMap) {
        EventListenerHashMap::iterator it = m_hashMap->find(eventType);
        if (it == m_hashMap->end())
            return 0;
        return it->second.get();
    }

    if (m_singleEventListenerType == eventType)
        return m_singleEventListenerVector.get();

    return 0;
}

#if ENABLE(SVG)

static void removeFirstListenerCreatedFromMarkup(EventListenerVector* listenerVector)
{
    bool foundListener = false;

    for (size_t i = 0; i < listenerVector->size(); ++i) {
        if (!listenerVector->at(i).listener->wasCreatedFromMarkup())
            continue;
        foundListener = true;
        listenerVector->remove(i);
        break;
    }

    ASSERT_UNUSED(foundListener, foundListener);
}

void EventListenerMap::removeFirstEventListenerCreatedFromMarkup(const AtomicString& eventType)
{
    ASSERT(!m_activeIteratorCount);

    if (m_hashMap) {
        EventListenerHashMap::iterator result = m_hashMap->find(eventType);
        ASSERT(result != m_hashMap->end());

        EventListenerVector* listenerVector = result->second.get();
        ASSERT(listenerVector);

        removeFirstListenerCreatedFromMarkup(listenerVector);

        if (listenerVector->isEmpty())
            m_hashMap->remove(result);

        return;
    }

    ASSERT(m_singleEventListenerVector);
    ASSERT(m_singleEventListenerType == eventType);

    removeFirstListenerCreatedFromMarkup(m_singleEventListenerVector.get());
    if (m_singleEventListenerVector->isEmpty()) {
        m_singleEventListenerVector.clear();
        m_singleEventListenerType = nullAtom;
    }
}

static void copyListenersNotCreatedFromMarkupToTarget(const AtomicString& eventType, EventListenerVector* listenerVector, EventTarget* target)
{
    for (size_t i = 0; i < listenerVector->size(); ++i) {
        // Event listeners created from markup have already been transfered to the shadow tree during cloning.
        if ((*listenerVector)[i].listener->wasCreatedFromMarkup())
            continue;
        target->addEventListener(eventType, (*listenerVector)[i].listener, (*listenerVector)[i].useCapture);
    }
}

void EventListenerMap::copyEventListenersNotCreatedFromMarkupToTarget(EventTarget* target)
{
    ASSERT(!m_activeIteratorCount);

    if (m_hashMap) {
        EventListenerHashMap::iterator end = m_hashMap->end();
        for (EventListenerHashMap::iterator it = m_hashMap->begin(); it != end; ++it)
            copyListenersNotCreatedFromMarkupToTarget(it->first, it->second.get(), target);
        return;
    }

    if (!m_singleEventListenerVector)
        return;

    copyListenersNotCreatedFromMarkupToTarget(m_singleEventListenerType, m_singleEventListenerVector.get(), target);
}

#endif // ENABLE(SVG)

EventListenerIterator::EventListenerIterator()
    : m_map(0)
    , m_index(0)
{
}

EventListenerIterator::EventListenerIterator(EventTarget* target)
    : m_map(0)
    , m_index(0)
{
    ASSERT(target);
    EventTargetData* data = target->eventTargetData();

    if (!data)
        return;

    m_map = &data->eventListenerMap;

#ifndef NDEBUG
    m_map->m_activeIteratorCount++;
#endif

    if (m_map->m_hashMap) {
        m_mapIterator = m_map->m_hashMap->begin();
        m_mapEnd = m_map->m_hashMap->end();
    }
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
        return 0;

    if (m_map->m_hashMap) {
        for (; m_mapIterator != m_mapEnd; ++m_mapIterator) {
            EventListenerVector& listeners = *m_mapIterator->second;
            if (m_index < listeners.size())
                return listeners[m_index++].listener.get();
            m_index = 0;
        }
        return 0;
    }

    if (!m_map->m_singleEventListenerVector)
        return 0;
    EventListenerVector& listeners = *m_map->m_singleEventListenerVector;
    if (m_index < listeners.size())
        return listeners[m_index++].listener.get();
    return 0;
}

} // namespace WebCore
