/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef SamplingProfiler_h
#define SamplingProfiler_h

#if ENABLE(SAMPLING_PROFILER)

#include "CallFrame.h"
#include "MachineStackMarker.h"
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Stopwatch.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

namespace JSC {

class VM;
class ExecutableBase;

class SamplingProfiler : public ThreadSafeRefCounted<SamplingProfiler> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class FrameType { 
        VerifiedExecutable, 
        UnverifiedCallee,
        Host, 
        Unknown 
    };
    struct StackFrame {
        StackFrame(FrameType frameType, EncodedJSValue callee)
            : frameType(frameType)
        {
            u.unverifiedCallee = callee;
        }
        StackFrame(FrameType frameType, ExecutableBase* executable)
            : frameType(frameType)
        {
            u.verifiedExecutable = executable;
        }
        StackFrame()
            : frameType(FrameType::Unknown)
        {
            u.verifiedExecutable = nullptr;
        }

        FrameType frameType;
        union {
            EncodedJSValue unverifiedCallee;
            ExecutableBase* verifiedExecutable;
        } u;
    };
    struct StackTrace {
        bool needsVerification;
        double timestamp;
        Vector<StackFrame> frames;
    };

    SamplingProfiler(VM&, RefPtr<Stopwatch>&&);
    ~SamplingProfiler();
    void noticeJSLockAcquisition();
    void noticeVMEntry();
    void shutdown();
    void visit(SlotVisitor&);
    Lock& getLock() { return m_lock; }
    void setTimingInterval(std::chrono::microseconds interval) { m_timingInterval = interval; }
    JS_EXPORT_PRIVATE void start();
    void stop();
    const Vector<StackTrace>& stackTraces() const { return m_stackTraces; }
    JS_EXPORT_PRIVATE String stacktracesAsJSON();
    JS_EXPORT_PRIVATE void noticeCurrentThreadAsJSCExecutionThread();
    void noticeCurrentThreadAsJSCExecutionThread(const LockHolder&);
    JS_EXPORT_PRIVATE void clearData();
    void processUnverifiedStackTraces(); // You should call this only after acquiring the lock.

private:
    void dispatchIfNecessary(const LockHolder&);
    void dispatchFunction(const LockHolder&);
    void pause();

    VM& m_vm;
    RefPtr<Stopwatch> m_stopwatch;
    Vector<StackTrace> m_stackTraces;
    size_t m_indexOfNextStackTraceToVerify;
    std::chrono::microseconds m_timingInterval;
    double m_lastTime;
    double m_totalTime;
    Ref<WorkQueue> m_timerQueue;
    std::function<void ()> m_handler;
    Lock m_lock;
    MachineThreads::Thread* m_jscExecutionThread;
    bool m_isActive;
    bool m_isPaused;
    bool m_hasDispatchedFunction;
    HashSet<ExecutableBase*> m_seenExecutables;
    Vector<StackFrame> m_currentFrames;
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::SamplingProfiler::FrameType);

} // namespace WTF

#endif // ENABLE(SAMPLING_PROFILER)

#endif // SamplingProfiler_h 
