/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "JSToWasm.h"
#include "WasmCallee.h"

#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)

#include "CCallHelpers.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmOperations.h"
#include "WasmThunks.h"
#include "WasmToJS.h"
#include "WebAssemblyFunctionBase.h"

namespace JSC {
namespace Wasm {

static void marshallJSResult(CCallHelpers& jit, const RTT& signature, const CallInformation& wasmFrameConvention, const RegisterAtOffsetList& savedResultRegisters, CCallHelpers::JumpList& exceptionChecks, int32_t stackResultReadOffset = 0)
{
    auto boxNativeCalleeResult = [](CCallHelpers& jit, Type type, ValueLocation src, GPRReg dst) {
        JIT_COMMENT(jit, "boxNativeCalleeResult ", type);
        switch (type.kind()) {
        case TypeKind::Void:
            jit.moveTrustedValue(jsUndefined(), dst);
            break;
        case TypeKind::I32:
            jit.zeroExtend32ToWord(src.gpr(), dst);
            jit.boxInt32(dst, dst, DoNotHaveTagRegisters);
            break;
        case TypeKind::F32:
            jit.convertFloatToDouble(src.fpr(), src.fpr());
            [[fallthrough]];
        case TypeKind::F64: {
            jit.moveTrustedValue(jsNumber(PNaN), dst);
            auto isNaN = jit.branchIfNaN(src.fpr());
            jit.boxDouble(src.fpr(), dst, DoNotHaveTagRegisters);
            isNaN.link(&jit);
            break;
        }
        default: {
            if (isRefType(type))
                jit.move(src.gpr(), dst);
            else
                jit.breakpoint();
        }
        }
    };

    if (signature.returnsVoid())
        jit.moveTrustedValue(jsUndefined(), GPRInfo::returnValueGPR);
    else if (signature.returnCount() == 1) {
        if (signature.returnType(0).isI64()) {
            JIT_COMMENT(jit, "convert wasm return to big int");
            GPRReg inputGPR = wasmFrameConvention.results[0].location.gpr();
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToBigInt)>(GPRInfo::wasmContextInstancePointer, inputGPR);
            jit.callOperation<OperationPtrTag>(operationConvertToBigInt);
            using ResultType = typename FunctionTraits<decltype(operationConvertToBigInt)>::ResultType;
            exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
        } else
            boxNativeCalleeResult(jit, signature.returnType(0), wasmFrameConvention.results[0].location, GPRInfo::returnValueGPR);
    } else {
        IndexingType indexingType = ArrayWithUndecided;
        GPRReg scratchGPR = wasmCallingConvention().prologueScratchGPRs[1];

        ASSERT(scratchGPR != GPRReg::InvalidGPRReg);

        // We can use the first floating point register as a scratch since it will always be moved onto the stack before other values.
        FPRReg fprScratch = wasmCallingConvention().fprArgs[0];
        JIT_COMMENT(jit, "scratchFPR: ", fprScratch, " - Scratch gpr: ", scratchGPR, " - saved result registers: ", savedResultRegisters);
        bool hasI64 = false;
        for (unsigned i = 0; i < signature.returnCount(); ++i) {
            ValueLocation loc = wasmFrameConvention.results[i].location;
            Type type = signature.returnType(i);

            hasI64 |= type.isI64();
            if (loc.isGPR() || loc.isFPR()) {
                auto address = CCallHelpers::Address(CCallHelpers::stackPointerRegister, wasmFrameConvention.headerAndArgumentStackSizeInBytes);
                switch (type.kind()) {
                case TypeKind::F32:
                case TypeKind::F64:
                    boxNativeCalleeResult(jit, type, loc, scratchGPR);
                    jit.storeValue(scratchGPR, address.withOffset(savedResultRegisters.find(loc.fpr())->offset()));
                    break;
                case TypeKind::I64:
                    jit.storeValue(loc.gpr(), address.withOffset(savedResultRegisters.find(loc.gpr())->offset()));
                    break;
                default:
                    boxNativeCalleeResult(jit, type, loc, scratchGPR);
                    jit.storeValue(scratchGPR, address.withOffset(savedResultRegisters.find(loc.gpr())->offset()));
                    break;
                }
            } else {
                if (!type.isI64()) {
                    auto readLocation = CCallHelpers::Address(CCallHelpers::stackPointerRegister, loc.offsetFromSP() + stackResultReadOffset);
                    auto writeLocation = CCallHelpers::Address(CCallHelpers::stackPointerRegister, loc.offsetFromSP());
                    ValueLocation tmp;
                    switch (type.kind()) {
                    case TypeKind::F32:
                        tmp = ValueLocation { fprScratch };
                        jit.loadFloat(readLocation, fprScratch);
                        break;
                    case TypeKind::F64:
                        tmp = ValueLocation { fprScratch };
                        jit.loadDouble(readLocation, fprScratch);
                        break;
                    case TypeKind::I32:
                        tmp = ValueLocation { scratchGPR };
                        jit.load32(readLocation, scratchGPR);
                        break;
                    default:
                        tmp = ValueLocation { scratchGPR };
                        jit.loadValue(readLocation, scratchGPR);
                        break;
                    }
                    boxNativeCalleeResult(jit, type, tmp, scratchGPR);
                    jit.storeValue(scratchGPR, writeLocation);
                }
            }

            switch (type.kind()) {
            case TypeKind::I32:
                indexingType = leastUpperBoundOfIndexingTypes(indexingType, ArrayWithInt32);
                break;
            case TypeKind::F32:
            case TypeKind::F64:
                indexingType = leastUpperBoundOfIndexingTypes(indexingType, ArrayWithDouble);
                break;
            default:
                indexingType = leastUpperBoundOfIndexingTypes(indexingType, ArrayWithContiguous);
                break;
            }
        }

        // Now, all return values are stored in memory. So we can call functions can clobber caller-save registers.
        // This is required to convert values to BigInt.
        if (hasI64) {
            for (unsigned i = 0; i < signature.returnCount(); ++i) {
                ValueLocation loc = wasmFrameConvention.results[i].location;
                Type type = signature.returnType(i);
                if (!type.isI64())
                    continue;

                constexpr GPRReg valueGPR = preferredArgumentGPR<decltype(operationConvertToBigInt), 1>();

                CCallHelpers::Address readAddress { CCallHelpers::stackPointerRegister };
                CCallHelpers::Address writeAddress { CCallHelpers::stackPointerRegister };
                if (loc.isGPR() || loc.isFPR()) {
                    auto offset = savedResultRegisters.find(loc.gpr())->offset() + wasmFrameConvention.headerAndArgumentStackSizeInBytes;
                    readAddress = readAddress.withOffset(offset);
                    writeAddress = writeAddress.withOffset(offset);
                } else {
                    readAddress = readAddress.withOffset(loc.offsetFromSP() + stackResultReadOffset);
                    writeAddress = writeAddress.withOffset(loc.offsetFromSP());
                }

                jit.loadValue(readAddress, valueGPR);
                jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                jit.setupArguments<decltype(operationConvertToBigInt)>(GPRInfo::wasmContextInstancePointer, valueGPR);
                jit.callOperation<OperationPtrTag>(operationConvertToBigInt);
                using ResultType = typename FunctionTraits<decltype(operationConvertToBigInt)>::ResultType;
                exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
                jit.storeValue(GPRInfo::returnValueGPR, writeAddress);
            }
        }

        constexpr GPRReg savedResultsGPR = preferredArgumentGPR<decltype(operationAllocateResultsArray), 3>();
        jit.move(CCallHelpers::stackPointerRegister, savedResultsGPR);
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.subPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
        static_assert(GPRInfo::wasmContextInstancePointer != savedResultsGPR);
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationAllocateResultsArray)>(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImmPtr(&signature), indexingType, savedResultsGPR);
        JIT_COMMENT(jit, "operationAllocateResultsArray");
        jit.callOperation<OperationPtrTag>(operationAllocateResultsArray);
        using ResultType = typename FunctionTraits<decltype(operationAllocateResultsArray)>::ResultType;
        static_assert(CCallHelpers::operationExceptionRegister<ResultType>() != InvalidGPRReg, "We don't have a VM readily available so we rely on exception being returned");
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
    }
}

MacroAssemblerCodeRef<JITThunkPtrTag> createJSToWasmJITShared()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        // JIT version of js_to_wasm_wrapper_entry
        // If you change this, make sure to modify InPlaceInterpreter.asm:op(js_to_wasm_wrapper_entry)
        CCallHelpers jit;

        CCallHelpers::JumpList exceptionChecks;
        CCallHelpers::JumpList stackOverflow;
        CCallHelpers::JumpList buildEntryFrameThrew;

        auto calleeSaves = Wasm::JSToWasmCallee::calleeSaveRegistersImpl();
        jit.emitFunctionPrologue();
        jit.subPtr(CCallHelpers::TrustedImmPtr(Wasm::JSToWasmCallee::SpillStackSpaceAligned), CCallHelpers::stackPointerRegister);
        jit.emitSaveCalleeSavesFor(calleeSaves);

        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regWS0, WebAssemblyFunction::offsetOfTargetInstance()), GPRInfo::wasmContextInstancePointer);

        // Now, the current frame is fully set up for exceptions.
        // Allocate stack space
        JIT_COMMENT(jit, "stack overflow check");
        jit.load32(CCallHelpers::Address(GPRInfo::regWS0, WebAssemblyFunction::offsetOfFrameSize()), GPRInfo::regWS1);
        jit.subPtr(CCallHelpers::stackPointerRegister, GPRInfo::regWS1, GPRInfo::regWS1);

        stackOverflow.append(jit.branchPtr(CCallHelpers::LessThanOrEqual, GPRInfo::regWS1, CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit())));

        jit.move(GPRInfo::regWS1, CCallHelpers::stackPointerRegister);
        jit.move(CCallHelpers::stackPointerRegister, GPRInfo::argumentGPR0);

        jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));

        // Save the current Callee before putting in our boxed callee for the stack visitor
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::wasmBaseMemoryPointer);
        jit.transferPtr(CCallHelpers::Address(GPRInfo::regWS0, WebAssemblyFunction::offsetOfBoxedJSToWasmCallee()), CCallHelpers::addressFor(CallFrameSlot::callee));

        // Prepare frame
        jit.setupArguments<decltype(operationJSToWasmEntryWrapperBuildFrame)>(GPRInfo::argumentGPR0, GPRInfo::callFrameRegister, GPRInfo::regWS0);
        jit.callOperation<OperationPtrTag>(operationJSToWasmEntryWrapperBuildFrame);

        // Restore Callee slot regardless
        jit.storePtr(GPRInfo::wasmBaseMemoryPointer, CCallHelpers::addressFor(CallFrameSlot::callee));

        {
            using ResultType = typename FunctionTraits<decltype(operationJSToWasmEntryWrapperBuildFrame)>::ResultType;
            static_assert(CCallHelpers::operationExceptionRegister<ResultType>() != InvalidGPRReg, "We don't have a VM readily available so we rely on exception being returned");
            JIT_COMMENT(jit, "Exception check: ", CCallHelpers::operationExceptionRegister<ResultType>());
            buildEntryFrameThrew.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
            jit.move(GPRInfo::returnValueGPR, GPRInfo::regWS0);
        }

        // Memory
        jit.loadPair64(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemoryBaseSizePair(0)), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
        jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, GPRInfo::regWA0);

#if CPU(ARM64)
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8), GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8), GPRInfo::regWA2, GPRInfo::regWA3);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8), GPRInfo::regWA4, GPRInfo::regWA5);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 6 * 8), GPRInfo::regWA6, GPRInfo::regWA7);
#elif CPU(X86_64)
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8), GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8), GPRInfo::regWA2, GPRInfo::regWA3);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8), GPRInfo::regWA4, GPRInfo::regWA5);
#else
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8), GPRInfo::regWA0);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 1 * 8), GPRInfo::regWA1);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8), GPRInfo::regWA2);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 3 * 8), GPRInfo::regWA3);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8), GPRInfo::regWA4);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 5 * 8), GPRInfo::regWA5);
#endif

#if CPU(ARM64)
        jit.loadPairDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 0) * 8), FPRInfo::argumentFPR0, FPRInfo::argumentFPR1);
        jit.loadPairDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 2) * 8), FPRInfo::argumentFPR2, FPRInfo::argumentFPR3);
        jit.loadPairDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 4) * 8), FPRInfo::argumentFPR4, FPRInfo::argumentFPR5);
        jit.loadPairDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 6) * 8), FPRInfo::argumentFPR6, FPRInfo::argumentFPR7);
#elif CPU(X86_64) || CPU(RISCV64)
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 0) * 8), FPRInfo::argumentFPR0);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 1) * 8), FPRInfo::argumentFPR1);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 2) * 8), FPRInfo::argumentFPR2);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 3) * 8), FPRInfo::argumentFPR3);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 4) * 8), FPRInfo::argumentFPR4);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 5) * 8), FPRInfo::argumentFPR5);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 6) * 8), FPRInfo::argumentFPR6);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 7) * 8), FPRInfo::argumentFPR7);
#endif

        // Pop argument space values
        jit.addPtr(CCallHelpers::TrustedImmPtr(Wasm::JSToWasmCallee::RegisterStackSpaceAligned), CCallHelpers::stackPointerRegister);

#if ASSERT_ENABLED
        for (int32_t i = 0; i < 30; ++i)
            jit.storePtr(CCallHelpers::TrustedImmPtr(0xbeef), CCallHelpers::Address(CCallHelpers::stackPointerRegister, -i * static_cast<int32_t>(sizeof(Register))));
#endif

        JIT_COMMENT(jit, "Re-load WebAssemblyFunction Callee");
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS1);

        JIT_COMMENT(jit, "Replace the WebAssemblyFunction Callee with our JSToWasm NativeCallee");
        jit.transferPtr(CCallHelpers::Address(GPRInfo::regWS1, WebAssemblyFunction::offsetOfBoxedJSToWasmCallee()), CCallHelpers::addressFor(CallFrameSlot::callee));
        jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));

        // FIXME: We could load the entrypoint much earlier on ARM64 since we have a ton of scratch registers and already have callee in a register. Maybe that's profitable?
        JIT_COMMENT(jit, "Load callee entrypoint");
        jit.loadPtr(MacroAssembler::Address(GPRInfo::regWS1, WebAssemblyFunction::offsetOfEntrypointLoadLocation()), GPRInfo::regWS0);
        jit.loadPtr(MacroAssembler::Address(GPRInfo::regWS0), GPRInfo::regWS0);

        // Store the new callee Callee[cfr]
        JIT_COMMENT(jit, "Set the callee's interpreter Wasm::Callee");
        jit.transferPtr(CCallHelpers::Address(GPRInfo::regWS1, WebAssemblyFunction::offsetOfBoxedCallee()), CCallHelpers::calleeFrameSlot(CallFrameSlot::callee));

        jit.call(GPRInfo::regWS0, WasmEntryPtrTag);

        // Don't restore SP to original position — stack results are above calleeSP.
        // After a tail call the callee's frame may differ, so derive from actual SP.
        // Just allocate register spill space below the callee's actual SP.
        jit.subPtr(CCallHelpers::TrustedImmPtr(JSToWasmCallee::RegisterStackSpaceAligned),
            CCallHelpers::stackPointerRegister);

        // Save return registers
#if CPU(ARM64)
        jit.storePair64(GPRInfo::regWA0, GPRInfo::regWA1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8));
        jit.storePair64(GPRInfo::regWA2, GPRInfo::regWA3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8));
        jit.storePair64(GPRInfo::regWA4, GPRInfo::regWA5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8));
        jit.storePair64(GPRInfo::regWA6, GPRInfo::regWA7, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 6 * 8));
#elif CPU(X86_64)
        jit.storePair64(GPRInfo::regWA0, GPRInfo::regWA1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8));
        jit.storePair64(GPRInfo::regWA2, GPRInfo::regWA3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8));
        jit.storePair64(GPRInfo::regWA4, GPRInfo::regWA5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8));
#else
        jit.store64(GPRInfo::regWA0, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8));
        jit.store64(GPRInfo::regWA1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 1 * 8));
        jit.store64(GPRInfo::regWA2, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8));
        jit.store64(GPRInfo::regWA3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 3 * 8));
        jit.store64(GPRInfo::regWA4, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8));
        jit.store64(GPRInfo::regWA5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 5 * 8));
#endif
#if CPU(ARM64)
        jit.storePairDouble(FPRInfo::argumentFPR0, FPRInfo::argumentFPR1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  0) * 8));
        jit.storePairDouble(FPRInfo::argumentFPR2, FPRInfo::argumentFPR3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  2) * 8));
        jit.storePairDouble(FPRInfo::argumentFPR4, FPRInfo::argumentFPR5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  4) * 8));
        jit.storePairDouble(FPRInfo::argumentFPR6, FPRInfo::argumentFPR7, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  6) * 8));
#elif CPU(X86_64) || CPU(RISCV64)
        jit.storeDouble(FPRInfo::argumentFPR0, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  0) * 8));
        jit.storeDouble(FPRInfo::argumentFPR1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  1) * 8));
        jit.storeDouble(FPRInfo::argumentFPR2, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  2) * 8));
        jit.storeDouble(FPRInfo::argumentFPR3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  3) * 8));
        jit.storeDouble(FPRInfo::argumentFPR4, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  4) * 8));
        jit.storeDouble(FPRInfo::argumentFPR5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  5) * 8));
        jit.storeDouble(FPRInfo::argumentFPR6, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  6) * 8));
        jit.storeDouble(FPRInfo::argumentFPR7, CCallHelpers::Address(CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  7) * 8));
#endif

        // Prepare frame
        {
            jit.setupArguments<decltype(operationJSToWasmEntryWrapperBuildReturnFrame)>(CCallHelpers::stackPointerRegister, GPRInfo::callFrameRegister);
            jit.callOperation<OperationPtrTag>(operationJSToWasmEntryWrapperBuildReturnFrame);
            using ResultType = typename FunctionTraits<decltype(operationJSToWasmEntryWrapperBuildReturnFrame)>::ResultType;
            static_assert(CCallHelpers::operationExceptionRegister<ResultType>() != InvalidGPRReg, "We don't have a VM readily available so we rely on exception being returned");
            JIT_COMMENT(jit, "Exception check: ", CCallHelpers::operationExceptionRegister<ResultType>());
            exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<ResultType>()));
        }

        jit.emitRestoreCalleeSavesFor(calleeSaves);
        jit.addPtr(CCallHelpers::TrustedImmPtr(Wasm::JSToWasmCallee::SpillStackSpaceAligned), CCallHelpers::stackPointerRegister);
        jit.emitFunctionEpilogue();
        jit.ret();

        stackOverflow.link(&jit);
        {
            JIT_COMMENT(jit, "Re-load WebAssemblyFunction Callee");
            jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS1);

            JIT_COMMENT(jit, "Replace the WebAssemblyFunction Callee with our JSToWasm NativeCallee");
            jit.transferPtr(CCallHelpers::Address(GPRInfo::regWS1, WebAssemblyFunction::offsetOfBoxedJSToWasmCallee()), CCallHelpers::addressFor(CallFrameSlot::callee));
            jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
            jit.jumpThunk(CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
        }

        buildEntryFrameThrew.link(&jit);
        JIT_COMMENT(jit, "Re-load WebAssemblyFunction Callee");
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS1);

        JIT_COMMENT(jit, "Replace the WebAssemblyFunction Callee with our JSToWasm NativeCallee");
        jit.transferPtr(CCallHelpers::Address(GPRInfo::regWS1, WebAssemblyFunction::offsetOfBoxedJSToWasmCallee()), CCallHelpers::addressFor(CallFrameSlot::callee));
        jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));

        exceptionChecks.link(&jit);
        jit.move(GPRInfo::wasmContextInstancePointer, GPRInfo::argumentGPR0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR1);
        jit.emitRestoreCalleeSavesFor(calleeSaves);
        jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR1);
        // These are mostly no-ops but we leave them here for bookkeeping.
        jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
        jit.setupArguments<decltype(operationWasmUnwind)>(GPRInfo::argumentGPR0);
        jit.callOperation<OperationPtrTag>(operationWasmUnwind);
        jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
        codeRef.construct(FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "JSToWasm"_s, "JSToWasm"));
    });
    return codeRef.get();
}

MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionThunkGenerator(VM&)
{
    return createJSToWasmJITShared();
}

static size_t NODELETE trampolineReservedStackSize()
{
    // If we are jumping to the function which can have stack-overflow check,
    // then, trampoline does not need to do the check again if it is smaller than a threshold.
    // 1. Caller of this trampoline ensures that at least our stack is lower than softStackLimit.
    // 2. Callee may omit stack check if the frame size is less than reservedZoneSize and it does not have a call.
    // Based on that, trampoline between 1 and 2 can use (softReservedZoneSize - reservedZoneSize) / 2 size safely at least.
    // Note that minimumReservedZoneSize is 16KB, and we ensure that softReservedZoneSize - reservedZoneSize is at least 16KB.
    return (Options::softReservedZoneSize() - Options::reservedZoneSize()) / 2;
}

static RegisterAtOffsetList usedCalleeSaveRegisters(const Wasm::RTT& signature)
{
    // Pessimistically save callee saves in BoundsChecking mode since the IPInt always bounds checks
    RegisterSet calleeSaves = RegisterSet::wasmPinnedRegisters();
    // FIXME: Is it really worth considering functions that have void() signature? Are those actually common?
    if (signature.argumentCount() || !signature.returnsVoid()) {
        RegisterSet tagCalleeSaves = RegisterSet::vmCalleeSaveRegisters();
        tagCalleeSaves.filter(RegisterSet::runtimeTagRegisters());
        calleeSaves.merge(tagCalleeSaves);
    }
    return RegisterAtOffsetList { calleeSaves, RegisterAtOffsetList::OffsetBaseType::FramePointerBased };
}

CodePtr<JSEntryPtrTag> RTT::jsToWasmICEntrypoint() const
{
    ASSERT(kind() == RTTKind::Function);
    if (m_jsToWasmICCallee) [[likely]] {
        ASSERT(m_jsToWasmICCallee->jsToWasm());
        return m_jsToWasmICCallee->jsToWasm();
    }

    if (Options::forceICFailure() || !Options::useJIT())
        return nullptr;

    Locker locker(m_jitCodeLock);
    // Someone else could have been creating the code when we checked before and blocked us before getting here.
    if (m_jsToWasmICCallee)
        return m_jsToWasmICCallee->jsToWasm();

    CCallHelpers jit;

    JIT_COMMENT(jit, "jsCallICEntrypoint");

    ASSERT(!m_jsToWasmICCallee);
    Ref<JSToWasmICCallee> jsToWasmICCallee = JSToWasmICCallee::create(usedCalleeSaveRegisters(*this));
    const RegisterAtOffsetList& registersToSpill = *jsToWasmICCallee->calleeSaveRegistersImpl();

    const Wasm::WasmCallingConvention& wasmCC = Wasm::wasmCallingConvention();
    Wasm::CallInformation wasmCallInfo = wasmCC.callInformationFor(*this);
    if (argumentsOrResultsIncludeV128() || argumentsOrResultsIncludeExnref()) [[unlikely]]
        return nullptr;
    Wasm::CallInformation jsCallInfo = Wasm::jsCallingConvention().callInformationFor(*this, Wasm::CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    unsigned resultAreaSize = wasmCallInfo.headerAndArgumentStackSizeInBytes + savedResultRegisters.sizeOfAreaInBytes();
    unsigned resultAreaSizeAligned = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(resultAreaSize);

    unsigned totalFrameSize = registersToSpill.sizeOfAreaInBytes();
    totalFrameSize += sizeof(CPURegister); // Slot for the VM's previous wasm instance.
    totalFrameSize += wasmCallInfo.headerAndArgumentStackSizeInBytes;
    totalFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(totalFrameSize);

    jit.emitFunctionPrologue();
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);
    jit.emitSave(registersToSpill);

    GPRReg scratchGPR = Wasm::wasmCallingConvention().prologueScratchGPRs[1];
    GPRReg stackLimitGPR = Wasm::wasmCallingConvention().prologueScratchGPRs[0];

    CCallHelpers::JumpList slowPath;

    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::wasmContextInstancePointer);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, WebAssemblyFunction::offsetOfBoxedCallee()), scratchGPR);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, WebAssemblyFunction::offsetOfTargetInstance()), GPRInfo::wasmContextInstancePointer);
    if (totalFrameSize >= trampolineReservedStackSize()) {
        JIT_COMMENT(jit, "stack overflow check");
        jit.loadPtr(MacroAssembler::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit()), stackLimitGPR);
        slowPath.append(jit.branchPtr(CCallHelpers::LessThanOrEqual, MacroAssembler::stackPointerRegister, stackLimitGPR));
    }
    // Don't store the Wasm::Callee until after our stack check.
    jit.storeWasmCalleeToCalleeCallFrame(scratchGPR);

    bool haveTagRegisters = false;
    auto materializeTagRegistersIfNeeded = [&] {
        if (!haveTagRegisters) {
            haveTagRegisters = true;
            jit.emitMaterializeTagCheckRegisters();
        }
    };

    // Loop backwards so we can use the first FP/GP argument as a scratch.
    FPRReg scratchFPR = Wasm::wasmCallingConvention().fprArgs[0];
    GPRReg argumentScratchGPR = Wasm::wasmCallingConvention().gprArgs[0];
    CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, 0);
    for (unsigned i = argumentCount(); i--;) {
        CCallHelpers::Address jsParam(GPRInfo::callFrameRegister, jsCallInfo.params[i].location.offsetFromFP());
        bool isStack = wasmCallInfo.params[i].location.isStackArgument();

        auto type = argumentType(i);
        JIT_COMMENT(jit, "Arg ", i, " : ", type);

        bool missingUsesUndefined = type.isI32() || type.isF32() || type.isF64() || Wasm::isExternref(type);
        auto missingArg = jit.branch32(CCallHelpers::BelowOrEqual, CCallHelpers::lowWordFor(CallFrameSlot::argumentCountIncludingThis), CCallHelpers::TrustedImm32(i + 1));
        if (!missingUsesUndefined)
            slowPath.append(missingArg);

        switch (type.kind()) {
        case Wasm::TypeKind::I32: {
            materializeTagRegistersIfNeeded();
            jit.loadValue(jsParam, scratchGPR);
            slowPath.append(jit.branchIfNotInt32(scratchGPR));
            if (isStack) {
                CCallHelpers::Address addr { calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()) };
                jit.store32(scratchGPR, addr.withOffset(LowWordOffset));
            } else {
                jit.zeroExtend32ToWord(scratchGPR, wasmCallInfo.params[i].location.gpr());
            }
            break;
        }
        case Wasm::TypeKind::I64: {
            jit.loadValue(jsParam, scratchGPR);
            slowPath.append(jit.branchIfNotCell(scratchGPR));
            slowPath.append(jit.branchIfNotHeapBigInt(scratchGPR));
            if (isStack) {
                jit.toBigInt64(scratchGPR, stackLimitGPR);
                jit.store64(stackLimitGPR, calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()));
            } else {
                static_assert(isX86() || noOverlap(GPRInfo::wasmBaseMemoryPointer, GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister));
                GPRReg scratch = isX86() ? argumentScratchGPR : GPRInfo::wasmBaseMemoryPointer;
                if (wasmCallInfo.params[i].location.gpr() == scratch) {
                    scratch = GPRInfo::numberTagRegister;
                    // FIXME: In theory this only needs to restore the numberTagRegister not both but this is rare.
                    haveTagRegisters = false;
                }
                jit.toBigInt64(scratchGPR, wasmCallInfo.params[i].location.gpr());
            }
            break;
        }
        case Wasm::TypeKind::Ref:
        case Wasm::TypeKind::RefNull:
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::Externref: {
            if (Wasm::isFuncref(type) || (Wasm::isRefWithTypeIndex(type) && Wasm::TypeInformation::tryGetRTT(type.index()) && Wasm::TypeInformation::tryGetRTT(type.index())->kind() == Wasm::RTTKind::Function)) {
                // Ensure we have a WASM exported function.
                jit.loadValue(jsParam, scratchGPR);
                auto isNull = jit.branchIfNull(scratchGPR);
                if (!type.isNullable())
                    slowPath.append(isNull);
                slowPath.append(jit.branchIfNotCell(scratchGPR));

                jit.emitLoadStructure(scratchGPR, scratchGPR);
                jit.loadPtr(CCallHelpers::Address(scratchGPR, Structure::classInfoOffset()), scratchGPR);

                static_assert(std::is_final<WebAssemblyFunction>::value, "We do not check for subtypes below");
                static_assert(std::is_final<WebAssemblyWrapperFunction>::value, "We do not check for subtypes below");

                auto isWasmFunction = jit.branchPtr(CCallHelpers::Equal, scratchGPR, CCallHelpers::TrustedImmPtr(WebAssemblyFunction::info()));
                slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(WebAssemblyWrapperFunction::info())));

                isWasmFunction.link(&jit);
                if (Wasm::isRefWithTypeIndex(type)) {
                    auto targetRTT = TypeInformation::getCanonicalRTT(type.index());
                    jit.loadPtr(jsParam, scratchGPR);
                    jit.loadPtr(CCallHelpers::Address(scratchGPR, WebAssemblyFunctionBase::offsetOfRTT()), scratchGPR);
                    slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(targetRTT.ptr())));
                }

                if (type.isNullable())
                    isNull.link(&jit);
            } else if (Wasm::isI31ref(type)) {
                jit.loadValue(jsParam, scratchGPR);
                auto isNull = jit.branchIfNull(scratchGPR);
                if (!type.isNullable())
                    slowPath.append(isNull);
                slowPath.append(jit.branchIfNotInt32(scratchGPR, DoNotHaveTagRegisters));
                slowPath.append(jit.branch32(CCallHelpers::GreaterThan, scratchGPR, CCallHelpers::TrustedImm32(Wasm::maxI31ref)));
                slowPath.append(jit.branch32(CCallHelpers::LessThan, scratchGPR, CCallHelpers::TrustedImm32(Wasm::minI31ref)));
                if (type.isNullable())
                    isNull.link(&jit);
            } else if (!Wasm::isExternref(type)) {
                slowPath.append(jit.jump());
            }

            if (isStack) {
                jit.loadValue(jsParam, scratchGPR);
                if (!type.isNullable())
                    slowPath.append(jit.branchIfNull(scratchGPR));
                jit.storeValue(scratchGPR, calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()));
            } else {
                auto externGPR = wasmCallInfo.params[i].location.gpr();
                jit.loadValue(jsParam, externGPR);
                if (!type.isNullable())
                    slowPath.append(jit.branchIfNull(externGPR));
            }
            break;
        }
        case Wasm::TypeKind::F32:
        case Wasm::TypeKind::F64: {
            materializeTagRegistersIfNeeded();
            if (!isStack)
                scratchFPR = wasmCallInfo.params[i].location.fpr();

            jit.loadValue(jsParam, scratchGPR);
            slowPath.append(jit.branchIfNotNumber(scratchGPR));
            auto isInt32 = jit.branchIfInt32(scratchGPR);
            jit.unboxDouble(scratchGPR, scratchGPR, scratchFPR);
            if (argumentType(i).isF32())
                jit.convertDoubleToFloat(scratchFPR, scratchFPR);
            auto done = jit.jump();

            isInt32.link(&jit);
            if (argumentType(i).isF32())
                jit.convertInt32ToFloat(scratchGPR, scratchFPR);
            else
                jit.convertInt32ToDouble(scratchGPR, scratchFPR);
            done.link(&jit);
            if (isStack) {
                CCallHelpers::Address addr { calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()) };
                if (argumentType(i).isF32()) {
                    jit.storeFloat(scratchFPR, addr.withOffset(LowWordOffset));
                } else
                    jit.storeDouble(scratchFPR, addr);
            }
            break;
        }
        case Wasm::TypeKind::V128:
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (missingUsesUndefined) {
            auto argDone = jit.jump();
            missingArg.link(&jit);
            if (type.isI32()) {
                if (isStack) {
                    CCallHelpers::Address addr { calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()) };
                    jit.store32(CCallHelpers::TrustedImm32(0), addr.withOffset(LowWordOffset));
                } else
                    jit.xorPtr(wasmCallInfo.params[i].location.jsr().payloadGPR(), wasmCallInfo.params[i].location.jsr().payloadGPR());
            } else if (type.isF32() || type.isF64()) {
                if (!isStack)
                    scratchFPR = wasmCallInfo.params[i].location.fpr();
                jit.move64ToDouble(CCallHelpers::TrustedImm64(std::bit_cast<uint64_t>(PNaN)), scratchFPR);
                if (type.isF32())
                    jit.convertDoubleToFloat(scratchFPR, scratchFPR);
                if (isStack) {
                    CCallHelpers::Address addr { calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()) };
                    if (type.isF32())
                        jit.storeFloat(scratchFPR, addr.withOffset(LowWordOffset));
                    else
                        jit.storeDouble(scratchFPR, addr);
                }
            } else if (isStack)
                jit.storeTrustedValue(jsUndefined(), calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()));
            else
                jit.moveTrustedValue(jsUndefined(), wasmCallInfo.params[i].location.jsr());
            argDone.link(&jit);
        }
    }

    // At this point, we're committed to doing a fast call.
    // We don't know what memory mode we're about to call into but it's always valid to fill both bounds checking and base memory.
    jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemoryBaseSizePair(0)), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
    jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, stackLimitGPR, scratchGPR);

    // FIXME: We could load this much earlier on ARM64 since we have a ton of scratch registers and already have callee in a register. Maybe that's profitable?
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), stackLimitGPR);
    jit.loadPtr(MacroAssembler::Address(stackLimitGPR, WebAssemblyFunction::offsetOfEntrypointLoadLocation()), stackLimitGPR);
    jit.loadPtr(MacroAssembler::Address(stackLimitGPR), stackLimitGPR);

    jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(jsToWasmICCallee.ptr())), scratchGPR);
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    jit.storePairPtr(GPRInfo::wasmContextInstancePointer, scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    JIT_COMMENT(jit, "Make the call");
    jit.call(stackLimitGPR, WasmEntryPtrTag);

    jit.subPtr(CCallHelpers::TrustedImm32(resultAreaSizeAligned),
        CCallHelpers::stackPointerRegister);

    CCallHelpers::JumpList exceptionChecks;

    // Read results before restoring SP. Results are at the bottom of the arg/result
    // area (at callee's SP + headerSize), so we must read them before restoring SP.
    // FIXME: This assumes we don't have tag registers but we could just rematerialize them here since we already saved them.
    marshallJSResult(jit, *this, wasmCallInfo, savedResultRegisters, exceptionChecks, resultAreaSizeAligned);

    // Restore stack pointer after call.
    jit.addPtr(MacroAssembler::TrustedImm32(-static_cast<int32_t>(totalFrameSize)), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);

    ASSERT(!RegisterSet::runtimeTagRegisters().contains(GPRInfo::nonPreservedNonReturnGPR, IgnoreVectors));

    jit.emitRestore(registersToSpill, GPRInfo::callFrameRegister);
    jit.emitFunctionEpilogue();
    jit.ret();

    slowPath.link(&jit);
    JIT_COMMENT(jit, "Slow path start");
    jit.emitRestore(registersToSpill, GPRInfo::callFrameRegister);
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0);
    jit.emitFunctionEpilogue();
#if CPU(ARM64E)
    jit.untagReturnAddress(scratchGPR);
#endif

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSFunction::offsetOfExecutableOrRareData()), scratchGPR);
    auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
    jit.loadPtr(CCallHelpers::Address(scratchGPR, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), scratchGPR);
    hasExecutable.link(&jit);
    jit.loadPtr(CCallHelpers::Address(scratchGPR, ExecutableBase::offsetOfJITCodeWithArityCheckFor(CodeSpecializationKind::CodeForCall)), scratchGPR);
    JIT_COMMENT(jit, "Slow path jump");
    jit.farJump(scratchGPR, JSEntryPtrTag);

    exceptionChecks.link(&jit);
    JIT_COMMENT(jit, "Exception handle start");
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR0);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
    jit.setupArguments<decltype(Wasm::operationWasmUnwind)>(GPRInfo::wasmContextInstancePointer);
    jit.callOperation<OperationPtrTag>(Wasm::operationWasmUnwind);
    JIT_COMMENT(jit, "Exception handle jump");
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);

    LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) [[unlikely]]
        return nullptr;

    auto code = FINALIZE_WASM_CODE(linkBuffer, JSEntryPtrTag, nullptr, "JS->Wasm IC %s", WTF::toCString(*this).data());
    jsToWasmICCallee->setEntrypoint(WTF::move(code));
    WTF::storeStoreFence();
    m_jsToWasmICCallee = WTF::move(jsToWasmICCallee);

    return m_jsToWasmICCallee->jsToWasm();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
