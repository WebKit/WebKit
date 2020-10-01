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

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "AudioWorkletThread.h"

#include "AudioWorkletGlobalScope.h"

#if PLATFORM(IOS_FAMILY)
#include "FloatingPointEnvironment.h"
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

AudioWorkletThread::AudioWorkletThread(const WorkletParameters& parameters)
    : m_parameters(parameters.isolatedCopy())
{
}

void AudioWorkletThread::start()
{
    auto lock = holdLock(m_threadCreationAndWorkletGlobalScopeLock);

    Ref<Thread> thread = Thread::create("WebCore: AudioWorklet", [this] {
        workletThread();
    }, ThreadType::Audio);
    // Force the Thread object to be initialized fully before storing it to m_thread (and becoming visible to other threads).
    WTF::storeStoreFence();
    m_thread = WTFMove(thread);
}

void AudioWorkletThread::workletThread()
{
    auto protectedThis = makeRef(*this);

    // Propagate the mainThread's fenv to workers.
#if PLATFORM(IOS_FAMILY)
    FloatingPointEnvironment::singleton().propagateMainThreadEnvironment();
#endif

#if USE(GLIB)
    GRefPtr<GMainContext> mainContext = adoptGRef(g_main_context_new());
    g_main_context_push_thread_default(mainContext.get());
#endif

    WorkletScriptController* scriptController;
    {
        auto lock = holdLock(m_threadCreationAndWorkletGlobalScopeLock);
        m_workletGlobalScope = AudioWorkletGlobalScope::create(*this, m_parameters);

        scriptController = m_workletGlobalScope->script();

        if (m_runLoop.terminated()) {
            // The worker was terminated before the thread had a chance to run. Since the context didn't exist yet,
            // forbidExecution() couldn't be called from stop().
            scriptController->scheduleExecutionTermination();
            scriptController->forbidExecution();
        }
    }

    runEventLoop();

#if USE(GLIB)
    g_main_context_pop_thread_default(mainContext.get());
#endif

    RefPtr<Thread> protector = m_thread;

    ASSERT(m_workletGlobalScope->hasOneRef());

    RefPtr<AudioWorkletGlobalScope> workletGlobalScopeToDelete;
    {
        // Mutex protection is necessary to ensure that we don't change m_workerGlobalScope
        // while WorkerThread::stop is accessing it.
        auto lock = holdLock(m_threadCreationAndWorkletGlobalScopeLock);

        // Delay the destruction of the WorkerGlobalScope context until after we've unlocked the
        // m_threadCreationAndWorkerGlobalScopeMutex. This is needed because destructing the
        // context will trigger the main thread to race against us to delete the WorkerThread
        // object, and the WorkerThread object owns the mutex we need to unlock after this.
        workletGlobalScopeToDelete = WTFMove(m_workletGlobalScope);
    }

    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    workletGlobalScopeToDelete = nullptr;

    // Send the last WorkerThread Ref to be Deref'ed on the main thread.
    callOnMainThread([protectedThis = WTFMove(protectedThis)] { });

    // The thread object may be already destroyed from notification now, don't try to access "this".
    protector->detach();
}

void AudioWorkletThread::runEventLoop()
{
    // Does not return until terminated.
    m_runLoop.run(m_workletGlobalScope.get());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
