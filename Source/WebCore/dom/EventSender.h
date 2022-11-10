/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Timer.h"
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Page;
class WeakPtrImplWithEventTargetData;

template<typename T, typename WeakPtrImpl> class EventSender {
    WTF_MAKE_NONCOPYABLE(EventSender); WTF_MAKE_FAST_ALLOCATED;
public:
    EventSender();

    void dispatchEventSoon(T&, const AtomString& eventType);
    void cancelEvent(T&);
    void cancelEvent(T&, const AtomString& eventType);
    void dispatchPendingEvents(Page*);

#if ASSERT_ENABLED
    bool hasPendingEvents(T& sender) const
    {
        return m_dispatchSoonList.containsIf([&](auto& task) { return task.sender == &sender; })
            || m_dispatchingList.containsIf([&](auto& task) { return task.sender == &sender; });
    }
#endif

private:
    void timerFired() { dispatchPendingEvents(nullptr); }

    Timer m_timer;
    struct DispatchTask {
        WeakPtr<T, WeakPtrImpl> sender;
        AtomString eventType;
    };
    Vector<DispatchTask> m_dispatchSoonList;
    Vector<DispatchTask> m_dispatchingList;
};

template<typename T, typename WeakPtrImpl> EventSender<T, WeakPtrImpl>::EventSender()
    : m_timer(*this, &EventSender::timerFired)
{
}

template<typename T, typename WeakPtrImpl> void EventSender<T, WeakPtrImpl>::dispatchEventSoon(T& sender, const AtomString& eventType)
{
    m_dispatchSoonList.append(DispatchTask { sender, eventType });
    if (!m_timer.isActive())
        m_timer.startOneShot(0_s);
}

template<typename T, typename WeakPtrImpl> void EventSender<T, WeakPtrImpl>::cancelEvent(T& senderToCancel)
{
    // Remove instances of this sender from both lists.
    // Use loops because we allow multiple instances to get into the lists.
    for (auto& [sender, eventType] : m_dispatchSoonList) {
        if (sender == &senderToCancel)
            sender = nullptr;
    }
    for (auto& [sender, eventType] : m_dispatchingList) {
        if (sender == &senderToCancel)
            sender = nullptr;
    }
}

template<typename T, typename WeakPtrImpl> void EventSender<T, WeakPtrImpl>::cancelEvent(T& senderToCancel, const AtomString& eventTypeToCancel)
{
    // Remove instances of this sender from both lists.
    // Use loops because we allow multiple instances to get into the lists.
    for (auto& [sender, eventType] : m_dispatchSoonList) {
        if (sender == &senderToCancel && eventType == eventTypeToCancel)
            sender = nullptr;
    }
    for (auto& [sender, eventType] : m_dispatchingList) {
        if (sender == &senderToCancel && eventType == eventTypeToCancel)
            sender = nullptr;
    }
}

template<typename T, typename WeakPtrImpl> void EventSender<T, WeakPtrImpl>::dispatchPendingEvents(Page* page)
{
    // Need to avoid re-entering this function; if new dispatches are
    // scheduled before the parent finishes processing the list, they
    // will set a timer and eventually be processed.
    if (!m_dispatchingList.isEmpty())
        return;

    m_timer.stop();

    m_dispatchSoonList.checkConsistency();

    m_dispatchingList = std::exchange(m_dispatchSoonList, { });
    for (auto& [weakSender, eventType] : m_dispatchingList) {
        if (!weakSender)
            continue;
        auto sender = std::exchange(weakSender, nullptr);
        if (!page || sender->document().page() == page)
            sender->dispatchPendingEvent(this, eventType);
        else
            dispatchEventSoon(*sender, eventType);
    }
    m_dispatchingList.clear();
}

} // namespace WebCore
