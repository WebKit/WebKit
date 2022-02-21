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

#if ENABLE(WEBASSEMBLY_B3JIT)

#include "AirCode.h"
#include "B3StackmapGenerationParams.h"
#include "B3StackmapValue.h"
#include "CCallHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "ProbeContext.h"
#include "WasmInstance.h"
#include "WasmOperations.h"

namespace JSC { namespace Wasm {

struct PatchpointExceptionHandle {
    template <typename Generator>
    void generate(CCallHelpers& jit, const B3::StackmapGenerationParams& params, Generator* generator) const
    {
        if (m_callSiteIndex == s_invalidCallSiteIndex)
            return;

        StackMap values(m_numLiveValues);
        unsigned paramsOffset = params.size() - m_numLiveValues;
        unsigned childrenOffset = params.value()->numChildren() - m_numLiveValues;
        for (unsigned i = 0; i < m_numLiveValues; ++i)
            values[i] = OSREntryValue(params[i + paramsOffset], params.value()->child(i + childrenOffset)->type());

        generator->addStackMap(m_callSiteIndex, WTFMove(values));
        jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    }

    static constexpr unsigned s_invalidCallSiteIndex = std::numeric_limits<unsigned>::max();

    unsigned m_callSiteIndex { s_invalidCallSiteIndex };
    unsigned m_numLiveValues;
};


static inline void computeExceptionHandlerAndLoopEntrypointLocations(Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>& handlers, Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints, const InternalFunction* function, const CompilationContext& context, LinkBuffer& linkBuffer)
{
    if (!context.procedure)
        return;

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
}

static inline void computeExceptionHandlerLocations(Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>& handlers, const InternalFunction* function, const CompilationContext& context, LinkBuffer& linkBuffer)
{
    Vector<CodeLocationLabel<WasmEntryPtrTag>> ignored;
    computeExceptionHandlerAndLoopEntrypointLocations(handlers, ignored, function, context, linkBuffer);
}

static inline void emitRethrowImpl(CCallHelpers& jit)
{
    // Instance in argumentGPR0
    // frame pointer in argumentGPR1
    // exception pointer in argumentGPR2

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfOwner()), scratch);
    {
        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, scratch, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), scratch);
        jit.loadPtr(CCallHelpers::Address(scratch, MarkedBlock::offsetOfFooter + MarkedBlock::Footer::offsetOfVM()), scratch);
        auto loadedCase = jit.jump();

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(scratch, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), scratch);

        loadedCase.link(&jit);
    }
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
        linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationWasmRethrow));
    });
}

static inline void emitThrowImpl(CCallHelpers& jit, unsigned exceptionIndex)
{
    // Instance in argumentGPR0
    // frame pointer in argumentGPR1
    // arguments to the exception off of stack pointer

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfOwner()), scratch);
    {
        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, scratch, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), scratch);
        jit.loadPtr(CCallHelpers::Address(scratch, MarkedBlock::offsetOfFooter + MarkedBlock::Footer::offsetOfVM()), scratch);
        auto loadedCase = jit.jump();

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(scratch, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), scratch);

        loadedCase.link(&jit);
    }
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.move(MacroAssembler::TrustedImm32(exceptionIndex), GPRInfo::argumentGPR2);
    jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR3);
    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
        linkBuffer.link(call, FunctionPtr<OperationPtrTag>(operationWasmThrow));
    });
}

static inline void buildEntryBufferForCatch(Probe::Context& context)
{
    CallFrame* callFrame = context.fp<CallFrame*>();
    CallSiteIndex callSiteIndex = callFrame->callSiteIndex();
    OptimizingJITCallee* callee = bitwise_cast<OptimizingJITCallee*>(callFrame->callee().asWasmCallee());
    const StackMap& stackmap = callee->stackmap(callSiteIndex);
    VM* vm = context.gpr<VM*>(GPRInfo::regT0);
    uint64_t* buffer = vm->wasmContext.scratchBufferForSize(stackmap.size() * 8);
    loadValuesIntoBuffer(context, stackmap, buffer);

    context.gpr(GPRInfo::argumentGPR0) = bitwise_cast<uintptr_t>(buffer);
}

static inline void emitCatchPrologueShared(B3::Air::Code& code, CCallHelpers& jit)
{
    jit.emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, GPRInfo::regT0);
    {
        // FIXME: Handling precise allocations in WasmB3IRGenerator catch entrypoints might be unnecessary
        // https://bugs.webkit.org/show_bug.cgi?id=231213
        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT0, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), GPRInfo::regT0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, MarkedBlock::offsetOfFooter + MarkedBlock::Footer::offsetOfVM()), GPRInfo::regT0);
        auto loadedCase = jit.jump();

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), GPRInfo::regT0);

        loadedCase.link(&jit);
    }
    jit.restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(GPRInfo::regT0, GPRInfo::regT3);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, VM::calleeForWasmCatchOffset()), GPRInfo::regT3);
    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::Address(GPRInfo::regT0, VM::calleeForWasmCatchOffset()));
    jit.emitPutToCallFrameHeader(GPRInfo::regT3, CallFrameSlot::callee);

    jit.load64(CCallHelpers::Address(GPRInfo::regT0, VM::callFrameForCatchOffset()), GPRInfo::callFrameRegister);
    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::Address(GPRInfo::regT0, VM::callFrameForCatchOffset()));

    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::thisArgument * sizeof(Register)), GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT3, JSWebAssemblyInstance::offsetOfInstance()), GPRInfo::regT3);
    jit.storeWasmContextInstance(GPRInfo::regT3);

    jit.probe(tagCFunction<JITProbePtrTag>(buildEntryBufferForCatch), nullptr);

    jit.addPtr(CCallHelpers::TrustedImm32(-code.frameSize()), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
