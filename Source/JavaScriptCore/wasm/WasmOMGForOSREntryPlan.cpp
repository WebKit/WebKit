/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WasmOMGForOSREntryPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "B3OpaqueByproducts.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "WasmB3IRGenerator.h"
#include "WasmCallee.h"
#include "WasmContext.h"
#include "WasmInstance.h"
#include "WasmMachineThreads.h"
#include "WasmMemory.h"
#include "WasmNameSection.h"
#include "WasmSignatureInlines.h"
#include "WasmValidate.h"
#include "WasmWorklist.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadMessage.h>

namespace JSC { namespace Wasm {

namespace WasmOMGForOSREntryPlanInternal {
static constexpr bool verbose = false;
}

OMGForOSREntryPlan::OMGForOSREntryPlan(Context* context, Ref<Module>&& module, Ref<BBQCallee>&& callee, uint32_t functionIndex, uint32_t loopIndex, CodeBlock& codeBlock, CompletionTask&& task)
    : Base(context, makeRef(const_cast<ModuleInformation&>(module->moduleInformation())), WTFMove(task))
    , m_module(WTFMove(module))
    , m_codeBlock(codeBlock)
    , m_callee(WTFMove(callee))
    , m_functionIndex(functionIndex)
    , m_loopIndex(loopIndex)
{
    setMode(codeBlock.mode());
    ASSERT(m_codeBlock->runnable());
    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(m_mode));
    dataLogLnIf(WasmOMGForOSREntryPlanInternal::verbose, "Starting OMGForOSREntry plan for ", functionIndex, " of module: ", RawPointer(&m_module.get()));
}

void OMGForOSREntryPlan::work(CompilationEffort)
{
    ASSERT(m_codeBlock->runnable());
    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(mode()));
    const FunctionData& function = m_moduleInformation->functions[m_functionIndex];

    const uint32_t functionIndexSpace = m_functionIndex + m_module->moduleInformation().importFunctionCount();
    ASSERT(functionIndexSpace < m_module->moduleInformation().functionIndexSpaceSize());

    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[m_functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);
    ASSERT(validateFunction(function, signature, m_moduleInformation.get()));

    CompilationContext context;
    unsigned osrEntryScratchBufferSize = 0;
    auto parseAndCompileResult = parseAndCompile(context, function, signature, m_codeBlock->m_wasmToWasmExitStubs, osrEntryScratchBufferSize, m_moduleInformation.get(), m_mode, CompilationMode::OMGForOSREntryMode, m_functionIndex, m_loopIndex);

    if (UNLIKELY(!parseAndCompileResult)) {
        fail(holdLock(m_lock), makeString(parseAndCompileResult.error(), "when trying to tier up ", String::number(m_functionIndex)));
        return;
    }

    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Base::fail(holdLock(m_lock), makeString("Out of executable memory while tiering up function at index ", String::number(m_functionIndex)));
        return;
    }

    Ref<OMGForOSREntryCallee> callee = OMGForOSREntryCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), osrEntryScratchBufferSize, m_loopIndex);
    callee->entrypoint().calleeSaveRegisters = WTFMove(context.calleeSaveRegisters);
    callee->entrypoint().compilation = makeUnique<B3::Compilation>(
        FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "WebAssembly OMGForOSREntry function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
        WTFMove(context.wasmEntrypointByproducts));

    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(mode()));
    {
        auto locker = holdLock(m_codeBlock->m_lock);
        for (auto& call : context.outgoingCalls) {
            BBQCallee& targetCallee = m_codeBlock->wasmBBQCalleeFromFunctionIndexSpace(call.targetFunctionIndexSpace);
            targetCallee.addAndLinkCaller(locker, linkBuffer, call.unlinkedMoveAndCall);
        }
    }
    resetInstructionCacheOnAllThreads();
    WTF::storeStoreFence();
    {
        auto locker = holdLock(m_codeBlock->m_lock);
        {
            auto locker = holdLock(m_callee->tierUpCount()->getLock());
            m_callee->setOSREntryCallee(callee.copyRef());
            m_callee->tierUpCount()->osrEntryTriggers()[m_loopIndex] = TierUpCount::TriggerReason::CompilationDone;
            m_callee->tierUpCount()->m_compilationStatusForOMGForOSREntry = TierUpCount::CompilationStatus::Compiled;
        }
    }
    dataLogLnIf(WasmOMGForOSREntryPlanInternal::verbose, "Finished OMGForOSREntry ", m_functionIndex, " with tier up count at: ", m_callee->tierUpCount()->count());
    complete(holdLock(m_lock));
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
