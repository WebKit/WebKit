/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "WasmBBQPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "JSToWasm.h"
#include "LinkBuffer.h"
#include "WasmAirIRGenerator.h"
#include "WasmB3IRGenerator.h"
#include "WasmCallee.h"
#include "WasmCodeBlock.h"
#include "WasmSignatureInlines.h"
#include "WasmTierUpCount.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

namespace WasmBBQPlanInternal {
static constexpr bool verbose = false;
}

BBQPlan::BBQPlan(Context* context, Ref<ModuleInformation> moduleInformation, uint32_t functionIndex, CodeBlock* codeBlock, CompletionTask&& completionTask)
    : EntryPlan(context, WTFMove(moduleInformation), AsyncWork::FullCompile, WTFMove(completionTask))
    , m_codeBlock(codeBlock)
    , m_functionIndex(functionIndex)
{
    ASSERT(Options::useBBQJIT());
    setMode(m_codeBlock->mode());
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Starting BBQ plan for ", functionIndex);
}

bool BBQPlan::prepareImpl()
{
    const auto& functions = m_moduleInformation->functions;
    if (!tryReserveCapacity(m_wasmInternalFunctions, functions.size(), " WebAssembly functions")
        || !tryReserveCapacity(m_compilationContexts, functions.size(), " compilation contexts")
        || !tryReserveCapacity(m_tierUpCounts, functions.size(), " tier-up counts"))
        return false;

    m_wasmInternalFunctions.resize(functions.size());
    m_compilationContexts.resize(functions.size());
    m_tierUpCounts.resize(functions.size());

    return true;
}

void BBQPlan::work(CompilationEffort effort)
{
    if (!m_codeBlock) {
        switch (m_state) {
        case State::Initial:
            parseAndValidateModule();
            if (!hasWork()) {
                ASSERT(m_state == State::Validated);
                complete(holdLock(m_lock));
                break;
            }
            FALLTHROUGH;
        case State::Validated:
            prepare();
            break;
        case State::Prepared:
            compileFunctions(effort);
            break;
        default:
            break;
        }
        return;
    }

    CompilationContext context;
    Vector<UnlinkedWasmToWasmCall> unlinkedWasmToWasmCalls;
    std::unique_ptr<TierUpCount> tierUp = makeUnique<TierUpCount>();
    std::unique_ptr<InternalFunction> function = compileFunction(m_functionIndex, context, unlinkedWasmToWasmCalls, tierUp.get());

    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Base::fail(holdLock(m_lock), makeString("Out of executable memory while tiering up function at index ", String::number(m_functionIndex)));
        return;
    }

    size_t functionIndexSpace = m_functionIndex + m_moduleInformation->importFunctionCount();
    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[m_functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);
    function->entrypoint.compilation = makeUnique<B3::Compilation>(
        FINALIZE_WASM_CODE_FOR_MODE(CompilationMode::BBQMode, linkBuffer, B3CompilationPtrTag, "WebAssembly BBQ function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
        WTFMove(context.wasmEntrypointByproducts));

    MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint;
    {
        Ref<BBQCallee> callee = BBQCallee::create(WTFMove(function->entrypoint), functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), WTFMove(tierUp), WTFMove(unlinkedWasmToWasmCalls));
        MacroAssembler::repatchPointer(function->calleeMoveLocation, CalleeBits::boxWasm(callee.ptr()));
        ASSERT(!m_codeBlock->m_bbqCallees[m_functionIndex]);
        entrypoint = callee->entrypoint();

        // We want to make sure we publish our callee at the same time as we link our callsites. This enables us to ensure we
        // always call the fastest code. Any function linked after us will see our new code and the new callsites, which they
        // will update. It's also ok if they publish their code before we reset the instruction caches because after we release
        // the lock our code is ready to be published too.
        LockHolder holder(m_codeBlock->m_lock);

        m_codeBlock->m_bbqCallees[m_functionIndex] = callee.copyRef();

        for (auto& call : callee->wasmToWasmCallsites()) {
            MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint;
            if (call.functionIndexSpace < m_moduleInformation->importFunctionCount())
                entrypoint = m_codeBlock->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
            else
                entrypoint = m_codeBlock->wasmEntrypointCalleeFromFunctionIndexSpace(call.functionIndexSpace).entrypoint().retagged<WasmEntryPtrTag>();

            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
        }

        Plan::updateCallSitesToCallUs(*m_codeBlock, CodeLocationLabel<WasmEntryPtrTag>(entrypoint), m_functionIndex, functionIndexSpace);

        {
            LLIntCallee& llintCallee = m_codeBlock->m_llintCallees->at(m_functionIndex).get();
            auto locker = holdLock(llintCallee.tierUpCounter().m_lock);
            llintCallee.setReplacement(callee.copyRef());
            llintCallee.tierUpCounter().m_compilationStatus = LLIntTierUpCounter::CompilationStatus::Compiled;
        }
    }

    dataLogLnIf(WasmBBQPlanInternal::verbose, "Finished BBQ ", m_functionIndex);

    auto locker = holdLock(m_lock);
    moveToState(State::Completed);
    runCompletionTasks(locker);
}

void BBQPlan::compileFunction(uint32_t functionIndex)
{
    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();

    if (Options::useBBQTierUpChecks())
        m_tierUpCounts[functionIndex] = makeUnique<TierUpCount>();
    else
        m_tierUpCounts[functionIndex] = nullptr;

    m_wasmInternalFunctions[functionIndex] = compileFunction(functionIndex, m_compilationContexts[functionIndex], m_unlinkedWasmToWasmCalls[functionIndex], m_tierUpCounts[functionIndex].get());

    if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->referencedFunctions().contains(functionIndex)) {
        auto locker = holdLock(m_lock);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);
        auto result = m_embedderToWasmInternalFunctions.add(functionIndex, createJSToWasmWrapper(*m_compilationContexts[functionIndex].embedderEntrypointJIT, signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), m_mode, functionIndex));
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}

std::unique_ptr<InternalFunction> BBQPlan::compileFunction(uint32_t functionIndex, CompilationContext& context, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, TierUpCount* tierUp)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);
    unsigned functionIndexSpace = m_moduleInformation->importFunctionCount() + functionIndex;
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace) == signatureIndex);
    Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileResult;
    unsigned osrEntryScratchBufferSize = 0;

    // FIXME: Some webpages use very large Wasm module, and it exhausts all executable memory in ARM64 devices since the size of executable memory region is only limited to 128MB.
    // The long term solution should be to introduce a Wasm interpreter. But as a short term solution, we introduce heuristics to switch back to BBQ B3 at the sacrifice of start-up time,
    // as BBQ Air bloats such lengthy Wasm code and will consume a large amount of executable memory.
    bool forceUsingB3 = false;
    if (Options::webAssemblyBBQAirModeThreshold() && m_moduleInformation->codeSectionSize >= Options::webAssemblyBBQAirModeThreshold())
        forceUsingB3 = true;

    if (!forceUsingB3 && Options::wasmBBQUsesAir())
        parseAndCompileResult = parseAndCompileAir(context, function, signature, unlinkedWasmToWasmCalls, m_moduleInformation.get(), m_mode, functionIndex, tierUp);
    else
        parseAndCompileResult = parseAndCompile(context, function, signature, unlinkedWasmToWasmCalls, osrEntryScratchBufferSize, m_moduleInformation.get(), m_mode, CompilationMode::BBQMode, functionIndex, UINT32_MAX, tierUp);

    if (UNLIKELY(!parseAndCompileResult)) {
        auto locker = holdLock(m_lock);
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(locker, makeString(parseAndCompileResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
        }
        m_currentIndex = m_moduleInformation->functions.size();
        return nullptr;
    }

    return WTFMove(*parseAndCompileResult);
}

void BBQPlan::didCompleteCompilation(const AbstractLocker& locker)
{
    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        CompilationContext& context = m_compilationContexts[functionIndex];
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);
        const uint32_t functionIndexSpace = functionIndex + m_moduleInformation->importFunctionCount();
        ASSERT(functionIndexSpace < m_moduleInformation->functionIndexSpaceSize());
        {
            LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail(locker, makeString("Out of executable memory in function at index ", String::number(functionIndex)));
                return;
            }

            m_wasmInternalFunctions[functionIndex]->entrypoint.compilation = makeUnique<B3::Compilation>(
                FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "WebAssembly BBQ function[%i] %s name %s", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
                WTFMove(context.wasmEntrypointByproducts));
        }

        if (const auto& embedderToWasmInternalFunction = m_embedderToWasmInternalFunctions.get(functionIndex)) {
            LinkBuffer linkBuffer(*context.embedderEntrypointJIT, nullptr, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail(locker, makeString("Out of executable memory in function entrypoint at index ", String::number(functionIndex)));
                return;
            }

            embedderToWasmInternalFunction->entrypoint.compilation = makeUnique<B3::Compilation>(
                FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "Embedder->WebAssembly entrypoint[%i] %s name %s", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
                WTFMove(context.embedderEntrypointByproducts));
        }
    }

    for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
        for (auto& call : unlinked) {
            MacroAssemblerCodePtr<WasmEntryPtrTag> executableAddress;
            if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndexSpace)) {
                // FIXME imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT. https://bugs.webkit.org/show_bug.cgi?id=166462
                executableAddress = m_wasmToWasmExitStubs.at(call.functionIndexSpace).code();
            } else
                executableAddress = m_wasmInternalFunctions.at(call.functionIndexSpace - m_moduleInformation->importFunctionCount())->entrypoint.compilation->code().retagged<WasmEntryPtrTag>();
            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(executableAddress));
        }
    }
}

void BBQPlan::initializeCallees(const CalleeInitializer& callback)
{
    ASSERT(!failed());
    for (unsigned internalFunctionIndex = 0; internalFunctionIndex < m_wasmInternalFunctions.size(); ++internalFunctionIndex) {

        RefPtr<EmbedderEntrypointCallee> embedderEntrypointCallee;
        if (auto embedderToWasmFunction = m_embedderToWasmInternalFunctions.get(internalFunctionIndex)) {
            embedderEntrypointCallee = EmbedderEntrypointCallee::create(WTFMove(embedderToWasmFunction->entrypoint));
            MacroAssembler::repatchPointer(embedderToWasmFunction->calleeMoveLocation, CalleeBits::boxWasm(embedderEntrypointCallee.get()));
        }

        InternalFunction* function = m_wasmInternalFunctions[internalFunctionIndex].get();
        size_t functionIndexSpace = internalFunctionIndex + m_moduleInformation->importFunctionCount();
        Ref<BBQCallee> wasmEntrypointCallee = BBQCallee::create(WTFMove(function->entrypoint), functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), WTFMove(m_tierUpCounts[internalFunctionIndex]), WTFMove(m_unlinkedWasmToWasmCalls[internalFunctionIndex]));
        MacroAssembler::repatchPointer(function->calleeMoveLocation, CalleeBits::boxWasm(wasmEntrypointCallee.ptr()));

        callback(internalFunctionIndex, WTFMove(embedderEntrypointCallee), WTFMove(wasmEntrypointCallee));
    }
}

bool BBQPlan::didReceiveFunctionData(unsigned, const FunctionData&)
{
    return true;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
