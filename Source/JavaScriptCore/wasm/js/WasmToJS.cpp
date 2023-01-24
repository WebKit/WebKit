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
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "ThunkGenerators.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmInstance.h"
#include "WasmOperations.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/FunctionTraits.h>

namespace JSC { namespace Wasm {

using JIT = CCallHelpers;

static void materializeImportJSCell(JIT& jit, unsigned importIndex, GPRReg result)
{
    // We're calling out of the current WebAssembly.Instance. That Instance has a list of all its import functions.
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfImportFunction(importIndex)), result);
}

static Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> handleBadImportTypeUse(JIT& jit, unsigned importIndex, Wasm::ExceptionType exceptionType)
{
    jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);

    emitThrowWasmToJSException(jit, GPRInfo::argumentGPR0, exceptionType);

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate()))
        return makeUnexpected(BindingFailure::OutOfMemory);

    return FINALIZE_WASM_CODE(linkBuffer, WasmEntryPtrTag, "WebAssembly->JavaScript throw exception due to invalid use of restricted type in import[%i]", importIndex);
}

Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> wasmToJS(VM& vm, WasmToJSCallee& callee, OptimizingCallLinkInfo& callLinkInfo, TypeIndex typeIndex, unsigned importIndex)
{
    // FIXME: This function doesn't properly abstract away the calling convention.
    // It'd be super easy to do so: https://bugs.webkit.org/show_bug.cgi?id=169401
    const auto& wasmCC = wasmCallingConvention();
    const auto& jsCC = jsCallingConvention();
    const TypeDefinition& typeDefinition = TypeInformation::get(typeIndex).expand();
    const auto& signature = *typeDefinition.as<FunctionSignature>();
    unsigned argCount = signature.argumentCount();
    constexpr GPRReg importJSCellGPRReg = GPRInfo::regT0; // Callee needs to be in regT0 for slow path below.
    JIT jit;

    CallInformation wasmCallInfo = wasmCC.callInformationFor(typeDefinition, CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    // Note: WasmB3IRGenerator assumes that this stub treats SP as a callee save.
    // If we ever change this, we will also need to change WasmB3IRGenerator.

    // Below, we assume that the JS calling convention is always on the stack.
    ASSERT_UNUSED(jsCC, !jsCC.jsrArgs.size());
    ASSERT(!jsCC.fprArgs.size());

    jit.emitFunctionPrologue();
    GPRReg scratchGPR = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(&callee)), scratchGPR);
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    if constexpr (is32Bit()) {
        CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
        jit.storePtr(scratchGPR, calleeSlot.withOffset(PayloadOffset));
        jit.move(CCallHelpers::TrustedImm32(JSValue::WasmTag), scratchGPR);
        jit.store32(scratchGPR, calleeSlot.withOffset(TagOffset));
        jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    } else
        jit.storePairPtr(GPRInfo::wasmContextInstancePointer, scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    callLinkInfo = OptimizingCallLinkInfo(CodeOrigin(), CallLinkInfo::UseDataIC::No);
    callLinkInfo.setUpCall(CallLinkInfo::Call, importJSCellGPRReg);

    // GC support is experimental and currently will throw a runtime error when struct
    // and array type indices are exported via a function signature to JS.
    if (Options::useWebAssemblyGC() && (wasmCallInfo.argumentsIncludeGCTypeIndex || wasmCallInfo.resultsIncludeGCTypeIndex))
        return handleBadImportTypeUse(jit, importIndex, Wasm::ExceptionType::InvalidGCTypeUse);

    // https://webassembly.github.io/spec/js-api/index.html#exported-function-exotic-objects
    // If parameters or results contain v128, throw a TypeError.
    // Note: the above error is thrown each time the [[Call]] method is invoked.
    if (Options::useWebAssemblySIMD() && (wasmCallInfo.argumentsOrResultsIncludeV128))
        return handleBadImportTypeUse(jit, importIndex, ExceptionType::TypeErrorInvalidV128Use);

    // Here we assume that the JS calling convention saves at least all the wasm callee saved. We therefore don't need to save and restore more registers since the wasm callee already took care of this.
#if ASSERT_ENABLED
    wasmCC.calleeSaveRegisters.forEachWithWidth([&] (Reg reg, Width width) {
        ASSERT(jsCC.calleeSaveRegisters.contains(reg, width));
    });
#endif

    // Note: We don't need to perform a stack check here since WasmB3IRGenerator
    // will do the stack check for us. Whenever it detects that it might make
    // a call to this thunk, it'll make sure its stack check includes space
    // for us here.

    const unsigned numberOfParameters = argCount + 1; // There is a "this" argument.
    const unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
    const unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    const unsigned numberOfBytesForSavedResults = savedResultRegisters.sizeOfAreaInBytes();
    const unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), std::max(numberOfBytesForCall, numberOfBytesForSavedResults));
    jit.subPtr(MacroAssembler::TrustedImm32(stackOffset), MacroAssembler::stackPointerRegister);
    JIT::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    // FIXME make these loops which switch on signature if there are many arguments on the stack. It'll otherwise be huge for huge type definitions. https://bugs.webkit.org/show_bug.cgi?id=165547

#if USE(JSVALUE64)
    constexpr JSValueRegs jsArg10 { GPRInfo::argumentGPR0 };
#elif USE(JSVALUE32_64)
    constexpr JSValueRegs jsArg10 { GPRInfo::argumentGPR1, GPRInfo::argumentGPR0 };
#endif

    // First go through the integer parameters, freeing up their register for use afterwards.
    {
        unsigned marshalledGPRs = 0;
        unsigned marshalledFPRs = 0;
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        unsigned frOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            Type argType = signature.argumentType(argNum);
            switch (argType.kind) {
            case TypeKind::Void:
            case TypeKind::Func:
            case TypeKind::Struct:
            case TypeKind::Structref:
            case TypeKind::Array:
            case TypeKind::Arrayref:
            case TypeKind::I31ref:
            case TypeKind::Rec:
            case TypeKind::Sub:
            case TypeKind::V128:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case TypeKind::RefNull:
            case TypeKind::Ref:
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::I32:
            case TypeKind::I64: {
                JSValueRegs argReg;
                if (marshalledGPRs < wasmCC.jsrArgs.size())
                    argReg = wasmCC.jsrArgs[marshalledGPRs];
                else {
                    // We've already spilled all arguments, these registers are available as scratch.
                    argReg = jsArg10;
                    jit.loadValue(JIT::Address(GPRInfo::callFrameRegister, frOffset), argReg);
                    frOffset += sizeof(Register);
                }
                ++marshalledGPRs;
                if (argType.isI32()) {
                    jit.zeroExtend32ToWord(argReg.payloadGPR(), argReg.payloadGPR()); // Clear non-int32 and non-tag bits.
                    jit.boxInt32(argReg.payloadGPR(), argReg, DoNotHaveTagRegisters);
                }
                jit.storeValue(argReg, calleeFrame.withOffset(calleeFrameOffset));
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
#if USE(JSVALUE64)
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
#endif

        unsigned marshalledGPRs = 0;
        unsigned marshalledFPRs = 0;
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        unsigned frOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));

        auto marshallFPR = [&] (FPRReg fprReg) {
            jit.purifyNaN(fprReg);
#if USE(JSVALUE64)
            jit.moveDoubleTo64(fprReg, scratch);
            materializeDoubleEncodeOffset(doubleEncodeOffsetGPRReg);
            jit.add64(doubleEncodeOffsetGPRReg, scratch);
            jit.store64(scratch, calleeFrame.withOffset(calleeFrameOffset));
#else
            jit.storeDouble(fprReg, calleeFrame.withOffset(calleeFrameOffset));
#endif
            calleeFrameOffset += sizeof(Register);
            ++marshalledFPRs;
        };

        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            Type argType = signature.argumentType(argNum);
            switch (argType.kind) {
            case TypeKind::Void:
            case TypeKind::Func:
            case TypeKind::Struct:
            case TypeKind::Structref:
            case TypeKind::Array:
            case TypeKind::Arrayref:
            case TypeKind::I31ref:
            case TypeKind::Rec:
            case TypeKind::Sub:
            case TypeKind::V128:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case TypeKind::RefNull:
            case TypeKind::Ref:
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::I32:
            case TypeKind::I64: {
                // Skipped: handled above.
                if (marshalledGPRs >= wasmCC.jsrArgs.size())
                    frOffset += sizeof(Register);
                ++marshalledGPRs;
                calleeFrameOffset += sizeof(Register);
                break;
            }
            case TypeKind::F32: {
                FPRReg fprReg;
                if (marshalledFPRs < wasmCC.fprArgs.size())
                    fprReg = wasmCC.fprArgs[marshalledFPRs];
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
                    fprReg = wasmCC.fprArgs[marshalledFPRs];
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
            if (signature.argumentType(argNum).isI64()) {
                using Operation = decltype(operationConvertToBigInt);
                constexpr JSValueRegs valueJSR = preferredArgumentJSR<Operation, 1>();
                jit.loadValue(calleeFrame.withOffset(calleeFrameOffset), valueJSR);
                jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                jit.setupArguments<Operation>(GPRInfo::wasmContextInstancePointer, valueJSR);
                auto call = jit.call(OperationPtrTag);
                exceptionChecks.append(jit.emitJumpIfException(vm));
                jit.storeValue(JSRInfo::returnValueJSR, calleeFrame.withOffset(calleeFrameOffset));
                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    linkBuffer.link<OperationPtrTag>(call, operationConvertToBigInt);
                });
            }
            calleeFrameOffset += sizeof(Register);
        }
    }

    // jsArg10 might overlap with regT0, so store 'this' argument first
    jit.storeValue(jsUndefined(), calleeFrame.withOffset(CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register))), jsArg10);
    ASSERT(!wasmCC.calleeSaveRegisters.contains(importJSCellGPRReg, IgnoreVectors));
    materializeImportJSCell(jit, importIndex, importJSCellGPRReg);
    jit.storeCell(importJSCellGPRReg, calleeFrame.withOffset(CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    jit.store32(JIT::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset));

    // FIXME Tail call if the wasm return type is void and no registers were spilled. https://bugs.webkit.org/show_bug.cgi?id=165488

    auto slowPath = CallLinkInfo::emitFastPath(jit, &callLinkInfo, importJSCellGPRReg, InvalidGPRReg);

    JIT::Jump done = jit.jump();
    slowPath.link(&jit);
    auto slowPathStart = jit.label();
    // Callee needs to be in regT0 here.
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfGlobalObject()), GPRInfo::regT3);
    callLinkInfo.emitSlowPath(vm, jit);
    done.link(&jit);
    auto doneLocation = jit.label();

    if (signature.returnCount() == 1) {
        const auto& returnType = signature.returnType(0);
        switch (returnType.kind) {
        case TypeKind::I64: {
            // FIXME: Optimize I64 extraction from BigInt.
            // https://bugs.webkit.org/show_bug.cgi?id=220053
            JSValueRegs dest = wasmCallInfo.results[0].location.jsr();
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToI64)>(GPRInfo::wasmContextInstancePointer, JSRInfo::returnValueJSR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.moveValueRegs(JSRInfo::returnValueJSR, dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(call, operationConvertToI64);
            });
            break;
        }
        case TypeKind::I32: {
            CCallHelpers::JumpList done;
            CCallHelpers::JumpList slowPath;
            JSValueRegs destJSR = wasmCallInfo.results[0].location.jsr();

            slowPath.append(jit.branchIfNotNumber(JSRInfo::returnValueJSR, jit.scratchRegister(), DoNotHaveTagRegisters));
            slowPath.append(jit.branchIfNotInt32(JSRInfo::returnValueJSR, DoNotHaveTagRegisters));
            jit.zeroExtend32ToWord(JSRInfo::returnValueJSR.payloadGPR(), destJSR.payloadGPR());
            done.append(jit.jump());

            slowPath.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToI32)>(GPRInfo::wasmContextInstancePointer, JSRInfo::returnValueJSR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(JSRInfo::returnValueJSR.payloadGPR(), destJSR.payloadGPR());
#if USE(JSVALUE32_64)
            jit.move(CCallHelpers::TrustedImm32(0), destJSR.tagGPR());
#endif

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(call, operationConvertToI32);
            });

            done.link(&jit);
            break;
        }
        case TypeKind::F32: {
            FPRReg dest = wasmCallInfo.results[0].location.fpr();

            CCallHelpers::JumpList done;
            auto notANumber = jit.branchIfNotNumber(JSRInfo::returnValueJSR, jit.scratchRegister(), DoNotHaveTagRegisters);
            auto isDouble = jit.branchIfNotInt32(JSRInfo::returnValueJSR, DoNotHaveTagRegisters);
            // We're an int32
            jit.convertInt32ToFloat(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            isDouble.link(&jit);
#if USE(JSVALUE64)
            jit.unboxDoubleWithoutAssertions(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2, dest, DoNotHaveTagRegisters);
#else
            jit.unboxDouble(JSRInfo::returnValueJSR, dest);
#endif
            jit.convertDoubleToFloat(dest, dest);
            done.append(jit.jump());

            notANumber.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToF32)>(GPRInfo::wasmContextInstancePointer, JSRInfo::returnValueJSR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(FPRInfo::returnValueFPR , dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(call, operationConvertToF32);
            });

            done.link(&jit);
            break;
        }
        case TypeKind::F64: {
            FPRReg dest = wasmCallInfo.results[0].location.fpr();
            CCallHelpers::JumpList done;

            auto notANumber = jit.branchIfNotNumber(JSRInfo::returnValueJSR, jit.scratchRegister(), DoNotHaveTagRegisters);
            auto isDouble = jit.branchIfNotInt32(JSValueRegs(JSRInfo::returnValueJSR), DoNotHaveTagRegisters);
            // We're an int32
            jit.convertInt32ToDouble(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            isDouble.link(&jit);
#if USE(JSVALUE64)
            jit.unboxDoubleWithoutAssertions(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2, dest, DoNotHaveTagRegisters);
#else
            jit.unboxDouble(JSRInfo::returnValueJSR, dest);
#endif
            done.append(jit.jump());

            notANumber.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToF64)>(GPRInfo::wasmContextInstancePointer, JSRInfo::returnValueJSR);
            auto call = jit.call(OperationPtrTag);
            exceptionChecks.append(jit.emitJumpIfException(vm));
            jit.move(FPRInfo::returnValueFPR, dest);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(call, operationConvertToF64);
            });

            done.link(&jit);
            break;
        }
        default:  {
            if (Wasm::isRefType(returnType)) {
                if (Wasm::isExternref(returnType))
                    ASSERT(returnType.isNullable());
                else if (Wasm::isFuncref(returnType)) {
                    ASSERT(returnType.isNullable());
                    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                    jit.setupArguments<decltype(operationConvertToFuncref)>(GPRInfo::wasmContextInstancePointer, JSRInfo::returnValueJSR);
                    auto call = jit.call(OperationPtrTag);
                    exceptionChecks.append(jit.emitJumpIfException(vm));
                    jit.addLinkTask([=](LinkBuffer& linkBuffer) {
                        linkBuffer.link<OperationPtrTag>(call, operationConvertToFuncref);
                    });
                } else if (Wasm::isI31ref(returnType))
                    RELEASE_ASSERT_NOT_REACHED();
                jit.moveValueRegs(JSRInfo::returnValueJSR, wasmCallInfo.results[0].location.jsr());
            } else
                // For the JavaScript embedding, imports with these types in their type definition return are a WebAssembly.Module validation error.
                RELEASE_ASSERT_NOT_REACHED();
        }
        }
    } else if (signature.returnCount() > 1) {
        constexpr GPRReg savedResultsGPR = preferredArgumentGPR<decltype(operationIterateResults), 3>();
        jit.move(CCallHelpers::stackPointerRegister, savedResultsGPR);
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.subPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
        static_assert(noOverlap(savedResultsGPR, JSRInfo::returnValueJSR));
        static_assert(GPRInfo::wasmContextInstancePointer != savedResultsGPR);
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationIterateResults)>(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImmPtr(&typeDefinition), JSRInfo::returnValueJSR, savedResultsGPR, CCallHelpers::framePointerRegister);
        jit.callOperation(operationIterateResults);
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);

        exceptionChecks.append(jit.emitJumpIfException(vm));

        for (unsigned i = 0; i < signature.returnCount(); ++i) {
            ValueLocation loc = wasmCallInfo.results[i].location;
            if (loc.isGPR()) {
#if USE(JSVALUE32_64)
                ASSERT(savedResultRegisters.find(loc.jsr().payloadGPR())->offset() + 4 == savedResultRegisters.find(loc.jsr().tagGPR())->offset());
#endif
                jit.loadValue(CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.jsr().payloadGPR())->offset()), loc.jsr());
            } else if (loc.isFPR())
                jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.fpr())->offset()), loc.fpr());
        }
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    if (!exceptionChecks.empty()) {
        exceptionChecks.link(&jit);
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::argumentGPR0);
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationWasmUnwind)>(GPRInfo::wasmContextInstancePointer);
        auto call = jit.call(OperationPtrTag);
        jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link<OperationPtrTag>(call, operationWasmUnwind);
        });
    }

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (UNLIKELY(patchBuffer.didFailToAllocate()))
        return makeUnexpected(BindingFailure::OutOfMemory);

    callLinkInfo.setCodeLocations(
        patchBuffer.locationOf<JSInternalPtrTag>(slowPathStart),
        patchBuffer.locationOf<JSInternalPtrTag>(doneLocation));

    return FINALIZE_WASM_CODE(patchBuffer, WasmEntryPtrTag, "WebAssembly->JavaScript import[%i] %s", importIndex, signature.toString().ascii().data());
}

void emitThrowWasmToJSException(CCallHelpers& jit, GPRReg wasmInstance, Wasm::ExceptionType type)
{
    ASSERT(wasmInstance != GPRInfo::argumentGPR2);
    ASSERT(wasmInstance != InvalidGPRReg);
    jit.loadPtr(CCallHelpers::Address(wasmInstance, Wasm::Instance::offsetOfVM()), GPRInfo::argumentGPR2);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, VM::topEntryFrameOffset()), GPRInfo::argumentGPR2);
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR2);

    if (wasmInstance != GPRInfo::argumentGPR0)
        jit.move(wasmInstance, GPRInfo::argumentGPR0);

    jit.move(CCallHelpers::TrustedImm32(static_cast<int32_t>(type)), GPRInfo::argumentGPR1);

    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    CCallHelpers::Call call = jit.call(OperationPtrTag);

    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.breakpoint(); // We should not reach this.

    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        linkBuffer.link<OperationPtrTag>(call, Wasm::operationWasmToJSException);
    });
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
