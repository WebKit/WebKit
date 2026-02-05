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

#include "config.h"
#include "VMManager.h"

#include "JSCConfig.h"
#include "JSLock.h"
#include "VM.h"
#include "VMThreadContext.h"
#include <wtf/RunLoop.h>

namespace JSC {

VM* VMManager::s_recentVM { nullptr };

VMManager& VMManager::singleton()
{
    static LazyNeverDestroyed<VMManager> manager;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        manager.construct();
    });
    return manager.get();
}

VMThreadContext::VMThreadContext()
{
    VM* vm = VM::fromThreadContext(this);
    // Ensure that VM is not in-service yet. Since notifyVMConstruction has memory barrier (lock),
    // if we are ensuring this condition here, concurrent threads will see this consistent state.
    // Make sure m_isInService is initialized to false before VMThreadContext is initialized.
    RELEASE_ASSERT(!vm->isInService());
    VMManager::singleton().notifyVMConstruction(*vm);
}

VMThreadContext::~VMThreadContext()
{
    VM* vm = VM::fromThreadContext(this);
    VMManager::singleton().notifyVMDestruction(*vm);
}

bool VMManager::isValidVMSlow(VM* vm)
{
    bool found = false;
    forEachVM([&] (VM& nextVM) {
        if (vm == &nextVM) {
            s_recentVM = vm;
            found = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return found;
}

void VMManager::dumpVMs()
{
    unsigned i = 0;
    WTFLogAlways("Registered VMs:");
    forEachVM([&] (VM& nextVM) {
        WTFLogAlways("  [%u] VM %p", i++, &nextVM);
        return IterationStatus::Continue;
    });
}

void VMManager::iterateVMs(const Invocable<IterationStatus(VM&)> auto& functor) WTF_REQUIRES_LOCK(m_worldLock)
{
    for (auto* context = m_vmList.head(); context; context = context->next()) {
        VM& vm = *VM::fromThreadContext(context);
        IterationStatus status = functor(vm);
        if (status == IterationStatus::Done)
            return;
    }
}

VM* VMManager::findMatchingVMImpl(const ScopedLambda<VMManager::TestCallback>& test)
{
    Locker lock { m_worldLock };
    if (s_recentVM && test(*s_recentVM))
        return s_recentVM;

    VM* result = nullptr;
    iterateVMs(scopedLambda<IteratorCallback>([&] (VM& vm) {
        if (test(vm)) {
            result = &vm;
            s_recentVM = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }));
    return result;
}

void VMManager::forEachVMImpl(const ScopedLambda<VMManager::IteratorCallback>& func)
{
    Locker lock { m_worldLock };
    iterateVMs(func);
}

VMManager::Error VMManager::forEachVMWithTimeoutImpl(Seconds timeout, const ScopedLambda<VMManager::IteratorCallback>& func)
{
    if (!m_worldLock.tryLockWithTimeout(timeout))
        return Error::TimedOut;

    Locker locker { AdoptLock, m_worldLock };
    iterateVMs(func);
    return Error::None;
}

auto VMManager::info() -> Info
{
    Info info;
    auto& manager = singleton();

    // The reason for locking here is so that we capture a consistent snapshot
    // of all the values in info.
    Locker lock { manager.m_worldLock };
    info.numberOfVMs = manager.m_numberOfVMs;
    info.numberOfActiveVMs = manager.m_numberOfActiveVMs;
    info.numberOfStoppedVMs = manager.m_numberOfStoppedVMs.loadRelaxed();
    info.worldMode = manager.m_worldMode;
    info.targetVM = manager.m_targetVM;
    return info;
}

void VMManager::setWasmDebuggerOnStop(StopTheWorldCallback callback)
{
    g_jscConfig.wasmDebuggerOnStop = callback;
}

void VMManager::setWasmDebuggerOnResume(PostResumeCallback callback)
{
    g_jscConfig.wasmDebuggerOnResume = callback;
}

void VMManager::setMemoryDebuggerCallback(StopTheWorldCallback callback)
{
    g_jscConfig.memoryDebuggerStopTheWorld = callback;
}

void VMManager::incrementActiveVMs(VM& vm) WTF_REQUIRES_LOCK(m_worldLock)
{
    if (!vm.traps().m_hasBeenCountedAsActive) {
        m_numberOfActiveVMs++;
        vm.traps().m_hasBeenCountedAsActive = true;
    }
}

void VMManager::decrementActiveVMs(VM& vm) WTF_REQUIRES_LOCK(m_worldLock)
{
    // We only need to track m_numberOfActiveVMs changes if we're in RunOne
    // mode. If we're running because the world was resumed with RunAll,
    // then m_numberOfActiveVMs is invalid, and resumeTheWorld() would set
    // it to a token value of invalidNumberOfActiveVMs (to aid debugging).
    if (m_worldMode == Mode::RunAll)
        ASSERT(m_numberOfActiveVMs == invalidNumberOfActiveVMs);
    else if (vm.traps().m_hasBeenCountedAsActive) {
        m_numberOfActiveVMs--;
        vm.traps().m_hasBeenCountedAsActive = false;
    }

    auto shouldResumeAll = [&] {
        if (m_worldMode != Mode::RunAll && !m_numberOfActiveVMs)
            return true;
        if (m_worldMode == Mode::RunOne) {
            RELEASE_ASSERT(m_targetVM == &vm);
            return true;
        }
        return false;
    };

    if (shouldResumeAll()) {
        if (m_targetVM) {
            // There's a designated targetVM thread to continue in, but we don't have the
            // ability to just wake the desired one up. So, wake up all the threads and let
            // them sort themselves out.
            //
            // But if the targetVM thread is this thread, then pass the control to another
            // thread, any thread. That's because this thread is dying imminently.
            if (m_targetVM == &vm) {
                m_targetVM = nullptr;
                m_useRunOneMode = false;
            }
            m_worldConditionVariable.notifyAll();
        } else {
            // There's no designated targetVM thread. So, just waking up any one thread will do.
            m_worldConditionVariable.notifyOne();
        }
    }
}

CONCURRENT_SAFE void VMManager::requestStopAllInternal(StopReason reason)
{
    // StopReason is synonymous with "StopRequest".
    // From the client's perspective, it is the reason for a stop request.
    // From the VMManager's perspective, it is the type of stop request.
    auto requestBits = static_cast<StopRequestBits>(reason);
    m_pendingStopRequestBits.exchangeOr(requestBits);
    {
        Locker lock { m_worldLock };
        if (m_worldMode >= Mode::Stopping)
            return;

        if (m_worldMode == Mode::RunAll) {
            // RunOne mode allows execution of 1 VM without resumeTheWorld(). We did not clear
            // the m_hasBeenCountedAsActive flags on each VM on resuming with RunOne. As a
            // result, m_numberOfActiveVMs is still valid in RunOne mode. We don't want
            // to reset m_numberOfActiveVMs to 0 here because we won't be re-calculating
            // it on stop like we do for RunAll mode.
            //
            // For RunAll mode, do want to reset m_numberOfActiveVMs, and incrementActiveVMs()
            // below will re-calculate the current true value of m_numberOfActiveVMs.
            m_numberOfActiveVMs = 0;
        }

        m_worldMode = Mode::Stopping;

        bool enableWasmDebugger = reason == StopReason::WasmDebugger;

        // Have to use iterateVMs() instead of forEachVM() because we're already
        // holding the m_worldLock.
        iterateVMs(scopedLambda<IteratorCallback>([&] (VM& vm) {
            vm.requestStop();
            WTF::storeLoadFence();

            if (enableWasmDebugger) [[unlikely]]
                dispatchStopHandler(vm);

            if (vm.isEntered()) {
                // incrementActiveVMs() relies on m_worldLock being held, which it
                // obviously is above. However, Clang is not smart enough to see this.
                // So, we need to suppress this warning here.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
                incrementActiveVMs(vm);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
            }
            return IterationStatus::Continue;
        }));
    }
}

// Dispatch a callback to VM's RunLoop to handle Stop-The-World for idle VMs.
// Idle VMs (not executing code) never check traps, so they can't respond to requestStop().
// Dispatching to RunLoop ensures the callback executes when VM processes events, allowing
// idle VMs to call notifyVMStop(). Currently only used for WasmDebugger interrupts.
void VMManager::dispatchStopHandler(VM& vm)
{
    // Use JSLock coordination pattern (like JSRunLoopTimer) to safely detect VM destruction.
    Ref<JSLock> apiLock = vm.apiLock();
    vm.runLoop().dispatch([apiLock = WTF::move(apiLock)]() {
        Locker locker { apiLock.get() };

        RefPtr<VM> vm = apiLock->vm();
        if (!vm)
            return;

        VMManager::singleton().handleStopViaDispatch(*vm);
    });
}

void VMManager::handleStopViaDispatch(VM& vm)
{
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());

    // Test-and-clear to ensure exactly one notifyVMStop() call.
    // If trap already cleared (by handleTraps or resumeTheWorld), skip.
    if (!vm.traps().clearTrap(VMTraps::NeedStopTheWorld))
        return;

    {
        Locker lock { m_worldLock };
        // Count VM as active so notifyVMStop's accounting is correct.
        incrementActiveVMs(vm);
    }

    notifyVMStop(vm, StopTheWorldEvent::VMStopped);

    {
        Locker lock { m_worldLock };
        decrementActiveVMs(vm);
    }
}

CONCURRENT_SAFE void VMManager::requestResumeAllInternal(StopReason reason)
{
    // StopReason is synonymous with StopRequest.
    // From the client's perspective, it is the reason for a stop request.
    // From the VMManager's perspective, it is the type of stop request.
    auto requestBits = static_cast<StopRequestBits>(reason);
    m_pendingStopRequestBits.exchangeAnd(~requestBits);
    if (hasPendingStopRequests())
        return; // There are still pending stop requests. Nothing more to do.

    Locker lock { m_worldLock };
    resumeTheWorld();
}

void VMManager::resumeTheWorld() WTF_REQUIRES_LOCK(m_worldLock)
{
    // We can call resumeTheWorld() more than once. Hence, we may already be in RunAll mode.
    if (m_worldMode == Mode::RunAll)
        return; // Already resumed. Nothing more to do.

    // If we're in RunOne mode, then we want to still call into notifyVMStop() all
    // the time. So, we don't want to resumeTheWorld() just yet as that will disable
    // all the stop checks yet.
    if (m_useRunOneMode)
        return;

    // Have to use iterateVMs() instead of forEachVM() because we're already
    // holding the m_worldLock.
    iterateVMs(scopedLambda<IteratorCallback>([&] (VM& vm) {
        vm.cancelStop();
        vm.traps().m_hasBeenCountedAsActive = false;
        return IterationStatus::Continue;
    }));

    m_targetVM = nullptr;
    m_numberOfActiveVMs = invalidNumberOfActiveVMs; // invalid when not Stopped.
    m_worldMode = Mode::RunAll;
    m_worldConditionVariable.notifyAll();
}

void VMManager::notifyVMStop(VM& vm, StopTheWorldEvent event)
{
    // Due to races, we may end up calling notifyVMStop() even when there is no stop to be serviced.
    // It should always be safe to call notifyVMStop() as many times as we like. The only cost is
    // is performance.
    //
    // In Mode::RunOne, we will call notifyVMStop() even if there are no requested stops. The code
    // below will simply determine that there's nothing to do and return back out. This is fine
    // since Mode::RunOne is only used by debuggers, and peek performance is not a concern.
    // We need to ensure that StopTheWorld VMTraps remained installed and that notifyVMStop() gets
    // called when in Mode::RunOne because new VM thread can be started, and we want those new
    // threads to also stop since they aren't the targetVM thread.

    m_numberOfStoppedVMs.exchangeAdd(1);

    for (;;) {
        {
            Locker lock { m_worldLock };

            auto fetchTopPriorityStopReason = [&] {
                auto pendingRequests = m_pendingStopRequestBits.loadRelaxed();
                for (unsigned i = 0; i < NumberOfStopReasons; ++i) {
                    auto requestToCheck = static_cast<StopRequestBits>(1 << i);
                    if (pendingRequests & requestToCheck)
                        return static_cast<StopReason>(requestToCheck);
                }
                return StopReason::None;
            };

            // Fetch the top priority stop request and finish servicing it before entertaining
            // another one. This reduces complexity as servicing a different stop request while
            // one is in still being processed may result in unexpected state change that the
            // the current stop request handler is unprepared to handle.
            if (m_currentStopReason == StopReason::None) {
                m_currentStopReason = fetchTopPriorityStopReason();
                // We cannot break out early here even if m_currentStopReason is None. That's
                // because we may be in RunOne mode, and the current thread may not be the
                // targetVM thead. So, we must flow thru to the target VM check and wait loop
                // below.
            }

            auto shouldStop = [&] {
                // 1. If the targetVM is already selected, and we're not the targetVM, then stop.
                //    We need to check this first because in RunOne mode, even if there is no more
                //    STW request to service, any VM that is not the targetVM still needs to stop.
                if (m_targetVM)
                    return m_targetVM != &vm;

                // 2. If there's no more STW requests, then we don't need to stop.
                //    This is superseded by the condition above during RunOne mode.
                if (m_currentStopReason == StopReason::None)
                    return false;

                // 3. We have a STW request. If not all active VMs are at the stopping point yet,
                //    then stop and wait for the last VM to stop.
                return m_numberOfStoppedVMs.loadRelaxed() != m_numberOfActiveVMs;
            };

            while (shouldStop())
                m_worldConditionVariable.wait(m_worldLock);

            // We can only get here under one the following possible circumstance:
            // 1. No targetVM thread was specified (therefore, any thread may service this stop)
            //    and this is the last thread that stopped. Or ...
            // 2. This is a subsequent iteration through this loop after context switches (see the
            //    m_worldConditionVariable.notifyAll() at the bottom of the loop). In which case,
            //    the targetVM thread is the only one that can get past the wait() above. Or ...
            // 3. We're executing in RunOne mode and entering this function due to a subsequent
            //    stop request. In that case, all other threads remained stopped, and only the
            //    targetVM thread is allowed to run.
            RELEASE_ASSERT(!m_targetVM || m_targetVM == &vm);

            // Now we can break out of the handler loop is there are no more requests.
            if (m_currentStopReason == StopReason::None) {
                if (m_useRunOneMode) {
                    m_worldMode = Mode::RunOne;
                    RELEASE_ASSERT(m_targetVM);
                } else if (m_worldMode != Mode::RunAll)
                    resumeTheWorld(); // Sets m_worldMode = Mode::RunAll.
                break; // Exit this loop.
            }

            m_targetVM = &vm;
            m_worldMode = Mode::Stopped;
        }

        auto status = STW_RESUME();
        switch (m_currentStopReason) {
        case StopReason::GC:
            RELEASE_ASSERT_NOT_REACHED();
        case StopReason::WasmDebugger:
            status = g_jscConfig.wasmDebuggerOnStop(vm, event);
            break;
        case StopReason::MemoryDebugger:
            status = g_jscConfig.memoryDebuggerStopTheWorld(vm, event);
            break;
        case StopReason::None:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (status.first == IterationStatus::Done) {
            // Done servicing this request. We can't just exit the loop here yet because there
            // may be other requests that need to be serviced. So, we'll just clear the
            // current request and go back to the top of the loop to check if there are other
            // requests. It's safe to clear m_currentStopReason without acquiring m_worldLock
            // here because currently, all other VM threads are already stopped.
            // Same reason for why it's safe to set m_useRunOneMode here.
            auto requestBits = static_cast<StopRequestBits>(m_currentStopReason);
            m_pendingStopRequestBits.exchangeAnd(~requestBits);
            if (m_currentStopReason == StopReason::WasmDebugger)
                m_needsWasmDebuggerOnResume.store(true);
            m_currentStopReason = StopReason::None;

            // targetVM not being specified means that we should not change m_useRunOneMode.
            if (status.second)
                m_useRunOneMode = status.second != STW_RESUME_ALL_TOKEN;
        }

        if (status.second && status.second != STW_RESUME_ALL_TOKEN && status.second != m_targetVM) {
            // A context switch was requested. Wake all so that a context switch can occur, and
            // continue on the targetVM thread.
            Locker lock { m_worldLock };
            m_targetVM = status.second;
            m_worldConditionVariable.notifyAll();
        }
    }

    auto previousCount = m_numberOfStoppedVMs.exchangeSub(1);

    // If we get here, we're either transitioning to RunOne or Running mode.
    RELEASE_ASSERT(!m_targetVM || m_targetVM == &vm);

    // Call post-resume callback once when last VM exits and all VMs are running.
    if (previousCount == 1 && m_needsWasmDebuggerOnResume.exchange(false))
        g_jscConfig.wasmDebuggerOnResume();
}

void VMManager::notifyVMConstruction(VM& vm)
{
    bool needsStopping = false;
    {
        Locker locker { m_worldLock };
        s_recentVM = &vm;
        m_vmList.append(vm.threadContext());
        m_numberOfVMs++;
        needsStopping = m_worldMode != Mode::RunAll;
        if (needsStopping) {
            // Since this is the VM construction point, the VM is obviously not active yet.
            // However, notifyVMStop()'s accounting logic relies on the VM being active in
            // order to stop it. So, pretend the VM is active and undo this on exit.
            incrementActiveVMs(vm);
        }
    }
    if (needsStopping) {
        // If a stop is in progress, we cannot proceed onto initializing (i.e. mutating)
        // the heap in the VM constructor. GlobalGC may be expecting a quiescent world
        // state at this point. So, go park this thread if needed.
        vm.requestStop();
        notifyVMStop(vm, StopTheWorldEvent::VMCreated); // Cannot be called while holding m_worldLock.

        Locker locker { m_worldLock };
        decrementActiveVMs(vm);
    }
}

void VMManager::notifyVMDestruction(VM& vm)
{
    bool worldIsStopped = false;
    {
        Locker locker { m_worldLock };
        if (s_recentVM == &vm)
            s_recentVM = nullptr;
        m_vmList.remove(vm.threadContext());
        m_numberOfVMs--;

        worldIsStopped = (m_worldMode != Mode::RunAll);
    }
    if (worldIsStopped) {
        // If a stop is in progress, some threads may have stopped, and may need to be
        // woken up.
        handleVMDestructionWhileWorldStopped(vm);
    }
}

void VMManager::notifyVMActivation(VM& vm)
{
    // The main concern for this notification is that if we are currently Stopping or Stopped,
    // then we need to block this newly activated VM from executing.
    bool needsStopping = false;
    {
        Locker lock { m_worldLock };
        s_recentVM = &vm;
        incrementActiveVMs(vm);
        needsStopping = m_worldMode != Mode::RunAll;
    }
    if (needsStopping) {
        vm.requestStop();
        notifyVMStop(vm, StopTheWorldEvent::VMActivated);
    }
}

void VMManager::notifyVMDeactivation(VM& vm)
{
    // The main concern for this notification is that if we are currently Stopping or Stopped,
    // then we may need to wake up another thread to potentially service the StopTheWorld
    // request. That's because this may be the last thread that STW is waiting on.
    Locker lock { m_worldLock };
    decrementActiveVMs(vm);
}

void VMManager::handleVMDestructionWhileWorldStopped(VM& vm)
{
    Locker lock { m_worldLock };
    if (m_worldMode == Mode::RunAll) {
        // World has been resumed already. Nothing more to do.
        return;
    }

    if (!m_numberOfVMs) {
        // We're the last VM, and we're about to shutdown. So, there's nothing to
        // resume. Fix m_worldMode to reflect this.
        m_worldMode = Mode::RunAll;
        return;
    }

    // If we get here, then the world is either in Stopping / Stopped / RunOne state,
    // and there's at least one other VM thread in play out there. Wake them up so
    // that the right thread can take next step.
    if (m_targetVM) {
        // There's a designated targetVM thread to continue in, but we don't have the
        // ability to just wake the desired one up. So, wake up all the threads and let
        // them sort themselves out.
        //
        // But if the targetVM thread is this thread, then pass the control to another
        // thread, any thread. That's because this thread is dying imminently.
        if (m_targetVM == &vm) {
            m_targetVM = nullptr;
            m_useRunOneMode = false;
        }
        m_worldConditionVariable.notifyAll();
    } else {
        // There's no designated targetVM thread. So, just waking up any one thread will do.
        m_worldConditionVariable.notifyOne();
    }
}

} // namespace JSC
