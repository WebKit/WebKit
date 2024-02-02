/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)

#include "AllowMacroScratchRegisterUsage.h"
#include "CCallHelpers.h"
#include "JSCJSValue.h"
#include "JSInterfaceJIT.h"
#include "LinkBuffer.h"
#include "ProbeContext.h"
#include "ScratchRegisterAllocator.h"
#include "WasmExceptionType.h"
#include "WasmInstance.h"
#include "WasmOperations.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Thunks);

MacroAssemblerCodeRef<JITThunkPtrTag> throwExceptionFromWasmThunkGenerator(const AbstractLocker&)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "throwExceptionFromWasmThunkGenerator");

    // The thing that jumps here must move ExceptionType into the argumentGPR1 before jumping here.
    // We're allowed to use temp registers here. We are not allowed to use callee saves.
    jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfVM()), GPRInfo::argumentGPR2);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, VM::topEntryFrameOffset()), GPRInfo::argumentGPR2);
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR2);

    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.breakpoint(); // We should not reach this.

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    linkBuffer.link<OperationPtrTag>(call, operationWasmToJSException);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Throw exception from Wasm");
}

// This is just here to give us a unique backtrace if we ever actually hit this.
MacroAssemblerCodeRef<JITThunkPtrTag> crashDueToBBQStackOverflowGenerator(const AbstractLocker&)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "crashDueToBBQStackOverflow");

    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.breakpoint(); // We should not reach this.

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    linkBuffer.link<OperationPtrTag>(call, operationCrashDueToBBQStackOverflow);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "crashDueToBBQStackOverflow");
}

MacroAssemblerCodeRef<JITThunkPtrTag> crashDueToOMGStackOverflowGenerator(const AbstractLocker&)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "crashDueToOMGStackOverflow");

    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.breakpoint(); // We should not reach this.

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    linkBuffer.link<OperationPtrTag>(call, operationCrashDueToOMGStackOverflow);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "crashDueToBBQStackOverflow");
}

MacroAssemblerCodeRef<JITThunkPtrTag> throwStackOverflowFromWasmThunkGenerator(const AbstractLocker& locker)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "throwStackOverflowFromWasmThunkGenerator");

    int32_t stackSpace = WTF::roundUpToMultipleOf(stackAlignmentBytes(), RegisterSetBuilder::calleeSaveRegisters().numberOfSetRegisters() * sizeof(Register));
    ASSERT(static_cast<unsigned>(stackSpace) < Options::softReservedZoneSize());
    jit.addPtr(CCallHelpers::TrustedImm32(-stackSpace), GPRInfo::callFrameRegister, MacroAssembler::stackPointerRegister);
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ExceptionType::StackOverflow)), GPRInfo::argumentGPR1);
    jit.jumpThunk(CodeLocationLabel<JITThunkPtrTag> { Thunks::singleton().stub(locker, throwExceptionFromWasmThunkGenerator).code() });
    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Throw stack overflow from Wasm");
}

#if USE(JSVALUE64)
MacroAssemblerCodeRef<JITThunkPtrTag> catchInWasmThunkGenerator(const AbstractLocker&)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "catch runway");

    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0);
    jit.move(GPRInfo::regT0, GPRInfo::regT3);
    jit.and64(CCallHelpers::TrustedImm64(JSValue::NativeCalleeMask), GPRInfo::regT3);
    auto isWasmCallee = jit.branch64(CCallHelpers::Equal, GPRInfo::regT3, CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag));
    CCallHelpers::JumpList doneCases;
    {
        // FIXME: Handling precise allocations in WasmB3IRGenerator catch entrypoints might be unnecessary
        // https://bugs.webkit.org/show_bug.cgi?id=231213
        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT0, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), GPRInfo::regT0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, MarkedBlock::offsetOfHeader + MarkedBlock::Header::offsetOfVM()), GPRInfo::regT0);
        doneCases.append(jit.jump());

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), GPRInfo::regT0);
        doneCases.append(jit.jump());
    }

    isWasmCallee.link(&jit);
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::regT0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, Instance::offsetOfVM()), GPRInfo::regT0);

    doneCases.link(&jit);

    JIT_COMMENT(jit, "restore callee saves from vm entry buffer");
    jit.restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(GPRInfo::regT0, GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, VM::callFrameForCatchOffset()), GPRInfo::callFrameRegister);

    JIT_COMMENT(jit, "Configure wasm context instance");
    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * sizeof(Register)), GPRInfo::wasmContextInstancePointer);

    // So, this is calling a function. This means that we clobber all caller-save registers!
    // Is it safe? Yes. OMG / BBQ(Air) generates stackmap for values with the patchpoint which says "this is function call". Thus, stackmap must hold
    // StackSlots / Registers which are not these caller-save registers. So, clobbering these registers here does not break the stackmap restoration.
    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
    jit.setupArguments<decltype(operationWasmRetrieveAndClearExceptionIfCatchable)>(GPRInfo::wasmContextInstancePointer);
    jit.callOperation<OperationPtrTag>(operationWasmRetrieveAndClearExceptionIfCatchable);
    static_assert(noOverlap(GPRInfo::nonPreservedNonArgumentGPR0, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2));
    jit.farJump(GPRInfo::returnValueGPR2, ExceptionHandlerPtrTag);

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Wasm catch runway");
}
#elif USE(JSVALUE32_64)
// Same as JSVALUE64 version, except operationWasmRetrieveAndClearExceptionIfCatchable returns exception on stack.
MacroAssemblerCodeRef<JITThunkPtrTag> catchInWasmThunkGenerator(const AbstractLocker&)
{
    CCallHelpers jit;
    JIT_COMMENT(jit, "catch runway");

    jit.loadPair32(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0, GPRInfo::regT3);
    auto isWasmCallee = jit.branch32(CCallHelpers::Equal, GPRInfo::regT3, CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag));
    CCallHelpers::JumpList isJSCallee;
    {
        // FIXME: Handling precise allocations in WasmB3IRGenerator catch entrypoints might be unnecessary
        // https://bugs.webkit.org/show_bug.cgi?id=231213
        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT0, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), GPRInfo::regT0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, MarkedBlock::offsetOfHeader + MarkedBlock::Header::offsetOfVM()), GPRInfo::regT0);
        isJSCallee.append(jit.jump());

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), GPRInfo::regT0);
        isJSCallee.append(jit.jump());
    }

    isWasmCallee.link(&jit);
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::regT0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, Instance::offsetOfVM()), GPRInfo::regT0);

    isJSCallee.link(&jit);

    JIT_COMMENT(jit, "restore callee saves from vm entry buffer");
    jit.restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(GPRInfo::regT0, GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, VM::callFrameForCatchOffset()), GPRInfo::callFrameRegister);

    JIT_COMMENT(jit, "Configure wasm context instance");
    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * sizeof(Register)), GPRInfo::wasmContextInstancePointer);

    // Returning one EncodedJSValue on the stack
    constexpr int32_t resultSpace = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(static_cast<int32_t>(sizeof(EncodedJSValue)));
    jit.subPtr(CCallHelpers::TrustedImm32(resultSpace), MacroAssembler::stackPointerRegister);

    // So, this is calling a function. This means that we clobber all caller-save registers!
    // Is it safe? Yes. OMG / BBQ(Air) generates stackmap for values with the patchpoint which says "this is function call". Thus, stackmap must hold
    // StackSlots / Registers which are not these caller-save registers. So, clobbering these registers here does not break the stackmap restoration.
    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
    jit.setupArguments<decltype(operationWasmRetrieveAndClearExceptionIfCatchable)>(GPRInfo::wasmContextInstancePointer, MacroAssembler::stackPointerRegister);
    jit.callOperation<OperationPtrTag>(operationWasmRetrieveAndClearExceptionIfCatchable);

    // Move targetMachinePCAfterCatch out of the way
    jit.move(GPRInfo::returnValueGPR, GPRInfo::regT3);
    // Load exception from stack
    jit.loadPair32(CCallHelpers::Address(MacroAssembler::stackPointerRegister), GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
    // Reset SP
    jit.addPtr(CCallHelpers::TrustedImm32(resultSpace), MacroAssembler::stackPointerRegister);

    // Jump to Catch
    jit.farJump(GPRInfo::regT3, ExceptionHandlerPtrTag);

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
    return FINALIZE_WASM_CODE(linkBuffer, JITThunkPtrTag, "Wasm catch runway");
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT)

MacroAssemblerCodeRef<JITThunkPtrTag> triggerOMGEntryTierUpThunkGeneratorImpl(const AbstractLocker&, bool isSIMDContext)
{
    // We expect that the user has already put the function index into GPRInfo::nonPreservedNonArgumentGPR0
    CCallHelpers jit;
    JIT_COMMENT(jit, "triggerOMGEntryTierUpThunkGenerator");

    jit.emitFunctionPrologue();

    const unsigned extraPaddingBytes = 0;
    auto registersToSpill = RegisterSetBuilder::registersToSaveForCCall(isSIMDContext ? RegisterSetBuilder::allRegisters() : RegisterSetBuilder::allScalarRegisters()).buildWithLowerBits();
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

    // We can clobber these argument registers now since we saved them and later we restore them.
    jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
    jit.move(GPRInfo::nonPreservedNonArgumentGPR0, GPRInfo::argumentGPR1);
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

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
