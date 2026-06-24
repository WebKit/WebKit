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

#pragma once

#include <dispatch/dispatch.h>
#include <wtf/AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h>
#include <wtf/Lock.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakHashSet.h>

namespace WebCore {

// Periodic dispatch timer that broadcasts ticks to multiple clients. A single
// dispatch source serves every registered client; the source is suspended
// whenever the client set is empty, so an idle instance pays no wakeups.
class PeriodicSharedTimer final {
    WTF_MAKE_NONCOPYABLE(PeriodicSharedTimer);
    WTF_MAKE_TZONE_ALLOCATED(PeriodicSharedTimer);
public:
    class Client : public AbstractThreadSafeRefCountedAndCanMakeWeakPtr {
    public:
        virtual ~Client() = default;
        // Invoked on the PeriodicSharedTimer's serial queue. Implementations
        // should keep the body short; while running, the client is kept alive
        // by a strong reference held by the iteration snapshot.
        virtual void periodicSharedTimerFired() = 0;
    };

    explicit PeriodicSharedTimer(Seconds interval);
    ~PeriodicSharedTimer();

    void addClient(Client&);
    void removeClient(Client&);

private:
    void timerFired();

    Lock m_activeLock;
    bool m_active WTF_GUARDED_BY_LOCK(m_activeLock) { false };
    ThreadSafeWeakHashSet<Client> m_clients WTF_GUARDED_BY_LOCK(m_activeLock);
    OSObjectPtr<dispatch_queue_t> m_queue;
    OSObjectPtr<dispatch_source_t> m_timer;
};

} // namespace WebCore
