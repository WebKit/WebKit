/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyRuntimeError.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmSignatureInlines.h"
#include "WasmToJS.h"

namespace JSC { namespace Wasm {

std::unique_ptr<InternalFunction> createJSToWasmWrapper(CompilationContext& compilationContext, const Signature& signature, Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, unsigned functionIndex)
{
    CCallHelpers& jit = *compilationContext.embedderEntrypointJIT;

    auto result = std::make_unique<InternalFunction>();
    jit.emitFunctionPrologue();

    // FIXME Stop using 0 as codeBlocks. https://bugs.webkit.org/show_bug.cgi?id=165321
    jit.store64(CCallHelpers::TrustedImm64(0), CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * static_cast<int>(sizeof(Register))));
    MacroAssembler::DataLabelPtr calleeMoveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), GPRInfo::nonPreservedNonReturnGPR);
    jit.storePtr(GPRInfo::nonPreservedNonReturnGPR, CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    CodeLocationDataLabelPtr<WasmEntryPtrTag>* linkedCalleeMove = &result->calleeMoveLocation;
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        *linkedCalleeMove = linkBuffer.locationOf<WasmEntryPtrTag>(calleeMoveLocation);
    });

    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
    RegisterSet toSave = pinnedRegs.toSave(mode);

#if !ASSERT_DISABLED
    unsigned toSaveSize = toSave.numberOfSetGPRs();
    // They should all be callee saves.
    toSave.filter(RegisterSet::calleeSaveRegisters());
    ASSERT(toSave.numberOfSetGPRs() == toSaveSize);
#endif

    RegisterAtOffsetList registersToSpill(toSave, RegisterAtOffsetList::OffsetBaseType::FramePointerBased);
    result->entrypoint.calleeSaveRegisters = registersToSpill;

    unsigned totalFrameSize = registersToSpill.size() * sizeof(void*);
    totalFrameSize += WasmCallingConvention::headerSizeInBytes();
    totalFrameSize -= sizeof(CallerFrameAndPC);
    unsigned numGPRs = 0;
    unsigned numFPRs = 0;
    bool argumentsIncludeI64 = false;
    for (unsigned i = 0; i < signature.argumentCount(); i++) {
        switch (signature.argument(i)) {
        case Wasm::I64:
            argumentsIncludeI64 = true;
            FALLTHROUGH;
        case Wasm::I32:
            if (numGPRs >= wasmCallingConvention().m_gprArgs.size())
                totalFrameSize += sizeof(void*);
            ++numGPRs;
            break;
        case Wasm::F32:
        case Wasm::F64:
            if (numFPRs >= wasmCallingConvention().m_fprArgs.size())
                totalFrameSize += sizeof(void*);
            ++numFPRs;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

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

    if (argumentsIncludeI64 || signature.returnType() == Wasm::I64) {
        if (Context::useFastTLS())
            jit.loadWasmContextInstance(GPRInfo::argumentGPR2);
        else {
            // vmEntryToWasm passes the JSWebAssemblyInstance corresponding to Wasm::Context*'s
            // instance as the first JS argument when we're not using fast TLS to hold the
            // Wasm::Context*'s instance.
            jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::thisArgument * sizeof(EncodedJSValue)), GPRInfo::argumentGPR2);
            jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, JSWebAssemblyInstance::offsetOfPoisonedInstance()), GPRInfo::argumentGPR2);
            jit.move(CCallHelpers::TrustedImm64(JSWebAssemblyInstancePoison::key()), GPRInfo::argumentGPR0);
            jit.xor64(GPRInfo::argumentGPR0, GPRInfo::argumentGPR2);
        }

        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, Instance::offsetOfPointerToTopEntryFrame()), GPRInfo::argumentGPR0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0), GPRInfo::argumentGPR0);
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
        jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        jit.move(CCallHelpers::TrustedImm32(static_cast<int32_t>(argumentsIncludeI64 ? ExceptionType::I64ArgumentType : ExceptionType::I64ReturnType)), GPRInfo::argumentGPR1);

        CCallHelpers::Call call = jit.call(OperationPtrTag);

        jit.jump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
        jit.breakpoint(); // We should not reach this.

        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(call, FunctionPtr<OperationPtrTag>(wasmToJSException));
        });
        return result;
    }

    GPRReg wasmContextInstanceGPR = pinnedRegs.wasmContextInstancePointer;

    {
        CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));
        numGPRs = 0;
        numFPRs = 0;
        // We're going to set the pinned registers after this. So
        // we can use this as a scratch for now since we saved it above.
        GPRReg scratchReg = pinnedRegs.baseMemoryPointer;

        ptrdiff_t jsOffset = CallFrameSlot::thisArgument * sizeof(EncodedJSValue);

        // vmEntryToWasm passes the JSWebAssemblyInstance corresponding to Wasm::Context*'s
        // instance as the first JS argument when we're not using fast TLS to hold the
        // Wasm::Context*'s instance.
        if (!Context::useFastTLS()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmContextInstanceGPR);
            jit.loadPtr(CCallHelpers::Address(wasmContextInstanceGPR, JSWebAssemblyInstance::offsetOfPoisonedInstance()), wasmContextInstanceGPR);
            jit.move(CCallHelpers::TrustedImm64(JSWebAssemblyInstancePoison::key()), scratchReg);
            jit.xor64(scratchReg, wasmContextInstanceGPR);
            jsOffset += sizeof(EncodedJSValue);
        }

        ptrdiff_t wasmOffset = CallFrame::headerSizeInRegisters * sizeof(void*);
        for (unsigned i = 0; i < signature.argumentCount(); i++) {
            switch (signature.argument(i)) {
            case Wasm::I32:
            case Wasm::I64:
                if (numGPRs >= wasmCallingConvention().m_gprArgs.size()) {
                    if (signature.argument(i) == Wasm::I32) {
                        jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store32(scratchReg, calleeFrame.withOffset(wasmOffset));
                    } else {
                        jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store64(scratchReg, calleeFrame.withOffset(wasmOffset));
                    }
                    wasmOffset += sizeof(void*);
                } else {
                    if (signature.argument(i) == Wasm::I32)
                        jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_gprArgs[numGPRs].gpr());
                    else
                        jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_gprArgs[numGPRs].gpr());
                }
                ++numGPRs;
                break;
            case Wasm::F32:
            case Wasm::F64:
                if (numFPRs >= wasmCallingConvention().m_fprArgs.size()) {
                    if (signature.argument(i) == Wasm::F32) {
                        jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store32(scratchReg, calleeFrame.withOffset(wasmOffset));
                    } else {
                        jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store64(scratchReg, calleeFrame.withOffset(wasmOffset));
                    }
                    wasmOffset += sizeof(void*);
                } else {
                    if (signature.argument(i) == Wasm::F32)
                        jit.loadFloat(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_fprArgs[numFPRs].fpr());
                    else
                        jit.loadDouble(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_fprArgs[numFPRs].fpr());
                }
                ++numFPRs;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            jsOffset += sizeof(EncodedJSValue);
        }
    }

    if (!!info.memory) {
        GPRReg baseMemory = pinnedRegs.baseMemoryPointer;

        if (Context::useFastTLS())
            jit.loadWasmContextInstance(baseMemory);

        GPRReg currentInstanceGPR = Context::useFastTLS() ? baseMemory : wasmContextInstanceGPR;
        if (mode != MemoryMode::Signaling) {
            const auto& sizeRegs = pinnedRegs.sizeRegisters;
            ASSERT(sizeRegs.size() >= 1);
            ASSERT(!sizeRegs[0].sizeOffset); // The following code assumes we start at 0, and calculates subsequent size registers relative to 0.
            jit.loadPtr(CCallHelpers::Address(currentInstanceGPR, Wasm::Instance::offsetOfCachedMemorySize()), sizeRegs[0].sizeRegister);
            for (unsigned i = 1; i < sizeRegs.size(); ++i)
                jit.add64(CCallHelpers::TrustedImm32(-sizeRegs[i].sizeOffset), sizeRegs[0].sizeRegister, sizeRegs[i].sizeRegister);
        }

        jit.loadPtr(CCallHelpers::Address(currentInstanceGPR, Wasm::Instance::offsetOfCachedMemory()), baseMemory);
    }

    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
    unsigned functionIndexSpace = functionIndex + info.importFunctionCount();
    ASSERT(functionIndexSpace < info.functionIndexSpaceSize());
    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndexSpace });
    });

    for (const RegisterAtOffset& regAtOffset : registersToSpill) {
        GPRReg reg = regAtOffset.reg().gpr();
        ASSERT(reg != GPRInfo::returnValueGPR);
        ptrdiff_t offset = regAtOffset.offset();
        jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, offset), reg);
    }

    switch (signature.returnType()) {
    case Wasm::Void:
        jit.moveTrustedValue(jsUndefined(), JSValueRegs { GPRInfo::returnValueGPR });
        break;
    case Wasm::I32:
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        jit.boxInt32(GPRInfo::returnValueGPR, JSValueRegs { GPRInfo::returnValueGPR }, DoNotHaveTagRegisters);
        break;
    case Wasm::F32:
        jit.convertFloatToDouble(FPRInfo::returnValueFPR, FPRInfo::returnValueFPR);
        FALLTHROUGH;
    case Wasm::F64: {
        jit.moveTrustedValue(jsNumber(pureNaN()), JSValueRegs { GPRInfo::returnValueGPR });
        auto isNaN = jit.branchIfNaN(FPRInfo::returnValueFPR);
        jit.boxDouble(FPRInfo::returnValueFPR, JSValueRegs { GPRInfo::returnValueGPR }, DoNotHaveTagRegisters);
        isNaN.link(&jit);
        break;
    }
    case Wasm::I64:
    case Wasm::Func:
    case Wasm::Anyfunc:
        jit.breakpoint();
        break;
    default:
        break;
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    return result;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
