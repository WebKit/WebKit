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
#include "Operations.h"
#include "RepatchBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

static void compileStub(
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
    
    // Start by dumping all argument exit values to the stack.
    
    Vector<ExitArgumentForOperand, 16> sortedArguments;
    for (unsigned i = exit.m_values.size(); i--;) {
        ExitValue value = exit.m_values[i];
        int operand = exit.m_values.operandForIndex(i);
        
        if (!value.isArgument())
            continue;
        
        sortedArguments.append(ExitArgumentForOperand(value.exitArgument(), operand));
    }
    std::sort(sortedArguments.begin(), sortedArguments.end(), lesserArgumentIndex);
    
    for (unsigned i = 0; i < sortedArguments.size(); ++i) {
        ExitArgumentForOperand argument = sortedArguments[i];
        
        arguments.loadNextAndBox(argument.exitArgument().format(), GPRInfo::nonArgGPR0);
        jit.store64(GPRInfo::nonArgGPR0, AssemblyHelpers::addressFor(argument.operand()));
    }
    
    // Box anything that is already on the stack, or that is a constant.
    
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
            break;
        case ExitValueInJSStackAsInt32:
            jit.load32(
                address.withOffset(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)),
                GPRInfo::regT0);
            jit.or64(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, address);
            break;
        case ExitValueInJSStackAsDouble:
            jit.loadDouble(address, FPRInfo::fpRegT0);
            jit.boxDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, address);
            break;
        case ExitValueArgument:
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
    
    if (exit.m_lastSetOperand != std::numeric_limits<int>::max()) {
        jit.load64(
            AssemblyHelpers::addressFor(exit.m_lastSetOperand), GPRInfo::cachedResultRegister);
    }
    
    adjustAndJumpToTarget(jit, exit);
    
    LinkBuffer patchBuffer(*vm, &jit, codeBlock);
    exit.m_code = FINALIZE_CODE_IF(
        shouldShowDisassembly(),
        patchBuffer,
        ("FTL OSR exit #%u (bc#%u, %s) from %s",
            exitID, exit.m_codeOrigin.bytecodeIndex,
            exitKindToString(exit.m_kind), toCString(*codeBlock).data()));
}

extern "C" void* compileFTLOSRExit(ExecState* exec, unsigned exitID)
{
    SamplingRegion samplingRegion("FTL OSR Exit Compilation");
    
    CodeBlock* codeBlock = exec->codeBlock();
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITCode::FTLJIT);
    
    VM* vm = &exec->vm();
    
    OSRExit& exit = codeBlock->jitCode()->ftl()->osrExit[exitID];
    
    prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);
    
    compileStub(exitID, exit, vm, codeBlock);
    
    RepatchBuffer repatchBuffer(codeBlock);
    repatchBuffer.relink(
        exit.codeLocationForRepatch(codeBlock), CodeLocationLabel(exit.m_code.code()));
    
    return exit.m_code.code().executableAddress();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

