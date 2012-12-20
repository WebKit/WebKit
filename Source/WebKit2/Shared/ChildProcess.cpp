/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ChildProcess.h"

#if !OS(WINDOWS)
#include <unistd.h>
#endif

using namespace WebCore;

namespace WebKit {

void ChildProcess::disableTermination()
{
    m_terminationCounter++;
    m_terminationTimer.stop();
}

void ChildProcess::enableTermination()
{
    ASSERT(m_terminationCounter > 0);
    m_terminationCounter--;

    if (m_terminationCounter)
        return;

    if (!m_terminationTimeout) {
        terminationTimerFired();
        return;
    }

    m_terminationTimer.startOneShot(m_terminationTimeout);
}

ChildProcess::ChildProcess()
    : m_terminationTimeout(0)
    , m_terminationCounter(0)
    , m_terminationTimer(RunLoop::main(), this, &ChildProcess::terminationTimerFired)
{
    // FIXME: The termination timer should not be scheduled on the main run loop.
    // It won't work with the threaded mode, but it's not really useful anyway as is.
    
    platformInitialize();
}

ChildProcess::~ChildProcess()
{
}

void ChildProcess::terminationTimerFired()
{
    if (!shouldTerminate())
        return;

    terminate();
}

void ChildProcess::terminate()
{
    RunLoop::main()->stop();
}

NO_RETURN static void watchdogCallback()
{
    // We use _exit here since the watchdog callback is called from another thread and we don't want 
    // global destructors or atexit handlers to be called from this thread while the main thread is busy
    // doing its thing.
    _exit(EXIT_FAILURE);
}

void ChildProcess::didCloseOnConnectionWorkQueue(WorkQueue& workQueue, CoreIPC::Connection*)
{
    // If the connection has been closed and we haven't responded in the main thread for 10 seconds
    // the process will exit forcibly.
    static const double watchdogDelay = 10.0;

    workQueue.dispatchAfterDelay(bind(static_cast<void(*)()>(watchdogCallback)), watchdogDelay);
}

#if !PLATFORM(MAC)
void ChildProcess::platformInitialize()
{
}
#endif

} // namespace WebKit
