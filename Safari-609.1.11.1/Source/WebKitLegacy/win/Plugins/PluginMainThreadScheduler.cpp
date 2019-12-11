/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "PluginMainThreadScheduler.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

PluginMainThreadScheduler& PluginMainThreadScheduler::scheduler()
{
    static NeverDestroyed<PluginMainThreadScheduler> scheduler;
    return scheduler;
}

PluginMainThreadScheduler::PluginMainThreadScheduler()
    : m_callPending(false)
{
}

void PluginMainThreadScheduler::scheduleCall(NPP npp, MainThreadFunction function, void* userData)
{
    LockHolder lock(m_queueMutex);

    CallQueueMap::iterator it = m_callQueueMap.find(npp);
    if (it == m_callQueueMap.end())
        return;

    it->value.append(Call(function, userData));

    if (!m_callPending) {
        callOnMainThread([this] {
            dispatchCalls();
        });
        m_callPending = true;
    }
}

void PluginMainThreadScheduler::registerPlugin(NPP npp)
{
    LockHolder lock(m_queueMutex);

    ASSERT(!m_callQueueMap.contains(npp));
    m_callQueueMap.set(npp, Deque<Call>());
}

void PluginMainThreadScheduler::unregisterPlugin(NPP npp)
{
    LockHolder lock(m_queueMutex);

    ASSERT(m_callQueueMap.contains(npp));
    m_callQueueMap.remove(npp);
}

void PluginMainThreadScheduler::dispatchCallsForPlugin(NPP npp, const Deque<Call>& calls)
{
    for (auto& call : calls) {
        // Check if the plug-in has been destroyed.
        {
            LockHolder lock(m_queueMutex);
            if (!m_callQueueMap.contains(npp))
                return;
        }

        call.performCall();
    }
}

void PluginMainThreadScheduler::dispatchCalls()
{
    m_queueMutex.lock();
    CallQueueMap copy(m_callQueueMap);

    // Empty all the queues in the original map
    for (auto& call : m_callQueueMap.values())
        call.clear();

    m_callPending = false;
    m_queueMutex.unlock();

    for (auto& entry : copy)
        dispatchCallsForPlugin(entry.key, entry.value);
}

} // namespace WebCore
