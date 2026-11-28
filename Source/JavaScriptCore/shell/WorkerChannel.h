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

#include "DeferredWorkTimer.h"
#include "Message.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {
class JSObject;
class VM;
}

namespace JSCShell {

class WorkerChannel : public ThreadSafeRefCounted<WorkerChannel> {
public:
    enum class Side : uint8_t { Parent, Worker };

    static Ref<WorkerChannel> create() { return adoptRef(*new WorkerChannel); }

    static WorkerChannel* currentWorkerSide();

    // open() and close() must be called with the respective side and not from other threads.
    void open(Side, JSC::VM&, JSC::JSObject* target);
    void close(Side);

    // If no onmessage handler is installed on the Worker it can't receive any further messages.
    void closeWorkerIfItCannotReceive(JSC::JSGlobalObject*);
    void post(Side from, Ref<Message>&&);
    void terminate();
    void workerDidExit();

private:
    // Messages can be delivered to a Worker before it's Thread/RunLoop are attached. They'll be
    // delivered once the Worker is initialized. However, once the port is closed then any
    // messages are discarded.
    enum class PortState : uint8_t { NotStarted, Open, Closed };

    struct Port {
        RefPtr<RunLoop> runLoop;
        JSC::VM* vm { nullptr };
        // This ticket keeps the respective side's DeferredWorkTimer active while the channel is open.
        JSC::DeferredWorkTimer::WeakTicket keepAlive;
        PortState state { PortState::NotStarted };
        Deque<Ref<Message>> messages;
    };

    Port& port(Side side) WTF_REQUIRES_LOCK(m_lock) { return side == Side::Parent ? m_parent : m_worker; }
    void scheduleDelivery(Side to) WTF_REQUIRES_LOCK(m_lock);
    void terminateWorkerIfStarted() WTF_REQUIRES_LOCK(m_lock);
    void deliverNext(Side to);
    void stopWorker();

    Lock m_lock;
    Port m_parent WTF_GUARDED_BY_LOCK(m_lock);
    Port m_worker WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isTerminated WTF_GUARDED_BY_LOCK(m_lock) { false };
};

} // namespace JSCShell
