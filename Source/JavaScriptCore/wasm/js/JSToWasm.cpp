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
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmOperations.h"
#include "WasmToJS.h"

namespace JSC { namespace Wasm {

void marshallJSResult(CCallHelpers& jit, const Signature& signature, const CallInformation& wasmFrameConvention, const RegisterAtOffsetList& savedResultRegisters)
{
    auto boxWasmResult = [](CCallHelpers& jit, Type type, Reg src, JSValueRegs dst) {
        switch (type.kind) {
        case TypeKind::Void:
            jit.moveTrustedValue(jsUndefined(), dst);
            break;
        case TypeKind::I32:
            jit.zeroExtend32ToWord(src.gpr(), dst.payloadGPR());
            jit.boxInt32(dst.payloadGPR(), dst, DoNotHaveTagRegisters);
            break;
        case TypeKind::F32:
            jit.convertFloatToDouble(src.fpr(), src.fpr());
            FALLTHROUGH;
        case TypeKind::F64: {
            jit.moveTrustedValue(jsNumber(pureNaN()), dst);
            auto isNaN = jit.branchIfNaN(src.fpr());
            jit.boxDouble(src.fpr(), dst, DoNotHaveTagRegisters);
            isNaN.link(&jit);
            break;
        }
        default: {
            if (isFuncref(type) || isExternref(type))
                jit.move(src.gpr(), dst.payloadGPR());
            else
                jit.breakpoint();
        }
        }
    };

    if (signature.returnsVoid())
        jit.moveTrustedValue(jsUndefined(), JSValueRegs { GPRInfo::returnValueGPR });
    else if (signature.returnCount() == 1) {
        if (signature.returnType(0).isI64()) {
            GPRReg inputGPR = wasmFrameConvention.results[0].reg().gpr();
            GPRReg wasmContextInstanceGPR = PinnedRegisterInfo::get().wasmContextInstancePointer;
            if (Context::useFastTLS()) {
                wasmContextInstanceGPR = inputGPR == GPRInfo::argumentGPR1 ? GPRInfo::argumentGPR0 : GPRInfo::argumentGPR1;
                static_assert(std::is_same_v<Wasm::Instance*, typename FunctionTraits<decltype(operationAllocateResultsArray)>::ArgumentType<1>>);
                jit.loadWasmContextInstance(wasmContextInstanceGPR);
            }
            jit.setupArguments<decltype(operationConvertToBigInt)>(wasmContextInstanceGPR, inputGPR);
            jit.callOperation(FunctionPtr<OperationPtrTag>(operationConvertToBigInt));
        } else
            boxWasmResult(jit, signature.returnType(0), wasmFrameConvention.results[0].reg(), JSValueRegs { GPRInfo::returnValueGPR });
    } else {
        IndexingType indexingType = ArrayWithUndecided;
        JSValueRegs scratch = JSValueRegs { wasmCallingConvention().prologueScratchGPRs[1] };
        // We can use the first floating point register as a scratch since it will always be moved onto the stack before other values.
        FPRReg fprScratch = wasmCallingConvention().fprArgs[0].fpr();
        bool hasI64 = false;
        for (unsigned i = 0; i < signature.returnCount(); ++i) {
            ValueLocation loc = wasmFrameConvention.results[i];
            Type type = signature.returnType(i);

            hasI64 |= type.isI64();
            if (loc.isReg()) {
                if (!type.isI64()) {
                    boxWasmResult(jit, signature.returnType(i), loc.reg(), scratch);
                    jit.storeValue(scratch, CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.reg())->offset() + wasmFrameConvention.headerAndArgumentStackSizeInBytes));
                } else
                    jit.storeValue(JSValueRegs { loc.reg().gpr() }, CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.reg())->offset() + wasmFrameConvention.headerAndArgumentStackSizeInBytes));
            } else {
                if (!type.isI64()) {
                    auto location = CCallHelpers::Address(CCallHelpers::stackPointerRegister, loc.offsetFromSP());
                    Reg tmp = (type.isF32() || type.isF64()) ? Reg(fprScratch) : Reg(scratch.gpr());
                    jit.load64ToReg(location, tmp);
                    boxWasmResult(jit, signature.returnType(i), tmp, scratch);
                    jit.storeValue(scratch, location);
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
                ValueLocation loc = wasmFrameConvention.results[i];
                Type type = signature.returnType(i);
                if (!type.isI64())
                    continue;

                GPRReg wasmContextInstanceGPR = PinnedRegisterInfo::get().wasmContextInstancePointer;
                if (Context::useFastTLS()) {
                    wasmContextInstanceGPR = GPRInfo::argumentGPR1;
                    static_assert(std::is_same_v<Wasm::Instance*, typename FunctionTraits<decltype(operationAllocateResultsArray)>::ArgumentType<1>>);
                    jit.loadWasmContextInstance(wasmContextInstanceGPR);
                }

                if (loc.isReg())
                    jit.load64(CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.reg())->offset() + wasmFrameConvention.headerAndArgumentStackSizeInBytes), GPRInfo::argumentGPR0);
                else {
                    auto location = CCallHelpers::Address(CCallHelpers::stackPointerRegister, loc.offsetFromSP());
                    jit.load64ToReg(location, GPRInfo::argumentGPR0);
                }
                jit.setupArguments<decltype(operationConvertToBigInt)>(wasmContextInstanceGPR, GPRInfo::argumentGPR0);
                jit.callOperation(FunctionPtr<OperationPtrTag>(operationConvertToBigInt));
                if (loc.isReg())
                    jit.storeValue(JSValueRegs { GPRInfo::returnValueGPR }, CCallHelpers::Address(CCallHelpers::stackPointerRegister, savedResultRegisters.find(loc.reg())->offset() + wasmFrameConvention.headerAndArgumentStackSizeInBytes));
                else {
                    auto location = CCallHelpers::Address(CCallHelpers::stackPointerRegister, loc.offsetFromSP());
                    jit.storeValue(JSValueRegs { GPRInfo::returnValueGPR }, location);
                }
            }
        }

        GPRReg wasmContextInstanceGPR = PinnedRegisterInfo::get().wasmContextInstancePointer;
        if (Context::useFastTLS()) {
            wasmContextInstanceGPR = GPRInfo::argumentGPR1;
            static_assert(std::is_same_v<Wasm::Instance*, typename FunctionTraits<decltype(operationAllocateResultsArray)>::ArgumentType<1>>);
            jit.loadWasmContextInstance(wasmContextInstanceGPR);
        }

        jit.setupArguments<decltype(operationAllocateResultsArray)>(wasmContextInstanceGPR, CCallHelpers::TrustedImmPtr(&signature), indexingType, CCallHelpers::stackPointerRegister);
        jit.callOperation(FunctionPtr<OperationPtrTag>(operationAllocateResultsArray));
    }
}

std::unique_ptr<InternalFunction> createJSToWasmWrapper(CCallHelpers& jit, const Signature& signature, Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, unsigned functionIndex)
{
    auto result = makeUnique<InternalFunction>();
    jit.emitFunctionPrologue();

    // FIXME Stop using 0 as codeBlocks. https://bugs.webkit.org/show_bug.cgi?id=165321
    jit.emitZeroToCallFrameHeader(CallFrameSlot::codeBlock);
    MacroAssembler::DataLabelPtr calleeMoveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), GPRInfo::nonPreservedNonReturnGPR);
    jit.emitPutToCallFrameHeader(GPRInfo::nonPreservedNonReturnGPR, CallFrameSlot::callee);
    Vector<CodeLocationDataLabelPtr<WasmEntryPtrTag>>* linkedCalleeMove = &result->calleeMoveLocations;
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        linkedCalleeMove->append(linkBuffer.locationOf<WasmEntryPtrTag>(calleeMoveLocation));
    });

    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
    RegisterSet toSave = pinnedRegs.toSave(mode);

#if ASSERT_ENABLED
    unsigned toSaveSize = toSave.numberOfSetGPRs();
    // They should all be callee saves.
    toSave.filter(RegisterSet::calleeSaveRegisters());
    ASSERT(toSave.numberOfSetGPRs() == toSaveSize);
#endif

    RegisterAtOffsetList registersToSpill(toSave, RegisterAtOffsetList::OffsetBaseType::FramePointerBased);
    result->entrypoint.calleeSaveRegisters = registersToSpill;

    size_t totalFrameSize = registersToSpill.size() * sizeof(CPURegister);
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature);
    RegisterAtOffsetList savedResultRegisters = wasmFrameConvention.computeResultsOffsetList();
    totalFrameSize += wasmFrameConvention.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.size() * sizeof(CPURegister);

    totalFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), totalFrameSize);
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);

    // We save all these registers regardless of having a memory or not.
    // The reason is that we use one of these as a scratch. That said,
    // almost all real wasm programs use memory, so it's not really
    // worth optimizing for the case that they don't.
    for (const RegisterAtOffset& regAtOffset : registersToSpill) {
        GPRReg reg = regAtOffset.reg().gpr();
        ptrdiff_t offset = regAtOffset.offset();
        jit.storePtr(reg, CCallHelpers::Address(GPRInfo::callFrameRegister, offset));
    }

    GPRReg wasmContextInstanceGPR = pinnedRegs.wasmContextInstancePointer;

    {
        CallInformation jsFrameConvention = jsCallingConvention().callInformationFor(signature, CallRole::Callee);

        CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, 0);

        // We're going to set the pinned registers after this. So
        // we can use this as a scratch for now since we saved it above.
        GPRReg scratchReg = pinnedRegs.baseMemoryPointer;

        if (!Context::useFastTLS()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, JSCallingConvention::instanceStackOffset), wasmContextInstanceGPR);
            jit.loadPtr(CCallHelpers::Address(wasmContextInstanceGPR, JSWebAssemblyInstance::offsetOfInstance()), wasmContextInstanceGPR);
        }

        for (unsigned i = 0; i < signature.argumentCount(); i++) {
            RELEASE_ASSERT(jsFrameConvention.params[i].isStack());

            Type type = signature.argument(i);
            CCallHelpers::Address jsParam(GPRInfo::callFrameRegister, jsFrameConvention.params[i].offsetFromFP());
            if (wasmFrameConvention.params[i].isStackArgument()) {
                if (type.isI32() || type.isF32()) {
                    jit.load32(jsParam, scratchReg);
                    jit.store32(scratchReg, calleeFrame.withOffset(wasmFrameConvention.params[i].offsetFromSP()));
                } else {
                    jit.load64(jsParam, scratchReg);
                    jit.store64(scratchReg, calleeFrame.withOffset(wasmFrameConvention.params[i].offsetFromSP()));
                }
            } else {
                if (type.isI32() || type.isF32())
                    jit.load32ToReg(jsParam, wasmFrameConvention.params[i].reg());
                else
                    jit.load64ToReg(jsParam, wasmFrameConvention.params[i].reg());
            }
        }
    }

    if (!!info.memory) {
        GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
        GPRReg size = wasmCallingConvention().prologueScratchGPRs[0];
        GPRReg scratch = wasmCallingConvention().prologueScratchGPRs[1];

        if (Context::useFastTLS())
            jit.loadWasmContextInstance(baseMemory);

        GPRReg currentInstanceGPR = Context::useFastTLS() ? baseMemory : wasmContextInstanceGPR;
        if (isARM64E()) {
            if (mode != Wasm::MemoryMode::Signaling)
                size = pinnedRegs.boundsCheckingSizeRegister;
            jit.loadPtr(CCallHelpers::Address(currentInstanceGPR, Wasm::Instance::offsetOfCachedBoundsCheckingSize()), size);
        } else {
            if (mode != Wasm::MemoryMode::Signaling)
                jit.loadPtr(CCallHelpers::Address(currentInstanceGPR, Wasm::Instance::offsetOfCachedBoundsCheckingSize()), pinnedRegs.boundsCheckingSizeRegister);
        }

        jit.loadPtr(CCallHelpers::Address(currentInstanceGPR, Wasm::Instance::offsetOfCachedMemory()), baseMemory);
        jit.cageConditionallyAndUntag(Gigacage::Primitive, baseMemory, size, scratch);
    }

    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
    unsigned functionIndexSpace = functionIndex + info.importFunctionCount();
    ASSERT(functionIndexSpace < info.functionIndexSpaceSize());
    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace });
    });

    marshallJSResult(jit, signature, wasmFrameConvention, savedResultRegisters);

    for (const RegisterAtOffset& regAtOffset : registersToSpill) {
        GPRReg reg = regAtOffset.reg().gpr();
        ASSERT(reg != GPRInfo::returnValueGPR);
        ptrdiff_t offset = regAtOffset.offset();
        jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, offset), reg);
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    return result;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
