/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)

#include "BaselineJITRegisters.h"
#include "CCallHelpers.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "ThunkGenerators.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmOperations.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/FunctionTraits.h>

namespace JSC { namespace Wasm {

using JIT = CCallHelpers;

static void materializeImportJSCell(JIT& jit, const Wasm::ModuleInformation& info, unsigned importIndex, GPRReg result)
{
    // We're calling out of the current WebAssembly.Instance. That JSWebAssemblyInstance has a list of all its import functions.
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfImportFunction(info, importIndex)), result);
}

static std::expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> handleBadImportTypeUse(JIT& jit, unsigned importIndex, Wasm::ExceptionType exceptionType)
{
    jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);

    emitThrowWasmToJSException(jit, GPRInfo::argumentGPR0, exceptionType);

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) [[unlikely]]
        return makeUnexpected(BindingFailure::OutOfMemory);

    return FINALIZE_WASM_CODE(linkBuffer, WasmEntryPtrTag, nullptr, "WebAssembly->JavaScript throw exception due to invalid use of restricted type in import[%i]", importIndex);
}

enum class StackArgClass : uint8_t { I32, Copy64, F32, F64 };

static StackArgClass stackArgClass(Type type)
{
    if (type.isI32())
        return StackArgClass::I32;
    if (type.isF32())
        return StackArgClass::F32;
    if (type.isF64())
        return StackArgClass::F64;
    return StackArgClass::Copy64;
}

static bool isFloatArg(Type type)
{
    return type.isF32() || type.isF64();
}

static unsigned firstArgumentAfterLastRegisterArg(const RTT& signature, unsigned nGpr, unsigned nFpr)
{
    unsigned gprs = 0;
    unsigned fprs = 0;
    unsigned start = 0;
    unsigned argCount = signature.argumentCount();
    for (unsigned i = 0; i < argCount; ++i) {
        Type type = signature.argumentType(i);
        bool onStack = isFloatArg(type) ? fprs >= nFpr : gprs >= nGpr;
        if (!onStack)
            start = i + 1;
        if (isFloatArg(type))
            ++fprs;
        else
            ++gprs;
    }
    return start;
}

static void emitStackArgRun(JIT& jit, StackArgClass klass, unsigned count, unsigned destOffset, unsigned srcOffset)
{
    GPRReg destGPR = GPRInfo::argumentGPR2;
    GPRReg srcGPR = GPRInfo::argumentGPR3;
    GPRReg countGPR = GPRInfo::nonPreservedNonArgumentGPR0;
    constexpr JSValueRegs valueJSR { GPRInfo::argumentGPR0 };
    GPRReg scratch = GPRInfo::argumentGPR1;
    FPRReg fpr = FPRInfo::argumentFPR0;

    jit.addPtr(JIT::TrustedImm32(destOffset - sizeof(CallerFrameAndPC)), MacroAssembler::stackPointerRegister, destGPR);
    jit.addPtr(JIT::TrustedImm32(srcOffset), GPRInfo::callFrameRegister, srcGPR);
    jit.move(JIT::TrustedImm32(count), countGPR);

    auto boxDouble = [&] {
        jit.purifyNaN(fpr, fpr);
        jit.moveDoubleTo64(fpr, scratch);
#if CPU(ARM64)
        jit.move(JIT::TrustedImm64(JSValue::DoubleEncodeOffset), valueJSR.payloadGPR());
#else
        jit.move(JIT::TrustedImm32(1), valueJSR.payloadGPR());
        jit.lshift64(JIT::TrustedImm32(JSValue::DoubleEncodeOffsetBit), valueJSR.payloadGPR());
#endif
        jit.add64(valueJSR.payloadGPR(), scratch);
        jit.store64(scratch, JIT::Address(destGPR));
    };

    auto loop = jit.label();
    switch (klass) {
    case StackArgClass::I32:
        jit.load32(JIT::Address(srcGPR), valueJSR.payloadGPR());
        jit.zeroExtend32ToWord(valueJSR.payloadGPR(), valueJSR.payloadGPR());
        jit.boxInt32(valueJSR.payloadGPR(), valueJSR, DoNotHaveTagRegisters);
        jit.storeValue(valueJSR, JIT::Address(destGPR));
        break;
    case StackArgClass::Copy64:
        jit.loadValue(JIT::Address(srcGPR), valueJSR);
        jit.storeValue(valueJSR, JIT::Address(destGPR));
        break;
    case StackArgClass::F32:
        jit.loadFloat(JIT::Address(srcGPR), fpr);
        jit.convertFloatToDouble(fpr, fpr);
        boxDouble();
        break;
    case StackArgClass::F64:
        jit.loadDouble(JIT::Address(srcGPR), fpr);
        boxDouble();
        break;
    }
    jit.addPtr(JIT::TrustedImm32(sizeof(Register)), destGPR);
    jit.addPtr(JIT::TrustedImm32(sizeof(Register)), srcGPR);
    jit.sub32(JIT::TrustedImm32(1), countGPR);
    jit.branchTest32(MacroAssembler::NonZero, countGPR).linkTo(loop, &jit);
}

std::expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> wasmToJS(const Wasm::ModuleInformation& info, unsigned importIndex)
{
    // FIXME: This function doesn't properly abstract away the calling convention.
    // It'd be super easy to do so: https://bugs.webkit.org/show_bug.cgi?id=169401
    const auto& wasmCC = wasmCallingConvention();
    const auto& jsCC = jsCallingConvention();
    const RTT& signature = info.rtt(info.importFunctionTypeSignatureIndices[importIndex]);
    unsigned argCount = signature.argumentCount();
    constexpr GPRReg importJSCellGPRReg = GPRInfo::regT0; // Callee needs to be in regT0 for slow path below.
    JIT jit;

    CallInformation wasmCallInfo = wasmCC.callInformationFor(signature, CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    // Note: WasmOMGIRGenerator assumes that this stub treats SP as a callee save.
    // If we ever change this, we will also need to change WasmOMGIRGenerator.

    // Below, we assume that the JS calling convention is always on the stack.
    ASSERT_UNUSED(jsCC, !jsCC.gprArgs.size());
    ASSERT(!jsCC.fprArgs.size());

    jit.emitFunctionPrologue();
    GPRReg scratchGPR = GPRInfo::nonPreservedNonArgumentGPR0;
    JIT_COMMENT(jit, "Store callee from ptr: ", RawPointer(&WasmToJSCallee::singleton()), " value, ", RawPointer(CalleeBits::boxNativeCallee(&WasmToJSCallee::singleton())));
    jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(&WasmToJSCallee::singleton())), scratchGPR);
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    jit.storePairPtr(GPRInfo::wasmContextInstancePointer, scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    // https://webassembly.github.io/spec/js-api/index.html#exported-function-exotic-objects
    // If parameters or results contain v128, throw a TypeError.
    // Note: the above error is thrown each time the [[Call]] method is invoked.
    if (signature.argumentsOrResultsIncludeV128() || signature.argumentsOrResultsIncludeExnref()) [[unlikely]]
        return handleBadImportTypeUse(jit, importIndex, ExceptionType::TypeErrorInvalidValueUse);

    // Here we assume that the JS calling convention saves at least all the wasm callee saved. We therefore don't need to save and restore more registers since the wasm callee already took care of this.
#if ASSERT_ENABLED
    wasmCC.calleeSaveRegisters.forEachWithWidth([&] (Reg reg, Width width) {
        ASSERT(jsCC.calleeSaveRegisters.contains(reg, width));
    });
#endif

    // Note: We don't need to perform a stack check here since WasmOMGIRGenerator
    // will do the stack check for us. Whenever it detects that it might make
    // a call to this thunk, it'll make sure its stack check includes space
    // for us here.

    const unsigned numberOfParameters = argCount + 1; // There is a "this" argument.
    const unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
    const unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    const unsigned numberOfBytesForSavedResults = savedResultRegisters.sizeOfAreaInBytes();
    const unsigned stackOffset = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(std::max(numberOfBytesForCall, numberOfBytesForSavedResults) + static_cast<unsigned>(Wasm::WasmToJSScratchSpaceSize));
    jit.subPtr(MacroAssembler::TrustedImm32(stackOffset), MacroAssembler::stackPointerRegister);
    jit.storePtr(GPRInfo::wasmIPIntPCRegister, CCallHelpers::Address(GPRInfo::callFrameRegister, WasmToJSIPIntReturnPCSlot));
    JIT::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    constexpr unsigned minStackArgsToLoop = 4;
    unsigned stackLoopStart = firstArgumentAfterLastRegisterArg(signature, wasmCC.jsrArgs.size(), wasmCC.fprArgs.size());
    if (argCount - stackLoopStart < minStackArgsToLoop)
        stackLoopStart = argCount;

    constexpr GPRReg argumentScratchGPR = GPRInfo::argumentGPR0;

    // First go through the integer parameters, freeing up their register for use afterwards.
    {
        unsigned marshalledGPRs = 0;
        unsigned marshalledFPRs = 0;
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        unsigned frOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        for (unsigned argNum = 0; argNum < stackLoopStart; ++argNum) {
            Type argType = signature.argumentType(argNum);
            switch (argType.kind()) {
            case TypeKind::Void:
            case TypeKind::Structref:
            case TypeKind::Arrayref:
            case TypeKind::Eqref:
            case TypeKind::Anyref:
            case TypeKind::Noexnref:
            case TypeKind::Noneref:
            case TypeKind::Nofuncref:
            case TypeKind::Noexternref:
            case TypeKind::I31ref:
            case TypeKind::V128:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case TypeKind::RefNull:
            case TypeKind::Ref:
            case TypeKind::Exnref:
            case TypeKind::Externref:
            case TypeKind::Funcref:
            case TypeKind::I32:
            case TypeKind::I64: {
                GPRReg argReg = InvalidGPRReg;
                if (marshalledGPRs < wasmCC.gprArgs.size())
                    argReg = wasmCC.gprArgs[marshalledGPRs];
                else {
                    // We've already spilled all arguments, these registers are available as scratch.
                    argReg = argumentScratchGPR;
                    jit.loadValue(JIT::Address(GPRInfo::callFrameRegister, frOffset), argReg);
                    frOffset += sizeof(Register);
                }
                ++marshalledGPRs;
                if (argType.isI32()) {
                    jit.zeroExtend32ToWord(argReg, argReg); // Clear non-int32 and non-tag bits.
                    jit.boxInt32(argReg, argReg, DoNotHaveTagRegisters);
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
        unsigned frOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));

        auto marshallFPR = [&] (FPRReg fprReg) {
            jit.purifyNaN(fprReg, fprReg);
            jit.moveDoubleTo64(fprReg, scratch);
            materializeDoubleEncodeOffset(doubleEncodeOffsetGPRReg);
            jit.add64(doubleEncodeOffsetGPRReg, scratch);
            jit.store64(scratch, calleeFrame.withOffset(calleeFrameOffset));
            calleeFrameOffset += sizeof(Register);
            ++marshalledFPRs;
        };

        for (unsigned argNum = 0; argNum < stackLoopStart; ++argNum) {
            Type argType = signature.argumentType(argNum);
            switch (argType.kind()) {
            case TypeKind::Void:
            case TypeKind::Structref:
            case TypeKind::Arrayref:
            case TypeKind::Eqref:
            case TypeKind::Anyref:
            case TypeKind::Noexnref:
            case TypeKind::Noneref:
            case TypeKind::Nofuncref:
            case TypeKind::Noexternref:
            case TypeKind::I31ref:
            case TypeKind::V128:
                RELEASE_ASSERT_NOT_REACHED(); // Handled above.
            case TypeKind::RefNull:
            case TypeKind::Ref:
            case TypeKind::Exnref:
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

    if (stackLoopStart < argCount) {
        unsigned destOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register)) + stackLoopStart * sizeof(Register);
        unsigned srcOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        unsigned gprs = 0;
        unsigned fprs = 0;
        for (unsigned i = 0; i < stackLoopStart; ++i) {
            Type type = signature.argumentType(i);
            if (isFloatArg(type)) {
                if (fprs >= wasmCC.fprArgs.size())
                    srcOffset += sizeof(Register);
                ++fprs;
            } else {
                if (gprs >= wasmCC.jsrArgs.size())
                    srcOffset += sizeof(Register);
                ++gprs;
            }
        }
        for (unsigned i = stackLoopStart; i < argCount; ) {
            StackArgClass klass = stackArgClass(signature.argumentType(i));
            unsigned run = 1;
            while (i + run < argCount && stackArgClass(signature.argumentType(i + run)) == klass)
                ++run;
            emitStackArgRun(jit, klass, run, destOffset, srcOffset);
            destOffset += run * sizeof(Register);
            srcOffset += run * sizeof(Register);
            i += run;
        }
    }

    CCallHelpers::JumpList exceptionChecks;

    if (signature.argumentsOrResultsIncludeI64()) {
        // Since all argument GPRs and FPRs are stored into stack frames, clobbering caller-save registers is OK here.
        // We call functions to convert I64 to BigInt.
        unsigned calleeFrameOffset = CallFrameSlot::firstArgument * static_cast<int>(sizeof(Register));
        for (unsigned argNum = 0; argNum < argCount; ++argNum) {
            if (signature.argumentType(argNum).isI64()) {
                using Operation = decltype(operationConvertToBigInt);
                constexpr GPRReg valueGPR = preferredArgumentGPR<Operation, 1>();
                jit.loadValue(calleeFrame.withOffset(calleeFrameOffset), valueGPR);
                jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                jit.setupArguments<Operation>(GPRInfo::wasmContextInstancePointer, valueGPR);
                jit.callOperation<OperationPtrTag>(operationConvertToBigInt);
                using ResultType = typename FunctionTraits<decltype(operationConvertToBigInt)>::ResultType;
                exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
                jit.storeValue(GPRInfo::returnValueGPR, calleeFrame.withOffset(calleeFrameOffset));
            }
            calleeFrameOffset += sizeof(Register);
        }
    }

    jit.storeValue(jsUndefined(), calleeFrame.withOffset(CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register))));
    ASSERT(!wasmCC.calleeSaveRegisters.contains(importJSCellGPRReg, IgnoreVectors));
    materializeImportJSCell(jit, info, importIndex, importJSCellGPRReg);
    jit.storeValue(importJSCellGPRReg, calleeFrame.withOffset(CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    jit.store32(JIT::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + LowWordOffset));

    // FIXME Tail call if the wasm return type is void and no registers were spilled. https://bugs.webkit.org/show_bug.cgi?id=165488

    // Callee needs to be in regT0 here.
    jit.move(importJSCellGPRReg, BaselineJITRegisters::Call::calleeGPR);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfCallLinkInfo(info, importIndex)), BaselineJITRegisters::Call::callLinkInfoGPR);
    CallLinkInfo::emitDataICFastPath(jit);

    if (signature.returnCount() == 1) {
        const auto& returnType = signature.returnType(0);
        switch (returnType.kind()) {
        case TypeKind::I64: {
            CCallHelpers::JumpList done;
            CCallHelpers::JumpList slowPath;
            GPRReg destGPR = wasmCallInfo.results[0].location.gpr();
            GPRReg cellGPR = GPRInfo::returnValueGPR;

            slowPath.append(jit.branchIfNotCell(GPRInfo::returnValueGPR, DoNotHaveTagRegisters));
            slowPath.append(jit.branchIfNotHeapBigInt(cellGPR));
            if (cellGPR == destGPR) {
                GPRReg scratch = GPRInfo::nonPreservedNonReturnGPR;
                jit.move(cellGPR, scratch);
                jit.toBigInt64(scratch, destGPR);
            } else
                jit.toBigInt64(cellGPR, destGPR);
            done.append(jit.jump());

            slowPath.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToI64)>(GPRInfo::wasmContextInstancePointer, GPRInfo::returnValueGPR);
            jit.callOperation<OperationPtrTag>(operationConvertToI64);
            using ResultType = typename FunctionTraits<decltype(operationConvertToI64)>::ResultType;
            exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
            jit.move(GPRInfo::returnValueGPR, destGPR);
            done.link(&jit);
            break;
        }
        case TypeKind::I32: {
            CCallHelpers::JumpList done;
            CCallHelpers::JumpList slowPath;
            GPRReg destGPR = wasmCallInfo.results[0].location.gpr();

            slowPath.append(jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters));
            slowPath.append(jit.branchIfNotInt32(GPRInfo::returnValueGPR, DoNotHaveTagRegisters));
            jit.zeroExtend32ToWord(GPRInfo::returnValueGPR, destGPR);
            done.append(jit.jump());

            slowPath.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToI32)>(GPRInfo::wasmContextInstancePointer, GPRInfo::returnValueGPR);
            jit.callOperation<OperationPtrTag>(operationConvertToI32);
            using ResultType = typename FunctionTraits<decltype(operationConvertToI32)>::ResultType;
            exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
            jit.move(GPRInfo::returnValueGPR, destGPR);
            done.link(&jit);
            break;
        }
        case TypeKind::F32: {
            FPRReg dest = wasmCallInfo.results[0].location.fpr();

            CCallHelpers::JumpList done;
            auto notANumber = jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
            auto isDouble = jit.branchIfNotInt32(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
            // We're an int32
            jit.convertInt32ToFloat(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            isDouble.link(&jit);
            jit.unboxDoubleWithoutAssertions(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2, dest, DoNotHaveTagRegisters);
            jit.convertDoubleToFloat(dest, dest);
            done.append(jit.jump());

            notANumber.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToF32)>(GPRInfo::wasmContextInstancePointer, GPRInfo::returnValueGPR);
            jit.callOperation<OperationPtrTag>(operationConvertToF32);
            jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::nonPreservedNonReturnGPR);
            exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::Address(GPRInfo::nonPreservedNonReturnGPR, VM::exceptionOffset())));
            jit.moveDouble(FPRInfo::returnValueFPR , dest);
            done.link(&jit);
            break;
        }
        case TypeKind::F64: {
            FPRReg dest = wasmCallInfo.results[0].location.fpr();
            CCallHelpers::JumpList done;

            auto notANumber = jit.branchIfNotNumber(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
            auto isDouble = jit.branchIfNotInt32(GPRInfo::returnValueGPR, DoNotHaveTagRegisters);
            // We're an int32
            jit.convertInt32ToDouble(GPRInfo::returnValueGPR, dest);
            done.append(jit.jump());

            isDouble.link(&jit);
            jit.unboxDoubleWithoutAssertions(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2, dest, DoNotHaveTagRegisters);
            done.append(jit.jump());

            notANumber.link(&jit);
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToF64)>(GPRInfo::wasmContextInstancePointer, GPRInfo::returnValueGPR);
            jit.callOperation<OperationPtrTag>(operationConvertToF64);
            jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::nonPreservedNonReturnGPR);
            exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::Address(GPRInfo::nonPreservedNonReturnGPR, VM::exceptionOffset())));
            jit.moveDouble(FPRInfo::returnValueFPR, dest);
            done.link(&jit);
            break;
        }
        default:  {
            if (Wasm::isRefType(returnType)) {
                if (!returnType.isNullable()) {
                    auto isNotNull = jit.branchIfNotNull(GPRInfo::returnValueGPR);
                    jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
                    emitThrowWasmToJSException(jit, GPRInfo::argumentGPR0, ExceptionType::TypeErrorUnexpectedNullReference);
                    isNotNull.link(&jit);
                }

                if (Wasm::isExternref(returnType)) {
                    // Do nothing.
                } else if (Wasm::isFuncref(returnType)) {
                    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                    jit.setupArguments<decltype(operationConvertToFuncref)>(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImmPtr(&signature), GPRInfo::returnValueGPR);
                    jit.callOperation<OperationPtrTag>(operationConvertToFuncref);
                    using ResultType = typename FunctionTraits<decltype(operationConvertToFuncref)>::ResultType;
                    exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
                } else {
                    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                    jit.setupArguments<decltype(operationConvertToAnyref)>(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImmPtr(&signature), GPRInfo::returnValueGPR);
                    jit.callOperation<OperationPtrTag>(operationConvertToAnyref);
                    using ResultType = typename FunctionTraits<decltype(operationConvertToAnyref)>::ResultType;
                    exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
                }
                jit.move(GPRInfo::returnValueGPR, wasmCallInfo.results[0].location.gpr());
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
        static_assert(noOverlap(savedResultsGPR, GPRInfo::returnValueGPR));
        static_assert(GPRInfo::wasmContextInstancePointer != savedResultsGPR);
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationIterateResults)>(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImmPtr(&signature), GPRInfo::returnValueGPR, savedResultsGPR, CCallHelpers::framePointerRegister);
        jit.callOperation<OperationPtrTag>(operationIterateResults);
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
        using ResultType = typename FunctionTraits<decltype(operationIterateResults)>::ResultType;
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
        for (unsigned i = 0; i < signature.returnCount(); ++i) {
            ValueLocation loc = wasmCallInfo.results[i].location;
            if (loc.isGPR()) {
                jit.loadValue(CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.gpr())->offset()), loc.gpr());
            } else if (loc.isFPR())
                jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.fpr())->offset()), loc.fpr());
        }
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    if (!exceptionChecks.empty()) {
        exceptionChecks.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR0);
        jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationWasmUnwind)>(GPRInfo::wasmContextInstancePointer);
        jit.callOperation<OperationPtrTag>(operationWasmUnwind);
        jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    }

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationMustSucceed);
    if (patchBuffer.didFailToAllocate()) [[unlikely]]
        return makeUnexpected(BindingFailure::OutOfMemory);

    return FINALIZE_WASM_CODE(patchBuffer, WasmEntryPtrTag, nullptr, "WebAssembly->JavaScript import[%i] %s", importIndex, signature.toString().ascii().data());
}

void emitThrowWasmToJSException(CCallHelpers& jit, GPRReg wasmInstance, Wasm::ExceptionType type)
{
    ASSERT(wasmInstance != GPRInfo::argumentGPR2);
    ASSERT(wasmInstance != InvalidGPRReg);
    jit.loadPtr(CCallHelpers::Address(wasmInstance, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR2);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR2);

    if (wasmInstance != GPRInfo::argumentGPR0)
        jit.move(wasmInstance, GPRInfo::argumentGPR0);

    jit.move(CCallHelpers::TrustedImm32(static_cast<int32_t>(type)), GPRInfo::argumentGPR1);

    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    jit.callOperation<OperationPtrTag>(Wasm::operationWasmToJSException);

    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.breakpoint(); // We should not reach this.
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
