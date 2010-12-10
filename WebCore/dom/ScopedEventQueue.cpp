/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "ScopedEventQueue.h"

#include "Event.h"
#include "EventTarget.h"

namespace WebCore {

ScopedEventQueue* ScopedEventQueue::s_instance = 0;

ScopedEventQueue::ScopedEventQueue()
    : m_scopingLevel(0)
{
}

ScopedEventQueue::~ScopedEventQueue()
{
    ASSERT(!m_scopingLevel);
    ASSERT(!m_queuedEvents.size());
}

void ScopedEventQueue::initialize()
{
    ASSERT(!s_instance);
    OwnPtr<ScopedEventQueue> instance = adoptPtr(new ScopedEventQueue);
    s_instance = instance.leakPtr();
}

void ScopedEventQueue::enqueueEvent(PassRefPtr<Event> event)
{
    if (m_scopingLevel)
        m_queuedEvents.append(event);
    else
        dispatchEvent(event);
}

void ScopedEventQueue::dispatchAllEvents()
{
    Vector<RefPtr<Event> > queuedEvents;
    queuedEvents.swap(m_queuedEvents);

    for (size_t i = 0; i < queuedEvents.size(); i++)
        dispatchEvent(queuedEvents[i].release());
}

void ScopedEventQueue::dispatchEvent(PassRefPtr<Event> event) const
{
    RefPtr<EventTarget> eventTarget = event->target();
    eventTarget->dispatchEvent(event);
}

ScopedEventQueue* ScopedEventQueue::instance()
{
    if (!s_instance)
        initialize();

    return s_instance;
}

void ScopedEventQueue::incrementScopingLevel()
{
    m_scopingLevel++;
}

void ScopedEventQueue::decrementScopingLevel()
{
    ASSERT(m_scopingLevel);
    m_scopingLevel--;
    if (!m_scopingLevel)
        dispatchAllEvents();
}

}
