/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO)
#include "WorkerRunLoop.h"
#include "WorkletParameters.h"
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class AudioWorkletGlobalScope;

class AudioWorkletThread : public ThreadSafeRefCounted<AudioWorkletThread> {
public:
    static Ref<AudioWorkletThread> create(const WorkletParameters& parameters)
    {
        return adoptRef(*new AudioWorkletThread(parameters));
    }

    void start();

    WorkerRunLoop& runLoop() { return m_runLoop; }
    Thread* thread() const { return m_thread.get(); }

private:
    explicit AudioWorkletThread(const WorkletParameters&);

    void runEventLoop();
    void workletThread();

    RefPtr<Thread> m_thread;
    WorkerRunLoop m_runLoop;
    WorkletParameters m_parameters;
    RefPtr<AudioWorkletGlobalScope> m_workletGlobalScope;
    Lock m_threadCreationAndWorkletGlobalScopeLock;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
