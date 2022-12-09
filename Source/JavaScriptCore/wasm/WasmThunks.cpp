/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "AllowMacroScratchRegisterUsage.h"
#include "CCallHelpers.h"
#include "JSInterfaceJIT.h"
#include "LinkBuffer.h"
#include "ProbeContext.h"
#include "ScratchRegisterAllocator.h"
#include "WasmExceptionType.h"
#include "WasmInstance.h"
#include "WasmOperations.h"

namespace JSC { namespace Wasm {

MacroAssemblerCodeRef<JITThunkPtrTag> throwExceptionFromWasmThunkGenerator(const AbstractLocker&)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "throwExceptionFromWasmThunkGenerator");

    // The thing that jumps here must move ExceptionType into the argumentGPR1 before jumping here.
    // We're allowed to use temp registers here. We are not allowed to use callee saves.
    jit.loadWasmContextInstance(GPRInfo::argumentGPR2);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, Instance::offsetOfPointerToTopEntryFrame()), GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0), GPRInfo::argumentGPR0);
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);

    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.breakpoint(); // We should not reach this.

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    linkBuffer.link<OperationPtrTag>(call, operationWasmToJSException);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Throw exception from Wasm");
}

MacroAssemblerCodeRef<JITThunkPtrTag> throwStackOverflowFromWasmThunkGenerator(const AbstractLocker& locker)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "throwStackOverflowFromWasmThunkGenerator");

    int32_t stackSpace = WTF::roundUpToMultipleOf(stackAlignmentBytes(), RegisterSetBuilder::calleeSaveRegisters().numberOfSetRegisters() * sizeof(Register));
    ASSERT(static_cast<unsigned>(stackSpace) < Options::softReservedZoneSize());
    jit.addPtr(CCallHelpers::TrustedImm32(-stackSpace), GPRInfo::callFrameRegister, MacroAssembler::stackPointerRegister);
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ExceptionType::StackOverflow)), GPRInfo::argumentGPR1);
    auto jumpToExceptionHandler = jit.jump();
    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    linkBuffer.link(jumpToExceptionHandler, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(locker, throwExceptionFromWasmThunkGenerator).code()));
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Throw stack overflow from Wasm");
}

#if ENABLE(WEBASSEMBLY_B3JIT)

MacroAssemblerCodeRef<JITThunkPtrTag> triggerOMGEntryTierUpThunkGeneratorImpl(const AbstractLocker&, bool isSIMDContext)
{
    // We expect that the user has already put the function index into GPRInfo::argumentGPR1
    CCallHelpers jit;
    JIT_COMMENT(jit, "triggerOMGEntryTierUpThunkGenerator");

    jit.emitFunctionPrologue();

    const unsigned extraPaddingBytes = 0;
    auto registersToSpill = RegisterSetBuilder::registersToSaveForCCall(isSIMDContext ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()).buildWithLowerBits();
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

    jit.loadWasmContextInstance(GPRInfo::argumentGPR0);
    jit.move(MacroAssembler::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationWasmTriggerTierUpNow)), GPRInfo::argumentGPR2);
    jit.call(GPRInfo::argumentGPR2, OperationPtrTag);

    ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, { }, numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);

    jit.emitFunctionEpilogue();
    jit.ret();
    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Trigger OMG entry tier up");
}

MacroAssemblerCodeRef<JITThunkPtrTag> triggerOMGEntryTierUpThunkGeneratorSIMD(const AbstractLocker& locker)
{
    return triggerOMGEntryTierUpThunkGeneratorImpl(locker, true);
}

MacroAssemblerCodeRef<JITThunkPtrTag> triggerOMGEntryTierUpThunkGeneratorNoSIMD(const AbstractLocker& locker)
{
    return triggerOMGEntryTierUpThunkGeneratorImpl(locker, false);
}

#endif

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

MacroAssemblerCodeRef<JITThunkPtrTag> Thunks::stub(ThunkGenerator generator)
{
    Locker locker { m_lock };
    return stub(locker, generator);
}

MacroAssemblerCodeRef<JITThunkPtrTag> Thunks::stub(const AbstractLocker& locker, ThunkGenerator generator)
{
    ASSERT(!!generator);
    {
        auto addResult = m_stubs.add(generator, MacroAssemblerCodeRef<JITThunkPtrTag>());
        if (!addResult.isNewEntry)
            return addResult.iterator->value;
    }

    MacroAssemblerCodeRef<JITThunkPtrTag> code = generator(locker);
    // We specifically don't use the iterator here to allow generator to recursively change m_stubs.
    m_stubs.set(generator, code);
    return code;
}

MacroAssemblerCodeRef<JITThunkPtrTag> Thunks::existingStub(ThunkGenerator generator)
{
    Locker locker { m_lock };

    auto iter = m_stubs.find(generator);
    if (iter != m_stubs.end())
        return iter->value;

    return MacroAssemblerCodeRef<JITThunkPtrTag>();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
