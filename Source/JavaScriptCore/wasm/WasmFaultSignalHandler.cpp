/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "JSCellInlines.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntData.h"
#include "MachineContext.h"
#include "NativeCalleeRegistry.h"
#include "WasmCallee.h"
#include "WasmCapabilities.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmMemory.h"
#include "WasmThunks.h"
#include <wtf/CodePtr.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/threads/Signals.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

using WTF::CodePtr;

#if CPU(ARM64E) && HAVE(HARDENED_MACH_EXCEPTIONS)
void* presignedTrampoline { nullptr };
#endif

namespace {
namespace WasmFaultSignalHandlerInternal {
static constexpr bool verbose = false;
}
}

static SignalAction trapHandler(Signal signal, SigInfo& sigInfo, PlatformRegisters& context)
{
    RELEASE_ASSERT(signal == Signal::AccessFault);

    auto instructionPointer = MachineContext::instructionPointer(context);
    if (!instructionPointer)
        return SignalAction::NotHandled;
    void* faultingInstruction = instructionPointer->untaggedPtr();
    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "starting handler for fault at: ", RawPointer(faultingInstruction));

#if ENABLE(JIT)
    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "JIT memory start: ", RawPointer(startOfFixedExecutableMemoryPool()), " end: ", RawPointer(endOfFixedExecutableMemoryPool()));
#endif
    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "WasmIPInt memory start: ", RawPointer(untagCodePtr<void*, CFunctionPtrTag>(LLInt::wasmIPIntPCRangeStart)), " end: ", RawPointer(untagCodePtr<void*, CFunctionPtrTag>(LLInt::wasmIPIntPCRangeEnd)));
    // First we need to make sure we are in JIT code or Wasm IPInt code before we can aquire any locks. Otherwise,
    // we might have crashed in code that is already holding one of the locks we want to aquire.
    assertIsNotTagged(faultingInstruction);
    if (isJITPC(faultingInstruction) || LLInt::isWasmIPIntPC(faultingInstruction)) {
        std::optional<Wasm::ExceptionType> exception;
        {
            void* faultingAddress = sigInfo.faultingAddress;
            dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "checking faulting address: ", RawPointer(faultingAddress), " is in an active fast memory");

            // Trapping null access for WasmGC Array / Struct.
            if (std::bit_cast<uintptr_t>(faultingAddress) < lowestAccessibleAddress())
                exception = ExceptionType::NullAccess;
            else if (Wasm::Memory::addressIsInGrowableOrFastMemory(faultingAddress))
                exception = ExceptionType::OutOfBoundsMemoryAccess;
        }

        if (exception) {
            dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "found active fast memory for faulting address");

            auto didFaultInWasm = [](void* faultingInstruction) -> std::tuple<bool, Wasm::Callee*> {
                if (LLInt::isWasmIPIntPC(faultingInstruction))
                    return { true, nullptr };
                auto& calleeRegistry = NativeCalleeRegistry::singleton();
                Locker locker { calleeRegistry.getLock() };
                for (auto* callee : calleeRegistry.allCallees()) {
                    if (callee->category() != NativeCallee::Category::Wasm)
                        continue;
                    auto* wasmCallee = uncheckedDowncast<Wasm::Callee>(callee);
                    auto [start, end] = wasmCallee->range();
                    dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "function start: ", RawPointer(start), " end: ", RawPointer(end));
                    if (start <= faultingInstruction && faultingInstruction < end) {
                        dataLogLnIf(WasmFaultSignalHandlerInternal::verbose, "found match");
                        return { true, wasmCallee };
                    }
                }
                return { false, nullptr };
            };

            auto [isWasm, callee] = didFaultInWasm(faultingInstruction);
            if (isWasm) {
                auto* instance = jsSecureCast<JSWebAssemblyInstance*>(static_cast<JSCell*>(MachineContext::wasmInstancePointer(context)));
                instance->setFaultPC(exception.value(), faultingInstruction);
#if CPU(ARM64E) && HAVE(HARDENED_MACH_EXCEPTIONS)
                if (g_wtfConfig.signalHandlers.useHardenedHandler) {
                    MachineContext::setInstructionPointer(context, presignedTrampoline);
                    return SignalAction::Handled;
                }
#endif
                MachineContext::setInstructionPointer(context, LLInt::getCodePtr<CFunctionPtrTag>(wasm_throw_from_fault_handler_trampoline_reg_instance));
                return SignalAction::Handled;
            }
        }
    }
    return SignalAction::NotHandled;
}

void activateSignalingMemory()
{
    static std::once_flag once;
    std::call_once(once, [] {
        if (!Wasm::isSupported())
            return;

        if (!Options::useWasmFaultSignalHandler())
            return;

        activateSignalHandlersFor(Signal::AccessFault);
    });
}

void prepareSignalingMemory()
{
    static std::once_flag once;
    std::call_once(once, [] {
        if (!Wasm::isSupported())
            return;

        if (!Options::useWasmFaultSignalHandler())
            return;

#if CPU(ARM64E) && HAVE(HARDENED_MACH_EXCEPTIONS)
        presignedTrampoline = g_wtfConfig.signalHandlers.presignReturnPCForHandler(LLInt::getCodePtr<NoPtrTag>(wasm_throw_from_fault_handler_trampoline_reg_instance));
#endif
        addSignalHandler(Signal::AccessFault, [] (Signal signal, SigInfo& sigInfo, PlatformRegisters& ucontext) {
            return trapHandler(signal, sigInfo, ucontext);
        });
    });
}

ptrdiff_t maxAcceptableOffsetForNullReference()
{
#if CPU(ADDRESS64)
    if (!Options::useWasmFaultSignalHandler())
        return 0;

    ptrdiff_t address = lowestAccessibleAddress();
    ptrdiff_t nullValue =JSValue::encode(jsNull());
    ptrdiff_t accessed = sizeof(v128_t) * 2; // Paired v128_t access.
    if (address < (nullValue + accessed))
        return 0;
    return address - nullValue - accessed;
#else
    return 0;
#endif
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
