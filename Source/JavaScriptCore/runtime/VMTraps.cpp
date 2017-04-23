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
    SignalContext(mcontext_t& mcontext)
        : mcontext(mcontext)
        , trapPC(MachineContext::instructionPointer(mcontext))
        , stackPointer(MachineContext::stackPointer(mcontext))
        , framePointer(MachineContext::framePointer(mcontext))
    {
#if CPU(X86_64) || CPU(X86)
        // On X86_64, SIGTRAP reports the address after the trapping PC. So, dec by 1.
        trapPC = reinterpret_cast<uint8_t*>(trapPC) - 1;
#endif
    }

    void adjustPCToPointToTrappingInstruction()
    {
#if CPU(X86_64) || CPU(X86)
        MachineContext::instructionPointer(mcontext) = trapPC;
#endif
    }

    mcontext_t& mcontext;
    void* trapPC;
    void* stackPointer;
    void* framePointer;
};

inline static bool vmIsInactive(VM& vm)
{
    return !vm.entryScope && !vm.ownerThread();
}

struct VMAndStackBounds {
    VM* vm;
    StackBounds stackBounds;
};

static Expected<VMAndStackBounds, VMTraps::Error> findActiveVMAndStackBounds(SignalContext& context)
{
    VMInspector& inspector = VMInspector::instance();
    auto locker = tryHoldLock(inspector.getLock());
    if (UNLIKELY(!locker))
        return makeUnexpected(VMTraps::Error::LockUnavailable);
    
    VM* activeVM = nullptr;
    StackBounds stackBounds = StackBounds::emptyBounds();
    void* stackPointer = context.stackPointer;
    bool unableToAcquireMachineThreadsLock = false;
    inspector.iterate(locker, [&] (VM& vm) {
        if (vmIsInactive(vm))
            return VMInspector::FunctorStatus::Continue;

        auto& machineThreads = vm.heap.machineThreads();
        auto machineThreadsLocker = tryHoldLock(machineThreads.getLock());
        if (UNLIKELY(!machineThreadsLocker)) {
            unableToAcquireMachineThreadsLock = true;
            return VMInspector::FunctorStatus::Continue; // Try next VM.
        }

        const auto& threadList = machineThreads.threadsListHead(machineThreadsLocker);
        for (MachineThreads::MachineThread* thread = threadList.head(); thread; thread = thread->next()) {
            RELEASE_ASSERT(thread->stackBase());
            RELEASE_ASSERT(thread->stackEnd());
            if (stackPointer <= thread->stackBase() && stackPointer >= thread->stackEnd()) {
                activeVM = &vm;
                stackBounds = StackBounds(thread->stackBase(), thread->stackEnd());
                return VMInspector::FunctorStatus::Done;
            }
        }
        return VMInspector::FunctorStatus::Continue;
    });

    if (!activeVM && unableToAcquireMachineThreadsLock)
        return makeUnexpected(VMTraps::Error::LockUnavailable);
    return VMAndStackBounds { activeVM, stackBounds };
}

static void installSignalHandler()
{
    installSignalHandler(Signal::Trap, [] (int, siginfo_t*, void* uap) -> SignalAction {
        SignalContext context(static_cast<ucontext_t*>(uap)->uc_mcontext);

        if (!isJITPC(context.trapPC))
            return SignalAction::NotHandled;

        // FIXME: This currently eats all traps including jit asserts we should make sure this
        // always works. https://bugs.webkit.org/show_bug.cgi?id=171039
        auto activeVMAndStackBounds = findActiveVMAndStackBounds(context);
        if (!activeVMAndStackBounds)
            return SignalAction::Handled; // Let the SignalSender try again later.

        VM* vm = activeVMAndStackBounds.value().vm;
        if (vm) {
            VMTraps& traps = vm->traps();
            if (!traps.needTrapHandling())
                return SignalAction::Handled; // The polling code beat us to handling the trap already.

            auto expectedSuccess = traps.tryJettisonCodeBlocksOnStack(context);
            if (!expectedSuccess)
                return SignalAction::Handled; // Let the SignalSender try again later.
            if (expectedSuccess.value())
                return SignalAction::Handled; // We've success jettison the codeBlocks.
        }

        return SignalAction::Handled;
    });
}

ALWAYS_INLINE static CallFrame* sanitizedTopCallFrame(CallFrame* topCallFrame)
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

    auto codeBlockSetLocker = tryHoldLock(vm.heap.codeBlockSet().getLock());
    if (!codeBlockSetLocker)
        return; // Let the SignalSender try again later.

    {
        auto& allocator = ExecutableAllocator::singleton();
        auto allocatorLocker = tryHoldLock(allocator.getLock());
        if (!allocatorLocker)
            return; // Let the SignalSender try again later.

        if (allocator.isValidExecutableMemory(allocatorLocker, trapPC)) {
            if (vm.isExecutingInRegExpJIT) {
                // We need to do this because a regExpJIT frame isn't a JS frame.
                callFrame = sanitizedTopCallFrame(vm.topCallFrame);
            }
        } else if (LLInt::isLLIntPC(trapPC)) {
            // The framePointer probably has the callFrame. We're good to go.
        } else {
            // We resort to topCallFrame to see if we can get anything
            // useful. We usually get here when we're executing C code.
            callFrame = sanitizedTopCallFrame(vm.topCallFrame);
        }
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
        auto locker = tryHoldLock(m_lock);
        if (!locker)
            return; // Let the SignalSender try again later.

        if (!foundCodeBlock->hasInstalledVMTrapBreakpoints())
            foundCodeBlock->installVMTrapBreakpoints();
        return;
    }
}

auto VMTraps::tryJettisonCodeBlocksOnStack(SignalContext& context) -> Expected<bool, Error>
{
    VM& vm = this->vm();
    auto codeBlockSetLocker = tryHoldLock(vm.heap.codeBlockSet().getLock());
    if (!codeBlockSetLocker)
        return makeUnexpected(Error::LockUnavailable);

    CallFrame* topCallFrame = reinterpret_cast<CallFrame*>(context.framePointer);
    void* trapPC = context.trapPC;
    bool trapPCIsVMTrap = false;
    
    vm.heap.forEachCodeBlockIgnoringJITPlans(codeBlockSetLocker, [&] (CodeBlock* codeBlock) {
        if (!codeBlock->hasInstalledVMTrapBreakpoints())
            return false; // Not found yet.

        JITCode* jitCode = codeBlock->jitCode().get();
        ASSERT(JITCode::isOptimizingJIT(jitCode->jitType()));
        if (jitCode->dfgCommon()->isVMTrapBreakpoint(trapPC)) {
            trapPCIsVMTrap = true;
            // At the codeBlock trap point, we're guaranteed that:
            // 1. the pc is not in the middle of any range of JIT code which invalidation points
            //    may write over. Hence, it's now safe to patch those invalidation points and
            //    jettison the codeBlocks.
            // 2. The top frame must be an optimized JS frame.
            ASSERT(codeBlock == topCallFrame->codeBlock());
            codeBlock->jettison(Profiler::JettisonDueToVMTraps);
            return true;
        }

        return false; // Not found yet.
    });

    if (!trapPCIsVMTrap)
        return false;

    invalidateCodeBlocksOnStack(codeBlockSetLocker, topCallFrame);

    // Re-run the trapping instruction now that we've patched it with the invalidation
    // OSR exit off-ramp.
    context.adjustPCToPointToTrappingInstruction();
    return true;
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

#endif // ENABLE(SIGNAL_BASED_VM_TRAPS)

VMTraps::VMTraps()
{
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (!Options::usePollingTraps()) {
        static std::once_flag once;
        std::call_once(once, [] {
            installSignalHandler();
        });
    }
#endif
}

void VMTraps::willDestroyVM()
{
    m_isShuttingDown = true;
    WTF::storeStoreFence();
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    while (!m_signalSenders.isEmpty()) {
        RefPtr<SignalSender> sender;
        {
            // We don't want to be holding the VMTraps lock when calling
            // SignalSender::willDestroyVM() because SignalSender::willDestroyVM()
            // will acquire the SignalSender lock, and SignalSender::send() needs
            // to acquire these locks in the opposite order.
            auto locker = holdLock(m_lock);
            sender = m_signalSenders.takeAny();
            if (!sender)
                break;
        }
        sender->willDestroyVM();
    }
    ASSERT(m_signalSenders.isEmpty());
#endif
}

#if ENABLE(SIGNAL_BASED_VM_TRAPS)
void VMTraps::addSignalSender(VMTraps::SignalSender* sender)
{
    auto locker = holdLock(m_lock);
    m_signalSenders.add(sender);
}

void VMTraps::removeSignalSender(VMTraps::SignalSender* sender)
{
    auto locker = holdLock(m_lock);
    m_signalSenders.remove(sender);
}

void VMTraps::SignalSender::willDestroyVM()
{
    auto locker = holdLock(m_lock);
    m_vm = nullptr;
}

void VMTraps::SignalSender::send()
{
    while (true) {
        // We need a nested scope so that we'll release the lock before we sleep below.
        {
            auto locker = holdLock(m_lock);
            if (!m_vm)
                break;

            VM& vm = *m_vm;
            auto optionalOwnerThread = vm.ownerThread();
            if (optionalOwnerThread) {
                sendMessage(*optionalOwnerThread.value().get(), [] (siginfo_t*, ucontext_t* ucontext) -> void {
                    SignalContext context(ucontext->uc_mcontext);
                    auto activeVMAndStackBounds = findActiveVMAndStackBounds(context);
                    if (activeVMAndStackBounds) {
                        VM* vm = activeVMAndStackBounds.value().vm;
                        if (vm) {
                            StackBounds stackBounds = activeVMAndStackBounds.value().stackBounds;
                            VMTraps& traps = vm->traps();
                            if (traps.needTrapHandling())
                                traps.tryInstallTrapBreakpoints(context, stackBounds);
                        }
                    }
                });
                break;
            }

            if (vmIsInactive(vm))
                break;

            VMTraps::Mask mask(m_eventType);
            if (!vm.needTrapHandling(mask))
                break;
        }

        sleepMS(1);
    }

    auto locker = holdLock(m_lock);
    if (m_vm)
        m_vm->traps().removeSignalSender(this);
}
#endif // ENABLE(SIGNAL_BASED_VM_TRAPS)

void VMTraps::fireTrap(VMTraps::EventType eventType)
{
    ASSERT(!vm().currentThreadIsHoldingAPILock());
    {
        auto locker = holdLock(m_lock);
        ASSERT(!m_isShuttingDown);
        setTrapForEvent(locker, eventType);
        m_needToInvalidatedCodeBlocks = true;
    }
    
#if ENABLE(SIGNAL_BASED_VM_TRAPS)
    if (!Options::usePollingTraps()) {
        // sendSignal() can loop until it has confirmation that the mutator thread
        // has received the trap request. We'll call it from another trap so that
        // fireTrap() does not block.
        RefPtr<SignalSender> sender = adoptRef(new SignalSender(vm(), eventType));
        addSignalSender(sender.get());
        Thread::create("jsc.vmtraps.signalling.thread", [sender] {
            sender->send();
        });
    }
#endif
}

void VMTraps::handleTraps(ExecState* exec, VMTraps::Mask mask)
{
    VM& vm = this->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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
            invalidateCodeBlocksOnStack(exec);
            throwException(exec, scope, createTerminatedExecutionException(&vm));
            return;

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}

auto VMTraps::takeTopPriorityTrap(VMTraps::Mask mask) -> EventType
{
    auto locker = holdLock(m_lock);
    for (int i = 0; i < NumberOfEventTypes; ++i) {
        EventType eventType = static_cast<EventType>(i);
        if (hasTrapForEvent(locker, eventType, mask)) {
            clearTrapForEvent(locker, eventType);
            return eventType;
        }
    }
    return Invalid;
}

} // namespace JSC
