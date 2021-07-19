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
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "ThunkGenerators.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmExceptionType.h"
#include "WasmInstance.h"
#include "WasmOperations.h"
#include "WasmSignatureInlines.h"
#include <wtf/FunctionTraits.h>

namespace JSC { namespace Wasm {

using JIT = CCallHelpers;

static void materializeImportJSCell(JIT& jit, unsigned importIndex, GPRReg result)
{
    // We're calling out of the current WebAssembly.Instance. That Instance has a list of all its import functions.
    jit.loadWasmContextInstance(result);
    jit.loadPtr(JIT::Address(result, Instance::offsetOfImportFunction(importIndex)), result);
}

Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> wasmToJS(VM& vm, Bag<CallLinkInfo>& callLinkInfos, SignatureIndex signatureIndex, unsigned importIndex)
{
    // FIXME: This function doesn't properly abstract away the calling convention.
    // It'd be super easy to do so: https://bugs.webkit.org/show_bug.cgi?id=169401
    const auto& wasmCC = wasmCallingConvention();
    const auto& jsCC = jsCallingConvention();
    const Signature& signature = SignatureInformation::get(signatureIndex);
    unsigned argCount = signature.argumentCount();
    JIT jit;

    CallInformation wasmCallInfo = wasmCC.callInformationFor(signature, CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    // Note: WasmB3IRGenerator assumes that this stub treats SP as a callee save.
    // If we ever change this, we will also need to change WasmB3IRGenerator.

    // Below, we assume that the JS calling convention is always on the stack.
    ASSERT(!jsCC.gprArgs.size());
    ASSERT(!jsCC.fprArgs.size());

    jit.emitFunctionPrologue();
    jit.emitZeroToCallFrameHeader(CallFrameSlot::codeBlock); // FIXME Stop using 0 as codeBlocks. https://bugs.webkit.org/show_bug.cgi?id=165321

    // Here we assume that the JS calling convention saves at least all the wasm callee saved. We therefore don't need to save and restore more registers since the wasm callee already took care of this.
    RegisterSet missingCalleeSaves = wasmCC.calleeSaveRegisters;
    missingCalleeSaves.exclude(jsCC.calleeSaveRegisters);
    ASSERT(missingCalleeSaves.isEmpty());

    // Note: We don't need to perform a stack check here since WasmB3IRGenerator
    // will do the stack check for us. Whenever it detects that it might make
    // a call to this thunk, it'll make sure its stack check includes space
    // for us here.

    const unsigned numberOfParameters = argCount + 1; // There is a "this" argument.
    const unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + numberOfParameters;
    const unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    const unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), std::max<unsigned>(numberOfBytesForCall, savedResultRegisters.size() * sizeof(CPURegister)));
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
            switch (argType.kind) {
            case TypeKind::Void:
            case TypeKind::Func:
            case TypeKind::RefNull:
            case TypeKind::Ref:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case TypeKind::TypeIdx:
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::I32:
            case TypeKind::I64: {
                GPRReg gprReg;
                if (marshalledGPRs < wasmCC.gprArgs.size())
                    gprReg = wasmCC.gprArgs[marshalledGPRs].gpr();
                else {
                    // We've already spilled all arguments, these registers are available as scratch.
                    gprReg = GPRInfo::argumentGPR0;
                    jit.load64(JIT::Address(GPRInfo::callFrameRegister, frOffset), gprReg);
                    frOffset += sizeof(Register);
                }
                ++marshalledGPRs;
                if (argType.isI32()) {
                    jit.zeroExtend32ToWord(gprReg, gprReg); // Clear non-int32 and non-tag bits.
                    jit.boxInt32(gprReg, JSValueRegs(gprReg), DoNotHaveTagRegisters);
                }
                jit.store64(gprReg, calleeFrame.withOffset(calleeFrameOffset));
                calleeFrameOffset += sizeof(Register);
                break;
            }
            case TypeKind::F32:
            case TypeKind::F64:
                // Skipped: handled below.
                if (marshalledFPRs >= wasmCC.fprArgs.size())
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
#if CPU(ARM64)
                jit.move(JIT::TrustedImm64(JSValue::DoubleEncodeOffset), dest);
#else
                jit.move(JIT::TrustedImm32(1), dest);
                jit.lshift64(JIT::TrustedImm32(JSValue::DoubleEncodeOffsetBit), dest);
#endif
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
            switch (argType.kind) {
            case TypeKind::Void:
            case TypeKind::Func:
            case TypeKind::RefNull:
            case TypeKind::Ref:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case TypeKind::TypeIdx:
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::I32:
            case TypeKind::I64: {
                // Skipped: handled above.
                if (marshalledGPRs >= wasmCC.gprArgs.size())
                    frOffset += sizeof(Register);
                ++marshalledGPRs;
                calleeFrameOffset += sizeof(Register);
                break;
            }
            case TypeKind::F32: {
                FPRReg fprReg;
                if (marshalledFPRs < wasmCC.fprArgs.size())
                    fprReg = wasmCC.fprArgs[marshalledFPRs].fpr();
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
            case TypeKind::F64: {
                FPRReg fprReg;
                if (marshalledFPRs < wasmCC.fprArgs.size())
                    fprReg = wasmCC.fprArgs[marshalledFPRs].fpr();
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

    CCallHelpers::JumpList exceptionChecks;

    if (wasmCallInfo.argumentsIncludeI64) {
        // Since all argument GPRs and FPRs are stored into stack frames, clobbering caller-save registers is OK here.
        // We call functions to convert I64 to BigInt.
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            if (signature.argument(argNum).isI64()) {
                jit.loadWasmContextInstance(GPRInfo::argumentGPR0);
                jit.load64(calleeFrame.withOffset(calleeFrameOffset), GPRInfo::argumentGPR1);
                jit.setupArguments<decltype(operationConvertToBigInt)>(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);
                auto call = jit.call(OperationPtrTag);
                exceptionChecks.append(jit.emitJumpIfException(vm));
                jit.store64(GPRInfo::returnValueGPR, calleeFrame.withOffset(calleeFrameOffset));
                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationConvertToBigInt));
                });
            }
            calleeFrameOffset += sizeof(Register);
        }
    }

    jit.loadWasmContextInstance(GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfOwner()), GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, JSWebAssemblyInstance::offsetOfModule()), GPRInfo::argumentGPR0);
    jit.emitPutToCallFrameHeader(GPRInfo::argumentGPR0, CallFrameSlot::callee);

    GPRReg importJSCellGPRReg = GPRInfo::regT0; // Callee needs to be in regT0 for slow path below.

    ASSERT(!wasmCC.calleeSaveRegisters.get(importJSCellGPRReg));
    materializeImportJSCell(jit, importIndex, importJSCellGPRReg);

    jit.store64(importJSCellGPRReg, calleeFrame.withOffset(CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    jit.store32(JIT::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset));
    jit.store64(JIT::TrustedImm64(JSValue::ValueUndefined), calleeFrame.withOffset(CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register))));

    // FIXME Tail call if the wasm return type is void and no registers were spilled. https://bugs.webkit.org/show_bug.cgi?id=165488

    CallLinkInfo* callLinkInfo = callLinkInfos.add(CodeOrigin());
    callLinkInfo->setUpCall(CallLinkInfo::Call, importJSCellGPRReg);
    auto slowPath = callLinkInfo->emitFastPath(jit, importJSCellGPRReg, InvalidGPRReg, CallLinkInfo::UseDataIC::No);

    JIT::Jump done = jit.jump();
    slowPath.link(&jit);
    auto slowPathStart = jit.label();
    // Callee needs to be in regT0 here.
    jit.loadWasmContextInstance(GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT3, Instance::offsetOfOwner()), GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT3, JSWebAssemblyInstance::offsetOfGlobalObject()), GPRInfo::regT3);
    callLinkInfo->emitSlowPath(vm, jit);
    done.link(&jit);
    auto doneLocation = jit.label();

    if (signature.returnCount() == 1) {
        switch (signature.returnType(0).kind) {
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            // For the JavaScript embedding, imports with these types in their signature return are a WebAssembly.Module validation error.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case TypeKind::I64: {
            // FIXME: Optimize I64 extraction from BigInt.
            // https://bugs.webkit.org/show_bug.cgi?id=220053
            GPRReg dest = wasmCallInfo.results[0].gpr();
            jit.setupArguments<decltype(operationConvertToI64)>(GPRInfo::returnValueGPR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(GPRInfo::returnValueGPR, dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationConvertToI64));
            });
            break;
        }
        case TypeKind::I32: {
            CCallHelpers::JumpList done;
            CCallHelpers::JumpList slowPath;
            GPRReg dest = wasmCallInfo.results[0].gpr();

            slowPath.append(jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters));
            slowPath.append(jit.branchIfNotInt32(JSValueRegs(GPRInfo::returnValueGPR), DoNotHaveTagRegisters));
            jit.zeroExtend32ToWord(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            slowPath.link(&jit);
            jit.setupArguments<decltype(operationConvertToI32)>(GPRInfo::returnValueGPR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(GPRInfo::returnValueGPR, dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationConvertToI32));
            });

            done.link(&jit);
            break;
        }
        case TypeKind::Funcref:
        case TypeKind::Externref:
        case TypeKind::TypeIdx:
            jit.move(GPRInfo::returnValueGPR, wasmCallInfo.results[0].gpr());
            break;
        case TypeKind::F32: {
            CCallHelpers::JumpList done;
            FPRReg dest = wasmCallInfo.results[0].fpr();

            auto notANumber = jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
            auto isDouble = jit.branchIfNotInt32(JSValueRegs(GPRInfo::returnValueGPR), DoNotHaveTagRegisters);
            // We're an int32
            jit.signExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
            jit.convertInt64ToFloat(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            isDouble.link(&jit);
            jit.move(JIT::TrustedImm64(JSValue::NumberTag), GPRInfo::returnValueGPR2);
            jit.add64(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
            jit.move64ToDouble(GPRInfo::returnValueGPR, dest);
            jit.convertDoubleToFloat(dest, dest);
            done.append(jit.jump());

            notANumber.link(&jit);
            jit.setupArguments<decltype(operationConvertToF32)>(GPRInfo::returnValueGPR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(FPRInfo::returnValueFPR , dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationConvertToF32));
            });

            done.link(&jit);
            break;
        }
        case TypeKind::F64: {
            CCallHelpers::JumpList done;
            FPRReg dest = wasmCallInfo.results[0].fpr();

            auto notANumber = jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
            auto isDouble = jit.branchIfNotInt32(JSValueRegs(GPRInfo::returnValueGPR), DoNotHaveTagRegisters);
            // We're an int32
            jit.signExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
            jit.convertInt64ToDouble(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            isDouble.link(&jit);
            jit.move(JIT::TrustedImm64(JSValue::NumberTag), GPRInfo::returnValueGPR2);
            jit.add64(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
            jit.move64ToDouble(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            notANumber.link(&jit);
            jit.setupArguments<decltype(operationConvertToF64)>(GPRInfo::returnValueGPR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(FPRInfo::returnValueFPR, dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationConvertToF64));
            });

            done.link(&jit);
            break;
        }
        }
    } else if (signature.returnCount() > 1) {
        GPRReg wasmContextInstanceGPR = PinnedRegisterInfo::get().wasmContextInstancePointer;
        if (Context::useFastTLS()) {
            wasmContextInstanceGPR = GPRInfo::argumentGPR1;
            static_assert(std::is_same_v<Wasm::Instance*, typename FunctionTraits<decltype(operationIterateResults)>::ArgumentType<1>>, "Instance should be the second parameter.");
            jit.loadWasmContextInstance(wasmContextInstanceGPR);
        }

        jit.setupArguments<decltype(operationIterateResults)>(wasmContextInstanceGPR, &signature, GPRInfo::returnValueGPR, CCallHelpers::stackPointerRegister, CCallHelpers::framePointerRegister);
        jit.callOperation(FunctionPtr<OperationPtrTag>(operationIterateResults));
        exceptionChecks.append(jit.emitJumpIfException(vm));

        for (RegisterAtOffset location : savedResultRegisters)
            jit.load64ToReg(CCallHelpers::Address(CCallHelpers::stackPointerRegister, location.offset()), location.reg());
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    if (!exceptionChecks.empty()) {
        exceptionChecks.link(&jit);
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        auto call = jit.call(OperationPtrTag);
        jit.jumpToExceptionHandler(vm);

        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationWasmUnwind));
        });
    }

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (UNLIKELY(patchBuffer.didFailToAllocate()))
        return makeUnexpected(BindingFailure::OutOfMemory);

    callLinkInfo->setCodeLocations(
        patchBuffer.locationOf<JSInternalPtrTag>(slowPathStart),
        patchBuffer.locationOf<JSInternalPtrTag>(doneLocation));

    return FINALIZE_WASM_CODE(patchBuffer, WasmEntryPtrTag, "WebAssembly->JavaScript import[%i] %s", importIndex, signature.toString().ascii().data());
}

void emitThrowWasmToJSException(CCallHelpers& jit, GPRReg wasmInstance, Wasm::ExceptionType type)
{
    ASSERT(wasmInstance != GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(wasmInstance, Wasm::Instance::offsetOfPointerToTopEntryFrame()), GPRInfo::argumentGPR0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0), GPRInfo::argumentGPR0);
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    jit.move(CCallHelpers::TrustedImm32(static_cast<int32_t>(type)), GPRInfo::argumentGPR1);

    CCallHelpers::Call call = jit.call(OperationPtrTag);

    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.breakpoint(); // We should not reach this.

    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        linkBuffer.link(call, FunctionPtr<OperationPtrTag>(Wasm::operationWasmToJSException));
    });
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
