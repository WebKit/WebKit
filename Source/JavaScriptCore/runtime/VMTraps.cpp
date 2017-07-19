/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "CodeBlock.h"
#include "CodeBlockSet.h"
#include "DFGCommonData.h"
#include "ExceptionHelpers.h"
#include "HeapInlines.h"
#include "LLIntPCRanges.h"
#include "MachineContext.h"
#include "MachineStackMarker.h"
#include "MacroAssembler.h"
#include "VM.h"
#include "VMInspector.h"
#include "Watchdog.h"
#include <wtf/ProcessID.h>
#include <wtf/ThreadMessage.h>
#include <wtf/threads/Signals.h>

namespace JSC {

ALWAYS_INLINE VM& VMTraps::vm() const
{
    return *bitwise_cast<VM*>(bitwise_cast<uintptr_t>(this) - OBJECT_OFFSETOF(VM, m_traps));
}

#if ENABLE(SIGNAL_BASED_VM_TRAPS)

struct SignalContext {
    SignalContext(PlatformRegisters& registers)
        : registers(registers)
        , trapPC(MachineContext::instructionPointer(registers))
        , stackPointer(MachineContext::stackPointer(registers))
        , framePointer(MachineContext::framePointer(registers))
    {
    }

    PlatformRegisters& registers;
    void* trapPC;
    void* stackPointer;
    void* framePointer;
};

inline static bool vmIsInactive(VM& vm)
{
    return !vm.entryScope && !vm.ownerThread();
}

inline CallFrame* sanitizedTopCallFrame(CallFrame* topCallFrame)
{
#if !defined(NDEBUG) && !CPU(ARM) && !CPU(MIPS)
    // prepareForExternalCall() in DFGSpeculativeJIT.h may set topCallFrame to a bad word
    // before calling native functions, but tryInstallTrapBreakpoints() below expects
    // topCallFrame to be null if not set.
#if USE(JSVALUE64)
    const uintptr_t badBeefWord = 0xbadbeef0badbeef;
#else
    const uintptr_t badBeefWord = 0xbadbeef;
#endif
    if (topCallFrame == reinterpret_cast<CallFrame*>(badBeefWord))
        topCallFrame = nullptr;
#endif
    return topCallFrame;
}

static bool isSaneFrame(CallFrame* frame, CallFrame* calleeFrame, VMEntryFrame* entryFrame, StackBounds stackBounds)
{
    if (reinterpret_cast<void*>(frame) >= reinterpret_cast<void*>(entryFrame))
        return false;
    if (calleeFrame >= frame)
        return false;
    return stackBounds.contains(frame);
}
    
void VMTraps::tryInstallTrapBreakpoints(SignalContext& context, StackBounds stackBounds)
{
    // This must be the initial signal to get the mutator thread's attention.
    // Let's get the thread to break at invalidation points if needed.
    VM& vm = this->vm();
    void* trapPC = context.trapPC;

    CallFrame* callFrame = reinterpret_cast<CallFrame*>(context.framePointer);

    auto& lock = vm.heap.codeBlockSet().getLock();
    // If the target thread is in C++ code it might be holding the codeBlockSet lock.
    // if it's in JIT code then it cannot be holding that lock but the GC might be.
    auto codeBlockSetLocker = isJITPC(trapPC) ? holdLock(lock) : tryHoldLock(lock);
    if (!codeBlockSetLocker)
        return; // Let the SignalSender try again later.

    if (!isJITPC(trapPC) && !LLInt::isLLIntPC(trapPC)) {
        // We resort to topCallFrame to see if we can get anything
        // useful. We usually get here when we're executing C code.
        callFrame = sanitizedTopCallFrame(vm.topCallFrame);
    }

    CodeBlock* foundCodeBlock = nullptr;
    VMEntryFrame* vmEntryFrame = vm.topVMEntryFrame;

    // We don't have a callee to start with. So, use the end of the stack to keep the
    // isSaneFrame() checker below happy for the first iteration. It will still check
    // to ensure that the address is in the stackBounds.
    CallFrame* calleeFrame = reinterpret_cast<CallFrame*>(stackBounds.end());

    if (!vmEntryFrame || !callFrame)
        return; // Not running JS code. Let the SignalSender try again later.

    do {
        if (!isSaneFrame(callFrame, calleeFrame, vmEntryFrame, stackBounds))
            return; // Let the SignalSender try again later.

        CodeBlock* candidateCodeBlock = callFrame->codeBlock();
        if (candidateCodeBlock && vm.heap.codeBlockSet().contains(codeBlockSetLocker, candidateCodeBlock)) {
            foundCodeBlock = candidateCodeBlock;
            break;
        }

        calleeFrame = callFrame;
        callFrame = callFrame->callerFrame(vmEntryFrame);

    } while (callFrame && vmEntryFrame);

    if (!foundCodeBlock) {
        // We may have just entered the frame and the codeBlock pointer is not
        // initialized yet. Just bail and let the SignalSender try again later.
        return;
    }

    if (JITCode::isOptimizingJIT(foundCodeBlock->jitType())) {
        auto locker = tryHoldLock(*m_lock);
        if (!locker)
            return; // Let the SignalSender try again later.

        if (!foundCodeBlock->hasInstalledVMTrapBreakpoints())
            foundCodeBlock->installVMTrapBreakpoints();
        return;
    }
}

void VMTraps::invalidateCodeBlocksOnStack()
{
    invalidateCodeBlocksOnStack(vm().topCallFrame);
}

void VMTraps::invalidateCodeBlocksOnStack(ExecState* topCallFrame)
{
    auto codeBlockSetLocker = holdLock(vm().heap.codeBlockSet().getLock());
    invalidateCodeBlocksOnStack(codeBlockSetLocker, topCallFrame);
}
    
void VMTraps::invalidateCodeBlocksOnStack(Locker<Lock>&, ExecState* topCallFrame)
{
    if (!m_needToInvalidatedCodeBlocks)
        return;

    m_needToInvalidatedCodeBlocks = false;

    VMEntryFrame* vmEntryFrame = vm().topVMEntryFrame;
    CallFrame* callFrame = topCallFrame;

    if (!vmEntryFrame)
        return; // Not running JS code. Nothing to invalidate.

    while (callFrame) {
        CodeBlock* codeBlock = callFrame->codeBlock();
        if (codeBlock && JITCode::isOptimizingJIT(codeBlock->jitType()))
            codeBlock->jettison(Profiler::JettisonDueToVMTraps);
        callFrame = callFrame->callerFrame(vmEntryFrame);
    }
}

class VMTraps::SignalSender final : public AutomaticThread {
public:
    using Base = AutomaticThread;
    SignalSender(const AbstractLocker& locker, VM& vm)
        : Base(locker, vm.traps().m_lock, vm.traps().m_trapSet)
        , m_vm(vm)
    {
        static std::once_flag once;
        std::call_once(once, [] {
            installSignalHandler(Signal::BadAccess, [] (Signal, SigInfo&, PlatformRegisters& registers) -> SignalAction {
                SignalContext context(registers);

                if (!isJITPC(context.trapPC))
                    return SignalAction::NotHandled;

                CodeBlock* currentCodeBlock = DFG::codeBlockForVMTrapPC(context.trapPC);
                ASSERT(currentCodeBlock->hasInstalledVMTrapBreakpoints());
                VM& vm = *currentCodeBlock->vm();
                ASSERT(vm.traps().needTrapHandling()); // We should have already jettisoned this code block when we handled the trap.

                // We are in JIT code so it's safe to aquire this lock.
                auto codeBlockSetLocker = holdLock(vm.heap.codeBlockSet().getLock());
                bool sawCurrentCodeBlock = false;
                vm.heap.forEachCodeBlockIgnoringJITPlans(codeBlockSetLocker, [&] (CodeBlock* codeBlock) {
                    // We want to jettison all code blocks that have vm traps breakpoints, otherwise we could hit them later.
                    if (codeBlock->hasInstalledVMTrapBreakpoints()) {
                        if (currentCodeBlock == codeBlock)
                            sawCurrentCodeBlock = true;

                        codeBlock->jettison(Profiler::JettisonDueToVMTraps);
                    }
                    return false;
                });
                RELEASE_ASSERT(sawCurrentCodeBlock);
                
                return SignalAction::Handled; // We've successfully jettisoned the codeBlocks.
            });
        });
    }

    VMTraps& traps() { return m_vm.traps(); }

protected:
    PollResult poll(const AbstractLocker&) override
    {
        if (traps().m_isShuttingDown)
            return PollResult::Stop;

        if (!traps().needTrapHandling())
            return PollResult::Wait;

        // We know that no trap could have been processed and re-added because we are holding the lock.
        if (vmIsInactive(m_vm))
            return PollResult::Wait;
        return PollResult::Work;
    }

    WorkResult work() override
    {
        VM& vm = m_vm;

        auto optionalOwnerThread = vm.ownerThread();
        if (optionalOwnerThread) {
            sendMessage(*optionalOwnerThread.value().get(), [&] (PlatformRegisters& registers) -> void {
                SignalContext context(registers);

                auto ownerThread = vm.apiLock().ownerThread();
                // We can't mess with a thread unless it's the one we suspended.
                if (!ownerThread || ownerThread != optionalOwnerThread)
                    return;

                Thread& thread = *ownerThread->get();
                vm.traps().tryInstallTrapBreakpoints(context, thread.stack());
            });
        }

        {
            auto locker = holdLock(*traps().m_lock);
            if (traps().m_isShuttingDown)
                return WorkResult::Stop;
            traps().m_trapSet->waitFor(*traps().m_lock, 1_ms);
        }
        return WorkResult::Continue;
    }
    
private:

    VM& m_vm;
};

#endif // ENABLE(SIGNAL_BASED_VM_TRAPS)

void VMTraps::willDestroyVM()
{
    m_isShuttingDown = true;
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (m_signalSender) {
        {
            auto locker = holdLock(*m_lock);
            if (!m_signalSender->tryStop(locker))
                m_trapSet->notifyAll(locker);
        }
        m_signalSender->join();
        m_signalSender = nullptr;
    }
#endif
}

void VMTraps::fireTrap(VMTraps::EventType eventType)
{
    ASSERT(!vm().currentThreadIsHoldingAPILock());
    {
        auto locker = holdLock(*m_lock);
        ASSERT(!m_isShuttingDown);
        setTrapForEvent(locker, eventType);
        m_needToInvalidatedCodeBlocks = true;
    }
    
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (!Options::usePollingTraps()) {
        // sendSignal() can loop until it has confirmation that the mutator thread
        // has received the trap request. We'll call it from another trap so that
        // fireTrap() does not block.
        auto locker = holdLock(*m_lock);
        if (!m_signalSender)
            m_signalSender = adoptRef(new SignalSender(locker, vm()));
        m_trapSet->notifyAll(locker);
    }
#endif
}

void VMTraps::handleTraps(ExecState* exec, VMTraps::Mask mask)
{
    VM& vm = this->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    {
        auto codeBlockSetLocker = holdLock(vm.heap.codeBlockSet().getLock());
        vm.heap.forEachCodeBlockIgnoringJITPlans(codeBlockSetLocker, [&] (CodeBlock* codeBlock) {
            // We want to jettison all code blocks that have vm traps breakpoints, otherwise we could hit them later.
            if (codeBlock->hasInstalledVMTrapBreakpoints())
                codeBlock->jettison(Profiler::JettisonDueToVMTraps);
            return false;
        });
    }

    ASSERT(needTrapHandling(mask));
    while (needTrapHandling(mask)) {
        auto eventType = takeTopPriorityTrap(mask);
        switch (eventType) {
        case NeedDebuggerBreak:
            dataLog("VM ", RawPointer(&vm), " on pid ", getCurrentProcessID(), " received NeedDebuggerBreak trap\n");
            invalidateCodeBlocksOnStack(exec);
            break;
                
        case NeedWatchdogCheck:
            ASSERT(vm.watchdog());
            if (LIKELY(!vm.watchdog()->shouldTerminate(exec)))
                continue;
            FALLTHROUGH;

        case NeedTermination:
            throwException(exec, scope, createTerminatedExecutionException(&vm));
            return;

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}

auto VMTraps::takeTopPriorityTrap(VMTraps::Mask mask) -> EventType
{
    auto locker = holdLock(*m_lock);
    for (int i = 0; i < NumberOfEventTypes; ++i) {
        EventType eventType = static_cast<EventType>(i);
        if (hasTrapForEvent(locker, eventType, mask)) {
            clearTrapForEvent(locker, eventType);
            return eventType;
        }
    }
    return Invalid;
}

VMTraps::VMTraps()
    : m_lock(Box<Lock>::create())
    , m_trapSet(AutomaticThreadCondition::create())
{
}

VMTraps::~VMTraps()
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    ASSERT(!m_signalSender);
#endif
}

} // namespace JSC
