/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceBufferList.h"

#if ENABLE(MEDIA_SOURCE)

#include "Event.h"
#include "SourceBuffer.h"

namespace WebCore {

SourceBufferList::SourceBufferList(ScriptExecutionContext* context)
    : m_scriptExecutionContext(context)
    , m_asyncEventQueue(*this)
{
}

SourceBufferList::~SourceBufferList()
{
    ASSERT(m_list.isEmpty());
}

void SourceBufferList::add(PassRefPtr<SourceBuffer> buffer)
{
    m_list.append(buffer);
    scheduleEvent(eventNames().addsourcebufferEvent);
}

void SourceBufferList::remove(SourceBuffer* buffer)
{
    size_t index = m_list.find(buffer);
    if (index == notFound)
        return;
    m_list.remove(index);
    scheduleEvent(eventNames().removesourcebufferEvent);
}

void SourceBufferList::clear()
{
    m_list.clear();
    scheduleEvent(eventNames().removesourcebufferEvent);
}

void SourceBufferList::swap(Vector<RefPtr<SourceBuffer>>& other)
{
    int changeInSize = other.size() - m_list.size();
    int addedEntries = 0;
    for (auto& sourceBuffer : other) {
        if (!m_list.contains(sourceBuffer))
            ++addedEntries;
    }
    int removedEntries = addedEntries - changeInSize;

    m_list.swap(other);

    if (addedEntries)
        scheduleEvent(eventNames().addsourcebufferEvent);
    if (removedEntries)
        scheduleEvent(eventNames().removesourcebufferEvent);
}

void SourceBufferList::scheduleEvent(const AtomicString& eventName)
{
    RefPtr<Event> event = Event::create(eventName, false, false);
    event->setTarget(this);

    m_asyncEventQueue.enqueueEvent(event.release());
}


} // namespace WebCore

#endif
