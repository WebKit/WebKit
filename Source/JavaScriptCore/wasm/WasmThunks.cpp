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
#include "WasmThunks.h"

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "FrameTracers.h"
#include "HeapCellInlines.h"
#include "JITExceptions.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyRuntimeError.h"
#include "LinkBuffer.h"
#include "ScratchRegisterAllocator.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmOMGPlan.h"

namespace JSC { namespace Wasm {

MacroAssemblerCodeRef throwExceptionFromWasmThunkGenerator(const AbstractLocker&)
{
    CCallHelpers jit;

    // The thing that jumps here must move ExceptionType into the argumentGPR1 before jumping here.
    // We're allowed to use temp registers here. We are not allowed to use callee saves.
    jit.loadWasmContext(GPRInfo::argumentGPR2);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, Context::offsetOfVM()), GPRInfo::argumentGPR0);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    CCallHelpers::Call call = jit.call();
    jit.jump(GPRInfo::returnValueGPR);
    jit.breakpoint(); // We should not reach this.

    void* (*throwWasmException)(ExecState*, Wasm::ExceptionType, Wasm::Context*) = [] (ExecState* exec, Wasm::ExceptionType type, Wasm::Context* wasmContext) -> void* {
        VM* vm = wasmContext->vm();
        NativeCallFrameTracer tracer(vm, exec);

        {
            auto throwScope = DECLARE_THROW_SCOPE(*vm);
            JSGlobalObject* globalObject = wasmContext->globalObject();

            JSObject* error; 
            if (type == ExceptionType::StackOverflow)
                error = createStackOverflowError(exec, globalObject);
            else
                error = JSWebAssemblyRuntimeError::create(exec, *vm, globalObject->WebAssemblyRuntimeErrorStructure(), Wasm::errorMessageForExceptionType(type));
            throwException(exec, throwScope, error);
        }

        genericUnwind(vm, exec);
        ASSERT(!!vm->callFrameForCatch);
        ASSERT(!!vm->targetMachinePCForThrow);
        // FIXME: We could make this better:
        // This is a total hack, but the llint (both op_catch and handleUncaughtException)
        // require a cell in the callee field to load the VM. (The baseline JIT does not require
        // this since it is compiled with a constant VM pointer.) We could make the calling convention
        // for exceptions first load callFrameForCatch info call frame register before jumping
        // to the exception handler. If we did this, we could remove this terrible hack.
        // https://bugs.webkit.org/show_bug.cgi?id=170440
        bitwise_cast<uint64_t*>(exec)[CallFrameSlot::callee] = bitwise_cast<uint64_t>(wasmContext->webAssemblyToJSCallee());
        return vm->targetMachinePCForThrow;
    };

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID);
    linkBuffer.link(call, throwWasmException);
    return FINALIZE_CODE(linkBuffer, ("Throw exception from Wasm"));
}

MacroAssemblerCodeRef throwStackOverflowFromWasmThunkGenerator(const AbstractLocker& locker)
{
    CCallHelpers jit;

    int32_t stackSpace = WTF::roundUpToMultipleOf(stackAlignmentBytes(), RegisterSet::calleeSaveRegisters().numberOfSetRegisters() * sizeof(Register));
    ASSERT(static_cast<unsigned>(stackSpace) < Options::softReservedZoneSize());
    jit.addPtr(CCallHelpers::TrustedImm32(-stackSpace), GPRInfo::callFrameRegister, MacroAssembler::stackPointerRegister);
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ExceptionType::StackOverflow)), GPRInfo::argumentGPR1);
    auto jumpToExceptionHandler = jit.jump();
    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID);
    linkBuffer.link(jumpToExceptionHandler, CodeLocationLabel(Thunks::singleton().stub(locker, throwExceptionFromWasmThunkGenerator).code()));
    return FINALIZE_CODE(linkBuffer, ("Throw stack overflow from Wasm"));
}

MacroAssemblerCodeRef triggerOMGTierUpThunkGenerator(const AbstractLocker&)
{
    // We expect that the user has already put the function index into GPRInfo::argumentGPR1
    CCallHelpers jit;

    const unsigned extraPaddingBytes = 0;
    RegisterSet registersToSpill = RegisterSet::allRegisters();
    registersToSpill.exclude(RegisterSet::registersToNotSaveForCCall());
#if CPU(ARM64)
    // We also want to spill x30 since that holds our return pc.
    registersToSpill.set(ARM64Registers::x30);
#endif
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

    jit.loadWasmContext(GPRInfo::argumentGPR0);
    jit.move(MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(runOMGPlanForIndex)), GPRInfo::argumentGPR2);
    jit.call(GPRInfo::argumentGPR2);

    ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, RegisterSet(), numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);

    jit.ret();
    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(linkBuffer, ("Trigger OMG tier up"));
}

static Thunks* thunks;
void Thunks::initialize()
{
    thunks = new Thunks;
}

Thunks& Thunks::singleton()
{
    ASSERT(thunks);
    return *thunks;
}

MacroAssemblerCodeRef Thunks::stub(ThunkGenerator generator)
{
    auto locker = holdLock(m_lock);
    return stub(locker, generator);
}

MacroAssemblerCodeRef Thunks::stub(const AbstractLocker& locker, ThunkGenerator generator)
{
    ASSERT(!!generator);
    {
        auto addResult = m_stubs.add(generator, MacroAssemblerCodeRef());
        if (!addResult.isNewEntry)
            return addResult.iterator->value;
    }

    MacroAssemblerCodeRef code = generator(locker);
    // We specifically don't use the iterator here to allow generator to recursively change m_stubs.
    m_stubs.set(generator, code);
    return code;
}

MacroAssemblerCodeRef Thunks::existingStub(ThunkGenerator generator)
{
    auto locker = holdLock(m_lock);

    auto iter = m_stubs.find(generator);
    if (iter != m_stubs.end())
        return iter->value;

    return MacroAssemblerCodeRef();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
