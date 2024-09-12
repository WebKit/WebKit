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
#include "JSToWasm.h"
#include "WasmCallee.h"
#include <wtf/Gigacage.h>
#include <wtf/WTFConfig.h>

#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)

#include "CCallHelpers.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmOperations.h"
#include "WasmToJS.h"

namespace JSC {
namespace Wasm {

void marshallJSResult(CCallHelpers& jit, const FunctionSignature& signature, const CallInformation& wasmFrameConvention, const RegisterAtOffsetList& savedResultRegisters, CCallHelpers::JumpList& exceptionChecks)
{
    auto boxNativeCalleeResult = [](CCallHelpers& jit, Type type, ValueLocation src, JSValueRegs dst) {
        JIT_COMMENT(jit, "boxNativeCalleeResult ", type);
        switch (type.kind) {
        case TypeKind::Void:
            jit.moveTrustedValue(jsUndefined(), dst);
            break;
        case TypeKind::I32:
            jit.zeroExtend32ToWord(src.jsr().payloadGPR(), dst.payloadGPR());
            jit.boxInt32(dst.payloadGPR(), dst, DoNotHaveTagRegisters);
            break;
        case TypeKind::F32:
            jit.convertFloatToDouble(src.fpr(), src.fpr());
            FALLTHROUGH;
        case TypeKind::F64: {
            jit.moveTrustedValue(jsNumber(PNaN), dst);
            auto isNaN = jit.branchIfNaN(src.fpr());
#if USE(JSVALUE64)
            jit.boxDouble(src.fpr(), dst, DoNotHaveTagRegisters);
#else
            jit.boxDouble(src.fpr(), dst);
#endif
            isNaN.link(&jit);
            break;
        }
        default: {
            if (isRefType(type))
                jit.moveValueRegs(src.jsr(), dst);
            else
                jit.breakpoint();
        }
        }
    };

    if (signature.returnsVoid())
        jit.moveTrustedValue(jsUndefined(), JSRInfo::returnValueJSR);
    else if (signature.returnCount() == 1) {
        if (signature.returnType(0).isI64()) {
            JIT_COMMENT(jit, "convert wasm return to big int");
            JSValueRegs inputJSR = wasmFrameConvention.results[0].location.jsr();
            jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
            jit.setupArguments<decltype(operationConvertToBigInt)>(GPRInfo::wasmContextInstancePointer, inputJSR);
            jit.callOperation<OperationPtrTag>(operationConvertToBigInt);
        } else
            boxNativeCalleeResult(jit, signature.returnType(0), wasmFrameConvention.results[0].location, JSRInfo::returnValueJSR);
    } else {
        IndexingType indexingType = ArrayWithUndecided;
        JSValueRegs scratchJSR = JSValueRegs {
#if USE(JSVALUE32_64)
            wasmCallingConvention().prologueScratchGPRs[2],
#endif
            wasmCallingConvention().prologueScratchGPRs[1]
        };

        ASSERT(scratchJSR.payloadGPR() != GPRReg::InvalidGPRReg);
#if USE(JSVALUE32_64)
        ASSERT(scratchJSR.tagGPR() != GPRReg::InvalidGPRReg);
        ASSERT(scratchJSR.payloadGPR() != scratchJSR.tagGPR());
#endif

        // We can use the first floating point register as a scratch since it will always be moved onto the stack before other values.
        FPRReg fprScratch = wasmCallingConvention().fprArgs[0];
        JIT_COMMENT(jit, "scratchFPR: ", fprScratch, " - Scratch jsr: ", scratchJSR, " - saved result registers: ", savedResultRegisters);
        bool hasI64 = false;
        for (unsigned i = 0; i < signature.returnCount(); ++i) {
            ValueLocation loc = wasmFrameConvention.results[i].location;
            Type type = signature.returnType(i);

            hasI64 |= type.isI64();
            if (loc.isGPR() || loc.isFPR()) {
#if USE(JSVALUE32_64)
                ASSERT(!loc.isGPR() || savedResultRegisters.find(loc.jsr().payloadGPR())->offset() + 4 == savedResultRegisters.find(loc.jsr().tagGPR())->offset());
#endif
                auto address = CCallHelpers::Address(CCallHelpers::stackPointerRegister, wasmFrameConvention.headerAndArgumentStackSizeInBytes);
                switch (type.kind) {
                case TypeKind::F32:
                case TypeKind::F64:
                    boxNativeCalleeResult(jit, type, loc, scratchJSR);
                    jit.storeValue(scratchJSR, address.withOffset(savedResultRegisters.find(loc.fpr())->offset()));
                    break;
                case TypeKind::I64:
                    jit.storeValue(loc.jsr(), address.withOffset(savedResultRegisters.find(loc.jsr().payloadGPR())->offset()));
                    break;
                default:
                    boxNativeCalleeResult(jit, type, loc, scratchJSR);
                    jit.storeValue(scratchJSR, address.withOffset(savedResultRegisters.find(loc.jsr().payloadGPR())->offset()));
                    break;
                }
            } else {
                if (!type.isI64()) {
                    auto location = CCallHelpers::Address(CCallHelpers::stackPointerRegister, loc.offsetFromSP());
                    ValueLocation tmp;
                    switch (type.kind) {
                    case TypeKind::F32:
                        tmp = ValueLocation { fprScratch };
                        jit.loadFloat(location, fprScratch);
                        break;
                    case TypeKind::F64:
                        tmp = ValueLocation { fprScratch };
                        jit.loadDouble(location, fprScratch);
                        break;
                    case TypeKind::I32:
                        tmp = ValueLocation { scratchJSR };
                        jit.load32(location, scratchJSR.payloadGPR());
                        break;
                    default:
                        tmp = ValueLocation { scratchJSR };
                        jit.loadValue(location, scratchJSR);
                        break;
                    }
                    boxNativeCalleeResult(jit, type, tmp, scratchJSR);
                    jit.storeValue(scratchJSR, location);
                }
            }

            switch (type.kind) {
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

                constexpr JSValueRegs valueJSR = preferredArgumentJSR<decltype(operationConvertToBigInt), 1>();

                CCallHelpers::Address address { CCallHelpers::stackPointerRegister };
                if (loc.isGPR() || loc.isFPR()) {
#if USE(JSVALUE32_64)
                    ASSERT(savedResultRegisters.find(loc.jsr().payloadGPR())->offset() + 4 == savedResultRegisters.find(loc.jsr().tagGPR())->offset());
#endif
                    address = address.withOffset(savedResultRegisters.find(loc.jsr().payloadGPR())->offset() + wasmFrameConvention.headerAndArgumentStackSizeInBytes);
                } else
                    address = address.withOffset(loc.offsetFromSP());

                jit.loadValue(address, valueJSR);
                jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
                jit.setupArguments<decltype(operationConvertToBigInt)>(GPRInfo::wasmContextInstancePointer, valueJSR);
                jit.callOperation<OperationPtrTag>(operationConvertToBigInt);
                jit.storeValue(JSRInfo::returnValueJSR, address);
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
#if USE(JSVALUE64)
        static_assert(CCallHelpers::operationExceptionRegister<operationAllocateResultsArray>() != InvalidGPRReg, "We don't have a VM readily available so we rely on exception being returned");
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<operationAllocateResultsArray>()));
#else
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::returnValueGPR2));
#endif
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);

        jit.boxCell(GPRInfo::returnValueGPR, JSRInfo::returnValueJSR);
    }
}

std::shared_ptr<InternalFunction> createJSToWasmJITSharedCrashForSIMDParameters()
{
    static LazyNeverDestroyed<std::shared_ptr<InternalFunction>> thunk;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;
        JIT_COMMENT(jit, "jsToWasm shared wrapper");
        thunk.construct(std::make_shared<InternalFunction>());
        jit.emitFunctionPrologue();
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::wasmContextInstancePointer);
        JIT_COMMENT(jit, "Throw an exception because this function uses v128");
        emitThrowWasmToJSException(jit, GPRInfo::wasmContextInstancePointer, ExceptionType::TypeErrorInvalidV128Use);

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
        thunk->get()->entrypoint.compilation = makeUnique<Compilation>(FINALIZE_WASM_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly shared error entrypoint"), nullptr);
    });
    return thunk;
}

std::shared_ptr<InternalFunction> createJSToWasmJITShared()
{
    static LazyNeverDestroyed<std::shared_ptr<InternalFunction>> thunk;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        // JIT version of js_to_wasm_wrapper_entry
        // If you change this, make sure to modify WebAssembly.asm:op(js_to_wasm_wrapper_entry)
        CCallHelpers jit;
        CCallHelpers::JumpList exceptionChecks;
        JIT_COMMENT(jit, "jsToWasm shared wrapper");
        thunk.construct(std::make_shared<InternalFunction>());
        jit.emitFunctionPrologue();
        jit.subPtr(CCallHelpers::TrustedImmPtr(Wasm::JITLessJSEntrypointCallee::SpillStackSpaceAligned), CCallHelpers::stackPointerRegister);
        jit.emitSaveCalleeSavesFor(Wasm::JITLessJSEntrypointCallee::calleeSaveRegistersImpl());

        // Callee[cfr]
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regWS0, WebAssemblyFunction::offsetOfJSToWasmCallee()), GPRInfo::regWS0);

#if ASSERT_ENABLED
        jit.load32(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfIdent()), GPRInfo::regWS1);
        jit.move(MacroAssembler::TrustedImm32(0xBF), GPRInfo::regWA0);
        auto ok = jit.branch32(CCallHelpers::Equal, GPRInfo::regWS1, GPRInfo::regWA0);
        jit.breakpoint();
        ok.link(&jit);
#endif

        // Allocate stack space (no stack check)
        jit.load32(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfFrameSize()), GPRInfo::regWS1);
        jit.subPtr(GPRInfo::regWS1, CCallHelpers::stackPointerRegister);

        // Prepare frame
        jit.setupArguments<decltype(operationJSToWasmEntryWrapperBuildFrame)>(CCallHelpers::stackPointerRegister, GPRInfo::callFrameRegister);
        jit.callOperation<OperationPtrTag>(operationJSToWasmEntryWrapperBuildFrame);

        // Instance
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::wasmContextInstancePointer);

        // Memory
#if USE(JSVALUE64)
        jit.loadPair64(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
        jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, GPRInfo::regWS0);
#endif

#if CPU(ARM64)
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8), GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8), GPRInfo::regWA2, GPRInfo::regWA3);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8), GPRInfo::regWA4, GPRInfo::regWA5);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 6 * 8), GPRInfo::regWA6, GPRInfo::regWA7);
#elif CPU(X86_64)
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8), GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8), GPRInfo::regWA2, GPRInfo::regWA3);
        jit.loadPair64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8), GPRInfo::regWA4, GPRInfo::regWA5);
#elif USE(JSVALUE64)
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8), GPRInfo::regWA0);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 1 * 8), GPRInfo::regWA1);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8), GPRInfo::regWA2);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 3 * 8), GPRInfo::regWA3);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8), GPRInfo::regWA4);
        jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, 5 * 8), GPRInfo::regWA5);
#else
        jit.loadPair32(CCallHelpers::stackPointerRegister, MacroAssembler::TrustedImm32(0 * 8), GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair32(CCallHelpers::stackPointerRegister, MacroAssembler::TrustedImm32(1 * 8), GPRInfo::regWA2, GPRInfo::regWA3);
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
#elif CPU(ARM_THUMB2)
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 0 * 8), FPRInfo::argumentFPR0);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 1 * 8), FPRInfo::argumentFPR1);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 2 * 8), FPRInfo::argumentFPR2);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 3 * 8), FPRInfo::argumentFPR3);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 4 * 8), FPRInfo::argumentFPR4);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 5 * 8), FPRInfo::argumentFPR5);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 6 * 8), FPRInfo::argumentFPR6);
        jit.loadDouble(CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 + 7 * 8), FPRInfo::argumentFPR7);
#endif

        // Pop argument space values
        jit.addPtr(MacroAssembler::TrustedImmPtr(Wasm::JITLessJSEntrypointCallee::RegisterStackSpaceAligned), CCallHelpers::stackPointerRegister);

#if ASSERT_ENABLED
        for (int32_t i = 0; i < 30; ++i)
            jit.storePtr(MacroAssembler::TrustedImmPtr(0xbeef), CCallHelpers::Address(CCallHelpers::stackPointerRegister, -i * static_cast<int32_t>(sizeof(Register))));
#endif

        // Callee[cfr]
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS0);
        jit.unboxNativeCallee(GPRInfo::regWS0, GPRInfo::regWS0);

        // Store Callee's wasm callee
#if USE(JSVALUE64)
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmCallee()), GPRInfo::regWS1);
        jit.storePtr(GPRInfo::regWS1, CCallHelpers::calleeFrameSlot(CallFrameSlot::callee));
#else
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmCallee() + PayloadOffset), GPRInfo::regWS1);
        jit.storePtr(GPRInfo::regWS1, CCallHelpers::calleeFramePayloadSlot(CallFrameSlot::callee));
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmCallee() + TagOffset), GPRInfo::regWS1);
        jit.storePtr(GPRInfo::regWS1, CCallHelpers::calleeFrameTagSlot(CallFrameSlot::callee));
#endif
        jit.call(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmFunctionPrologue()), WasmEntryPtrTag);

        // Restore SP

        // Callee[cfr]
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regWS0);
        jit.unboxNativeCallee(GPRInfo::regWS0, GPRInfo::regWS0);

        jit.load32(CCallHelpers::Address(GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfFrameSize()), GPRInfo::regWS1);
        jit.addPtr(MacroAssembler::TrustedImmPtr(JITLessJSEntrypointCallee::SpillStackSpaceAligned), GPRInfo::regWS1);
        jit.subPtr(GPRInfo::callFrameRegister, GPRInfo::regWS1, CCallHelpers::stackPointerRegister);

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
#elif USE(JSVALUE64)
        jit.store64(GPRInfo::regWA0, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8));
        jit.store64(GPRInfo::regWA1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 1 * 8));
        jit.store64(GPRInfo::regWA2, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 2 * 8));
        jit.store64(GPRInfo::regWA3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 3 * 8));
        jit.store64(GPRInfo::regWA4, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 4 * 8));
        jit.store64(GPRInfo::regWA5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 5 * 8));
#else
        jit.storePair32(GPRInfo::regWA0, GPRInfo::regWA1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0 * 8));
        jit.storePair32(GPRInfo::regWA2, GPRInfo::regWA3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, 1 * 8));
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
#elif CPU(ARM_THUMB2)
        jit.storeDouble(FPRInfo::argumentFPR0, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  0 * 8));
        jit.storeDouble(FPRInfo::argumentFPR1, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  1 * 8));
        jit.storeDouble(FPRInfo::argumentFPR2, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  2 * 8));
        jit.storeDouble(FPRInfo::argumentFPR3, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  3 * 8));
        jit.storeDouble(FPRInfo::argumentFPR4, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  4 * 8));
        jit.storeDouble(FPRInfo::argumentFPR5, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  5 * 8));
        jit.storeDouble(FPRInfo::argumentFPR6, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  6 * 8));
        jit.storeDouble(FPRInfo::argumentFPR7, CCallHelpers::Address(CCallHelpers::stackPointerRegister, GPRInfo::numberOfArgumentRegisters * 4 +  7 * 8));
#endif

        // Prepare frame
        jit.setupArguments<decltype(operationJSToWasmEntryWrapperBuildReturnFrame)>(CCallHelpers::stackPointerRegister, GPRInfo::callFrameRegister);
        jit.callOperation<OperationPtrTag>(operationJSToWasmEntryWrapperBuildReturnFrame);
#if USE(JSVALUE64)
        static_assert(CCallHelpers::operationExceptionRegister<operationJSToWasmEntryWrapperBuildReturnFrame>() != InvalidGPRReg, "We don't have a VM readily available so we rely on exception being returned");
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::operationExceptionRegister<operationJSToWasmEntryWrapperBuildReturnFrame>()));
#else
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::returnValueGPR2));
#endif

        jit.emitRestoreCalleeSavesFor(Wasm::JITLessJSEntrypointCallee::calleeSaveRegistersImpl());
        jit.addPtr(CCallHelpers::TrustedImmPtr(Wasm::JITLessJSEntrypointCallee::SpillStackSpaceAligned), CCallHelpers::stackPointerRegister);
        jit.emitFunctionEpilogue();
        jit.ret();

        exceptionChecks.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR0);
        jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
        jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
        jit.setupArguments<decltype(operationWasmUnwind)>(GPRInfo::wasmContextInstancePointer);
        jit.callOperation<OperationPtrTag>(operationWasmUnwind);
        jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk);
        thunk->get()->entrypoint.compilation = makeUnique<Compilation>(FINALIZE_WASM_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly shared entrypoint"), nullptr);
    });
    return thunk;
}

std::unique_ptr<InternalFunction> createJSToWasmWrapper(CCallHelpers& jit, JSEntrypointCallee& entryCallee, Callee* wasmCallee, const TypeDefinition& typeDefinition, Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, unsigned functionIndex)
{
    const auto& signature = *typeDefinition.as<FunctionSignature>();
    JIT_COMMENT(jit, "jsToWasm wrapper for wasm-function[", functionIndex, "] : ", typeDefinition);
    auto result = makeUnique<InternalFunction>();
    jit.emitFunctionPrologue();

    // |codeBlock| and |this| slots are already initialized by the caller of this function because it is JS->Wasm transition.
    jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(&entryCallee)), GPRInfo::nonPreservedNonReturnGPR);
    CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
    jit.storePtr(GPRInfo::nonPreservedNonReturnGPR, calleeSlot.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
    jit.store32(CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag), calleeSlot.withOffset(TagOffset));
#endif

    // Pessimistically save callee saves in BoundsChecking mode since the LLInt / single-pass BBQ always can clobber bound checks
    RegisterSetBuilder toSave = RegisterSetBuilder::wasmPinnedRegisters();

#if ASSERT_ENABLED
    unsigned toSaveSize = toSave.numberOfSetGPRs();
    // They should all be callee saves.
    toSave.filter(RegisterSetBuilder::calleeSaveRegisters());
    ASSERT(toSave.numberOfSetGPRs() == toSaveSize);
#endif

    RegisterAtOffsetList registersToSpill(toSave.buildAndValidate(), RegisterAtOffsetList::OffsetBaseType::FramePointerBased);
    result->entrypoint.calleeSaveRegisters = registersToSpill;

    size_t totalFrameSize = registersToSpill.sizeOfAreaInBytes();
#if USE(JSVALUE64)
    ASSERT(totalFrameSize == toSave.numberOfSetGPRs() * sizeof(CPURegister));
#endif
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature);
    RegisterAtOffsetList savedResultRegisters = wasmFrameConvention.computeResultsOffsetList();
    totalFrameSize += wasmFrameConvention.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.sizeOfAreaInBytes();

    totalFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(totalFrameSize);
    JIT_COMMENT(jit, "Saved result registers: ", savedResultRegisters, "; Total frame size: ", totalFrameSize);
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);

    // We save all these registers regardless of having a memory or not.
    // The reason is that we use one of these as a scratch. That said,
    // almost all real wasm programs use memory, so it's not really
    // worth optimizing for the case that they don't.
    jit.emitSave(registersToSpill);

    // https://webassembly.github.io/spec/js-api/index.html#exported-function-exotic-objects
    // If parameters or results contain v128, throw a TypeError.
    // Note: the above error is thrown each time the [[Call]] method is invoked.
    if (Options::useWasmSIMD() && (wasmFrameConvention.argumentsOrResultsIncludeV128)) {
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::wasmContextInstancePointer);
        JIT_COMMENT(jit, "Throw an exception because this function uses v128");
        emitThrowWasmToJSException(jit, GPRInfo::wasmContextInstancePointer, ExceptionType::TypeErrorInvalidV128Use);
        return result;
    }

    {
        CallInformation jsFrameConvention = jsCallingConvention().callInformationFor(signature, CallRole::Callee);

        CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, 0);

        JSValueRegs scratchJSR {
#if USE(JSVALUE32_64)
            wasmCallingConvention().prologueScratchGPRs[2],
#endif
            wasmCallingConvention().prologueScratchGPRs[1]
        };

        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::wasmContextInstancePointer);
        uintptr_t boxed = reinterpret_cast<uintptr_t>(CalleeBits::boxNativeCalleeIfExists(wasmCallee));
        jit.storeWasmCalleeCallee(&boxed);

        for (unsigned i = 0; i < signature.argumentCount(); i++) {
            RELEASE_ASSERT(jsFrameConvention.params[i].location.isStack());

            Type type = signature.argumentType(i);
            JIT_COMMENT(jit, "Arg ", i, " : ", type);
            CCallHelpers::Address jsParam(GPRInfo::callFrameRegister, jsFrameConvention.params[i].location.offsetFromFP());
            if (wasmFrameConvention.params[i].location.isStackArgument()) {
                CCallHelpers::Address addr { calleeFrame.withOffset(wasmFrameConvention.params[i].location.offsetFromSP()) };
                if (type.isI32() || type.isF32()) {
                    jit.load32(jsParam, scratchJSR.payloadGPR());
                    jit.store32(scratchJSR.payloadGPR(), addr.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
                    jit.store32(CCallHelpers::TrustedImm32(0), addr.withOffset(TagOffset));
#endif
                } else {
                    jit.loadValue(jsParam, scratchJSR);
                    jit.storeValue(scratchJSR, addr);
                }
            } else {
                if (type.isF32())
                    jit.loadFloat(jsParam, wasmFrameConvention.params[i].location.fpr());
                else if (type.isF64())
                    jit.loadDouble(jsParam, wasmFrameConvention.params[i].location.fpr());
                else if (type.isI32()) {
                    jit.load32(jsParam, wasmFrameConvention.params[i].location.jsr().payloadGPR());
#if USE(JSVALUE32_64)
                    jit.move(CCallHelpers::TrustedImm32(0), wasmFrameConvention.params[i].location.jsr().tagGPR());
#endif
                } else
                    jit.loadValue(jsParam, wasmFrameConvention.params[i].location.jsr());
            }
        }
    }

#if CPU(ARM) // ARM has no pinned registers for Wasm Memory, so no need to set them up
    UNUSED_PARAM(mode);
#else
    if (!!info.memory) {
        GPRReg size = wasmCallingConvention().prologueScratchGPRs[0];
        GPRReg scratch = wasmCallingConvention().prologueScratchGPRs[1];
        if (isARM64E()) {
            if (mode == MemoryMode::BoundsChecking)
                size = GPRInfo::wasmBoundsCheckingSizeRegister;
            jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, size);
        } else {
            if (mode == MemoryMode::BoundsChecking)
                jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
            else
                jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer);
        }
        jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, size, scratch);
    }
#endif

    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
    unsigned functionIndexSpace = functionIndex + info.importFunctionCount();
    ASSERT(functionIndexSpace < info.functionIndexSpaceSize());
    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace, { } });
    });

    // Restore stack pointer after call
    jit.addPtr(MacroAssembler::TrustedImm32(-static_cast<int32_t>(totalFrameSize)), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);

    CCallHelpers::JumpList exceptionChecks;

    JIT_COMMENT(jit, "marshallJSResult");
    marshallJSResult(jit, signature, wasmFrameConvention, savedResultRegisters, exceptionChecks);

    JIT_COMMENT(jit, "restore registersToSpill");
    jit.emitRestore(registersToSpill);
    jit.emitFunctionEpilogue();
    jit.ret();

    exceptionChecks.link(&jit);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR0);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
    jit.setupArguments<decltype(operationWasmUnwind)>(GPRInfo::wasmContextInstancePointer);
    jit.callOperation<OperationPtrTag>(operationWasmUnwind);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);

    return result;
}

static size_t trampolineReservedStackSize()
{
    // If we are jumping to the function which can have stack-overflow check,
    // then, trampoline does not need to do the check again if it is smaller than a threshold.
    // 1. Caller of this trampoline ensures that at least our stack is lower than softStackLimit.
    // 2. Callee may omit stack check if the frame size is less than reservedZoneSize and it does not have a call.
    // Based on that, trampoline between 1 and 2 can use (softReservedZoneSize - reservedZoneSize) / 2 size safely at least.
    // Note that minimumReservedZoneSize is 16KB, and we ensure that softReservedZoneSize - reservedZoneSize is at least 16KB.
    return (Options::softReservedZoneSize() - Options::reservedZoneSize()) / 2;
}

static RegisterAtOffsetList usedCalleeSaveRegisters(const Wasm::FunctionSignature& signature)
{
    // Pessimistically save callee saves in BoundsChecking mode since the LLInt always bounds checks
    RegisterSetBuilder calleeSaves = RegisterSetBuilder::wasmPinnedRegisters();
    // FIXME: Is it really worth considering functions that have void() signature? Are those actually common?
    if (signature.argumentCount() || !signature.returnsVoid()) {
        RegisterSetBuilder tagCalleeSaves = RegisterSetBuilder::vmCalleeSaveRegisters();
        tagCalleeSaves.filter(RegisterSetBuilder::runtimeTagRegisters());
        calleeSaves.merge(tagCalleeSaves);
    }
    return RegisterAtOffsetList { calleeSaves.buildAndValidate(), RegisterAtOffsetList::OffsetBaseType::FramePointerBased };
}

CodePtr<JSEntryPtrTag> FunctionSignature::jsToWasmICEntrypoint() const
{
    if (LIKELY(m_jsToWasmICCallee)) {
        ASSERT(m_jsToWasmICCallee->jsEntrypoint());
        return m_jsToWasmICCallee->jsEntrypoint();
    }

    if (Options::forceICFailure())
        return nullptr;

    Locker locker(m_jitCodeLock);
    // Someone else could have been creating the code when we checked before and blocked us before getting here.
    if (m_jsToWasmICCallee)
        return m_jsToWasmICCallee->jsEntrypoint();

    CCallHelpers jit;

    JIT_COMMENT(jit, "jsCallICEntrypoint");

    ASSERT(!m_jsToWasmICCallee);
    Ref<JSToWasmICCallee> jsToWasmICCallee = JSToWasmICCallee::create(usedCalleeSaveRegisters(*this));
    const RegisterAtOffsetList& registersToSpill = *jsToWasmICCallee->calleeSaveRegistersImpl();

    const Wasm::WasmCallingConvention& wasmCC = Wasm::wasmCallingConvention();
    Wasm::CallInformation wasmCallInfo = wasmCC.callInformationFor(*this);
    if (wasmCallInfo.argumentsOrResultsIncludeV128)
        return nullptr;
    Wasm::CallInformation jsCallInfo = Wasm::jsCallingConvention().callInformationFor(*this, Wasm::CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    unsigned totalFrameSize = registersToSpill.sizeOfAreaInBytes();
    totalFrameSize += sizeof(CPURegister); // Slot for the VM's previous wasm instance.
    totalFrameSize += wasmCallInfo.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.sizeOfAreaInBytes();
    totalFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(totalFrameSize);

#if USE(JSVALUE32_64)
    if (wasmCallInfo.argumentsIncludeI64)
        return nullptr;
#endif

    jit.emitFunctionPrologue();
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);
    jit.emitSave(registersToSpill);

    JSValueRegs scratchJSR {
#if USE(JSVALUE32_64)
        Wasm::wasmCallingConvention().prologueScratchGPRs[2],
#endif
        Wasm::wasmCallingConvention().prologueScratchGPRs[1]
    };
    GPRReg stackLimitGPR = Wasm::wasmCallingConvention().prologueScratchGPRs[0];

    CCallHelpers::JumpList slowPath;

    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::wasmContextInstancePointer);
    static_assert(WebAssemblyFunction::offsetOfInstance() + sizeof(WriteBarrier<JSWebAssemblyInstance>) == WebAssemblyFunction::offsetOfBoxedWasmCallee());
    jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, MacroAssembler::TrustedImm32(WebAssemblyFunction::offsetOfInstance()), GPRInfo::wasmContextInstancePointer, scratchJSR.payloadGPR());
    if (totalFrameSize >= trampolineReservedStackSize()) {
        JIT_COMMENT(jit, "stack overflow check");
        jit.loadPtr(MacroAssembler::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfSoftStackLimit()), stackLimitGPR);

        slowPath.append(jit.branchPtr(CCallHelpers::Above, MacroAssembler::stackPointerRegister, GPRInfo::callFrameRegister));
        slowPath.append(jit.branchPtr(CCallHelpers::Below, MacroAssembler::stackPointerRegister, stackLimitGPR));
    }
    // Don't store the Wasm::Callee until after our stack check.
    jit.storeWasmCalleeCallee(scratchJSR.payloadGPR());

    // Ensure:
    // argCountPlusThis - 1 >= argumentCount()
    // argCountPlusThis >= argumentCount() + 1
    // FIXME: We should handle mismatched arity
    // https://bugs.webkit.org/show_bug.cgi?id=196564
    if (argumentCount() > 0) {
        slowPath.append(jit.branch32(CCallHelpers::Below,
            CCallHelpers::payloadFor(CallFrameSlot::argumentCountIncludingThis), CCallHelpers::TrustedImm32(argumentCount() + 1)));
    }

    bool haveTagRegisters = false;
    auto materializeTagRegistersIfNeeded = [&] {
        if (!haveTagRegisters) {
            haveTagRegisters = true;
            jit.emitMaterializeTagCheckRegisters();
        }
    };

    // Loop backwards so we can use the first FP/GP argument as a scratch.
    FPRReg scratchFPR = Wasm::wasmCallingConvention().fprArgs[0];
    GPRReg scratchGPR = Wasm::wasmCallingConvention().jsrArgs[0].payloadGPR();
    CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, 0);
    for (unsigned i = argumentCount(); i--;) {
        CCallHelpers::Address jsParam(GPRInfo::callFrameRegister, jsCallInfo.params[i].location.offsetFromFP());
        bool isStack = wasmCallInfo.params[i].location.isStackArgument();

        auto type = argumentType(i);
        JIT_COMMENT(jit, "Arg ", i, " : ", type);
        switch (type.kind) {
        case Wasm::TypeKind::I32: {
            materializeTagRegistersIfNeeded();
            jit.loadValue(jsParam, scratchJSR);
            slowPath.append(jit.branchIfNotInt32(scratchJSR));
            if (isStack) {
                CCallHelpers::Address addr { calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()) };
                jit.store32(scratchJSR.payloadGPR(), addr.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
                jit.store32(CCallHelpers::TrustedImm32(0), addr.withOffset(TagOffset));
#endif
            } else {
                jit.zeroExtend32ToWord(scratchJSR.payloadGPR(), wasmCallInfo.params[i].location.jsr().payloadGPR());
#if USE(JSVALUE32_64)
                jit.move(CCallHelpers::TrustedImm32(0), wasmCallInfo.params[i].location.jsr().tagGPR());
#endif
            }
            break;
        }
        case Wasm::TypeKind::I64: {
#if USE(JSVALUE64)
            jit.loadValue(jsParam, scratchJSR);
            slowPath.append(jit.branchIfNotCell(scratchJSR));
            slowPath.append(jit.branchIfNotHeapBigInt(scratchJSR.payloadGPR()));
            if (isStack) {
                // Since we're looping backwards we don't have to worry about the arguments being in registers yet so we can use them as scratches.
                jit.toBigInt64(scratchJSR.payloadGPR(), stackLimitGPR, scratchGPR, Wasm::wasmCallingConvention().jsrArgs[1].payloadGPR());
                jit.store64(stackLimitGPR, calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()));
            } else {
                static_assert(isX86() || noOverlap(GPRInfo::wasmBaseMemoryPointer, GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister));
                GPRReg scratch = isX86() ? scratchGPR : GPRInfo::wasmBaseMemoryPointer;
                if (wasmCallInfo.params[i].location.jsr().payloadGPR() == scratch) {
                    scratch = GPRInfo::numberTagRegister;
                    // FIXME: In theory this only needs to restore the numberTagRegister not both but this is rare.
                    haveTagRegisters = false;
                }
                jit.toBigInt64(scratchJSR.payloadGPR(), wasmCallInfo.params[i].location.jsr().payloadGPR(), stackLimitGPR, scratch);
            }
#else
            UNUSED_PARAM(scratchGPR);
            UNREACHABLE_FOR_PLATFORM();
#endif
            break;
        }
        case Wasm::TypeKind::Ref:
        case Wasm::TypeKind::RefNull:
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::Externref: {
            if (Wasm::isFuncref(type) || (Wasm::isRefWithTypeIndex(type) && Wasm::TypeInformation::get(type.index).is<Wasm::FunctionSignature>())) {
                // Ensure we have a WASM exported function.
                jit.loadValue(jsParam, scratchJSR);
                auto isNull = jit.branchIfNull(scratchJSR);
                if (!type.isNullable())
                    slowPath.append(isNull);
                slowPath.append(jit.branchIfNotCell(scratchJSR));

                jit.emitLoadStructure(scratchJSR.payloadGPR(), scratchJSR.payloadGPR());
                jit.loadCompactPtr(CCallHelpers::Address(scratchJSR.payloadGPR(), Structure::classInfoOffset()), scratchJSR.payloadGPR());

                static_assert(std::is_final<WebAssemblyFunction>::value, "We do not check for subtypes below");
                static_assert(std::is_final<WebAssemblyWrapperFunction>::value, "We do not check for subtypes below");

                auto isWasmFunction = jit.branchPtr(CCallHelpers::Equal, scratchJSR.payloadGPR(), CCallHelpers::TrustedImmPtr(WebAssemblyFunction::info()));
                slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchJSR.payloadGPR(), CCallHelpers::TrustedImmPtr(WebAssemblyWrapperFunction::info())));

                isWasmFunction.link(&jit);
                if (Wasm::isRefWithTypeIndex(type)) {
                    jit.loadPtr(jsParam, scratchJSR.payloadGPR());
                    jit.loadPtr(CCallHelpers::Address(scratchJSR.payloadGPR(), WebAssemblyFunctionBase::offsetOfSignatureIndex()), scratchJSR.payloadGPR());
                    slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchJSR.payloadGPR(), CCallHelpers::TrustedImmPtr(type.index)));
                }

                if (type.isNullable())
                    isNull.link(&jit);
            } else if (!Wasm::isExternref(type)) {
                // FIXME: this should implement some fast paths for, e.g., i31refs and other
                // types that can be easily handled.
                slowPath.append(jit.jump());
            }

            if (isStack) {
                jit.loadValue(jsParam, scratchJSR);
                if (!type.isNullable())
                    slowPath.append(jit.branchIfNull(scratchJSR));
                jit.storeValue(scratchJSR, calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()));
            } else {
                auto externJSR = wasmCallInfo.params[i].location.jsr();
                jit.loadValue(jsParam, externJSR);
                if (!type.isNullable())
                    slowPath.append(jit.branchIfNull(externJSR));
            }
            break;
        }
        case Wasm::TypeKind::F32:
        case Wasm::TypeKind::F64: {
            materializeTagRegistersIfNeeded();
            if (!isStack)
                scratchFPR = wasmCallInfo.params[i].location.fpr();

            jit.loadValue(jsParam, scratchJSR);
#if USE(JSVALUE64)
            slowPath.append(jit.branchIfNotNumber(scratchJSR, InvalidGPRReg));
#elif USE(JSVALUE32_64)
            slowPath.append(jit.branchIfNotNumber(scratchJSR, stackLimitGPR));
#endif
            auto isInt32 = jit.branchIfInt32(scratchJSR);
#if USE(JSVALUE64)
            jit.unboxDouble(scratchJSR.payloadGPR(), scratchJSR.payloadGPR(), scratchFPR);
#elif USE(JSVALUE32_64)
            jit.unboxDouble(scratchJSR, scratchFPR);
#endif
            if (argumentType(i).isF32())
                jit.convertDoubleToFloat(scratchFPR, scratchFPR);
            auto done = jit.jump();

            isInt32.link(&jit);
            if (argumentType(i).isF32())
                jit.convertInt32ToFloat(scratchJSR.payloadGPR(), scratchFPR);
            else
                jit.convertInt32ToDouble(scratchJSR.payloadGPR(), scratchFPR);
            done.link(&jit);
            if (isStack) {
                CCallHelpers::Address addr { calleeFrame.withOffset(wasmCallInfo.params[i].location.offsetFromSP()) };
                if (argumentType(i).isF32()) {
                    jit.storeFloat(scratchFPR, addr.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
                    jit.store32(CCallHelpers::TrustedImm32(0), addr.withOffset(TagOffset));
#endif
                } else
                    jit.storeDouble(scratchFPR, addr);
            }
            break;
        }
        case Wasm::TypeKind::V128:
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    // At this point, we're committed to doing a fast call.
#if !CPU(ARM) // ARM has no pinned registers for Wasm Memory, so no need to set them up
    // We don't know what memory mode we're about to call into but it's always valid to fill both bounds checking and base memory.
    jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
    jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, stackLimitGPR, scratchJSR.payloadGPR());
#endif

    // FIXME: We could load this much earlier on ARM64 since we have a ton of scratch registers and already have callee in a register. Maybe that's profitable?
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), stackLimitGPR);
    jit.loadPtr(MacroAssembler::Address(stackLimitGPR, WebAssemblyFunction::offsetOfEntrypointLoadLocation()), stackLimitGPR);
    jit.loadPtr(MacroAssembler::Address(stackLimitGPR), stackLimitGPR);

    jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxNativeCallee(jsToWasmICCallee.ptr())), scratchJSR.payloadGPR());
#if USE(JSVALUE32_64)
    jit.storePtr(scratchJSR.payloadGPR(), CCallHelpers::addressFor(CallFrameSlot::callee));
    jit.store32(CCallHelpers::TrustedImm32(JSValue::NativeCalleeTag), CCallHelpers::addressFor(CallFrameSlot::callee).withOffset(TagOffset));
    jit.storePtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::addressFor(CallFrameSlot::codeBlock));
#else
    static_assert(CallFrameSlot::codeBlock + 1 == CallFrameSlot::callee);
    jit.storePairPtr(GPRInfo::wasmContextInstancePointer, scratchJSR.payloadGPR(), GPRInfo::callFrameRegister, CCallHelpers::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));
#endif

    JIT_COMMENT(jit, "Make the call");
    jit.call(stackLimitGPR, WasmEntryPtrTag);

    // Restore stack pointer after call
    jit.addPtr(MacroAssembler::TrustedImm32(-static_cast<int32_t>(totalFrameSize)), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);

    CCallHelpers::JumpList exceptionChecks;

    // FIXME: This assumes we don't have tag registers but we could just rematerialize them here since we already saved them.
    marshallJSResult(jit, *this, wasmCallInfo, savedResultRegisters, exceptionChecks);

    ASSERT(!RegisterSetBuilder::runtimeTagRegisters().contains(GPRInfo::nonPreservedNonReturnGPR, IgnoreVectors));

    jit.emitRestore(registersToSpill, GPRInfo::callFrameRegister);
    jit.emitFunctionEpilogue();
    jit.ret();

    slowPath.link(&jit);
    JIT_COMMENT(jit, "Slow path start");
    jit.emitRestore(registersToSpill, GPRInfo::callFrameRegister);
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0);
    jit.emitFunctionEpilogue();
#if CPU(ARM64E)
    jit.untagReturnAddress(scratchJSR.payloadGPR());
#endif

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSFunction::offsetOfExecutableOrRareData()), scratchJSR.payloadGPR());
    auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, scratchJSR.payloadGPR(), CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
    jit.loadPtr(CCallHelpers::Address(scratchJSR.payloadGPR(), FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), scratchJSR.payloadGPR());
    hasExecutable.link(&jit);
    jit.loadPtr(CCallHelpers::Address(scratchJSR.payloadGPR(), ExecutableBase::offsetOfJITCodeWithArityCheckFor(CodeForCall)), scratchJSR.payloadGPR());
    JIT_COMMENT(jit, "Slow path jump");
    jit.farJump(scratchJSR.payloadGPR(), JSEntryPtrTag);

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
    if (UNLIKELY(linkBuffer.didFailToAllocate()))
        return nullptr;

    auto code = FINALIZE_WASM_CODE(linkBuffer, JSEntryPtrTag, nullptr, "JS->Wasm IC %s", WTF::toCString(*this).data());
    jsToWasmICCallee->setEntrypoint(WTFMove(code));
    WTF::storeStoreFence();
    m_jsToWasmICCallee = WTFMove(jsToWasmICCallee);

    return m_jsToWasmICCallee->jsEntrypoint();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
