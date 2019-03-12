/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "BPlatform.h"
#include "DeferredDecommit.h"
#include "Mutex.h"
#include "PerProcess.h"
#include "Vector.h"
#include <chrono>
#include <condition_variable>
#include <mutex>

#if BOS(DARWIN)
#include <dispatch/dispatch.h>
#endif

namespace bmalloc {

class Scavenger {
public:
    BEXPORT Scavenger(std::lock_guard<Mutex>&);
    
    ~Scavenger() = delete;
    
    void scavenge();
    
#if BOS(DARWIN)
    void setScavengerThreadQOSClass(qos_class_t overrideClass) { m_requestedScavengerThreadQOSClass = overrideClass; }
    qos_class_t requestedScavengerThreadQOSClass() const { return m_requestedScavengerThreadQOSClass; }
#endif
    
    bool willRun() { return m_state == State::Run; }
    void run();
    
    bool willRunSoon() { return m_state > State::Sleep; }
    void runSoon();
    
    BEXPORT void didStartGrowing();
    BEXPORT void scheduleIfUnderMemoryPressure(size_t bytes);
    BEXPORT void schedule(size_t bytes);

    // This is only here for debugging purposes.
    // FIXME: Make this fast so we can use it to help determine when to
    // run the scavenger:
    // https://bugs.webkit.org/show_bug.cgi?id=184176
    size_t freeableMemory();
    // This doesn't do any synchronization, so it might return a slightly out of date answer.
    // It's unlikely, but possible.
    size_t footprint();

    void enableMiniMode();

private:
    enum class State { Sleep, Run, RunSoon };
    
    void runHoldingLock();
    void runSoonHoldingLock();

    void scheduleIfUnderMemoryPressureHoldingLock(size_t bytes);

    BNO_RETURN static void threadEntryPoint(Scavenger*);
    BNO_RETURN void threadRunLoop();
    
    void setSelfQOSClass();
    void setThreadName(const char*);

    std::chrono::milliseconds timeSinceLastFullScavenge();
    std::chrono::milliseconds timeSinceLastPartialScavenge();
    void partialScavenge();

    std::atomic<State> m_state { State::Sleep };
    size_t m_scavengerBytes { 0 };
    bool m_isProbablyGrowing { false };
    bool m_isInMiniMode { false };
    
    Mutex m_mutex;
    Mutex m_scavengingMutex;
    std::condition_variable_any m_condition;

    std::thread m_thread;
    std::chrono::steady_clock::time_point m_lastFullScavengeTime { std::chrono::steady_clock::now() };
    std::chrono::steady_clock::time_point m_lastPartialScavengeTime { std::chrono::steady_clock::now() };
    
#if BOS(DARWIN)
    dispatch_source_t m_pressureHandlerDispatchSource;
    qos_class_t m_requestedScavengerThreadQOSClass { QOS_CLASS_USER_INITIATED };
#endif
    
    Vector<DeferredDecommit> m_deferredDecommits;
};

} // namespace bmalloc


