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
#include "FTLCArgumentGetter.h"
#include "FTLExitArgumentForOperand.h"
#include "FTLJITCode.h"
#include "FTLOSRExit.h"
#include "FTLSaveRestore.h"
#include "Operations.h"
#include "RepatchBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

// This implements two flavors of OSR exit: one that involves having LLVM intrinsics to help
// OSR exit, and one that doesn't. The one that doesn't will get killed off, so we don't attempt
// to share code between the two.

static void compileStubWithOSRExitStackmap(
    unsigned exitID, JITCode* jitCode, OSRExit& exit, VM* vm, CodeBlock* codeBlock)
{
    StackMaps::Record* record;
    
    for (unsigned i = jitCode->stackmaps.records.size(); i--;) {
        record = &jitCode->stackmaps.records[i];
        if (record->patchpointID == exit.m_stackmapID)
            break;
    }
    
    RELEASE_ASSERT(record->patchpointID == exit.m_stackmapID);
    
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
    record->locations[0].restoreInto(jit, jitCode->stackmaps, registerScratch, GPRInfo::callFrameRegister);
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
            jit.load64(AssemblyHelpers::addressFor(value.virtualRegister()), GPRInfo::regT0);
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
        jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
    }
    
    handleExitCounts(jit, exit);
    reifyInlinedCallFrames(jit, exit);
    
    jit.move(MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    jit.pop(MacroAssembler::framePointerRegister);
    jit.pop(GPRInfo::nonArgGPR0); // ignore the result.
    
    if (exit.m_lastSetOperand.isValid()) {
        jit.load64(
            AssemblyHelpers::addressFor(exit.m_lastSetOperand), GPRInfo::cachedResultRegister);
    }
    
    adjustAndJumpToTarget(jit, exit);
    
    LinkBuffer patchBuffer(*vm, &jit, codeBlock);
    exit.m_code = FINALIZE_CODE_IF(
        shouldShowDisassembly(),
        patchBuffer,
        ("FTL OSR exit #%u (bc#%u, %s) from %s, with operands = %s, and record = %s",
            exitID, exit.m_codeOrigin.bytecodeIndex,
            exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
            toCString(ignoringContext<DumpContext>(exit.m_values)).data(),
            toCString(*record).data()));
}

static void compileStubWithoutOSRExitStackmap(
    unsigned exitID, OSRExit& exit, VM* vm, CodeBlock* codeBlock)
{
    CCallHelpers jit(vm, codeBlock);
    
    // Make ourselves look like a real C function.
    jit.push(MacroAssembler::framePointerRegister);
    jit.move(MacroAssembler::stackPointerRegister, MacroAssembler::framePointerRegister);
    
    // This is actually fairly easy, even though it is horribly gross. We know that
    // LLVM would have passes us all of the state via arguments. We know how to get
    // the arguments. And, we know how to pop stack back to the JIT stack frame, sort
    // of: we know that it's two frames beneath us. This is terrible and I feel
    // ashamed of it, but it will work for now.
    
    CArgumentGetter arguments(jit, 2);
    
    // First recover our call frame and tag thingies.
    arguments.loadNextPtr(GPRInfo::callFrameRegister);
    jit.move(MacroAssembler::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
    jit.move(MacroAssembler::TrustedImm64(TagMask), GPRInfo::tagMaskRegister);
    
    // Do some value profiling.
    if (exit.m_profileValueFormat != InvalidValueFormat) {
        arguments.loadNextAndBox(exit.m_profileValueFormat, GPRInfo::nonArgGPR0);
        
        if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType) {
            CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
            if (ArrayProfile* arrayProfile = jit.baselineCodeBlockFor(codeOrigin)->getArrayProfile(codeOrigin.bytecodeIndex)) {
                jit.loadPtr(MacroAssembler::Address(GPRInfo::nonArgGPR0, JSCell::structureOffset()), GPRInfo::nonArgGPR1);
                jit.storePtr(GPRInfo::nonArgGPR1, arrayProfile->addressOfLastSeenStructure());
                jit.load8(MacroAssembler::Address(GPRInfo::nonArgGPR1, Structure::indexingTypeOffset()), GPRInfo::nonArgGPR1);
                jit.move(MacroAssembler::TrustedImm32(1), GPRInfo::nonArgGPR2);
                jit.lshift32(GPRInfo::nonArgGPR1, GPRInfo::nonArgGPR2);
                jit.or32(GPRInfo::nonArgGPR2, MacroAssembler::AbsoluteAddress(arrayProfile->addressOfArrayModes()));
            }
        }
        
        if (!!exit.m_valueProfile)
            jit.store64(GPRInfo::nonArgGPR0, exit.m_valueProfile.getSpecFailBucket(0));
    }
    
    // Use a scratch buffer to transfer all values.
    ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(sizeof(EncodedJSValue) * exit.m_values.size());
    EncodedJSValue* scratch = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;
    
    // Start by dumping all argument exit values to the stack.
    
    Vector<ExitArgumentForOperand, 16> sortedArguments;
    for (unsigned i = exit.m_values.size(); i--;) {
        ExitValue value = exit.m_values[i];
        int operand = exit.m_values.operandForIndex(i);
        
        if (!value.isArgument())
            continue;
        
        sortedArguments.append(ExitArgumentForOperand(value.exitArgument(), VirtualRegister(operand)));
    }
    std::sort(sortedArguments.begin(), sortedArguments.end(), lesserArgumentIndex);
    
    for (unsigned i = 0; i < sortedArguments.size(); ++i) {
        ExitArgumentForOperand argument = sortedArguments[i];
        
        arguments.loadNextAndBox(argument.exitArgument().format(), GPRInfo::nonArgGPR0);
        jit.store64(
            GPRInfo::nonArgGPR0, scratch + exit.m_values.indexForOperand(argument.operand()));
    }
    
    // All temp registers are free at this point.
    
    // Move anything on the stack into the appropriate place in the scratch buffer.
    
    for (unsigned i = exit.m_values.size(); i--;) {
        ExitValue value = exit.m_values[i];
        
        switch (value.kind()) {
        case ExitValueInJSStack:
            jit.load64(AssemblyHelpers::addressFor(value.virtualRegister()), GPRInfo::regT0);
            break;
        case ExitValueInJSStackAsInt32:
            jit.load32(
                AssemblyHelpers::addressFor(value.virtualRegister()).withOffset(
                    OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
                GPRInfo::regT0);
            jit.or64(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
            break;
        case ExitValueInJSStackAsInt52:
            jit.load64(AssemblyHelpers::addressFor(value.virtualRegister()), GPRInfo::regT0);
            jit.rshift64(
                AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            break;
        case ExitValueInJSStackAsDouble:
            jit.loadDouble(AssemblyHelpers::addressFor(value.virtualRegister()), FPRInfo::fpRegT0);
            jit.boxDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            break;
        case ExitValueDead:
        case ExitValueConstant:
        case ExitValueArgument:
            // Don't do anything for these.
            continue;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        jit.store64(GPRInfo::regT0, scratch + i);
    }
    
    // Move everything from the scratch buffer to the stack; this also reifies constants.
    
    for (unsigned i = exit.m_values.size(); i--;) {
        ExitValue value = exit.m_values[i];
        int operand = exit.m_values.operandForIndex(i);
        
        MacroAssembler::Address address = AssemblyHelpers::addressFor(operand);
        
        switch (value.kind()) {
        case ExitValueDead:
            jit.store64(MacroAssembler::TrustedImm64(JSValue::encode(jsUndefined())), address);
            break;
        case ExitValueConstant:
            jit.store64(MacroAssembler::TrustedImm64(JSValue::encode(value.constant())), address);
            break;
        case ExitValueInJSStack:
        case ExitValueInJSStackAsInt32:
        case ExitValueInJSStackAsInt52:
        case ExitValueInJSStackAsDouble:
        case ExitValueArgument:
            jit.load64(scratch + i, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, address);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    handleExitCounts(jit, exit);
    reifyInlinedCallFrames(jit, exit);
    
    jit.pop(MacroAssembler::framePointerRegister);
    jit.move(MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    jit.pop(MacroAssembler::framePointerRegister);
    jit.pop(GPRInfo::nonArgGPR0); // ignore the result.
    
    if (exit.m_lastSetOperand.isValid()) {
        jit.load64(
            AssemblyHelpers::addressFor(exit.m_lastSetOperand), GPRInfo::cachedResultRegister);
    }
    
    adjustAndJumpToTarget(jit, exit);
    
    LinkBuffer patchBuffer(*vm, &jit, codeBlock);
    exit.m_code = FINALIZE_CODE_IF(
        shouldShowDisassembly(),
        patchBuffer,
        ("FTL OSR exit #%u (bc#%u, %s) from %s, with operands = %s",
            exitID, exit.m_codeOrigin.bytecodeIndex,
            exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
            toCString(ignoringContext<DumpContext>(exit.m_values)).data()));
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
    
    if (Options::ftlOSRExitUsesStackmap())
        compileStubWithOSRExitStackmap(exitID, jitCode, exit, vm, codeBlock);
    else
        compileStubWithoutOSRExitStackmap(exitID, exit, vm, codeBlock);
    
    RepatchBuffer repatchBuffer(codeBlock);
    repatchBuffer.relink(
        exit.codeLocationForRepatch(codeBlock), CodeLocationLabel(exit.m_code.code()));
    
    return exit.m_code.code().executableAddress();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

