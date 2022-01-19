/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "WasmOMGPlan.h"

#if ENABLE(WEBASSEMBLY_B3JIT)

#include "JITCompilation.h"
#include "LinkBuffer.h"
#include "WasmB3IRGenerator.h"
#include "WasmCallee.h"
#include "WasmNameSection.h"
#include "WasmSignatureInlines.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

namespace WasmOMGPlanInternal {
static constexpr bool verbose = false;
}

OMGPlan::OMGPlan(Context* context, Ref<Module>&& module, uint32_t functionIndex, MemoryMode mode, CompletionTask&& task)
    : Base(context, makeRef(const_cast<ModuleInformation&>(module->moduleInformation())), WTFMove(task))
    , m_module(WTFMove(module))
    , m_codeBlock(*m_module->codeBlockFor(mode))
    , m_functionIndex(functionIndex)
{
    ASSERT(Options::useOMGJIT());
    setMode(mode);
    ASSERT(m_codeBlock->runnable());
    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(m_mode));
    dataLogLnIf(WasmOMGPlanInternal::verbose, "Starting OMG plan for ", functionIndex, " of module: ", RawPointer(&m_module.get()));
}

void OMGPlan::work(CompilationEffort)
{
    ASSERT(m_codeBlock->runnable());
    ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(mode()));
    const FunctionData& function = m_moduleInformation->functions[m_functionIndex];

    const uint32_t functionIndexSpace = m_functionIndex + m_module->moduleInformation().importFunctionCount();
    ASSERT(functionIndexSpace < m_module->moduleInformation().functionIndexSpaceSize());

    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[m_functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);

    Vector<UnlinkedWasmToWasmCall> unlinkedCalls;
    unsigned osrEntryScratchBufferSize;
    CompilationContext context;
    auto parseAndCompileResult = parseAndCompile(context, function, signature, unlinkedCalls, osrEntryScratchBufferSize, m_moduleInformation.get(), m_mode, CompilationMode::OMGMode, m_functionIndex, UINT32_MAX);

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        fail(makeString(parseAndCompileResult.error(), "when trying to tier up ", String::number(m_functionIndex)));
        return;
    }

    Entrypoint omgEntrypoint;
    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr, LinkBuffer::Profile::Wasm, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index ", String::number(m_functionIndex)));
        return;
    }

    omgEntrypoint.compilation = makeUnique<Compilation>(
        FINALIZE_WASM_CODE_FOR_MODE(CompilationMode::OMGMode, linkBuffer, JITCompilationPtrTag, "WebAssembly OMG function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
        WTFMove(context.wasmEntrypointByproducts));

    omgEntrypoint.calleeSaveRegisters = WTFMove(parseAndCompileResult.value()->entrypoint.calleeSaveRegisters);

    MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint;
    {
        ASSERT(m_codeBlock.ptr() == m_module->codeBlockFor(mode()));
        Ref<OMGCallee> callee = OMGCallee::create(WTFMove(omgEntrypoint), functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), WTFMove(unlinkedCalls));
        MacroAssembler::repatchPointer(parseAndCompileResult.value()->calleeMoveLocation, CalleeBits::boxWasm(callee.ptr()));
        ASSERT(!m_codeBlock->m_omgCallees[m_functionIndex]);
        entrypoint = callee->entrypoint();

        // We want to make sure we publish our callee at the same time as we link our callsites. This enables us to ensure we
        // always call the fastest code. Any function linked after us will see our new code and the new callsites, which they
        // will update. It's also ok if they publish their code before we reset the instruction caches because after we release
        // the lock our code is ready to be published too.
        Locker locker { m_codeBlock->m_lock };

        m_codeBlock->m_omgCallees[m_functionIndex] = callee.copyRef();

        for (auto& call : callee->wasmToWasmCallsites()) {
            MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint;
            if (call.functionIndexSpace < m_module->moduleInformation().importFunctionCount())
                entrypoint = m_codeBlock->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
            else
                entrypoint = m_codeBlock->wasmEntrypointCalleeFromFunctionIndexSpace(call.functionIndexSpace).entrypoint().retagged<WasmEntryPtrTag>();

            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
        }

        Plan::updateCallSitesToCallUs(m_codeBlock, CodeLocationLabel<WasmEntryPtrTag>(entrypoint), m_functionIndex, functionIndexSpace);

        {
            if (BBQCallee* bbqCallee = m_codeBlock->m_bbqCallees[m_functionIndex].get()) {
                Locker locker { bbqCallee->tierUpCount()->getLock() };
                bbqCallee->setReplacement(callee.copyRef());
                bbqCallee->tierUpCount()->m_compilationStatusForOMG = TierUpCount::CompilationStatus::Compiled;
            }
            if (m_codeBlock->m_llintCallees) {
                LLIntCallee& llintCallee = m_codeBlock->m_llintCallees->at(m_functionIndex).get();
                Locker locker { llintCallee.tierUpCounter().m_lock };
                llintCallee.setReplacement(callee.copyRef(), mode());
                llintCallee.tierUpCounter().m_compilationStatus = LLIntTierUpCounter::CompilationStatus::Compiled;
            }
        }
    }

    dataLogLnIf(WasmOMGPlanInternal::verbose, "Finished OMG ", m_functionIndex);
    Locker locker { m_lock };
    complete();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
