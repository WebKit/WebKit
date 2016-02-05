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

    struct UnprocessedStackFrame {
        UnprocessedStackFrame(CodeBlock* codeBlock, EncodedJSValue callee, CallSiteIndex callSiteIndex)
            : unverifiedCallee(callee)
            , verifiedCodeBlock(codeBlock)
            , callSiteIndex(callSiteIndex)
        { }
        UnprocessedStackFrame()
        {
            unverifiedCallee = JSValue::encode(JSValue());
            verifiedCodeBlock = nullptr;
        }

        EncodedJSValue unverifiedCallee;
        CodeBlock* verifiedCodeBlock;
        CallSiteIndex callSiteIndex;
    };

    enum class FrameType { 
        Executable,
        Host,
        Unknown 
    };

    struct StackFrame {
        StackFrame(ExecutableBase* executable)
            : frameType(FrameType::Executable)
            , executable(executable)
        { }

        StackFrame()
        { }

        FrameType frameType { FrameType::Unknown };
        ExecutableBase* executable { nullptr };
        JSObject* callee { nullptr };
        // These attempt to be expression-level line and column number.
        unsigned lineNumber { std::numeric_limits<unsigned>::max() };
        unsigned columnNumber { std::numeric_limits<unsigned>::max() };

        bool hasExpressionInfo() const
        {
            return lineNumber != std::numeric_limits<unsigned>::max()
                && columnNumber != std::numeric_limits<unsigned>::max();
        }

        // These are function-level data.
        String nameFromCallee(VM&);
        String displayName(VM&);
        String displayNameForJSONTests(VM&); // Used for JSC stress tests because they want the "(anonymous function)" string for anonymous functions and they want "(eval)" for eval'd code.
        int functionStartLine();
        unsigned functionStartColumn();
        intptr_t sourceID();
        String url();
    };

    struct UnprocessedStackTrace {
        double timestamp;
        void* topPC;
        bool topFrameIsLLInt;
        void* llintPC;
        Vector<UnprocessedStackFrame> frames;
    };

    struct StackTrace {
        double timestamp;
        Vector<StackFrame> frames;
        StackTrace()
        { }
        StackTrace(StackTrace&& other)
            : timestamp(other.timestamp)
            , frames(WTFMove(other.frames))
        { }
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
    void start(const LockHolder&);
    void stop();
    void stop(const LockHolder&);
    Vector<StackTrace> releaseStackTraces(const LockHolder&);
    JS_EXPORT_PRIVATE String stackTracesAsJSON();
    JS_EXPORT_PRIVATE void noticeCurrentThreadAsJSCExecutionThread();
    void noticeCurrentThreadAsJSCExecutionThread(const LockHolder&);
    void processUnverifiedStackTraces(); // You should call this only after acquiring the lock.
    double totalTime(const LockHolder&) { return m_totalTime; }
    void setStopWatch(const LockHolder&, Ref<Stopwatch>&& stopwatch) { m_stopwatch = WTFMove(stopwatch); }

private:
    void dispatchIfNecessary(const LockHolder&);
    void dispatchFunction(const LockHolder&);
    void pause();
    void clearData(const LockHolder&);

    VM& m_vm;
    RefPtr<Stopwatch> m_stopwatch;
    Vector<StackTrace> m_stackTraces;
    Vector<UnprocessedStackTrace> m_unprocessedStackTraces;
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
    HashSet<JSCell*> m_liveCellPointers;
    Vector<UnprocessedStackFrame> m_currentFrames;
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::SamplingProfiler::FrameType);

} // namespace WTF

#endif // ENABLE(SAMPLING_PROFILER)

#endif // SamplingProfiler_h 
