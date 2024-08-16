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

void marshallJSResult(CCallHelpers& jit, const TypeDefinition& typeDefinition, const CallInformation& wasmFrameConvention, const RegisterAtOffsetList& savedResultRegisters, CCallHelpers::JumpList& exceptionChecks)
{
    const auto& signature = *typeDefinition.as<FunctionSignature>();
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
        jit.setupArguments<decltype(operationAllocateResultsArray)>(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImmPtr(&typeDefinition), indexingType, savedResultsGPR);
        JIT_COMMENT(jit, "operationAllocateResultsArray");
        jit.callOperation<OperationPtrTag>(operationAllocateResultsArray);
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::returnValueGPR2));
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);

        jit.boxCell(GPRInfo::returnValueGPR, JSRInfo::returnValueJSR);
    }
}

std::shared_ptr<InternalFunction> createJSToWasmJITInterpreterCrashForSIMDParameters()
{
    static LazyNeverDestroyed<std::shared_ptr<InternalFunction>> thunk;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;
        JIT_COMMENT(jit, "jsToWasm interpreted wrapper");
        thunk.construct(std::make_shared<InternalFunction>());
        jit.emitFunctionPrologue();
        jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::wasmContextInstancePointer);
        JIT_COMMENT(jit, "Throw an exception because this function uses v128");
        emitThrowWasmToJSException(jit, GPRInfo::wasmContextInstancePointer, ExceptionType::TypeErrorInvalidV128Use);

        LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate()))
            RELEASE_ASSERT(false);
        thunk->get()->entrypoint.compilation = makeUnique<Compilation>(
            FINALIZE_WASM_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly interpreted error entrypoint"),
            nullptr);
    });
    return thunk;
}

std::shared_ptr<InternalFunction> createJSToWasmJITInterpreter()
{
    static LazyNeverDestroyed<std::shared_ptr<InternalFunction>> thunk;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        // JIT version of js_to_wasm_wrapper_entry
        // If you change this, make sure to modify WebAssembly.asm:op(js_to_wasm_wrapper_entry)
        CCallHelpers jit;
        CCallHelpers::JumpList exceptionChecks;
        JIT_COMMENT(jit, "jsToWasm interpreted wrapper");
        thunk.construct(std::make_shared<InternalFunction>());
        jit.emitFunctionPrologue();

        // saveJSEntrypointInterpreterRegisters
        jit.subPtr(CCallHelpers::TrustedImmPtr(Wasm::JITLessJSEntrypointCallee::SpillStackSpaceAligned), CCallHelpers::stackPointerRegister);

#if CPU(ARM64) || CPU(ARM64E) || CPU(X86_64)
        CCallHelpers::Address memBaseSlot { GPRInfo::callFrameRegister, -2 * static_cast<long>(sizeof(Register)) };
        CCallHelpers::Address wasmInstanceSlot { GPRInfo::callFrameRegister, -3 * static_cast<long>(sizeof(Register)) };
        jit.storePair64(GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, memBaseSlot);
        jit.store64(GPRInfo::wasmContextInstancePointer, wasmInstanceSlot);
#else
        CCallHelpers::Address wasmInstanceSlot { GPRInfo::callFrameRegister, -1 * static_cast<long>(sizeof(Register)) };
        jit.store32(GPRInfo::wasmContextInstancePointer, wasmInstanceSlot);
#endif

        {
            // Callee[cfr]
            CCallHelpers::Address functionSlot { GPRInfo::callFrameRegister, sizeof(CPURegister) * 2 + sizeof(Register) };
            jit.loadPtr(functionSlot, GPRInfo::regWS0);
        }

        {
            CCallHelpers::Address calleeSlot { GPRInfo::regWS0, WebAssemblyFunction::offsetOfJSToWasmCallee() };
            jit.loadPtr(calleeSlot, GPRInfo::regWS0);
        }

#if ASSERT_ENABLED
        {
            CCallHelpers::Address identSlot { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfIdent() };
            jit.load32(identSlot, GPRInfo::regWS1);
            jit.move(MacroAssembler::TrustedImm32(0xBF), GPRInfo::regWA0);
            auto ok = jit.branch32(CCallHelpers::Equal, GPRInfo::regWS1, GPRInfo::regWA0);
            jit.breakpoint();
            ok.link(&jit);
        }
#endif

        // Allocate stack space (no stack check)
        {
            CCallHelpers::Address frameSlot { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfFrameSize() };
            jit.load32(frameSlot, GPRInfo::regWS1);
            jit.subPtr(GPRInfo::regWS1, CCallHelpers::stackPointerRegister);
        }

        // Prepare frame
        jit.setupArguments<decltype(operationJSToWasmEntryWrapperBuildFrame)>(CCallHelpers::stackPointerRegister, GPRInfo::callFrameRegister);
        jit.callOperation<OperationPtrTag>(operationJSToWasmEntryWrapperBuildFrame);

        // Instance
        {
            CCallHelpers::Address instanceSlot { GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * 8 };
            jit.loadPtr(instanceSlot, GPRInfo::wasmContextInstancePointer);
        }

        // Memory
        {
#if CPU(ARM64) || CPU(ARM64E) || CPU(X86_64)
            jit.loadPair64(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(JSWebAssemblyInstance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
#elif CPU(X86_64)
            jit.loadPtr(CCallHelpers::Address { GPRInfo::wasmBaseMemoryPointer, JSWebAssemblyInstance::offsetOfCachedMemory() }, GPRInfo::wasmContextInstancePointer);
            jit.loadPtr(CCallHelpers::Address { GPRInfo::wasmBoundsCheckingSizeRegister, JSWebAssemblyInstance::offsetOfCachedBoundsCheckingSize() }, GPRInfo::wasmContextInstancePointer);
#endif
#if !CPU(ARMv7)
#if GIGACAGE_ENABLED && !ENABLE(C_LOOP)
            // cagePrimitive(GigacageConfig + Gigacage::Config::basePtrs + GigacagePrimitiveBasePtrOffset, constexpr Gigacage::primitiveGigacageMask, ptr, scratch)
            auto gigacageConfig = bitwise_cast<uint8_t*>(&WebConfig::g_config) + static_cast<ptrdiff_t>(Gigacage::startOffsetOfGigacageConfig);
            auto gigacageDisablingPrimitiveIsForbidden = gigacageConfig + OBJECT_OFFSETOF(Gigacage::Config, disablingPrimitiveGigacageIsForbidden);
            jit.load8(gigacageDisablingPrimitiveIsForbidden, GPRInfo::regWS0);
            auto doCaging = jit.branch32(CCallHelpers::NotEqual, GPRInfo::regWS0, MacroAssembler::TrustedImm32(0));
            jit.load8(&Gigacage::disablePrimitiveGigacageRequested, GPRInfo::regWS0);
            auto done = jit.branch32(CCallHelpers::NotEqual, GPRInfo::regWS0, MacroAssembler::TrustedImm32(0));
            doCaging.link(&jit);
            // cage()
            auto basePtr = gigacageConfig + OBJECT_OFFSETOF(Gigacage::Config, basePtrs) + Gigacage::offsetOfPrimitiveGigacageBasePtr;
            auto mask = Gigacage::primitiveGigacageMask;
            jit.loadPtr(basePtr, GPRInfo::regWS0);
            auto cageDone = jit.branch64(CCallHelpers::Equal, GPRInfo::regWS0, MacroAssembler::TrustedImm32(0));
            jit.andPtr(MacroAssembler::TrustedImmPtr(mask), GPRInfo::wasmBaseMemoryPointer);
            jit.addPtr(GPRInfo::regWS0, GPRInfo::wasmBaseMemoryPointer);
            cageDone.link(&jit);
            done.link(&jit);
#endif
#endif
        }

#if CPU(ARM64) || CPU(ARM64E)
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 }, GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 2 * 8 }, GPRInfo::regWA2, GPRInfo::regWA3);
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 4 * 8 }, GPRInfo::regWA4, GPRInfo::regWA5);
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 6 * 8 }, GPRInfo::regWA6, GPRInfo::regWA7);
#elif CPU(X86_64)
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 }, GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 2 * 8 }, GPRInfo::regWA2, GPRInfo::regWA3);
        jit.loadPair64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 4 * 8 }, GPRInfo::regWA4, GPRInfo::regWA5);
#elif USE(JSVALUE64)
        jit.load64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 }, GPRInfo::regWA0);
        jit.load64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 1 * 8 }, GPRInfo::regWA1);
        jit.load64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 2 * 8 }, GPRInfo::regWA2);
        jit.load64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 3 * 8 }, GPRInfo::regWA3);
        jit.load64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 4 * 8 }, GPRInfo::regWA4);
        jit.load64(CCallHelpers::Address { CCallHelpers::stackPointerRegister, 5 * 8 }, GPRInfo::regWA5);
#else
        jit.loadPair32(CCallHelpers::stackPointerRegister, MacroAssembler::TrustedImm32(0*8), GPRInfo::regWA0, GPRInfo::regWA1);
        jit.loadPair32(CCallHelpers::stackPointerRegister, MacroAssembler::TrustedImm32(1*8), GPRInfo::regWA2, GPRInfo::regWA3);
#endif

#if CPU(ARM64) || CPU(ARM64E)
        jit.loadPairDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 0) * 8 }, FPRInfo::argumentFPR0, FPRInfo::argumentFPR1);
        jit.loadPairDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 2) * 8 }, FPRInfo::argumentFPR2, FPRInfo::argumentFPR3);
        jit.loadPairDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 4) * 8 }, FPRInfo::argumentFPR4, FPRInfo::argumentFPR5);
        jit.loadPairDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 6) * 8 }, FPRInfo::argumentFPR6, FPRInfo::argumentFPR7);
#elif CPU(X86_64) || CPU(RISCV64)
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 0) * 8 }, FPRInfo::argumentFPR0);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 1) * 8 }, FPRInfo::argumentFPR1);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 2) * 8 }, FPRInfo::argumentFPR2);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 3) * 8 }, FPRInfo::argumentFPR3);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 4) * 8 }, FPRInfo::argumentFPR4);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 5) * 8 }, FPRInfo::argumentFPR5);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 6) * 8 }, FPRInfo::argumentFPR6);
        jit.loadDouble(CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters + 7) * 8 }, FPRInfo::argumentFPR7);
#endif

        // Pop argument space values
        jit.addPtr(MacroAssembler::TrustedImmPtr(Wasm::JITLessJSEntrypointCallee::RegisterStackSpaceAligned), CCallHelpers::stackPointerRegister);

#if ASSERT_ENABLED
        for (int32_t i = 0; i < 30; ++i)
            jit.storePtr(MacroAssembler::TrustedImmPtr(0xbeef), CCallHelpers::Address { CCallHelpers::stackPointerRegister, -i * static_cast<int32_t>(sizeof(Register)) });
#endif

        {
            // Callee[cfr]
            CCallHelpers::Address functionSlot { GPRInfo::callFrameRegister, sizeof(CPURegister) * 2 + sizeof(Register) };
            jit.loadPtr(functionSlot, GPRInfo::regWS0);
#if USE(JSVALUE64)
            jit.andPtr(MacroAssembler::TrustedImmPtr(~(JSValue::NativeCalleeTag)), GPRInfo::regWS0);
#endif
        }

        auto configPlusLowestAddress = bitwise_cast<uint8_t*>(&WebConfig::g_config) + static_cast<ptrdiff_t>(WTF::startOffsetOfWTFConfig + WTF::offsetOfWTFConfigLowestAccessibleAddress);
        jit.move(MacroAssembler::TrustedImmPtr(configPlusLowestAddress), GPRInfo::regWS1);
        jit.loadPtr(CCallHelpers::Address { GPRInfo::regWS1, 0 }, GPRInfo::regWS1);
        jit.addPtr(GPRInfo::regWS1, GPRInfo::regWS0);

        // Store Callee's wasm callee

#if USE(JSVALUE64)
        jit.loadPtr(CCallHelpers::Address { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmCallee() }, GPRInfo::regWS1);
        jit.storePtr(GPRInfo::regWS1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 });
#else
        jit.loadPtr(CCallHelpers::Address { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmCallee() + PayloadOffset }, GPRInfo::regWS1);
        jit.storePtr(GPRInfo::regWS1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + PayloadOffset });
        jit.loadPtr(CCallHelpers::Address { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmCallee() + TagOffset }, GPRInfo::regWS1);
        jit.storePtr(GPRInfo::regWS1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + TagOffset });
#endif

        jit.loadPtr(CCallHelpers::Address { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfWasmFunctionPrologue() }, GPRInfo::regWS0);
        jit.call(GPRInfo::regWS0, WasmEntryPtrTag);

        // Restore SP

        {
            // Callee[cfr]
            CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, sizeof(CPURegister) * 2 + sizeof(Register) };
            jit.loadPtr(calleeSlot, GPRInfo::regWS0);

#if USE(JSVALUE64)
            jit.andPtr(MacroAssembler::TrustedImmPtr(~(JSValue::NativeCalleeTag)), GPRInfo::regWS0);
#endif
        }

        jit.move(MacroAssembler::TrustedImmPtr(configPlusLowestAddress), GPRInfo::regWS1);
        jit.loadPtr(CCallHelpers::Address { GPRInfo::regWS1, 0 }, GPRInfo::regWS1);
        jit.addPtr(GPRInfo::regWS1, GPRInfo::regWS0);

        jit.load32(CCallHelpers::Address { GPRInfo::regWS0, JITLessJSEntrypointCallee::offsetOfFrameSize() }, GPRInfo::regWS1);
        jit.subPtr(GPRInfo::callFrameRegister, GPRInfo::regWS1, GPRInfo::regWS1);
        jit.move(GPRInfo::regWS1, CCallHelpers::stackPointerRegister);
        jit.subPtr(MacroAssembler::TrustedImmPtr(JITLessJSEntrypointCallee::SpillStackSpaceAligned), CCallHelpers::stackPointerRegister);

        // Save return registers
#if CPU(ARM64) || CPU(ARM64E)
        jit.storePair64(GPRInfo::regWA0, GPRInfo::regWA1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 });
        jit.storePair64(GPRInfo::regWA2, GPRInfo::regWA3, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 2 * 8 });
        jit.storePair64(GPRInfo::regWA4, GPRInfo::regWA5, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 4 * 8 });
        jit.storePair64(GPRInfo::regWA6, GPRInfo::regWA7, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 6 * 8 });
#elif CPU(X86_64)
        jit.storePair64(GPRInfo::regWA0, GPRInfo::regWA1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 });
        jit.storePair64(GPRInfo::regWA2, GPRInfo::regWA3, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 2 * 8 });
        jit.storePair64(GPRInfo::regWA4, GPRInfo::regWA5, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 4 * 8 });
#elif USE(JSVALUE64)
        jit.store64(GPRInfo::regWA0, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 });
        jit.store64(GPRInfo::regWA1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 1 * 8 });
        jit.store64(GPRInfo::regWA2, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 2 * 8 });
        jit.store64(GPRInfo::regWA3, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 3 * 8 });
        jit.store64(GPRInfo::regWA4, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 4 * 8 });
        jit.store64(GPRInfo::regWA5, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 5 * 8 });
#else
        jit.storePair32(GPRInfo::regWA0, GPRInfo::regWA1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 0 * 8 });
        jit.storePair32(GPRInfo::regWA2, GPRInfo::regWA3, CCallHelpers::Address { CCallHelpers::stackPointerRegister, 1 * 8 });
#endif
#if CPU(ARM64) || CPU(ARM64E)
        jit.storePairDouble(FPRInfo::argumentFPR0, FPRInfo::argumentFPR1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  0) * 8 });
        jit.storePairDouble(FPRInfo::argumentFPR2, FPRInfo::argumentFPR3, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  2) * 8 });
        jit.storePairDouble(FPRInfo::argumentFPR4, FPRInfo::argumentFPR5, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  4) * 8 });
        jit.storePairDouble(FPRInfo::argumentFPR6, FPRInfo::argumentFPR7, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  6) * 8 });
#elif CPU(X86_64) || CPU(RISCV64)
        jit.storeDouble(FPRInfo::argumentFPR0, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  0) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR1, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  1) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR2, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  2) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR3, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  3) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR4, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  4) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR5, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  5) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR6, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  6) * 8 });
        jit.storeDouble(FPRInfo::argumentFPR7, CCallHelpers::Address { CCallHelpers::stackPointerRegister, (GPRInfo::numberOfArgumentRegisters +  7) * 8 });
#endif

        // Prepare frame
        jit.setupArguments<decltype(operationJSToWasmEntryWrapperBuildReturnFrame)>(CCallHelpers::stackPointerRegister, GPRInfo::callFrameRegister);
        jit.callOperation<OperationPtrTag>(operationJSToWasmEntryWrapperBuildReturnFrame);
        exceptionChecks.append(jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::returnValueGPR2));

        // restoreJSEntrypointInterpreterRegisters

#if CPU(ARM64) || CPU(ARM64E) || CPU(X86_64)
        jit.loadPair64(memBaseSlot, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
        jit.load64(wasmInstanceSlot, GPRInfo::wasmContextInstancePointer);
#else
        jit.load32(wasmInstanceSlot, GPRInfo::wasmContextInstancePointer);
#endif
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

        LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate()))
            RELEASE_ASSERT(false);
        thunk->get()->entrypoint.compilation = makeUnique<Compilation>(
            FINALIZE_WASM_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly interpreted entrypoint"),
            nullptr);
    });
    return thunk;
}

std::unique_ptr<InternalFunction> createJSToWasmWrapper(CCallHelpers& jit, JSEntrypointCallee& entryCallee, Callee* wasmCallee, const TypeDefinition& typeDefinition, Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, unsigned functionIndex)
{
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
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(typeDefinition);
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
        CallInformation jsFrameConvention = jsCallingConvention().callInformationFor(typeDefinition, CallRole::Callee);

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

        const auto& signature = *typeDefinition.as<FunctionSignature>();
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
    marshallJSResult(jit, typeDefinition, wasmFrameConvention, savedResultRegisters, exceptionChecks);

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

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
