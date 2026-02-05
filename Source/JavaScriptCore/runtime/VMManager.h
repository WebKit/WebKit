/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/StopTheWorldCallback.h>
#include <JavaScriptCore/VMThreadContext.h>
#include <wtf/Atomics.h>
#include <wtf/Condition.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ScopedLambda.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class VM;

// Understanding Stop the World (STW)
// ==================================
//
// Intuition on how to think about things?
// =======================================
// The actors in play for a Stop the World story are:
// 1. VMManager
// 2. VM
// 3. a Conductor Agent
//
// Events / actions involved in the Stop the World story are:
// 1. Stop requests (with a given StopReason)
// 2. Stop callback handlers
//
// An intuitive way to think about the Stop the World story is:
//
// 1. VMManager is an abstraction representing the process. There is only 1 singleton
//    VMManager instance, and it coordinates the tracking and scheduling of VMs.
//
// 2. VM (and its VMThreadContext) represents a thread. A VM instance may actually
//    run on different machine threads at different times (JSC's API allows this).
//    However, from VMManager's perspective, each VM is like a thread that can be
//    suspended / stopped, and resumed.
//
//    FIXME: the current VMManager does NOT yet handle cases where more than one VM
//    is run on the same machine thread e.g. one VM1 calls into C++, which in turn
//    calls into VM2. VM2 now has control of the CPU, but VMManager does not know
//    that VM1 is in a way "deactivated". This scenario cannot manifest in WebKit
//    with web workloads though.
//
// 3. The Conductor Agent is something like a Debugger agent that tells VMManager
//    to stop or resume VMs / threads.
//
// 4. Stop requests are like interrupts. A stop being requested is analogous to an
//    interrupt firing.
//
// 5. Stop callback handlers are like interrupt handlers that masked out all interrupts
//    so that no other interrupts can fire while the current one is being handled.
//
//    When a stop request occurs, VMManager::notifyVMStop() will dispatch a callback
//    to the appropriate handler for that request. Similar to the interrupt
//    analogy, only one request can be serviced at one time. All other requests
//    regardless of priority will be blocked (and held in pending) until the current
//    request is done being serviced.
//
// World Execution Modes
// =====================
// The VMManager has a notion of a world mode (see VMManager::Mode). These modes are:
// 1. RunAll - all threads can run or are running.
// 2. RunOne - only one thread can run, like when a debugger is single stepping.
// 3. Stopping - a stop has been requested, and VMs are in the process of stopping.
// 4. Stopped - all threads have been stopped, and the highest priority stop request
//              can now be serviced.
//
// Querying VMManager Info
// =======================
// VMManager::info() provides a view into a few pieces of VMManager state:
// 1. numberOfVMs - the number of VMs that have been constructed and are alive.
// 2. numberOfActiveVMs - the number of VMs that are activated i.e. their threads
//        have entered the VM. This numberOfActiveVMs is only available while the
//        world is NOT in Mode::RunAll (i.e. must be in some form of stoppage).
// 3. numberOfStoppedVMs - the number of VMs that have reached the stopping point
//        in VMManager::notifyVMStop(). The currently executing targetVM is counted
//        as stopped when single stepping in Mode::RunOne.
// 3. worldMode - this is the current VM world mode (as described above).
//
// Currently, this info is mainly used for testing purposes only.
//
// Initiating Stop the World
// =========================
// Stop the World begins with some agent calling VMManager::requestStopAll() with a
// StopReason. This agent can be from mutator threads or from a helper thread like
// those employed by debuggers.
//
// More than one agent can request STW at the same time. Hence, there can be multiple
// stop requests queued up while the world is being stopped.
//
// StopReason and their Priority
// =============================
// Current StopReasons are:
//     None - no requests
//     GC - requesting stop for Global GC
//     WasmDebugger - requesting stop for Wasm Debugger (like Ctrl-C in lldb)
//     MemoryDebugger - similar to WasmDebugger, but for the Memory Debugger.
//
// The priority of these requests are defined by their order of declaration.
// See definition of FOR_EACH_STOP_THE_WORLD_REASON and enum class StopReason.
// StopReason::None is a special case and has no priority.
// StopReason::GC is the current highest priority request.
// StopReason::MemoryDebugger is the current lowest priority request.
//
// StopReason is synonymous with "StopRequest".
// From the client's perspective, it is the reason for a stop request.
// From the VMManager's perspective, it is the type of stop request.
//
// Servicing Order: one request at a time
// ======================================
// The order the stop requests came in does not matter. Once the world is finally
// stopped, the higher priority request is serviced first.
// See fetchTopPriorityStopReason() and m_currentStopReason.
//
// While this request is being serviced, other requests will be ignored. During
// this time of service, new stop requests can be added to m_pendingStopRequestBits,
// but they will be ignored even if they are higher priority. We will service them
// only after the current request has resumed with RunOne or RunAll mode.
//
// StopTheWorldCallback (i.e. stop request handlers)
// =================================================
// When the world is stopped, VMManager will call back to a request handler
// based on what StopReason is in m_currentStopReason. See VMManager::notifyVMStop().
//
// The handler for GC should be static but is not currently implemented yet.
// The handlers for WasmDebugger and MemoryDebugger may be overridden. They are
// made to be overrideable only to enable testing. See VMManagerStopTheWorldTest.cpp
// for how they are used in testing.
//
// Each handler must be of the shape StopTheWorldCallback. See StopTheWorldCallback.h
// The handler will be called with a StopTheWorldEvent. The StopTheWorldEvent indicates
// where the handler is called from. This may have a use later on, but for now, the
// StopTheWorldEvent is only informational.
//
// After the handler is done, it controls how execution will proceed thereafter by
// returning one of the StopTheWorldCallback return values (see StopTheWorldCallback.h).
// The possible return values are:
//
// 1. STW_CONTINUE()
//    - this is only used for testing purposes where we want to loop inside
//      VMManager::notifyVMStop() while waiting for more things to handle.
//    - VMManager::m_worldMode will remain in Mode::Stopped.
// 2. STW_CONTEXT_SWITCH(targetVM)
//    - this is used to switch control of the handler to another VM on a different
//      thread without resuming any execution. lldb's  "thread select ..." can be
//      implemented this way.
//    - VMManager::m_worldMode will remain in Mode::Stopped.
// 3. STW_RESUME_ONE()
//    - this is used to resume only the current VM thread in RunOne mode. This is
//      useful for debuggers that wish to single step in the current VM. It keeps
//      other threads paused / stopped while this thread executes. It is up to the
//      client to detect potential resource deadlocks (e.g. using a timeout) that
//      may arise from only resuming 1 thread.
//    - VMManager::m_worldMode will transition from Mode::Stopped to Mode::RunOne.
// 4. STW_RESUME_ALL()
//    - forces all threads to resume from a stop.
//    - VMManager::m_worldMode will transition from Mode::Stopped to Mode::RunAll.
// 5. STW_RESUME()
//    - Return to whatever run mode we were executing with before the current Stop
//      the World request. That may be either Mode::RunOne or Mode::RunAll.
//    - This allows the GC to run (with its own Stop the World requests) even while
//      while we're single stepping in a debugger with Mode::RunOne.
//
// Edge Cases and Special Circumstances
// ====================================
// While in Mode::RunOne, if the VM that is running either exits the VM (aka
// deactivates) or its VM is destructed (aka shutdown), the VMManager will transition
// the world mode back to RunAll (unblocking other VMs and threads)  since the current
// VM is no longer viable for continuing execution.

#define FOR_EACH_STOP_THE_WORLD_REASON(v) \
    v(GC) \
    v(WasmDebugger) \
    v(MemoryDebugger) \

class VMManager {
    WTF_MAKE_NONCOPYABLE(VMManager);

#define DECLARE_STOP_THE_WORLD_REASON_BIT_SHIFT(reason__) reason__##BitShift,
    enum StopReasonBitShift {
        FOR_EACH_STOP_THE_WORLD_REASON(DECLARE_STOP_THE_WORLD_REASON_BIT_SHIFT)
    };
#undef DECLARE_STOP_THE_WORLD_REASON_BIT_SHIFT

#define COUNT_STOP_REASON(event) + 1
    static constexpr unsigned NumberOfStopReasons = FOR_EACH_STOP_THE_WORLD_REASON(COUNT_STOP_REASON);
#undef COUNT_STOP_REASON

public:
    using StopRequestBits = uint32_t;
    static_assert(NumberOfStopReasons <= (sizeof(StopRequestBits) * CHAR_BIT));

#define DECLARE_STOP_THE_WORLD_REASON(reason__) reason__ = (1 << reason__##BitShift),
    enum class StopReason : StopRequestBits {
        None = 0,
        FOR_EACH_STOP_THE_WORLD_REASON(DECLARE_STOP_THE_WORLD_REASON)
    };
#undef DECLARE_STOP_THE_WORLD_REASON

    enum class Error : uint8_t {
        None,
        TimedOut
    };

    JS_EXPORT_PRIVATE static VMManager& singleton();

    ALWAYS_INLINE static bool isValidVM(VM* vm)
    {
        return vm == s_recentVM || isValidVMSlow(vm);
    }

    // StopTheWorld APIs ======================================================

    enum class Mode : uint8_t {
        RunAll, // no threads are stopped.
        RunOne, // all threads are stopped except for the 1 thread the debugger wants to run.
        Stopping, // still waiting for the right thread to service the stop.
        Stopped, // all threads have stopped, and the right thread is now servicing the stop.
    };

    struct Info {
        unsigned numberOfVMs;
        unsigned numberOfActiveVMs;
        unsigned numberOfStoppedVMs;
        Mode worldMode;
        VM* targetVM;

        void dump(PrintStream& out) const
        {
            out.print("VMManager::Info(numberOfVMs:", numberOfVMs);
            out.print(", numberOfActiveVMs:", numberOfActiveVMs);
            out.print(", numberOfStoppedVMs:", numberOfStoppedVMs);
            out.print(", worldMode:", worldMode);
            out.print(", targetVM:", RawPointer(targetVM), ")");
        }
    };

    JS_EXPORT_PRIVATE static Info info();
    static unsigned numberOfVMs() { return singleton().m_numberOfVMs; }

    JS_EXPORT_PRIVATE static void setWasmDebuggerOnStop(StopTheWorldCallback);
    JS_EXPORT_PRIVATE static void setWasmDebuggerOnResume(PostResumeCallback);
    JS_EXPORT_PRIVATE static void setMemoryDebuggerCallback(StopTheWorldCallback);

    ALWAYS_INLINE CONCURRENT_SAFE static void requestStopAll(StopReason reason)
    {
        singleton().requestStopAllInternal(reason);
    }
    ALWAYS_INLINE CONCURRENT_SAFE static void requestResumeAll(StopReason reason)
    {
        singleton().requestResumeAllInternal(reason);
    }

    void notifyVMConstruction(VM&);
    void notifyVMDestruction(VM&);
    void notifyVMActivation(VM&);
    void notifyVMDeactivation(VM&);
    void notifyVMStop(VM&, StopTheWorldEvent);

    void handleVMDestructionWhileWorldStopped(VM&);

    // Interation APIs ======================================================

    using IteratorCallback = IterationStatus(VM&);
    using TestCallback = bool(VM&);

    static inline VM* findMatchingVM(const Invocable<TestCallback> auto& test)
    {
        SUPPRESS_FORWARD_DECL_ARG return singleton().findMatchingVMImpl(scopedLambda<TestCallback>(test));
    }

    static inline void forEachVM(const Invocable<IteratorCallback> auto& functor)
    {
        SUPPRESS_FORWARD_DECL_ARG singleton().forEachVMImpl(scopedLambda<IteratorCallback>(functor));
    }

    static inline Error forEachVMWithTimeout(Seconds timeout, const Invocable<IteratorCallback> auto& functor)
    {
        SUPPRESS_FORWARD_DECL_ARG return singleton().forEachVMWithTimeoutImpl(timeout, scopedLambda<IteratorCallback>(functor));
    }

    JS_EXPORT_PRIVATE static void dumpVMs();

private:
    VMManager() = default;

    bool hasPendingStopRequests() const { return m_pendingStopRequestBits.loadRelaxed(); }

    JS_EXPORT_PRIVATE CONCURRENT_SAFE void requestStopAllInternal(StopReason);
    JS_EXPORT_PRIVATE CONCURRENT_SAFE void requestResumeAllInternal(StopReason);

    void resumeTheWorld() WTF_REQUIRES_LOCK(m_worldLock);
    void incrementActiveVMs(VM&) WTF_REQUIRES_LOCK(m_worldLock);
    void decrementActiveVMs(VM&) WTF_REQUIRES_LOCK(m_worldLock);

    void dispatchStopHandler(VM&);
    void handleStopViaDispatch(VM&);

    JS_EXPORT_PRIVATE static bool isValidVMSlow(VM*);
    JS_EXPORT_PRIVATE VM* findMatchingVMImpl(const ScopedLambda<TestCallback>&);
    JS_EXPORT_PRIVATE void forEachVMImpl(const ScopedLambda<IteratorCallback>&);
    JS_EXPORT_PRIVATE Error forEachVMWithTimeoutImpl(Seconds timeout, const ScopedLambda<IteratorCallback>&);

    void iterateVMs(const Invocable<IterationStatus(VM&)> auto&) WTF_REQUIRES_LOCK(m_worldLock);

    DoublyLinkedList<VMThreadContext> m_vmList WTF_GUARDED_BY_LOCK(m_worldLock);
    Lock m_worldLock;
    Condition m_worldConditionVariable;

    // === Variables only relevant for StopTheWorld ===================================

    // Indicates if the world is running or stopped (see Modes for details).
    // Requires m_worldLock to write to this, but not to read it.
    Mode m_worldMode { Mode::RunAll };

    // Indicates if the world needs to be in RunOne mode (and if it should resume in RunOne mode
    // after stops).
    bool m_useRunOneMode { false };

    // Indicates if there are pending StopTheWorld requests (analogous to pending interrupts).
    // In RunOne mode, all VM threads (except one) will be stopped even when m_pendingStopRequestBits
    // is empty. Hence, m_pendingStopRequestBits says nothing about whether threads are / should be
    // running or not.
    // Can be written and read concurrently without m_worldLock.
    Atomic<StopRequestBits> m_pendingStopRequestBits { 0 };

    // We need to track a m_currentStopReason because we may need to continue servicing the current
    // request after a context switch to a different targetVM. Conceptually, if StopTheWorld requests
    // are analogous to interrupts, then when a specific interrupt is being serviced, all other
    // interrupts are blocked / disabled though their status remains pending. Similarly, all other
    // pending StopTheWorld requests will be blocked, and only serviced after the current one being
    // serviced is done.
    // Only notifyVMStop() may modify m_currentStopReason.
    StopReason m_currentStopReason { StopReason::None };

    // Flags whether WasmDebugger post-resume callback is pending. Set when servicing WasmDebugger
    // stop, atomically read-and-cleared by last VM exiting notifyVMStop().
    Atomic<bool> m_needsWasmDebuggerOnResume { false };

    // Indicates the targetVM that will service the StopTheWorld request, or the targetVM that may
    // continue running in RunOne mode.
    // Can only be written to while holding the m_worldLock.
    // Can be read without the m_worldLock under some restricted circumstances.
    VM* m_targetVM { nullptr };

    // We'll set m_numberOfActiveVMs to 99999999 when it's not supposed to hold a valid value.
    // 99999999 is just some arbitrary token value that is easy to recognize but we're not likely
    // to see in any real world value of m_numberOfActiveVMs. The 99999999 value will easily
    // convey the idea that the value is invalid at any given point in time that info() is sampled.
    static constexpr unsigned invalidNumberOfActiveVMs = 99999999;

    // Indicated the number of VMs that have non-null EntryScopes.
    // This value is only valid while a StopTheWorld request is being processed. It is calculated
    // when the first requesting VM stops of all VMs. While a StopTheWorld request is being serviced,
    // it will be updated using the VM's ConcurrentEntryScopeService.
    //
    // The choice to not track a valid m_numberOfActiveVMs at all times is just an optimization so
    // that we can skip this work when not doing Stop the World.
    unsigned m_numberOfActiveVMs { invalidNumberOfActiveVMs };

    Atomic<unsigned> m_numberOfStoppedVMs { 0 };

    // === End of variables only relevant for StopTheWorld =================================

    unsigned m_numberOfVMs { 0 };

    JS_EXPORT_PRIVATE static VM* s_recentVM;

    friend class LazyNeverDestroyed<VMManager>;
};

#undef FOR_EACH_STOP_THE_WORLD_REASON

} // namespace JSC
