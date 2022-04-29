/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "StackOverflowFaultSignalHandler.h"

#include "LLIntData.h"
#include "LLIntPCRanges.h"
#include "MachineContext.h"
#include "VM.h"
#include "VMInlines.h"
#include "wtf/Assertions.h"
#include "wtf/DataLog.h"
#include "wtf/Lock.h"
#include "wtf/NeverDestroyed.h"
#include "wtf/RawPointer.h"
#include "wtf/StdLibExtras.h"
#include <_types/_uint64_t.h>
#include <sys/mman.h>
#include <wtf/threads/Signals.h>

namespace JSC {

namespace {
namespace StackOverflowSignalHandlerInternal {
static constexpr bool verbose = true;
}
}

static Lock contextLock = { };
static LazyNeverDestroyed<HashMap<void*, VM*>> activeVMs;

static void* pageForAddress(void* addr)
{
    uint64_t pageMask = ~(pageSize() - 1);
    uint64_t asInt = reinterpret_cast<uint64_t>(addr);
    uint64_t result = asInt & pageMask;
    ASSERT(asInt >= result && asInt - result < pageSize());
    return reinterpret_cast<void*>(result);
}

static void* findVM(VM* v) WTF_REQUIRES_LOCK(contextLock)
{
    for (auto pair : activeVMs.get()) {
        if (pair.value == v)
            return pair.key;
    }
    return nullptr;
}

static SignalAction trapHandler(Signal signal, SigInfo& sigInfo, PlatformRegisters& context)
{
    RELEASE_ASSERT(signal == Signal::AccessFault);

    auto instructionPointer = MachineContext::instructionPointer(context);
    void* stackPointer = MachineContext::stackPointer(context);
    void* framePointer = MachineContext::framePointer(context);
    void* faultingAddress = sigInfo.faultingAddress;
    if (!instructionPointer || !stackPointer || !faultingAddress)
        return SignalAction::NotHandled;
    void* faultingInstruction = instructionPointer->untaggedExecutableAddress();    
    assertIsNotTagged(faultingInstruction);
    assertIsNotTagged(faultingAddress);
    assertIsNotTagged(stackPointer);
    assertIsNotTagged(framePointer);
    void* faultingPage = pageForAddress(faultingAddress);

    dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "starting stack overflow handler for fault at insn: ", RawPointer(faultingInstruction), " sp: ", RawPointer(stackPointer), " fp: ", RawPointer(framePointer));
    dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "JIT memory start: ", RawPointer(startOfFixedExecutableMemoryPool()), " end: ", RawPointer(endOfFixedExecutableMemoryPool()));
    dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "Faulting address: ", RawPointer(faultingAddress), " page: ", RawPointer(faultingPage));
    LockHolder holder { contextLock };
    if (StackOverflowSignalHandlerInternal::verbose) {
        for (auto& pair : activeVMs.get())
            dataLogLn("Active VM: ", RawPointer(pair.value), " guard page:", RawPointer(pair.key), " -> ", RawPointer(static_cast<char*>(pair.key) + pageSize()));
    }

    if (!activeVMs->contains(faultingPage)) {
        dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "Faulting address is not a stack guard page, ignoring.");
        return SignalAction::NotHandled;
    }

    VM* vm = activeVMs->get(faultingPage);
    if (!vm) {
        dataLogLn("The VM appears to have been destroyed?");
        RELEASE_ASSERT_NOT_REACHED();
        return SignalAction::NotHandled;
    }

    if (stackPointer < faultingPage || stackPointer > static_cast<char*>(faultingPage) + 2*pageSize()) {
        dataLogLn("The stack pointer appears to be beyond the guard page, or too far before(above) it.");
        RELEASE_ASSERT_NOT_REACHED();
        return SignalAction::NotHandled;
    }

    if (framePointer < faultingPage || framePointer > static_cast<char*>(faultingPage) + 3*pageSize()) {
        dataLogLn("The frame pointer appears to be beyond the guard page, or too far before(above) it.");
        RELEASE_ASSERT_NOT_REACHED();
        return SignalAction::NotHandled;
    }

    if (isJITPC(faultingInstruction) || LLInt::isLLIntPC(faultingInstruction) || LLInt::isWasmLLIntPC(faultingInstruction)) {
        dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "JIT/LLint/WasmLLint?: ", isJITPC(faultingInstruction), LLInt::isLLIntPC(faultingInstruction), LLInt::isWasmLLIntPC(faultingInstruction));
        MachineContext::setInstructionPointer(context,
            LLInt::getCodePtr<CFunctionPtrTag>(llint_throw_from_slow_path_trampoline));
        __asm(".inst 0xd4200000");
        return SignalAction::Handled;
    }

    dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "C++ has used more than the js stack limit");
    __asm(".inst 0xd4200000");
    return SignalAction::Handled;
}

void prepareFastStackOverflow()
{
    static std::once_flag once;
    std::call_once(once, [] {
        if (!Options::useFastStackOverflowChecks())
            return;
        
        addSignalHandler(Signal::AccessFault, [] (Signal signal, SigInfo& sigInfo, PlatformRegisters& ucontext) {
            return trapHandler(signal, sigInfo, ucontext);
        });
        activateSignalHandlersFor(Signal::AccessFault);
        activeVMs.construct();
    });
}

void enableFastStackOverflow(VM& vm)
{
    static std::once_flag once;
    std::call_once(once, [] {
        activateSignalHandlersFor(Signal::AccessFault);
    });

    if (activeVMs->contains(&vm)) {
        dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "Called enableFastStackOverflow twice on the same VM");
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    if (activeVMs->size()) {
        dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "We have more than one vm, I didn't expect this");
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }

    LockHolder holder { contextLock };

    void* guardPage = pageForAddress(vm.addressOfSoftStackLimit() + pageSize());
    
    if (!mmap(guardPage, pageSize(), PROT_NONE, MAP_ANON|MAP_PRIVATE|MAP_FIXED, 0, 0)) {
        dataLogLn("mmap failed with result ", strerror(errno));
        RELEASE_ASSERT_NOT_REACHED();
    }

    dataLogLnIf(StackOverflowSignalHandlerInternal::verbose, "Protected stack page: ", RawPointer(guardPage));
    activeVMs->add(guardPage, &vm);
}

void disableFastStackOverflowForVMThatWillBeDestroyed(VM& vm)
{
    LockHolder holder { contextLock };
    auto* key = findVM(&vm);
    if (key)
        activeVMs->set(key, nullptr);
}


} // namespace JSC
