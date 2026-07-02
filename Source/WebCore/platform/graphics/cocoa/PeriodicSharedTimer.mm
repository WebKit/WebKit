/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PeriodicSharedTimer.h"

#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PeriodicSharedTimer);

PeriodicSharedTimer::PeriodicSharedTimer(Seconds interval)
    : m_queue(adoptOSObject(dispatch_queue_create("WebCore PeriodicSharedTimer", DISPATCH_QUEUE_SERIAL)))
    , m_timer(adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, m_queue.get())))
{
    auto intervalNs = interval.nanosecondsAs<uint64_t>();
    dispatch_source_set_timer(m_timer.get(), DISPATCH_TIME_NOW, intervalNs, intervalNs / 10);
    dispatch_source_set_event_handler(m_timer.get(), makeBlockPtr([this] {
        timerFired();
    }).get());
    // Source starts inactive; addClient() resumes it on the 0 -> 1 transition.
}

PeriodicSharedTimer::~PeriodicSharedTimer()
{
    // Per <dispatch/source.h>: "Releasing the last reference count on an inactive
    // object is undefined." Resume before cancelling so the cancel + release run
    // against an active source.
    if (!m_active)
        dispatch_resume(m_timer.get());
    dispatch_source_cancel(m_timer.get());
}

void PeriodicSharedTimer::timerFired()
{
    Vector<Ref<Client>> clients;
    {
        Locker locker { m_activeLock };
        clients = m_clients.values();
    }
    for (auto& client : clients)
        client->periodicSharedTimerFired();
}

void PeriodicSharedTimer::addClient(Client& client)
{
    Locker locker { m_activeLock };
    m_clients.add(client);
    if (m_active)
        return;
    m_active = true;
    dispatch_resume(m_timer.get());
}

void PeriodicSharedTimer::removeClient(Client& client)
{
    Locker locker { m_activeLock };
    m_clients.remove(client);
    if (!m_clients.isEmptyIgnoringNullReferences())
        return;
    if (!m_active)
        return;
    m_active = false;
    dispatch_suspend(m_timer.get());
}

} // namespace WebCore
