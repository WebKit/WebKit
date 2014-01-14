/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLOSRExitCompiler.h"

#if ENABLE(FTL_JIT)

#include "DFGOSRExitCompilerCommon.h"
#include "DFGOSRExitPreparation.h"
#include "FTLExitArgumentForOperand.h"
#include "FTLJITCode.h"
#include "FTLOSRExit.h"
#include "FTLSaveRestore.h"
#include "OperandsInlines.h"
#include "Operations.h"
#include "RepatchBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

static void compileStub(
    unsigned exitID, JITCode* jitCode, OSRExit& exit, VM* vm, CodeBlock* codeBlock)
{
    StackMaps::Record* record;
    
    for (unsigned i = jitCode->stackmaps.records.size(); i--;) {
        record = &jitCode->stackmaps.records[i];
        if (record->patchpointID == exit.m_stackmapID)
            break;
    }
    
    RELEASE_ASSERT(record->patchpointID == exit.m_stackmapID);
    
    // This code requires framePointerRegister is the same as callFrameRegister
    static_assert(MacroAssembler::framePointerRegister == GPRInfo::callFrameRegister, "MacroAssembler::framePointerRegister and GPRInfo::callFrameRegister must be the same");

    CCallHelpers jit(vm, codeBlock);
    
    // We need scratch space to save all registers and to build up the JSStack.
    // Use a scratch buffer to transfer all values.
    ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(sizeof(EncodedJSValue) * exit.m_values.size() + requiredScratchMemorySizeInBytes());
    EncodedJSValue* scratch = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;
    char* registerScratch = bitwise_cast<char*>(scratch + exit.m_values.size());
    
    // Make sure that saveAllRegisters() has a place on top of the stack to spill things. That
    // function expects to be able to use top of stack for scratch memory.
    jit.push(GPRInfo::regT0);
    saveAllRegisters(jit, registerScratch);
    
    // Bring the stack back into a sane form.
    jit.pop(GPRInfo::regT0);
    jit.pop(GPRInfo::regT0);
    
    // The remaining code assumes that SP/FP are in the same state that they were in the FTL's
    // call frame.
    
    // Get the call frame and tag thingies.
    // Restore the exiting function's callFrame value into a regT4
    record->locations[0].restoreInto(jit, jitCode->stackmaps, registerScratch, GPRInfo::regT4);
    jit.move(MacroAssembler::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
    jit.move(MacroAssembler::TrustedImm64(TagMask), GPRInfo::tagMaskRegister);
    
    // Do some value profiling.
    if (exit.m_profileValueFormat != InvalidValueFormat) {
        record->locations[1].restoreInto(jit, jitCode->stackmaps, registerScratch, GPRInfo::regT0);
        reboxAccordingToFormat(
            exit.m_profileValueFormat, jit, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT2);
        
        if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType) {
            CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
            if (ArrayProfile* arrayProfile = jit.baselineCodeBlockFor(codeOrigin)->getArrayProfile(codeOrigin.bytecodeIndex)) {
                jit.loadPtr(MacroAssembler::Address(GPRInfo::regT0, JSCell::structureOffset()), GPRInfo::regT1);
                jit.storePtr(GPRInfo::regT1, arrayProfile->addressOfLastSeenStructure());
                jit.load8(MacroAssembler::Address(GPRInfo::regT1, Structure::indexingTypeOffset()), GPRInfo::regT1);
                jit.move(MacroAssembler::TrustedImm32(1), GPRInfo::regT2);
                jit.lshift32(GPRInfo::regT1, GPRInfo::regT2);
                jit.or32(GPRInfo::regT2, MacroAssembler::AbsoluteAddress(arrayProfile->addressOfArrayModes()));
            }
        }
        
        if (!!exit.m_valueProfile)
            jit.store64(GPRInfo::regT0, exit.m_valueProfile.getSpecFailBucket(0));
    }

    // Save all state from wherever the exit data tells us it was, into the appropriate place in
    // the scratch buffer. This doesn't rebox any values yet.
    
    for (unsigned index = exit.m_values.size(); index--;) {
        ExitValue value = exit.m_values[index];
        
        switch (value.kind()) {
        case ExitValueDead:
            jit.move(MacroAssembler::TrustedImm64(JSValue::encode(jsUndefined())), GPRInfo::regT0);
            break;
            
        case ExitValueConstant:
            jit.move(MacroAssembler::TrustedImm64(JSValue::encode(value.constant())), GPRInfo::regT0);
            break;
            
        case ExitValueArgument:
            record->locations[value.exitArgument().argument()].restoreInto(
                jit, jitCode->stackmaps, registerScratch, GPRInfo::regT0);
            break;
            
        case ExitValueInJSStack:
        case ExitValueInJSStackAsInt32:
        case ExitValueInJSStackAsInt52:
        case ExitValueInJSStackAsDouble:
            jit.load64(AssemblyHelpers::addressFor(value.virtualRegister(), GPRInfo::regT4), GPRInfo::regT0);
            break;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        jit.store64(GPRInfo::regT0, scratch + index);
    }
    
    // Now get state out of the scratch buffer and place it back into the stack. This part does
    // all reboxing.
    for (unsigned index = exit.m_values.size(); index--;) {
        int operand = exit.m_values.operandForIndex(index);
        ExitValue value = exit.m_values[index];
        
        jit.load64(scratch + index, GPRInfo::regT0);
        reboxAccordingToFormat(
            value.valueFormat(), jit, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT2);
        jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(static_cast<VirtualRegister>(operand), GPRInfo::regT4));
    }
    
    // Restore the old stack pointer and then put regT4 into callFrameRegister. The idea is
    // that the FTL call frame is pushed onto the JS call frame and we can recover the old
    // value of the stack pointer by popping the FTL call frame. We already know what the
    // frame pointer in the JS call frame was because it would have been passed as an argument
    // to the FTL call frame.
    jit.move(MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    jit.pop(GPRInfo::nonArgGPR0);
    jit.pop(GPRInfo::nonArgGPR0);
    jit.move(GPRInfo::regT4, GPRInfo::callFrameRegister);
    
    handleExitCounts(jit, exit);
    reifyInlinedCallFrames(jit, exit);
    adjustAndJumpToTarget(jit, exit);
    
    LinkBuffer patchBuffer(*vm, &jit, codeBlock);
    exit.m_code = FINALIZE_CODE_IF(
        shouldShowDisassembly(),
        patchBuffer,
        ("FTL OSR exit #%u (%s, %s) from %s, with operands = %s, and record = %s",
            exitID, toCString(exit.m_codeOrigin).data(),
            exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
            toCString(ignoringContext<DumpContext>(exit.m_values)).data(),
            toCString(*record).data()));
}

extern "C" void* compileFTLOSRExit(ExecState* exec, unsigned exitID)
{
    SamplingRegion samplingRegion("FTL OSR Exit Compilation");
    
    CodeBlock* codeBlock = exec->codeBlock();
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITCode::FTLJIT);
    
    VM* vm = &exec->vm();
    
    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm->heap);

    JITCode* jitCode = codeBlock->jitCode()->ftl();
    OSRExit& exit = jitCode->osrExit[exitID];
    
    prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);
    
    compileStub(exitID, jitCode, exit, vm, codeBlock);
    
    RepatchBuffer repatchBuffer(codeBlock);
    repatchBuffer.relink(
        exit.codeLocationForRepatch(codeBlock), CodeLocationLabel(exit.m_code.code()));
    
    return exit.m_code.code().executableAddress();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

