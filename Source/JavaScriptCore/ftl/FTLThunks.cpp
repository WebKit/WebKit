/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "FTLThunks.h"

#if ENABLE(FTL_JIT)

#include "AssemblyHelpersSpoolers.h"
#include "DFGOSRExitCompilerCommon.h"
#include "FTLOSRExitCompiler.h"
#include "FTLOperations.h"
#include "FTLSaveRestore.h"
#include "GPRInfo.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

enum class FrameAndStackAdjustmentRequirement {
    Needed, 
    NotNeeded 
};

static MacroAssemblerCodeRef<JITThunkPtrTag> genericGenerationThunkGenerator(
    VM& vm, CodePtr<CFunctionPtrTag> generationFunction, PtrTag resultTag, const char* name, unsigned extraPopsToRestore, FrameAndStackAdjustmentRequirement frameAndStackAdjustmentRequirement)
{
    AssemblyHelpers jit(nullptr);

    if (frameAndStackAdjustmentRequirement == FrameAndStackAdjustmentRequirement::Needed) {
        // This needs to happen before we use the scratch buffer because this function also uses the scratch buffer.
        adjustFrameAndStackInOSRExitCompilerThunk<FTL::JITCode>(jit, vm, JITType::FTLJIT);
    }
    
    // Note that the "return address" will be the ID that we pass to the generation function.

    constexpr GPRReg stackPointerRegister = MacroAssembler::stackPointerRegister;
    constexpr GPRReg framePointerRegister = MacroAssembler::framePointerRegister;
    constexpr ptrdiff_t pushToSaveByteOffset = MacroAssembler::pushToSaveByteOffset();
    ptrdiff_t stackMisalignment = pushToSaveByteOffset;

    // Pretend that we're a C call frame.
    jit.pushToSave(framePointerRegister);
    jit.move(stackPointerRegister, framePointerRegister);
    stackMisalignment += pushToSaveByteOffset;

    // Now create ourselves enough stack space to give saveAllRegisters() a scratch slot.
    unsigned numberOfRequiredPops = 0;
    do {
        stackMisalignment += pushToSaveByteOffset;
        numberOfRequiredPops++;
    } while (stackMisalignment % stackAlignmentBytes());
    jit.subPtr(MacroAssembler::TrustedImm32(numberOfRequiredPops * pushToSaveByteOffset), stackPointerRegister);

    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(requiredScratchMemorySizeInBytes());
    char* buffer = static_cast<char*>(scratchBuffer->dataBuffer());
    
    saveAllRegisters(jit, buffer);

    jit.loadPtr(CCallHelpers::Address(framePointerRegister), GPRInfo::argumentGPR0);
    jit.peek(
        GPRInfo::argumentGPR1,
        (stackMisalignment - pushToSaveByteOffset) / sizeof(void*));
    jit.prepareCallOperation(vm);
    MacroAssembler::Call functionCall = jit.call(OperationPtrTag);

    // At this point we want to make a tail call to what was returned to us in the
    // returnValueGPR. But at the same time as we do this, we must restore all registers.
    // The way we will accomplish this is by arranging to have the tail call target in the
    // return address "slot" (be it a register or the stack).
    
    jit.move(GPRInfo::returnValueGPR, GPRInfo::regT0);

    // Prepare for tail call.

    jit.loadPtr(MacroAssembler::Address(stackPointerRegister, numberOfRequiredPops * pushToSaveByteOffset), framePointerRegister);

    // When we came in here, there was an additional thing pushed to the stack (extraPopsToRestore).
    // Some clients want it popped before proceeding. Also add 1 for the pushToSave of the framePointerRegister.
    numberOfRequiredPops += 1 + extraPopsToRestore;
    jit.addPtr(MacroAssembler::TrustedImm32(numberOfRequiredPops * pushToSaveByteOffset), stackPointerRegister);

    // Put the return address wherever the return instruction wants it. On all platforms, this
    // ensures that the return address is out of the way of register restoration.
    jit.restoreReturnAddressBeforeReturn(GPRInfo::regT0);

#if CPU(ARM64E)
    jit.untagPtr(resultTag, AssemblyHelpers::linkRegister);
    jit.validateUntaggedPtr(AssemblyHelpers::linkRegister);
    jit.tagReturnAddress();
#else
    UNUSED_PARAM(resultTag);
#endif

    restoreAllRegisters(jit, buffer);

    jit.ret();
    
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::FTLThunk);
    patchBuffer.link(functionCall, generationFunction.retagged<OperationPtrTag>());
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "%s", name);
}

MacroAssemblerCodeRef<JITThunkPtrTag> osrExitGenerationThunkGenerator(VM& vm)
{
    unsigned extraPopsToRestore = 0;
    return genericGenerationThunkGenerator(
        vm, operationCompileFTLOSRExit, OSRExitPtrTag, "FTL OSR exit generation thunk", extraPopsToRestore, FrameAndStackAdjustmentRequirement::Needed);
}

MacroAssemblerCodeRef<JITThunkPtrTag> lazySlowPathGenerationThunkGenerator(VM& vm)
{
    unsigned extraPopsToRestore = 1;
    return genericGenerationThunkGenerator(
        vm, operationCompileFTLLazySlowPath, JITStubRoutinePtrTag, "FTL lazy slow path generation thunk", extraPopsToRestore, FrameAndStackAdjustmentRequirement::NotNeeded);
}

static void registerClobberCheck(AssemblyHelpers& jit, RegisterSetBuilder dontClobber)
{
    ASSERT(Options::clobberAllRegsInFTLICSlowPath());
    RegisterSetBuilder clobber = RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters());
    clobber.exclude(dontClobber);
    auto wholeClobberedRegisters = clobber.buildWithLowerBits();
    
    GPRReg someGPR = InvalidGPRReg;
    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (!wholeClobberedRegisters.contains(reg, IgnoreVectors) || !reg.isGPR())
            continue;
        
        jit.move(AssemblyHelpers::TrustedImm32(0x1337beef), reg.gpr());
        someGPR = reg.gpr();
    }
    
    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (!wholeClobberedRegisters.contains(reg, IgnoreVectors) || !reg.isFPR())
            continue;
        
        jit.move64ToDouble(someGPR, reg.fpr());
    }
}

MacroAssemblerCodeRef<JITThunkPtrTag> slowPathCallThunkGenerator(VM& vm, const SlowPathCallKey& key)
{
    AssemblyHelpers jit(nullptr);
    jit.tagReturnAddress();

    // We want to save the given registers at the given offset, then we want to save the
    // old return address somewhere past that offset, and then finally we want to make the
    // call.
    
    size_t currentOffset = key.offset() + sizeof(void*);
    
#if CPU(X86_64)
    currentOffset += sizeof(void*);
#endif

    AssemblyHelpers::StoreRegSpooler storeSpooler(jit, MacroAssembler::stackPointerRegister);

    for (MacroAssembler::RegisterID reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = static_cast<MacroAssembler::RegisterID>(reg + 1)) {
        if (!key.usedRegisters().contains(reg, IgnoreVectors))
            continue;
        storeSpooler.storeGPR({ reg, static_cast<ptrdiff_t>(currentOffset), conservativeWidthWithoutVectors(reg) });
        currentOffset += sizeof(void*);
    }
    storeSpooler.finalizeGPR();

    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = static_cast<MacroAssembler::FPRegisterID>(reg + 1)) {
        if (!key.usedRegisters().contains(reg, IgnoreVectors))
            continue;
        storeSpooler.storeFPR({ reg, static_cast<ptrdiff_t>(currentOffset), conservativeWidthWithoutVectors(reg) });
        currentOffset += sizeof(double);
    }
    storeSpooler.finalizeFPR();

    jit.preserveReturnAddressAfterCall(GPRInfo::nonArgGPR1);
    jit.storePtr(GPRInfo::nonArgGPR1, AssemblyHelpers::Address(MacroAssembler::stackPointerRegister, key.offset()));
    jit.prepareCallOperation(vm);
    
    if (UNLIKELY(Options::clobberAllRegsInFTLICSlowPath())) {
        auto dontClobber = key.argumentRegistersIfClobberingCheckIsEnabled();
        if (!key.callTarget())
            dontClobber.add(GPRInfo::nonArgGPR0, IgnoreVectors);
        registerClobberCheck(jit, WTFMove(dontClobber));
    }

    AssemblyHelpers::Call call;
    if (key.callTarget())
        call = jit.call(OperationPtrTag);
    else
        jit.call(CCallHelpers::Address(GPRInfo::nonArgGPR0, key.indirectOffset()), OperationPtrTag);

    jit.loadPtr(AssemblyHelpers::Address(MacroAssembler::stackPointerRegister, key.offset()), GPRInfo::nonPreservedNonReturnGPR);
    jit.restoreReturnAddressBeforeReturn(GPRInfo::nonPreservedNonReturnGPR);
    
    AssemblyHelpers::LoadRegSpooler loadSpooler(jit, MacroAssembler::stackPointerRegister);

    for (MacroAssembler::FPRegisterID reg = MacroAssembler::lastFPRegister(); ; reg = static_cast<MacroAssembler::FPRegisterID>(reg - 1)) {
        if (key.usedRegisters().contains(reg, IgnoreVectors)) {
            currentOffset -= sizeof(double);
            loadSpooler.loadFPR({ reg, static_cast<ptrdiff_t>(currentOffset), conservativeWidthWithoutVectors(reg) });
        }
        if (reg == MacroAssembler::firstFPRegister())
            break;
    }
    loadSpooler.finalizeFPR();

    for (MacroAssembler::RegisterID reg = MacroAssembler::lastRegister(); ; reg = static_cast<MacroAssembler::RegisterID>(reg - 1)) {
        if (key.usedRegisters().contains(reg, IgnoreVectors)) {
            currentOffset -= sizeof(void*);
            loadSpooler.loadGPR({ reg, static_cast<ptrdiff_t>(currentOffset), conservativeWidthWithoutVectors(reg) });
        }
        if (reg == MacroAssembler::firstRegister())
            break;
    }
    loadSpooler.finalizeGPR();

    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::FTLThunk);
    if (key.callTarget())
        patchBuffer.link(call, key.callTarget());
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "FTL slow path call thunk for %s", toCString(key).data());
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

