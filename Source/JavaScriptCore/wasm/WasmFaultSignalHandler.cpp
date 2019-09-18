/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "WasmFaultSignalHandler.h"

#if ENABLE(WEBASSEMBLY)

#include "ExecutableAllocator.h"
#include "MachineContext.h"
#include "WasmCallee.h"
#include "WasmCalleeRegistry.h"
#include "WasmCapabilities.h"
#include "WasmExceptionType.h"
#include "WasmMemory.h"
#include "WasmThunks.h"

#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/threads/Signals.h>

namespace JSC { namespace Wasm {

namespace {
namespace WasmFaultSignalHandlerInternal {
static constexpr bool verbose = false;
}
}

static bool fastHandlerInstalled { false };

#if ENABLE(WEBASSEMBLY_FAST_MEMORY)

static SignalAction trapHandler(Signal, SigInfo& sigInfo, PlatformRegisters& context)
{
    auto instructionPointer = MachineContext::instructionPointer(context);
    if (!instructionPointer)
        return SignalAction::NotHandled;
    void* faultingInstruction = instructionPointer->untaggedExecutableAddress();
    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "starting handler for fault at: ", RawPointer(faultingInstruction));

    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "JIT memory start: ", RawPointer(startOfFixedExecutableMemoryPool()), " end: ", RawPointer(endOfFixedExecutableMemoryPool()));
    // First we need to make sure we are in JIT code before we can aquire any locks. Otherwise,
    // we might have crashed in code that is already holding one of the locks we want to aquire.
    assertIsNotTagged(faultingInstruction);
    if (isJITPC(faultingInstruction)) {
        bool faultedInActiveFastMemory = false;
        {
            void* faultingAddress = sigInfo.faultingAddress;
            dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "checking faulting address: ", RawPointer(faultingAddress), " is in an active fast memory");
            faultedInActiveFastMemory = Wasm::Memory::addressIsInActiveFastMemory(faultingAddress);
        }
        if (faultedInActiveFastMemory) {
            dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "found active fast memory for faulting address");
            auto& calleeRegistry = CalleeRegistry::singleton();
            auto locker = holdLock(calleeRegistry.getLock());
            for (auto* callee : calleeRegistry.allCallees(locker)) {
                auto [start, end] = callee->range();
                dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "function start: ", RawPointer(start), " end: ", RawPointer(end));
                if (start <= faultingInstruction && faultingInstruction < end) {
                    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "found match");
                    MacroAssemblerCodeRef<JITThunkPtrTag> exceptionStub = Thunks::singleton().existingStub(throwExceptionFromWasmThunkGenerator);
                    // If for whatever reason we don't have a stub then we should just treat this like a regular crash.
                    if (!exceptionStub)
                        break;
                    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "found stub: ", RawPointer(exceptionStub.code().executableAddress()));
                    MachineContext::argumentPointer<1>(context) = reinterpret_cast<void*>(ExceptionType::OutOfBoundsMemoryAccess);
                    MachineContext::setInstructionPointer(context, exceptionStub.code().retagged<CFunctionPtrTag>());
                    return SignalAction::Handled;
                }
            }
        }
    }
    return SignalAction::NotHandled;
}

#endif // ENABLE(WEBASSEMBLY_FAST_MEMORY)

bool fastMemoryEnabled()
{
    return fastHandlerInstalled;
}

void enableFastMemory()
{
#if ENABLE(WEBASSEMBLY_FAST_MEMORY)
    static std::once_flag once;
    std::call_once(once, [] {
        if (!Wasm::isSupported())
            return;

        if (!Options::useWebAssemblyFastMemory())
            return;

        installSignalHandler(Signal::BadAccess, [] (Signal signal, SigInfo& sigInfo, PlatformRegisters& ucontext) {
            return trapHandler(signal, sigInfo, ucontext);
        });

        fastHandlerInstalled = true;
    });
#endif // ENABLE(WEBASSEMBLY_FAST_MEMORY)
}
    
} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

