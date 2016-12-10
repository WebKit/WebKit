/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WasmBinding.h"

#if ENABLE(WEBASSEMBLY)

#include "AssemblyHelpers.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "WasmCallingConvention.h"

namespace JSC { namespace Wasm {

WasmToJSStub importStubGenerator(VM* vm, Bag<CallLinkInfo>& callLinkInfos, Signature* signature, unsigned importIndex)
{
    const WasmCallingConvention& wasmCC = wasmCallingConvention();
    const JSCCallingConvention& jsCC = jscCallingConvention();
    unsigned argCount = signature->arguments.size();
    typedef AssemblyHelpers JIT;
    JIT jit(vm, nullptr);

    // Below, we assume that the JS calling convention is always on the stack.
    ASSERT(!jsCC.m_gprArgs.size());
    ASSERT(!jsCC.m_fprArgs.size());

    jit.emitFunctionPrologue();
    jit.store64(JIT::TrustedImm32(0), JIT::Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * static_cast<int>(sizeof(Register)))); // FIXME Stop using 0 as codeBlocks. https://bugs.webkit.org/show_bug.cgi?id=165321
    jit.storePtr(JIT::TrustedImmPtr(vm->webAssemblyToJSCallee.get()), JIT::Address(GPRInfo::callFrameRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register))));

    // Here we assume that the JS calling convention saves at least all the wasm callee saved. We therefore don't need to save and restore more registers since the wasm callee already took care of this.
    RegisterSet missingCalleeSaves = wasmCC.m_calleeSaveRegisters;
    missingCalleeSaves.exclude(jsCC.m_calleeSaveRegisters);
    ASSERT(missingCalleeSaves.isEmpty());

    // FIXME perform a stack check before updating SP. https://bugs.webkit.org/show_bug.cgi?id=165546

    unsigned numberOfParameters = argCount + 1; // There is a "this" argument.
    unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + numberOfParameters;
    unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    const unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);
    jit.subPtr(MacroAssembler::TrustedImm32(stackOffset), MacroAssembler::stackPointerRegister);
    JIT::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    // FIXME make this a loop which switches on Signature if there are many arguments on the stack. It'll otherwise be huge for huge signatures. https://bugs.webkit.org/show_bug.cgi?id=165547
    unsigned marshalledGPRs = 0;
    unsigned marshalledFPRs = 0;
    unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
    unsigned frOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
    for (unsigned argNum = 0; argNum < argCount; ++argNum) {
        Type argType = signature->arguments[argNum];
        switch (argType) {
        case Void:
        case Func:
        case Anyfunc:
        case I64:
            // For the JavaScript embedding, imports with these types in their signature arguments are a WebAssembly.Module validation error.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case I32: {
            GPRReg gprReg;
            if (marshalledGPRs < wasmCC.m_gprArgs.size())
                gprReg = wasmCC.m_gprArgs[marshalledGPRs].gpr();
            else {
                // We've already spilled all arguments, these registers are available as scratch.
                gprReg = GPRInfo::argumentGPR0;
                jit.load64(JIT::Address(GPRInfo::callFrameRegister, frOffset), gprReg);
                frOffset += sizeof(Register);
            }
            ++marshalledGPRs;
            jit.boxInt32(gprReg, JSValueRegs(gprReg), DoNotHaveTagRegisters);
            jit.store64(gprReg, calleeFrame.withOffset(calleeFrameOffset));
            calleeFrameOffset += sizeof(Register);
            break;
        }
        case F32: {
            FPRReg fprReg;
            if (marshalledFPRs < wasmCC.m_fprArgs.size())
                fprReg = wasmCC.m_fprArgs[marshalledFPRs].fpr();
            else {
                // We've already spilled all arguments, these registers are available as scratch.
                fprReg = FPRInfo::argumentFPR0;
                jit.loadFloat(JIT::Address(GPRInfo::callFrameRegister, frOffset), fprReg);
                frOffset += sizeof(Register);
            }
            jit.convertFloatToDouble(fprReg, fprReg);
            jit.purifyNaN(fprReg);
            jit.storeDouble(fprReg, calleeFrame.withOffset(calleeFrameOffset));
            calleeFrameOffset += sizeof(Register);
            ++marshalledFPRs;
            break;
        }
        case F64: {
            FPRReg fprReg;
            if (marshalledFPRs < wasmCC.m_fprArgs.size())
                fprReg = wasmCC.m_fprArgs[marshalledFPRs].fpr();
            else {
                // We've already spilled all arguments, these registers are available as scratch.
                fprReg = FPRInfo::argumentFPR0;
                jit.loadDouble(JIT::Address(GPRInfo::callFrameRegister, frOffset), fprReg);
                frOffset += sizeof(Register);
            }
            jit.purifyNaN(fprReg);
            jit.storeDouble(fprReg, calleeFrame.withOffset(calleeFrameOffset));
            calleeFrameOffset += sizeof(Register);
            ++marshalledFPRs;
            break;
        }
        }
    }

    GPRReg importJSCellGPRReg = argumentRegisterForCallee();
    ASSERT(!wasmCC.m_calleeSaveRegisters.get(importJSCellGPRReg));

    // Each JS -> wasm entry sets the WebAssembly.Instance whose export is being called. We're calling out of this Instance, and can therefore figure out the import being called.
    jit.loadPtr(&vm->topJSWebAssemblyInstance, importJSCellGPRReg);
    jit.loadPtr(JIT::Address(importJSCellGPRReg, JSWebAssemblyInstance::offsetOfImportFunction(importIndex)), importJSCellGPRReg);

    uint64_t thisArgument = ValueUndefined; // FIXME what does the WebAssembly spec say this should be? https://bugs.webkit.org/show_bug.cgi?id=165471
    jit.store64(importJSCellGPRReg, calleeFrame.withOffset(CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    jit.store32(JIT::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset));
    jit.store64(JIT::TrustedImm64(thisArgument), calleeFrame.withOffset(CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register))));

    // FIXME Tail call if the wasm return type is void and no registers were spilled. https://bugs.webkit.org/show_bug.cgi?id=165488

    CallLinkInfo* callLinkInfo = callLinkInfos.add();
    callLinkInfo->setUpCall(CallLinkInfo::Call, StackArgs, CodeOrigin(), importJSCellGPRReg);
    JIT::DataLabelPtr targetToCheck;
    JIT::TrustedImmPtr initialRightValue(0);
    JIT::Jump slowPath = jit.branchPtrWithPatch(MacroAssembler::NotEqual, importJSCellGPRReg, targetToCheck, initialRightValue);
    JIT::Call fastCall = jit.nearCall();
    JIT::Jump done = jit.jump();
    slowPath.link(&jit);
    jit.move(MacroAssembler::TrustedImmPtr(callLinkInfo), GPRInfo::nonArgGPR0); // Link info needs to be in nonArgGPR0
    JIT::Call slowCall = jit.nearCall();
    done.link(&jit);

    switch (signature->returnType) {
    case Void:
        // Discard.
        break;
    case Func:
    case Anyfunc:
        // For the JavaScript embedding, imports with these types in their signature return are a WebAssembly.Module validation error.
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case I32: {
        jit.move(JIT::TrustedImm64(TagTypeNumber), GPRInfo::returnValueGPR2);
        JIT::Jump checkJSInt32 = jit.branch64(JIT::AboveOrEqual, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        jit.truncateDoubleToInt32(FPRInfo::returnValueFPR, GPRInfo::returnValueGPR);
        JIT::Jump checkJSNumber = jit.branchTest64(JIT::NonZero, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        jit.abortWithReason(AHIsNotJSNumber); // FIXME Coerce when the values aren't what we expect, instead of aborting. https://bugs.webkit.org/show_bug.cgi?id=165480
        checkJSInt32.link(&jit);
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        checkJSNumber.link(&jit);
        break;
    }
    case I64: {
        jit.move(JIT::TrustedImm64(TagTypeNumber), GPRInfo::returnValueGPR2);
        JIT::Jump checkJSInt32 = jit.branch64(JIT::AboveOrEqual, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        jit.truncateDoubleToInt64(FPRInfo::returnValueFPR, GPRInfo::returnValueGPR);
        JIT::Jump checkJSNumber = jit.branchTest64(JIT::NonZero, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        jit.abortWithReason(AHIsNotJSNumber); // FIXME Coerce when the values aren't what we expect, instead of aborting. https://bugs.webkit.org/show_bug.cgi?id=165480
        checkJSInt32.link(&jit);
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        checkJSNumber.link(&jit);
        break;
    }
    case F32: {
        jit.move(JIT::TrustedImm64(TagTypeNumber), GPRInfo::returnValueGPR2);
        jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        jit.convertDoubleToFloat(FPRInfo::returnValueFPR, FPRInfo::returnValueFPR);
        JIT::Jump checkJSInt32 = jit.branch64(JIT::AboveOrEqual, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        JIT::Jump checkJSNumber = jit.branchTest64(JIT::NonZero, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        jit.abortWithReason(AHIsNotJSNumber); // FIXME Coerce when the values aren't what we expect, instead of aborting. https://bugs.webkit.org/show_bug.cgi?id=165480
        checkJSInt32.link(&jit);
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        jit.convertInt64ToFloat(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        checkJSNumber.link(&jit);
        break;
    }
    case F64: {
        jit.move(JIT::TrustedImm64(TagTypeNumber), GPRInfo::returnValueGPR2);
        jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        JIT::Jump checkJSInt32 = jit.branch64(JIT::AboveOrEqual, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        JIT::Jump checkJSNumber = jit.branchTest64(JIT::NonZero, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
        jit.abortWithReason(AHIsNotJSNumber); // FIXME Coerce when the values aren't what we expect, instead of aborting. https://bugs.webkit.org/show_bug.cgi?id=165480
        checkJSInt32.link(&jit);
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        jit.convertInt64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        checkJSNumber.link(&jit);
        break;
    }
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    patchBuffer.link(slowCall, FunctionPtr(vm->getJITCallThunkEntryStub(linkCallThunkGenerator).entryFor(StackArgs).executableAddress()));
    CodeLocationLabel callReturnLocation(patchBuffer.locationOfNearCall(slowCall));
    CodeLocationLabel hotPathBegin(patchBuffer.locationOf(targetToCheck));
    CodeLocationNearCall hotPathOther = patchBuffer.locationOfNearCall(fastCall);
    callLinkInfo->setCallLocations(callReturnLocation, hotPathBegin, hotPathOther);
    return FINALIZE_CODE(patchBuffer, ("WebAssembly import[%i] stub for signature %p", importIndex, signature));
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
