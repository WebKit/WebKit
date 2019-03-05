/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WasmToJS.h"

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "FrameTracers.h"
#include "JITExceptions.h"
#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyRuntimeError.h"
#include "LinkBuffer.h"
#include "NativeErrorConstructor.h"
#include "ThunkGenerators.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmInstance.h"
#include "WasmSignatureInlines.h"

namespace JSC { namespace Wasm {

using JIT = CCallHelpers;

static void materializeImportJSCell(JIT& jit, unsigned importIndex, GPRReg poison, GPRReg result)
{
    // We're calling out of the current WebAssembly.Instance. That Instance has a list of all its import functions.
    jit.loadWasmContextInstance(result);
    jit.loadPtr(JIT::Address(result, Instance::offsetOfImportFunction(importIndex)), result);
    jit.xor64(poison, result);
}

static Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> handleBadI64Use(VM* vm, JIT& jit, const Signature& signature, unsigned importIndex)
{
    unsigned argCount = signature.argumentCount();

    bool hasBadI64Use = false;
    hasBadI64Use |= signature.returnType() == I64;
    for (unsigned argNum = 0; argNum < argCount && !hasBadI64Use; ++argNum) {
        Type argType = signature.argument(argNum);
        switch (argType) {
        case Void:
        case Func:
        case Anyfunc:
            RELEASE_ASSERT_NOT_REACHED();

        case I64: {
            hasBadI64Use = true;
            break;
        }

        default:
            break;
        }
    }

    if (hasBadI64Use) {
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm->topEntryFrame);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        jit.loadWasmContextInstance(GPRInfo::argumentGPR1);

        // Store Callee.
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR1, Instance::offsetOfOwner()), GPRInfo::argumentGPR1);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR1, JSWebAssemblyInstance::offsetOfPoisonedCallee()), GPRInfo::argumentGPR2);
        jit.move(CCallHelpers::TrustedImm64(JSWebAssemblyInstancePoison::key()), GPRInfo::argumentGPR3);
        jit.xor64(GPRInfo::argumentGPR3, GPRInfo::argumentGPR2);
        jit.storePtr(GPRInfo::argumentGPR2, JIT::Address(GPRInfo::callFrameRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register))));

        // Let's be paranoid on the exception path and zero out the poison instead of leaving it in an argument GPR.
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::argumentGPR3);

        auto call = jit.call(OperationPtrTag);
        jit.jumpToExceptionHandler(*vm);

        void (*throwBadI64)(ExecState*, JSWebAssemblyInstance*) = [] (ExecState* exec, JSWebAssemblyInstance* instance) -> void {
            VM* vm = &exec->vm();
            NativeCallFrameTracer tracer(vm, exec);

            {
                auto throwScope = DECLARE_THROW_SCOPE(*vm);
                JSGlobalObject* globalObject = instance->globalObject(*vm);
                auto* error = ErrorInstance::create(exec, *vm, globalObject->errorStructure(ErrorType::TypeError), "i64 not allowed as return type or argument to an imported function"_s);
                throwException(exec, throwScope, error);
            }

            genericUnwind(vm, exec);
            ASSERT(!!vm->callFrameForCatch);
        };

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate()))
            return makeUnexpected(BindingFailure::OutOfMemory);

        linkBuffer.link(call, FunctionPtr<OperationPtrTag>(throwBadI64));
        return FINALIZE_CODE(linkBuffer, WasmEntryPtrTag, "WebAssembly->JavaScript invalid i64 use in import[%i]", importIndex);
    }
    
    return MacroAssemblerCodeRef<WasmEntryPtrTag>();
}

Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> wasmToJS(VM* vm, Bag<CallLinkInfo>& callLinkInfos, SignatureIndex signatureIndex, unsigned importIndex)
{
    // FIXME: This function doesn't properly abstract away the calling convention.
    // It'd be super easy to do so: https://bugs.webkit.org/show_bug.cgi?id=169401
    const WasmCallingConvention& wasmCC = wasmCallingConvention();
    const JSCCallingConvention& jsCC = jscCallingConvention();
    const Signature& signature = SignatureInformation::get(signatureIndex);
    unsigned argCount = signature.argumentCount();
    JIT jit;

    // Note: WasmB3IRGenerator assumes that this stub treats SP as a callee save.
    // If we ever change this, we will also need to change WasmB3IRGenerator.

    // Below, we assume that the JS calling convention is always on the stack.
    ASSERT(!jsCC.m_gprArgs.size());
    ASSERT(!jsCC.m_fprArgs.size());

    jit.emitFunctionPrologue();
    jit.store64(JIT::TrustedImm32(0), JIT::Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * static_cast<int>(sizeof(Register)))); // FIXME Stop using 0 as codeBlocks. https://bugs.webkit.org/show_bug.cgi?id=165321

    auto badI64 = handleBadI64Use(vm, jit, signature, importIndex);
    if (!badI64 || badI64.value())
        return badI64;

    // Here we assume that the JS calling convention saves at least all the wasm callee saved. We therefore don't need to save and restore more registers since the wasm callee already took care of this.
    RegisterSet missingCalleeSaves = wasmCC.m_calleeSaveRegisters;
    missingCalleeSaves.exclude(jsCC.m_calleeSaveRegisters);
    ASSERT(missingCalleeSaves.isEmpty());

    if (!Options::useCallICsForWebAssemblyToJSCalls()) {
        ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(argCount * sizeof(uint64_t));
        char* buffer = argCount ? static_cast<char*>(scratchBuffer->dataBuffer()) : nullptr;
        unsigned marshalledGPRs = 0;
        unsigned marshalledFPRs = 0;
        unsigned bufferOffset = 0;
        unsigned frOffset = CallFrame::headerSizeInRegisters * static_cast<int>(sizeof(Register));
        const GPRReg scratchGPR = GPRInfo::regCS0;
        jit.subPtr(MacroAssembler::TrustedImm32(WTF::roundUpToMultipleOf(stackAlignmentBytes(), sizeof(Register))), MacroAssembler::stackPointerRegister);
        jit.storePtr(scratchGPR, MacroAssembler::Address(MacroAssembler::stackPointerRegister));

        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            Type argType = signature.argument(argNum);
            switch (argType) {
            case Void:
            case Func:
            case Anyfunc:
            case I64:
                RELEASE_ASSERT_NOT_REACHED();
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
                jit.zeroExtend32ToPtr(gprReg, gprReg);
                jit.store64(gprReg, buffer + bufferOffset);
                ++marshalledGPRs;
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
                jit.moveDoubleTo64(fprReg, scratchGPR);
                jit.store64(scratchGPR, buffer + bufferOffset);
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
                jit.moveDoubleTo64(fprReg, scratchGPR);
                jit.store64(scratchGPR, buffer + bufferOffset);
                ++marshalledFPRs;
                break;
            }
            }

            bufferOffset += sizeof(Register);
        }
        jit.loadPtr(MacroAssembler::Address(MacroAssembler::stackPointerRegister), scratchGPR);
        if (argCount) {
            // The GC should not look at this buffer at all, these aren't JSValues.
            jit.move(CCallHelpers::TrustedImmPtr(scratchBuffer->addressOfActiveLength()), GPRInfo::argumentGPR0);
            jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), GPRInfo::argumentGPR0);
        }

        uint64_t (*callFunc)(ExecState*, JSObject*, SignatureIndex, uint64_t*) =
            [] (ExecState* exec, JSObject* callee, SignatureIndex signatureIndex, uint64_t* buffer) -> uint64_t { 
                VM* vm = &exec->vm();
                NativeCallFrameTracer tracer(vm, exec);
                auto throwScope = DECLARE_THROW_SCOPE(*vm);
                const Signature& signature = SignatureInformation::get(signatureIndex);
                MarkedArgumentBuffer args;
                for (unsigned argNum = 0; argNum < signature.argumentCount(); ++argNum) {
                    Type argType = signature.argument(argNum);
                    JSValue arg;
                    switch (argType) {
                    case Void:
                    case Func:
                    case Anyfunc:
                    case I64:
                        RELEASE_ASSERT_NOT_REACHED();
                    case I32:
                        arg = jsNumber(static_cast<int32_t>(buffer[argNum]));
                        break;
                    case F32:
                    case F64:
                        arg = jsNumber(purifyNaN(bitwise_cast<double>(buffer[argNum])));
                        break;
                    }
                    args.append(arg);
                }
                if (UNLIKELY(args.hasOverflowed())) {
                    throwOutOfMemoryError(exec, throwScope);
                    return 0;
                }

                CallData callData;
                CallType callType = callee->methodTable(*vm)->getCallData(callee, callData);
                RELEASE_ASSERT(callType != CallType::None);
                JSValue result = call(exec, callee, callType, callData, jsUndefined(), args);
                RETURN_IF_EXCEPTION(throwScope, 0);

                uint64_t realResult;
                switch (signature.returnType()) {
                case Func:
                case Anyfunc:
                case I64:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                case Void:
                    break;
                case I32: {
                    realResult = static_cast<uint64_t>(static_cast<uint32_t>(result.toInt32(exec)));
                    break;
                }
                case F64:
                case F32: {
                    realResult = bitwise_cast<uint64_t>(result.toNumber(exec));
                    break;
                }
                }

                RETURN_IF_EXCEPTION(throwScope, 0);
                return realResult;
            };
        
        jit.loadWasmContextInstance(GPRInfo::argumentGPR0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfOwner()), GPRInfo::argumentGPR0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, JSWebAssemblyInstance::offsetOfPoisonedCallee()), GPRInfo::argumentGPR0);
        jit.move(CCallHelpers::TrustedImm64(JSWebAssemblyInstancePoison::key()), GPRInfo::argumentGPR3);
        jit.xor64(GPRInfo::argumentGPR3, GPRInfo::argumentGPR0);
        jit.storePtr(GPRInfo::argumentGPR0, JIT::Address(GPRInfo::callFrameRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
        
        materializeImportJSCell(jit, importIndex, GPRInfo::argumentGPR3, GPRInfo::argumentGPR1);
        
        // Let's be paranoid before the call and zero out the poison instead of leaving it in an argument GPR.
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::argumentGPR3);

        static_assert(GPRInfo::numberOfArgumentRegisters >= 4, "We rely on this with the call below.");
        static_assert(sizeof(SignatureIndex) == sizeof(uint64_t), "Following code assumes SignatureIndex is 64bit.");
        jit.setupArguments<decltype(callFunc)>(GPRInfo::argumentGPR1, CCallHelpers::TrustedImm64(signatureIndex), CCallHelpers::TrustedImmPtr(buffer));
        auto call = jit.call(OperationPtrTag);
        auto noException = jit.emitExceptionCheck(*vm, AssemblyHelpers::InvertedExceptionCheck);

        // Exception here.
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm->topEntryFrame);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        void (*doUnwinding)(ExecState*) = [] (ExecState* exec) -> void {
            VM* vm = &exec->vm();
            NativeCallFrameTracer tracer(vm, exec);
            genericUnwind(vm, exec);
            ASSERT(!!vm->callFrameForCatch);
        };
        auto exceptionCall = jit.call(OperationPtrTag);
        jit.jumpToExceptionHandler(*vm);

        noException.link(&jit);
        switch (signature.returnType()) {
        case F64: {
            jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
            break;
        }
        case F32: {
            jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
            jit.convertDoubleToFloat(FPRInfo::returnValueFPR, FPRInfo::returnValueFPR);
            break;
        }
        default:
            break;
        }

        jit.emitFunctionEpilogue();
        jit.ret();

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate()))
            return makeUnexpected(BindingFailure::OutOfMemory);

        linkBuffer.link(call, FunctionPtr<OperationPtrTag>(callFunc));
        linkBuffer.link(exceptionCall, FunctionPtr<OperationPtrTag>(doUnwinding));

        return FINALIZE_CODE(linkBuffer, WasmEntryPtrTag, "WebAssembly->JavaScript import[%i] %s", importIndex, signature.toString().ascii().data());
    }

    // Note: We don't need to perform a stack check here since WasmB3IRGenerator
    // will do the stack check for us. Whenever it detects that it might make
    // a call to this thunk, it'll make sure its stack check includes space
    // for us here.

    const unsigned numberOfParameters = argCount + 1; // There is a "this" argument.
    const unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + numberOfParameters;
    const unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    const unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);
    jit.subPtr(MacroAssembler::TrustedImm32(stackOffset), MacroAssembler::stackPointerRegister);
    JIT::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    // FIXME make these loops which switch on Signature if there are many arguments on the stack. It'll otherwise be huge for huge signatures. https://bugs.webkit.org/show_bug.cgi?id=165547
    
    // First go through the integer parameters, freeing up their register for use afterwards.
    {
        unsigned marshalledGPRs = 0;
        unsigned marshalledFPRs = 0;
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        unsigned frOffset = CallFrame::headerSizeInRegisters * static_cast<int>(sizeof(Register));
        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            Type argType = signature.argument(argNum);
            switch (argType) {
            case Void:
            case Func:
            case Anyfunc:
            case I64:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
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
                jit.zeroExtend32ToPtr(gprReg, gprReg); // Clear non-int32 and non-tag bits.
                jit.boxInt32(gprReg, JSValueRegs(gprReg), DoNotHaveTagRegisters);
                jit.store64(gprReg, calleeFrame.withOffset(calleeFrameOffset));
                calleeFrameOffset += sizeof(Register);
                break;
            }
            case F32:
            case F64:
                // Skipped: handled below.
                if (marshalledFPRs >= wasmCC.m_fprArgs.size())
                    frOffset += sizeof(Register);
                ++marshalledFPRs;
                calleeFrameOffset += sizeof(Register);
                break;
            }
        }
    }
    
    {
        // Integer registers have already been spilled, these are now available.
        GPRReg doubleEncodeOffsetGPRReg = GPRInfo::argumentGPR0;
        GPRReg scratch = GPRInfo::argumentGPR1;
        bool hasMaterializedDoubleEncodeOffset = false;
        auto materializeDoubleEncodeOffset = [&hasMaterializedDoubleEncodeOffset, &jit] (GPRReg dest) {
            if (!hasMaterializedDoubleEncodeOffset) {
                static_assert(DoubleEncodeOffset == 1ll << 48, "codegen assumes this below");
                jit.move(JIT::TrustedImm32(1), dest);
                jit.lshift64(JIT::TrustedImm32(48), dest);
                hasMaterializedDoubleEncodeOffset = true;
            }
        };

        unsigned marshalledGPRs = 0;
        unsigned marshalledFPRs = 0;
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        unsigned frOffset = CallFrame::headerSizeInRegisters * static_cast<int>(sizeof(Register));

        auto marshallFPR = [&] (FPRReg fprReg) {
            jit.purifyNaN(fprReg);
            jit.moveDoubleTo64(fprReg, scratch);
            materializeDoubleEncodeOffset(doubleEncodeOffsetGPRReg);
            jit.add64(doubleEncodeOffsetGPRReg, scratch);
            jit.store64(scratch, calleeFrame.withOffset(calleeFrameOffset));
            calleeFrameOffset += sizeof(Register);
            ++marshalledFPRs;
        };

        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            Type argType = signature.argument(argNum);
            switch (argType) {
            case Void:
            case Func:
            case Anyfunc:
            case I64:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case I32:
                // Skipped: handled above.
                if (marshalledGPRs >= wasmCC.m_gprArgs.size())
                    frOffset += sizeof(Register);
                ++marshalledGPRs;
                calleeFrameOffset += sizeof(Register);
                break;
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
                marshallFPR(fprReg);
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
                marshallFPR(fprReg);
                break;
            }
            }
        }
    }

    GPRReg poison = GPRInfo::argumentGPR1;
    ASSERT(poison != GPRInfo::argumentGPR0); // Both are used at the same time below.

    jit.loadWasmContextInstance(GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfOwner()), GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, JSWebAssemblyInstance::offsetOfPoisonedCallee()), GPRInfo::argumentGPR0);
    jit.move(CCallHelpers::TrustedImm64(JSWebAssemblyInstancePoison::key()), poison);
    jit.xor64(poison, GPRInfo::argumentGPR0);
    jit.storePtr(GPRInfo::argumentGPR0, JIT::Address(GPRInfo::callFrameRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register))));

    GPRReg importJSCellGPRReg = GPRInfo::regT0; // Callee needs to be in regT0 for slow path below.
    ASSERT(poison != importJSCellGPRReg);

    ASSERT(!wasmCC.m_calleeSaveRegisters.get(importJSCellGPRReg));
    materializeImportJSCell(jit, importIndex, poison, importJSCellGPRReg);

    // Let's be paranoid zero out the poison instead of leaving it in an argument GPR.
    jit.move(CCallHelpers::TrustedImm32(0), poison);

    jit.store64(importJSCellGPRReg, calleeFrame.withOffset(CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    jit.store32(JIT::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset));
    jit.store64(JIT::TrustedImm64(ValueUndefined), calleeFrame.withOffset(CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register))));

    // FIXME Tail call if the wasm return type is void and no registers were spilled. https://bugs.webkit.org/show_bug.cgi?id=165488

    CallLinkInfo* callLinkInfo = callLinkInfos.add();
    callLinkInfo->setUpCall(CallLinkInfo::Call, CodeOrigin(), importJSCellGPRReg);
    JIT::DataLabelPtr targetToCheck;
    JIT::TrustedImmPtr initialRightValue(nullptr);
    JIT::Jump slowPath = jit.branchPtrWithPatch(MacroAssembler::NotEqual, importJSCellGPRReg, targetToCheck, initialRightValue);
    JIT::Call fastCall = jit.nearCall();
    JIT::Jump done = jit.jump();
    slowPath.link(&jit);
    // Callee needs to be in regT0 here.
    jit.move(MacroAssembler::TrustedImmPtr(callLinkInfo), GPRInfo::regT2); // Link info needs to be in regT2.
    JIT::Call slowCall = jit.nearCall();
    done.link(&jit);

    CCallHelpers::JumpList exceptionChecks;

    switch (signature.returnType()) {
    case Void:
        // Discard.
        break;
    case Func:
    case Anyfunc:
        // For the JavaScript embedding, imports with these types in their signature return are a WebAssembly.Module validation error.
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case I64: {
        RELEASE_ASSERT_NOT_REACHED(); // Handled above.
    }
    case I32: {
        CCallHelpers::JumpList done;
        CCallHelpers::JumpList slowPath;

        int32_t (*convertToI32)(ExecState*, JSValue) = [] (ExecState* exec, JSValue v) -> int32_t {
            VM* vm = &exec->vm();
            NativeCallFrameTracer tracer(vm, exec);
            return v.toInt32(exec);
        };

        slowPath.append(jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters));
        slowPath.append(jit.branchIfNotInt32(JSValueRegs(GPRInfo::returnValueGPR), DoNotHaveTagRegisters));
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        done.append(jit.jump());

        slowPath.link(&jit);
        jit.setupArguments<decltype(convertToI32)>(GPRInfo::returnValueGPR);
        auto call = jit.call(OperationPtrTag);
        exceptionChecks.append(jit.emitJumpIfException(*vm));

        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(convertToI32));
        });

        done.link(&jit);
        break;
    }
    case F32: {
        CCallHelpers::JumpList done;

        float (*convertToF32)(ExecState*, JSValue) = [] (ExecState* exec, JSValue v) -> float {
            VM* vm = &exec->vm();
            NativeCallFrameTracer tracer(vm, exec);
            return static_cast<float>(v.toNumber(exec));
        };

        auto notANumber = jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
        auto isDouble = jit.branchIfNotInt32(JSValueRegs(GPRInfo::returnValueGPR), DoNotHaveTagRegisters);
        // We're an int32
        jit.signExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        jit.convertInt64ToFloat(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        done.append(jit.jump());

        isDouble.link(&jit);
        jit.move(JIT::TrustedImm64(TagTypeNumber), GPRInfo::returnValueGPR2);
        jit.add64(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
        jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        jit.convertDoubleToFloat(FPRInfo::returnValueFPR, FPRInfo::returnValueFPR);
        done.append(jit.jump());

        notANumber.link(&jit);
        jit.setupArguments<decltype(convertToF32)>(GPRInfo::returnValueGPR);
        auto call = jit.call(OperationPtrTag);
        exceptionChecks.append(jit.emitJumpIfException(*vm));

        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(convertToF32));
        });

        done.link(&jit);
        break;
    }
    case F64: {
        CCallHelpers::JumpList done;

        double (*convertToF64)(ExecState*, JSValue) = [] (ExecState* exec, JSValue v) -> double {
            VM* vm = &exec->vm();
            NativeCallFrameTracer tracer(vm, exec);
            return v.toNumber(exec);
        };

        auto notANumber = jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
        auto isDouble = jit.branchIfNotInt32(JSValueRegs(GPRInfo::returnValueGPR), DoNotHaveTagRegisters);
        // We're an int32
        jit.signExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        jit.convertInt64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        done.append(jit.jump());

        isDouble.link(&jit);
        jit.move(JIT::TrustedImm64(TagTypeNumber), GPRInfo::returnValueGPR2);
        jit.add64(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
        jit.move64ToDouble(GPRInfo::returnValueGPR, FPRInfo::returnValueFPR);
        done.append(jit.jump());

        notANumber.link(&jit);
        jit.setupArguments<decltype(convertToF64)>(GPRInfo::returnValueGPR);
        auto call = jit.call(OperationPtrTag);
        exceptionChecks.append(jit.emitJumpIfException(*vm));

        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(convertToF64));
        });

        done.link(&jit);
        break;
    }
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    if (!exceptionChecks.empty()) {
        exceptionChecks.link(&jit);
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm->topEntryFrame);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        auto call = jit.call(OperationPtrTag);
        jit.jumpToExceptionHandler(*vm);

        void (*doUnwinding)(ExecState*) = [] (ExecState* exec) -> void {
            VM* vm = &exec->vm();
            NativeCallFrameTracer tracer(vm, exec);
            genericUnwind(vm, exec);
            ASSERT(!!vm->callFrameForCatch);
        };

        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(doUnwinding));
        });
    }

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, JITCompilationCanFail);
    if (UNLIKELY(patchBuffer.didFailToAllocate()))
        return makeUnexpected(BindingFailure::OutOfMemory);

    patchBuffer.link(slowCall, FunctionPtr<JITThunkPtrTag>(vm->getCTIStub(linkCallThunkGenerator).code()));
    CodeLocationLabel<JSInternalPtrTag> callReturnLocation(patchBuffer.locationOfNearCall<JSInternalPtrTag>(slowCall));
    CodeLocationLabel<JSInternalPtrTag> hotPathBegin(patchBuffer.locationOf<JSInternalPtrTag>(targetToCheck));
    CodeLocationNearCall<JSInternalPtrTag> hotPathOther = patchBuffer.locationOfNearCall<JSInternalPtrTag>(fastCall);
    callLinkInfo->setCallLocations(callReturnLocation, hotPathBegin, hotPathOther);

    return FINALIZE_CODE(patchBuffer, WasmEntryPtrTag, "WebAssembly->JavaScript import[%i] %s", importIndex, signature.toString().ascii().data());
}

void* wasmToJSException(ExecState* exec, Wasm::ExceptionType type, Instance* wasmInstance)
{
    wasmInstance->storeTopCallFrame(exec);
    JSWebAssemblyInstance* instance = wasmInstance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = instance->globalObject();

    // Do not retrieve VM& from ExecState since ExecState's callee is not a JSCell.
    VM& vm = globalObject->vm();

    {
        auto throwScope = DECLARE_THROW_SCOPE(vm);

        JSObject* error;
        if (type == ExceptionType::StackOverflow)
            error = createStackOverflowError(exec, globalObject);
        else
            error = JSWebAssemblyRuntimeError::create(exec, vm, globalObject->WebAssemblyRuntimeErrorStructure(), Wasm::errorMessageForExceptionType(type));
        throwException(exec, throwScope, error);
    }

    genericUnwind(&vm, exec);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    // FIXME: We could make this better:
    // This is a total hack, but the llint (both op_catch and handleUncaughtException)
    // require a cell in the callee field to load the VM. (The baseline JIT does not require
    // this since it is compiled with a constant VM pointer.) We could make the calling convention
    // for exceptions first load callFrameForCatch info call frame register before jumping
    // to the exception handler. If we did this, we could remove this terrible hack.
    // https://bugs.webkit.org/show_bug.cgi?id=170440
    bitwise_cast<uint64_t*>(exec)[CallFrameSlot::callee] = bitwise_cast<uint64_t>(instance->webAssemblyToJSCallee());
    return vm.targetMachinePCForThrow;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
