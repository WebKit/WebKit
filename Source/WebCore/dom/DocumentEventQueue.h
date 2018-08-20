/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#pragma once

#include "Event.h"
#include "EventQueue.h"
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>

namespace WebCore {

class Document;
class Node;

class DocumentEventQueue final : public EventQueue {
public:
    explicit DocumentEventQueue(Document&);
    virtual ~DocumentEventQueue();

    bool enqueueEvent(Ref<Event>&&) override;
    bool cancelEvent(Event&) override;
    void close() override;

    void enqueueOrDispatchScrollEvent(Node&);
    void enqueueScrollEvent(EventTarget&, Event::CanBubble, Event::IsCancelable);
    void enqueueResizeEvent(EventTarget&, Event::CanBubble, Event::IsCancelable);

private:
    void pendingEventTimerFired();
    void dispatchEvent(Event&);

    class Timer;

    Document& m_document;
    std::unique_ptr<Timer> m_pendingEventTimer;
    ListHashSet<RefPtr<Event>> m_queuedEvents;
    HashSet<EventTarget*> m_targetsWithQueuedScrollEvents;
    HashSet<EventTarget*> m_targetsWithQueuedResizeEvents;
    bool m_isClosed;
};

} // namespace WebCore
