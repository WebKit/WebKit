/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "VMTraps.h"

#include "CallFrameInlines.h"
#include "CodeBlock.h"
#include "CodeBlockSet.h"
#include "DFGCommonData.h"
#include "ExceptionHelpers.h"
#include "HeapInlines.h"
#include "JSCJSValueInlines.h"
#include "LLIntPCRanges.h"
#include "MachineContext.h"
#include "MacroAssemblerCodeRef.h"
#include "VMEntryScope.h"
#include "VMTrapsInlines.h"
#include "Watchdog.h"
#include <wtf/ProcessID.h>
#include <wtf/ThreadMessage.h>
#include <wtf/threads/Signals.h>

namespace JSC {

#if ENABLE(SIGNAL_BASED_VM_TRAPS)

struct VMTraps::SignalContext {
private:
    SignalContext(PlatformRegisters& registers, CodePtr<PlatformRegistersPCPtrTag> trapPC)
        : registers(registers)
        , trapPC(trapPC)
        , stackPointer(MachineContext::stackPointer(registers))
        , framePointer(MachineContext::framePointer(registers))
    { }

public:
    static std::optional<SignalContext> tryCreate(PlatformRegisters& registers)
    {
        auto instructionPointer = MachineContext::instructionPointer(registers);
        if (!instructionPointer)
            return std::nullopt;
        return SignalContext(registers, *instructionPointer);
    }

    PlatformRegisters& registers;
    CodePtr<PlatformRegistersPCPtrTag> trapPC;
    void* stackPointer;
    void* framePointer;
};

inline static bool vmIsInactive(VM& vm)
{
    return !vm.entryScope && !vm.ownerThread();
}

static bool isSaneFrame(CallFrame* frame, CallFrame* calleeFrame, EntryFrame* entryFrame, StackBounds stackBounds)
{
    if (reinterpret_cast<void*>(frame) >= reinterpret_cast<void*>(entryFrame))
        return false;
    if (calleeFrame >= frame)
        return false;
    return stackBounds.contains(frame);
}

void VMTraps::tryInstallTrapBreakpoints(VMTraps::SignalContext& context, StackBounds stackBounds)
{
    // This must be the initial signal to get the mutator thread's attention.
    // Let's get the thread to break at invalidation points if needed.
    VM& vm = this->vm();
    void* trapPC = context.trapPC.untaggedPtr();
    // We must ensure we're in JIT/LLint code. If we are, we know a few things:
    // - The JS thread isn't holding the malloc lock. Therefore, it's safe to malloc below.
    // - The JS thread isn't holding the CodeBlockSet lock.
    // If we're not in JIT/LLInt code, we can't run the C++ code below because it
    // mallocs, and we must prove the JS thread isn't holding the malloc lock
    // to be able to do that without risking a deadlock.
    if (!isJITPC(trapPC) && !LLInt::isLLIntPC(trapPC))
        return;

    CallFrame* callFrame = reinterpret_cast<CallFrame*>(context.framePointer);

    // Even though we know the mutator thread is not in C++ code and therefore, not holding
    // this lock, the sampling profiler may have acquired this lock before acquiring
    // ThreadSuspendLocker and suspending the mutator. Since VMTraps acquires the
    // ThreadSuspendLocker first, we can deadlock with the Sampling Profiler thread, and
    // leave the mutator in a suspended state, or forever blocked on the codeBlockSet lock.
    Lock& codeBlockSetLock = vm.heap.codeBlockSet().getLock();
    if (!codeBlockSetLock.tryLock())
        return;

    Locker codeBlockSetLocker { AdoptLock, codeBlockSetLock };

    CodeBlock* foundCodeBlock = nullptr;
    EntryFrame* entryFrame = vm.topEntryFrame;

    // We don't have a callee to start with. So, use the end of the stack to keep the
    // isSaneFrame() checker below happy for the first iteration. It will still check
    // to ensure that the address is in the stackBounds.
    CallFrame* calleeFrame = reinterpret_cast<CallFrame*>(stackBounds.end());

    if (!entryFrame || !callFrame)
        return; // Not running JS code. Let the SignalSender try again later.

    do {
        if (!isSaneFrame(callFrame, calleeFrame, entryFrame, stackBounds))
            return; // Let the SignalSender try again later.

        CodeBlock* candidateCodeBlock = callFrame->unsafeCodeBlock();
        if (candidateCodeBlock && vm.heap.codeBlockSet().contains(codeBlockSetLocker, candidateCodeBlock)) {
            foundCodeBlock = candidateCodeBlock;
            break;
        }

        calleeFrame = callFrame;
        callFrame = callFrame->callerFrame(entryFrame);

    } while (callFrame && entryFrame);

    if (!foundCodeBlock) {
        // We may have just entered the frame and the codeBlock pointer is not
        // initialized yet. Just bail and let the SignalSender try again later.
        return;
    }

    if (foundCodeBlock->canInstallVMTrapBreakpoints()) {
        if (!m_lock->tryLock())
            return; // Let the SignalSender try again later.

        Locker locker { AdoptLock, *m_lock };
        if (!needHandling(VMTraps::AsyncEvents)) {
            // Too late. Someone else already handled the trap.
            return;
        }

        if (!foundCodeBlock->hasInstalledVMTrapsBreakpoints())
            foundCodeBlock->installVMTrapBreakpoints();
        return;
    }
}

void VMTraps::invalidateCodeBlocksOnStack()
{
    invalidateCodeBlocksOnStack(vm().topCallFrame);
}

void VMTraps::invalidateCodeBlocksOnStack(CallFrame* topCallFrame)
{
    Locker codeBlockSetLocker { vm().heap.codeBlockSet().getLock() };
    invalidateCodeBlocksOnStack(codeBlockSetLocker, topCallFrame);
}
    
void VMTraps::invalidateCodeBlocksOnStack(Locker<Lock>&, CallFrame* topCallFrame)
{
    if (!m_needToInvalidatedCodeBlocks)
        return;

    m_needToInvalidatedCodeBlocks = false;

    EntryFrame* entryFrame = vm().topEntryFrame;
    CallFrame* callFrame = topCallFrame;

    if (!entryFrame)
        return; // Not running JS code. Nothing to invalidate.

    while (callFrame) {
        CodeBlock* codeBlock = callFrame->isAnyWasmCallee() ? nullptr : callFrame->codeBlock();
        if (codeBlock && JITCode::isOptimizingJIT(codeBlock->jitType()))
            codeBlock->jettison(Profiler::JettisonDueToVMTraps);
        callFrame = callFrame->callerFrame(entryFrame);
    }
}

class VMTraps::SignalSender final : public AutomaticThread {
public:
    using Base = AutomaticThread;
    SignalSender(const AbstractLocker& locker, VM& vm)
        : Base(locker, vm.traps().m_lock, vm.traps().m_condition.copyRef())
        , m_vm(vm)
    {
        activateSignalHandlersFor(Signal::AccessFault);
    }

    static void initializeSignals()
    {
        static std::once_flag once;
        std::call_once(once, [] {
            addSignalHandler(Signal::AccessFault, [] (Signal signal, SigInfo&, PlatformRegisters& registers) -> SignalAction {
                RELEASE_ASSERT(signal == Signal::AccessFault);
                auto signalContext = SignalContext::tryCreate(registers);
                if (!signalContext)
                    return SignalAction::NotHandled;

                void* trapPC = signalContext->trapPC.untaggedPtr();
                if (!isJITPC(trapPC))
                    return SignalAction::NotHandled;

                CodeBlock* currentCodeBlock = DFG::codeBlockForVMTrapPC(trapPC);
                if (!currentCodeBlock) {
                    // Either we trapped for some other reason, e.g. Wasm OOB, or we didn't properly monitor the PC. Regardless, we can't do much now...
                    return SignalAction::NotHandled;
                }
                ASSERT(currentCodeBlock->hasInstalledVMTrapsBreakpoints());
                VM& vm = currentCodeBlock->vm();

                // This signal handler is triggered by the mutator thread due to the installed halt instructions
                // in JIT code (which we already confirmed above). Hence, the current thread (the mutator)
                // cannot be in C++ code, and therefore, cannot be already holding the codeBlockSet lock.
                // The only time the codeBlockSet lock could be in contention is if the Sampling Profiler thread
                // is holding it. In that case, we'll simply wait till the Sampling Profiler is done with it.
                // There are no lock ordering issues w.r.t. the Sampling Profiler on this code path.
                //
                // Note that it is not ok to return SignalAction::NotHandled here if we see contention. Doing
                // so will cause the fault to be handled by the default handler, which will crash. It is also not
                // productive to return SignalAction::Handled on contention. Doing so will simply trigger this
                // fault handler over and over again. We might as well wait for the Sampling Profiler to release
                // the lock, which is what we do here.
                Locker codeBlockSetLocker { vm.heap.codeBlockSet().getLock() };

                bool sawCurrentCodeBlock = false;
                vm.heap.forEachCodeBlockIgnoringJITPlans(codeBlockSetLocker, [&] (CodeBlock* codeBlock) {
                    // We want to jettison all code blocks that have vm traps breakpoints, otherwise we could hit them later.
                    if (codeBlock->hasInstalledVMTrapsBreakpoints()) {
                        if (currentCodeBlock == codeBlock)
                            sawCurrentCodeBlock = true;

                        codeBlock->jettison(Profiler::JettisonDueToVMTraps);
                    }
                });
                RELEASE_ASSERT(sawCurrentCodeBlock);
                
                return SignalAction::Handled; // We've successfully jettisoned the codeBlocks.
            });
        });
    }

    const char* name() const final
    {
        return "JSC VMTraps Signal Sender Thread";
    }

    VMTraps& traps() { return m_vm.traps(); }

private:
    PollResult poll(const AbstractLocker&) final
    {
        if (traps().m_isShuttingDown)
            return PollResult::Stop;

        if (!traps().needHandling(VMTraps::AsyncEvents))
            return PollResult::Wait;

        // We know that no trap could have been processed and re-added because we are holding the lock.
        if (vmIsInactive(m_vm))
            return PollResult::Wait;
        return PollResult::Work;
    }

    WorkResult work() final
    {
        VM& vm = m_vm;

        auto optionalOwnerThread = vm.ownerThread();
        if (optionalOwnerThread) {
            ThreadSuspendLocker locker;
            sendMessage(locker, *optionalOwnerThread.value().get(), [&] (PlatformRegisters& registers) -> void {
                auto signalContext = SignalContext::tryCreate(registers);
                if (!signalContext)
                    return;

                auto ownerThread = vm.apiLock().ownerThread();
                // We can't mess with a thread unless it's the one we suspended.
                if (!ownerThread || ownerThread != optionalOwnerThread)
                    return;

                Thread& thread = *ownerThread->get();
                vm.traps().tryInstallTrapBreakpoints(*signalContext, thread.stack());
            });
        }

        {
            Locker locker { *traps().m_lock };
            if (traps().m_isShuttingDown)
                return WorkResult::Stop;
            traps().m_condition->waitFor(*traps().m_lock, 1_ms);
        }
        return WorkResult::Continue;
    }

    VM& m_vm;
};

#endif // ENABLE(SIGNAL_BASED_VM_TRAPS)

void VMTraps::initializeSignals()
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (!Options::usePollingTraps()) {
        ASSERT(Options::useJIT());
        SignalSender::initializeSignals();
    }
#endif
}

void VMTraps::willDestroyVM()
{
    m_isShuttingDown = true;
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (m_signalSender) {
        {
            Locker locker { *m_lock };
            if (!m_signalSender->tryStop(locker))
                m_condition->notifyAll(locker);
        }
        m_signalSender->join();
        m_signalSender = nullptr;
    }
#endif
}

void VMTraps::fireTrap(VMTraps::Event event)
{
    ASSERT(!vm().currentThreadIsHoldingAPILock());
    ASSERT(onlyContainsAsyncEvents(event));
    {
        Locker locker { *m_lock };
        ASSERT(!m_isShuttingDown);
        setTrapBit(event);
        m_needToInvalidatedCodeBlocks = true;
    }
    
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (!Options::usePollingTraps()) {
        // sendSignal() can loop until it has confirmation that the mutator thread
        // has received the trap request. We'll call it from another thread so that
        // fireTrap() does not block.
        Locker locker { *m_lock };
        if (!m_signalSender)
            m_signalSender = adoptRef(new SignalSender(locker, vm()));
        m_condition->notifyAll(locker);
    }
#endif
}

void VMTraps::handleTraps(VMTraps::BitField mask)
{
    VM& vm = this->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(onlyContainsAsyncEvents(mask));
    ASSERT(needHandling(mask));
    ASSERT(!hasTrapBit(DeferTrapHandling));

    if (isDeferringTermination())
        mask &= ~NeedTermination;

    {
        Locker codeBlockSetLocker { vm.heap.codeBlockSet().getLock() };
        vm.heap.forEachCodeBlockIgnoringJITPlans(codeBlockSetLocker, [&] (CodeBlock* codeBlock) {
            // We want to jettison all code blocks that have vm traps breakpoints, otherwise we could hit them later.
            if (codeBlock->hasInstalledVMTrapsBreakpoints())
                codeBlock->jettison(Profiler::JettisonDueToVMTraps);
        });
    }

    while (needHandling(mask)) {
        auto event = takeTopPriorityTrap(mask);
        switch (event) {
        case NeedDebuggerBreak:
            dataLog("VM ", RawPointer(&vm), " on pid ", getCurrentProcessID(), " received NeedDebuggerBreak trap\n");
            invalidateCodeBlocksOnStack(vm.topCallFrame);
            break;

        case NeedShellTimeoutCheck:
            RELEASE_ASSERT(g_jscConfig.shellTimeoutCheckCallback);
            g_jscConfig.shellTimeoutCheckCallback(vm);
            break;

        case NeedWatchdogCheck:
            ASSERT(vm.watchdog());
            if (LIKELY(!vm.watchdog()->isActive() || !vm.watchdog()->shouldTerminate(vm.entryScope->globalObject())))
                continue;
            vm.setTerminationInProgress(true);
            FALLTHROUGH;

        case NeedTermination:
            ASSERT(vm.terminationInProgress());
            scope.release();
            if (!isDeferringTermination())
                vm.throwTerminationException();
            return;

        case NeedExceptionHandling:
        case DeferTrapHandling:
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}

auto VMTraps::takeTopPriorityTrap(VMTraps::BitField mask) -> Event
{
    Locker locker { *m_lock };

    // Note: the EventBitShift is already sorted in highest to lowest priority
    // i.e. a bit shift of 0 is highest priority, etc.
    for (unsigned i = 0; i < NumberOfEvents; ++i) {
        Event event = static_cast<Event>(1 << i);
        if (hasTrapBit(event, mask)) {
            clearTrapBit(event);
            return event;
        }
    }
    return NoEvent;
}

void VMTraps::deferTerminationSlow(DeferAction)
{
    ASSERT(m_deferTerminationCount == 1);

    VM& vm = this->vm();
    if (vm.hasPendingTerminationException()) {
        ASSERT(vm.terminationInProgress());
        vm.clearException();
        m_suspendedTerminationException = true;
    }
}

void VMTraps::undoDeferTerminationSlow(DeferAction deferAction)
{
    ASSERT(m_deferTerminationCount == 0);

    VM& vm = this->vm();
    ASSERT(vm.terminationInProgress());
    if (m_suspendedTerminationException || (deferAction == DeferAction::DeferUntilEndOfScope)) {
        vm.throwTerminationException();
        m_suspendedTerminationException = false;
    } else if (deferAction == DeferAction::DeferForAWhile)
        setTrapBit(NeedTermination); // Let the next trap check handle it.
}

VMTraps::VMTraps()
    : m_lock(Box<Lock>::create())
    , m_condition(AutomaticThreadCondition::create())
{
}

VMTraps::~VMTraps()
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    ASSERT(!m_signalSender);
#endif
}

} // namespace JSC
