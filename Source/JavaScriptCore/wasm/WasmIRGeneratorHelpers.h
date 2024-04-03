/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)

#include "AirCode.h"
#include "B3StackmapGenerationParams.h"
#include "B3StackmapValue.h"
#include "CCallHelpers.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "ProbeContext.h"
#include "WasmInstance.h"
#include "WasmOperations.h"

namespace JSC { namespace Wasm {

struct PatchpointExceptionHandleBase {
    static constexpr unsigned s_invalidCallSiteIndex = std::numeric_limits<unsigned>::max();
};

#if ENABLE(WEBASSEMBLY_OMGJIT)
struct PatchpointExceptionHandle : public PatchpointExceptionHandleBase {
    PatchpointExceptionHandle(std::optional<bool> hasExceptionHandlers)
        : m_hasExceptionHandlers(hasExceptionHandlers)
    { }

    PatchpointExceptionHandle(std::optional<bool> hasExceptionHandlers, unsigned callSiteIndex, unsigned numLiveValues)
        : m_hasExceptionHandlers(hasExceptionHandlers)
        , m_callSiteIndex(callSiteIndex)
        , m_numLiveValues(numLiveValues)
    { }

    template <typename Generator>
    void generate(CCallHelpers& jit, const B3::StackmapGenerationParams& params, Generator* generator) const
    {
        if (m_callSiteIndex == s_invalidCallSiteIndex) {
            if (!m_hasExceptionHandlers || m_hasExceptionHandlers.value())
                jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
            return;
        }

        StackMap values(m_numLiveValues);
        unsigned paramsOffset = params.size() - m_numLiveValues;
        unsigned childrenOffset = params.value()->numChildren() - m_numLiveValues;
        for (unsigned i = 0; i < m_numLiveValues; ++i)
            values[i] = OSREntryValue(params[i + paramsOffset], params.value()->child(i + childrenOffset)->type());

        generator->addStackMap(m_callSiteIndex, WTFMove(values));
        JIT_COMMENT(jit, "Store call site index ", m_callSiteIndex, " at throw or call site.");
        jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    }

    std::optional<bool> m_hasExceptionHandlers;
    unsigned m_callSiteIndex { s_invalidCallSiteIndex };
    unsigned m_numLiveValues;
};
#else

using PatchpointExceptionHandle = PatchpointExceptionHandleBase;

#endif


static inline void computeExceptionHandlerAndLoopEntrypointLocations(Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>& handlers, Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints, const InternalFunction* function, const CompilationContext& context, LinkBuffer& linkBuffer)
{
    if (!context.procedure) {
        ASSERT(Options::useBBQJIT());

        for (auto label : function->bbqLoopEntrypoints)
            loopEntrypoints.append(linkBuffer.locationOf<WasmEntryPtrTag>(label));

        unsigned index = 0;
        for (const UnlinkedHandlerInfo& handlerInfo : function->exceptionHandlers) {
            if (handlerInfo.m_type == HandlerType::Delegate) {
                handlers.append({ });
                continue;
            }
            handlers.append(linkBuffer.locationOf<ExceptionHandlerPtrTag>(context.catchEntrypoints[index++]));
        }
        return;
    }

#if ENABLE(WEBASSEMBLY_OMGJIT)

    unsigned entrypointIndex = 1;
    unsigned numEntrypoints = context.procedure->numEntrypoints();
    for (const UnlinkedHandlerInfo& handlerInfo : function->exceptionHandlers) {
        if (handlerInfo.m_type == HandlerType::Delegate) {
            handlers.append({ });
            continue;
        }

        RELEASE_ASSERT(entrypointIndex < numEntrypoints);
        handlers.append(linkBuffer.locationOf<ExceptionHandlerPtrTag>(context.procedure->code().entrypointLabel(entrypointIndex)));
        ++entrypointIndex;
    }

    for (; entrypointIndex < numEntrypoints; ++entrypointIndex)
        loopEntrypoints.append(linkBuffer.locationOf<WasmEntryPtrTag>(context.procedure->code().entrypointLabel(entrypointIndex)));
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif

}

static inline void computeExceptionHandlerLocations(Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>& handlers, const InternalFunction* function, const CompilationContext& context, LinkBuffer& linkBuffer)
{
    Vector<CodeLocationLabel<WasmEntryPtrTag>> ignored;
    computeExceptionHandlerAndLoopEntrypointLocations(handlers, ignored, function, context, linkBuffer);
}

static inline void emitRethrowImpl(CCallHelpers& jit)
{
    // Instance in argumentGPR0
    // exception pointer in argumentGPR1

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfVM()), scratch);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    jit.callOperation<OperationPtrTag>(operationWasmRethrow);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
}

static inline void emitThrowImpl(CCallHelpers& jit, unsigned exceptionIndex)
{
    JIT_COMMENT(jit, "throw impl, index: ", exceptionIndex);
    // Instance in argumentGPR0
    // arguments to the exception off of stack pointer

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfVM()), scratch);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.move(MacroAssembler::TrustedImm32(exceptionIndex), GPRInfo::argumentGPR1);
    jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR2);
    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    jit.callOperation<OperationPtrTag>(operationWasmThrow);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
}

#if ENABLE(WEBASSEMBLY_OMGJIT)
template<SavedFPWidth savedFPWidth>
static inline void buildEntryBufferForCatch(Probe::Context& context)
{
    unsigned valueSize = (savedFPWidth == SavedFPWidth::SaveVectors) ? 2 : 1;
    CallFrame* callFrame = context.fp<CallFrame*>();
    CallSiteIndex callSiteIndex = callFrame->callSiteIndex();
    OptimizingJITCallee* callee = bitwise_cast<OptimizingJITCallee*>(callFrame->callee().asNativeCallee());
    const StackMap& stackmap = callee->stackmap(callSiteIndex);
    Instance* instance = context.gpr<Instance*>(GPRInfo::wasmContextInstancePointer);
    EncodedJSValue exception = context.gpr<EncodedJSValue>(GPRInfo::returnValueGPR);
    uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(stackmap.size() * valueSize * 8);
    loadValuesIntoBuffer(context, stackmap, buffer, savedFPWidth);

    JSValue thrownValue = JSValue::decode(exception);
    void* payload = nullptr;
    if (JSWebAssemblyException* wasmException = jsDynamicCast<JSWebAssemblyException*>(thrownValue))
        payload = bitwise_cast<void*>(wasmException->payload().data());

    context.gpr(GPRInfo::argumentGPR0) = bitwise_cast<uintptr_t>(buffer);
    context.gpr(GPRInfo::argumentGPR1) = exception;
    context.gpr(GPRInfo::argumentGPR2) = bitwise_cast<uintptr_t>(payload);
}

static inline void buildEntryBufferForCatchSIMD(Probe::Context& context) { buildEntryBufferForCatch<SavedFPWidth::SaveVectors>(context); }
static inline void buildEntryBufferForCatchNoSIMD(Probe::Context& context) { buildEntryBufferForCatch<SavedFPWidth::DontSaveVectors>(context); }


static inline void prepareForTailCall(CCallHelpers& jit, const B3::StackmapGenerationParams& params, const Checked<int32_t>& tailCallStackOffsetFromFP)
{
    Checked<int32_t> frameSize = params.code().frameSize();
    Checked<int32_t> newStackOffset = frameSize + tailCallStackOffsetFromFP;

    RegisterAtOffsetList calleeSaves = params.code().calleeSaveRegisterAtOffsetList();

    // We will use sp-based offsets since the frame pointer is already pointing to the previous frame.
    calleeSaves.adjustOffsets(frameSize);
    jit.emitRestore(calleeSaves, MacroAssembler::stackPointerRegister);

    // The return PC was saved on the stack in the tail call patchpoint.
#if CPU(X86_64)
    newStackOffset -= Checked<int32_t>(sizeof(Register));
#elif CPU(ARM) || CPU(ARM64) || CPU(RISCV64)
    jit.loadPtr(CCallHelpers::Address(MacroAssembler::stackPointerRegister, newStackOffset - Checked<int32_t>(sizeof(Register))), MacroAssembler::linkRegister);
#if CPU(ARM64E)
    GPRReg callerSP = jit.scratchRegister();
    jit.addPtr(MacroAssembler::TrustedImm32(frameSize + Checked<int32_t>(sizeof(CallerFrameAndPC))), MacroAssembler::stackPointerRegister, callerSP);
    jit.untagPtr(callerSP, MacroAssembler::linkRegister);
    jit.validateUntaggedPtr(MacroAssembler::linkRegister);
#endif
#else
    UNREACHABLE_FOR_PLATFORM();
#endif

    jit.addPtr(MacroAssembler::TrustedImm32(newStackOffset), MacroAssembler::stackPointerRegister);
}

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
