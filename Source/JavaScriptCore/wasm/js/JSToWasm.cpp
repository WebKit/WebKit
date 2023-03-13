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
#include "JSToWasm.h"

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmOperations.h"
#include "WasmToJS.h"

namespace JSC { namespace Wasm {

void marshallJSResult(CCallHelpers& jit, const TypeDefinition& typeDefinition, const CallInformation& wasmFrameConvention, const RegisterAtOffsetList& savedResultRegisters)
{
    const auto& signature = *typeDefinition.as<FunctionSignature>();
    auto boxWasmResult = [](CCallHelpers& jit, Type type, ValueLocation src, JSValueRegs dst) {
        JIT_COMMENT(jit, "boxWasmResult ", type);
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
            jit.moveTrustedValue(jsNumber(pureNaN()), dst);
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
            jit.callOperation(operationConvertToBigInt);
        } else
            boxWasmResult(jit, signature.returnType(0), wasmFrameConvention.results[0].location, JSRInfo::returnValueJSR);
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
                    boxWasmResult(jit, type, loc, scratchJSR);
                    jit.storeValue(scratchJSR, address.withOffset(savedResultRegisters.find(loc.fpr())->offset()));
                    break;
                case TypeKind::I64:
                    jit.storeValue(loc.jsr(), address.withOffset(savedResultRegisters.find(loc.jsr().payloadGPR())->offset()));
                    break;
                default:
                    boxWasmResult(jit, type, loc, scratchJSR);
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
                    boxWasmResult(jit, type, tmp, scratchJSR);
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
                jit.callOperation(operationConvertToBigInt);
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
        jit.callOperation(operationAllocateResultsArray);
        if constexpr (!!maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);

        jit.boxCell(GPRInfo::returnValueGPR, JSRInfo::returnValueJSR);
    }
}

std::unique_ptr<InternalFunction> createJSToWasmWrapper(CCallHelpers& jit, Callee& callee, const TypeDefinition& typeDefinition, Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, unsigned functionIndex)
{
    JIT_COMMENT(jit, "jsToWasm wrapper for wasm-function[", functionIndex, "] : ", typeDefinition);
    auto result = makeUnique<InternalFunction>();
    jit.emitFunctionPrologue();

    // |codeBlock| and |this| slots are already initialized by the caller of this function because it is JS->Wasm transition.
    jit.move(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(&callee)), GPRInfo::nonPreservedNonReturnGPR);
    CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
    jit.storePtr(GPRInfo::nonPreservedNonReturnGPR, calleeSlot.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
    jit.store32(CCallHelpers::TrustedImm32(JSValue::WasmTag), calleeSlot.withOffset(TagOffset));
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

    totalFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), totalFrameSize);
    JIT_COMMENT(jit, "Saved result registers: ", savedResultRegisters);
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);

    // We save all these registers regardless of having a memory or not.
    // The reason is that we use one of these as a scratch. That said,
    // almost all real wasm programs use memory, so it's not really
    // worth optimizing for the case that they don't.
    jit.emitSave(registersToSpill);

    // https://webassembly.github.io/spec/js-api/index.html#exported-function-exotic-objects
    // If parameters or results contain v128, throw a TypeError.
    // Note: the above error is thrown each time the [[Call]] method is invoked.
    if (Options::useWebAssemblySIMD() && (wasmFrameConvention.argumentsOrResultsIncludeV128)) {
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
            jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(Wasm::Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, size);
        } else {
            if (mode == MemoryMode::BoundsChecking)
                jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(Wasm::Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
            else
                jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, Wasm::Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer);
        }
        jit.cageConditionallyAndUntag(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, size, scratch, /* validateAuth */ true, /* mayBeNull */ false);
    }
#endif

    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
    unsigned functionIndexSpace = functionIndex + info.importFunctionCount();
    ASSERT(functionIndexSpace < info.functionIndexSpaceSize());
    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace });
    });

    // Restore stack pointer after call
    jit.addPtr(MacroAssembler::TrustedImm32(-static_cast<int32_t>(totalFrameSize)), MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);

    JIT_COMMENT(jit, "marshallJSResult");
    marshallJSResult(jit, typeDefinition, wasmFrameConvention, savedResultRegisters);

    JIT_COMMENT(jit, "restore registersToSpill");
    jit.emitRestore(registersToSpill);
    jit.emitFunctionEpilogue();
    jit.ret();

    return result;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
