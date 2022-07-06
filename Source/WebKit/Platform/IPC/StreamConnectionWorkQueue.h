/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "IPCSemaphore.h"
#include "StreamServerConnection.h"
#include <atomic>
#include <wtf/Deque.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/HashCountedSet.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace IPC {

class WTF_CAPABILITY("is current") StreamConnectionWorkQueue final : public FunctionDispatcher, public ThreadSafeRefCounted<StreamConnectionWorkQueue> {
public:
    static Ref<StreamConnectionWorkQueue> create(const char* name)
    {
        return adoptRef(*new StreamConnectionWorkQueue(name));
    }

    StreamConnectionWorkQueue(const char*);
    ~StreamConnectionWorkQueue();
    void addStreamConnection(StreamServerConnection&);
    void removeStreamConnection(StreamServerConnection&);

    void dispatch(WTF::Function<void()>&&) final;
    void stopAndWaitForCompletion(WTF::Function<void()>&& cleanupFunction = nullptr);

    void wakeUp();

    Semaphore& wakeUpSemaphore();
private:
    void startProcessingThread() WTF_REQUIRES_LOCK(m_lock);
    void processStreams();
#if ASSERT_ENABLED
    void assertIsCurrent() const;
#endif

    const char* const m_name;

    Semaphore m_wakeUpSemaphore;
    std::atomic<bool> m_shouldQuit { false };

    mutable Lock m_lock;
    RefPtr<Thread> m_processingThread WTF_GUARDED_BY_LOCK(m_lock);
    Deque<Function<void()>> m_functions WTF_GUARDED_BY_LOCK(m_lock);
    WTF::Function<void()> m_cleanupFunction WTF_GUARDED_BY_LOCK(m_lock);
    HashCountedSet<Ref<StreamServerConnection>> m_connections WTF_GUARDED_BY_LOCK(m_lock);
    friend void assertIsCurrent(const StreamConnectionWorkQueue&);
};

inline void assertIsCurrent(const StreamConnectionWorkQueue& queue) WTF_ASSERTS_ACQUIRED_CAPABILITY(queue)
{
#if ASSERT_ENABLED
    queue.assertIsCurrent();
#endif
}

}
