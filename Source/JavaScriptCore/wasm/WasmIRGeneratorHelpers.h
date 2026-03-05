/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "WasmOperations.h"

namespace JSC { namespace Wasm {

static constexpr unsigned wasmInvalidCallSiteIndex = std::numeric_limits<unsigned>::max();

#if ENABLE(WEBASSEMBLY_OMGJIT)

class PatchpointExceptionHandle final : public RefCounted<PatchpointExceptionHandle> {
public:
    template <typename Generator>
    void collectStackMap(Generator* generator, const B3::StackmapGenerationParams& params) const
    {
        if (!m_hasExceptionHandlers)
            return;
        if (!m_numLiveValues)
            return;

        StackMap values(*m_numLiveValues);
        for (unsigned i = 0; i < *m_numLiveValues; ++i)
            values[i] = OSREntryValue(params[i + m_firstStackmapParamOffset], params.value()->child(i + m_firstStackmapChildOffset)->type());

        generator->addStackMap(m_callSiteIndex, WTF::move(values));
    }

    static Ref<PatchpointExceptionHandle> create(bool hasExceptionHandlers, unsigned callSiteIndex, unsigned numLiveValues, unsigned firstStackmapParamOffset, unsigned firstStackmapChildOffset)
    {
        return adoptRef(*new PatchpointExceptionHandle(hasExceptionHandlers, callSiteIndex, numLiveValues, firstStackmapParamOffset, firstStackmapChildOffset));
    }

private:
    PatchpointExceptionHandle(bool hasExceptionHandlers, unsigned callSiteIndex, unsigned numLiveValues, unsigned firstStackmapParamOffset, unsigned firstStackmapChildOffset)
        : m_hasExceptionHandlers(hasExceptionHandlers)
        , m_callSiteIndex(callSiteIndex)
        , m_numLiveValues(numLiveValues)
        , m_firstStackmapParamOffset(firstStackmapParamOffset)
        , m_firstStackmapChildOffset(firstStackmapChildOffset)
    { }

    bool m_hasExceptionHandlers;
    unsigned m_callSiteIndex { wasmInvalidCallSiteIndex };
    std::optional<unsigned> m_numLiveValues { };
    unsigned m_firstStackmapParamOffset { };
    unsigned m_firstStackmapChildOffset { };
};

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

static inline void emitThrowRefImpl(CCallHelpers& jit)
{
    // JSWebAssemblyInstance in argumentGPR0
    // exception pointer in argumentGPR1

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, JSWebAssemblyInstance::offsetOfVM()), scratch);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    jit.callOperation<OperationPtrTag>(operationWasmRethrow);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
}

static inline void emitThrowImpl(CCallHelpers& jit, unsigned exceptionIndex)
{
    JIT_COMMENT(jit, "throw impl, index: ", exceptionIndex);
    // JSWebAssemblyInstance in argumentGPR0
    // arguments to the exception off of stack pointer

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, JSWebAssemblyInstance::offsetOfVM()), scratch);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.move(MacroAssembler::TrustedImm32(exceptionIndex), GPRInfo::argumentGPR1);
    jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR2);
    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    jit.callOperation<OperationPtrTag>(operationWasmThrow);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
}

#if ENABLE(WEBASSEMBLY_OMGJIT)
static inline void SYSV_ABI buildEntryBufferForCatch(Probe::Context& context)
{
    CallFrame* callFrame = context.fp<CallFrame*>();
    CallSiteIndex callSiteIndex = callFrame->callSiteIndex();
    OptimizingJITCallee* callee = uncheckedDowncast<OptimizingJITCallee>(uncheckedDowncast<Wasm::Callee>(callFrame->callee().asNativeCallee()));
    JSWebAssemblyInstance* instance = callFrame->wasmInstance();
    const StackMap& stackmap = callee->stackmap(callSiteIndex);
    EncodedJSValue exception = context.gpr<EncodedJSValue>(GPRInfo::returnValueGPR);
    auto* buffer = instance->vm().wasmContext.scratchBufferForSize(stackmap.size());
    loadValuesIntoBuffer(context, stackmap, buffer);

    JSValue thrownValue = JSValue::decode(exception);
    void* payload = nullptr;
    if (JSWebAssemblyException* wasmException = jsDynamicCast<JSWebAssemblyException*>(thrownValue))
        payload = std::bit_cast<void*>(wasmException->payload().span().data());

    context.gpr(GPRInfo::argumentGPR0) = std::bit_cast<uintptr_t>(buffer);
    context.gpr(GPRInfo::argumentGPR1) = exception;
    context.gpr(GPRInfo::argumentGPR2) = std::bit_cast<uintptr_t>(payload);

    context.gpr(GPRInfo::wasmContextInstancePointer) = std::bit_cast<uintptr_t>(instance);
    if (instance->moduleInformation().memoryCount()) {
        context.gpr(GPRInfo::wasmBaseMemoryPointer) = std::bit_cast<uintptr_t>(instance->cachedMemory());
        context.gpr(GPRInfo::wasmBoundsCheckingSizeRegister) = instance->cachedBoundsCheckingSize();
    }
}

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

#endif // ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
