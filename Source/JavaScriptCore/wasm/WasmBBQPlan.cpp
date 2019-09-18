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
#include "WasmBBQPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "WasmAirIRGenerator.h"
#include "WasmB3IRGenerator.h"
#include "WasmBinding.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmFaultSignalHandler.h"
#include "WasmMemory.h"
#include "WasmSignatureInlines.h"
#include "WasmTierUpCount.h"
#include "WasmValidate.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace JSC { namespace Wasm {

namespace WasmBBQPlanInternal {
static constexpr bool verbose = false;
}

BBQPlan::BBQPlan(Context* context, const Vector<Ref<BBQCallee>>& callees, Ref<ModuleInformation> info, AsyncWork work, CompletionTask&& task, CreateEmbedderWrapper&& createEmbedderWrapper, ThrowWasmException throwWasmException)
    : Base(context, WTFMove(info), WTFMove(task), WTFMove(createEmbedderWrapper), throwWasmException)
    , m_streamingParser(m_moduleInformation.get(), *this)
    , m_callees(&callees)
    , m_state(State::Validated)
    , m_asyncWork(work)
{
}

BBQPlan::BBQPlan(Context* context, Vector<uint8_t>&& source, AsyncWork work, CompletionTask&& task, CreateEmbedderWrapper&& createEmbedderWrapper, ThrowWasmException throwWasmException)
    : Base(context, ModuleInformation::create(), WTFMove(task), WTFMove(createEmbedderWrapper), throwWasmException)
    , m_source(WTFMove(source))
    , m_streamingParser(m_moduleInformation.get(), *this)
    , m_state(State::Initial)
    , m_asyncWork(work)
{
}

BBQPlan::BBQPlan(Context* context, AsyncWork work, CompletionTask&& task)
    : Base(context, WTFMove(task))
    , m_streamingParser(m_moduleInformation.get(), *this)
    , m_state(State::Initial)
    , m_asyncWork(work)
{
}

const char* BBQPlan::stateString(State state)
{
    switch (state) {
    case State::Initial: return "Initial";
    case State::Validated: return "Validated";
    case State::Prepared: return "Prepared";
    case State::Compiled: return "Compiled";
    case State::Completed: return "Completed";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void BBQPlan::moveToState(State state)
{
    ASSERT(state >= m_state);
    dataLogLnIf(WasmBBQPlanInternal::verbose && state != m_state, "moving to state: ", stateString(state), " from state: ", stateString(m_state));
    m_state = state;
}

void BBQPlan::didReceiveFunctionData(unsigned functionIndex, const FunctionData& function)
{
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Processing function starting at: ", function.start, " and ending at: ", function.end);
    size_t functionLength = function.end - function.start;
    ASSERT(functionLength == function.data.size());
    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);

    auto validationResult = validateFunction(function, signature, m_moduleInformation.get());
    if (!validationResult) {
        if (WasmBBQPlanInternal::verbose) {
            for (unsigned i = 0; i < functionLength; ++i)
                dataLog(RawPointer(reinterpret_cast<void*>(function.data[i])), ", ");
            dataLogLn();
        }
        fail(holdLock(m_lock), makeString(validationResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
    }
}

bool BBQPlan::parseAndValidateModule(const uint8_t* source, size_t sourceLength)
{
    if (m_state != State::Initial)
        return true;

    dataLogLnIf(WasmBBQPlanInternal::verbose, "starting validation");
    MonotonicTime startTime;
    if (WasmBBQPlanInternal::verbose || Options::reportCompileTimes())
        startTime = MonotonicTime::now();

    m_streamingParser.addBytes(source, sourceLength);
    if (m_streamingParser.finalize() != StreamingParser::State::Finished) {
        fail(holdLock(m_lock), String(m_streamingParser.errorMessage()));
        return false;
    }
    {
        auto locker = holdLock(m_lock);
        if (failed())
            return false;
    }
    if (WasmBBQPlanInternal::verbose || Options::reportCompileTimes())
        dataLogLn("Took ", (MonotonicTime::now() - startTime).microseconds(), " us to validate module");

    moveToState(State::Validated);
    if (m_asyncWork == Validation)
        complete(holdLock(m_lock));
    return true;
}

void BBQPlan::prepare()
{
    ASSERT(m_state == State::Validated);
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Starting preparation");

    auto tryReserveCapacity = [this] (auto& vector, size_t size, const char* what) {
        if (UNLIKELY(!vector.tryReserveCapacity(size))) {
            fail(holdLock(m_lock), WTF::makeString("Failed allocating enough space for ", size, what));
            return false;
        }
        return true;
    };

    const auto& functions = m_moduleInformation->functions;
    if (!tryReserveCapacity(m_wasmToWasmExitStubs, m_moduleInformation->importFunctionSignatureIndices.size(), " WebAssembly to JavaScript stubs")
        || !tryReserveCapacity(m_compilationContexts, functions.size(), " compilation contexts"))
        return;

    m_compilationContexts.resize(functions.size());

    for (unsigned importIndex = 0; importIndex < m_moduleInformation->imports.size(); ++importIndex) {
        Import* import = &m_moduleInformation->imports[importIndex];
        if (import->kind != ExternalKind::Function)
            continue;
        unsigned importFunctionIndex = m_wasmToWasmExitStubs.size();
        dataLogLnIf(WasmBBQPlanInternal::verbose, "Processing import function number ", importFunctionIndex, ": ", makeString(import->module), ": ", makeString(import->field));
        auto binding = wasmToWasm(importFunctionIndex);
        if (UNLIKELY(!binding)) {
            switch (binding.error()) {
            case BindingFailure::OutOfMemory:
                return fail(holdLock(m_lock), makeString("Out of executable memory at import ", String::number(importIndex)));
            }
            RELEASE_ASSERT_NOT_REACHED();
        }
        m_wasmToWasmExitStubs.uncheckedAppend(binding.value());
    }

    const uint32_t importFunctionCount = m_moduleInformation->importFunctionCount();
    for (const auto& exp : m_moduleInformation->exports) {
        if (exp.kindIndex >= importFunctionCount)
            m_exportedFunctionIndices.add(exp.kindIndex - importFunctionCount);
    }

    for (const auto& element : m_moduleInformation->elements) {
        for (const uint32_t elementIndex : element.functionIndices) {
            if (elementIndex >= importFunctionCount)
                m_exportedFunctionIndices.add(elementIndex - importFunctionCount);
        }
    }

    if (m_moduleInformation->startFunctionIndexSpace && m_moduleInformation->startFunctionIndexSpace >= importFunctionCount)
        m_exportedFunctionIndices.add(*m_moduleInformation->startFunctionIndexSpace - importFunctionCount);

    moveToState(State::Prepared);
}

// We don't have a semaphore class... and this does kinda interesting things.
class BBQPlan::ThreadCountHolder {
public:
    ThreadCountHolder(BBQPlan& plan)
        : m_plan(plan)
    {
        LockHolder locker(m_plan.m_lock);
        m_plan.m_numberOfActiveThreads++;
    }

    ~ThreadCountHolder()
    {
        LockHolder locker(m_plan.m_lock);
        m_plan.m_numberOfActiveThreads--;

        if (!m_plan.m_numberOfActiveThreads && !m_plan.hasWork())
            m_plan.complete(locker);
    }

    BBQPlan& m_plan;
};

void BBQPlan::compileFunctions(CompilationEffort effort)
{
    RELEASE_ASSERT(m_callees);
    ASSERT(m_state >= State::Prepared);
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Starting compilation");

    if (!hasWork())
        return;

    Optional<TraceScope> traceScope;
    if (Options::useTracePoints())
        traceScope.emplace(WebAssemblyCompileStart, WebAssemblyCompileEnd);
    ThreadCountHolder holder(*this);

    size_t bytesCompiled = 0;
    const auto& functions = m_moduleInformation->functions;
    while (true) {
        if (effort == Partial && bytesCompiled >= Options::webAssemblyPartialCompileLimit())
            return;

        uint32_t functionIndex;
        {
            auto locker = holdLock(m_lock);
            if (m_currentIndex >= functions.size()) {
                if (hasWork())
                    moveToState(State::Compiled);
                return;
            }
            functionIndex = m_currentIndex;
            ++m_currentIndex;
        }

        const auto& function = functions[functionIndex];
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);
        unsigned functionIndexSpace = m_wasmToWasmExitStubs.size() + functionIndex;
        ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace) == signatureIndex);
        ASSERT(validateFunction(function, signature, m_moduleInformation.get()));

        Expected<void, String> parseAndCompileResult;
        unsigned osrEntryScratchBufferSize = 0;

        // FIXME: Some webpages use very large Wasm module, and it exhausts all executable memory in ARM64 devices since the size of executable memory region is only limited to 128MB.
        // The long term solution should be to introduce a Wasm interpreter. But as a short term solution, we introduce heuristics to switch back to BBQ B3 at the sacrifice of start-up time,
        // as BBQ Air bloats such lengthy Wasm code and will consume a large amount of executable memory.
        bool forceUsingB3 = false;
        if (Options::webAssemblyBBQAirModeThreshold() && m_moduleInformation->codeSectionSize >= Options::webAssemblyBBQAirModeThreshold())
            forceUsingB3 = true;

        TierUpCount* tierUp = m_callees->at(functionIndex)->tierUpCount();
        if (!forceUsingB3 && Options::wasmBBQUsesAir())
            parseAndCompileResult = parseAndCompileAir(m_compilationContexts[functionIndex], function, signature, m_wasmToWasmExitStubs, m_moduleInformation.get(), m_mode, functionIndex, tierUp, m_throwWasmException);
        else
            parseAndCompileResult = parseAndCompile(m_compilationContexts[functionIndex], function, signature, m_wasmToWasmExitStubs, osrEntryScratchBufferSize, m_moduleInformation.get(), m_mode, CompilationMode::BBQMode, functionIndex, UINT32_MAX, tierUp, m_throwWasmException);

        if (UNLIKELY(!parseAndCompileResult)) {
            auto locker = holdLock(m_lock);
            fail(locker, makeString(parseAndCompileResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
            m_currentIndex = functions.size();
            return;
        }

        if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->referencedFunctions().contains(functionIndex)) {
            auto locker = holdLock(m_lock);
            RefPtr<EmbedderEntrypointCallee> embedderEntrypointCallee = Wasm::EmbedderEntrypointCallee::create();
            m_createEmbedderWrapper(m_compilationContexts[functionIndex], signature, embedderEntrypointCallee.get(), m_moduleInformation.get(), m_mode);
            auto result = m_embedderToWasmInternalFunctions.add(functionIndex, WTFMove(embedderEntrypointCallee));
            ASSERT_UNUSED(result, result.isNewEntry);
        }

        bytesCompiled += function.data.size();
    }
}

void BBQPlan::complete(const AbstractLocker& locker)
{
    ASSERT(m_state != State::Compiled || m_currentIndex >= m_moduleInformation->functions.size());
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Starting Completion");

    if (!failed() && m_state == State::Compiled) {
        for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
            CompilationContext& context = m_compilationContexts[functionIndex];
            SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
            const Signature& signature = SignatureInformation::get(signatureIndex);
            const uint32_t importFunctionCount = m_moduleInformation->importFunctionCount();
            const uint32_t functionIndexSpace = functionIndex + importFunctionCount;
            BBQCallee& callee = m_callees->at(functionIndex).get();
            ASSERT(functionIndexSpace < m_moduleInformation->functionIndexSpaceSize());
            {
                LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr, JITCompilationCanFail);
                if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                    fail(locker, makeString("Out of executable memory in function at index ", String::number(functionIndex)));
                    return;
                }

                callee.entrypoint().calleeSaveRegisters = WTFMove(context.calleeSaveRegisters);
                callee.entrypoint().compilation = makeUnique<B3::Compilation>(
                    FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "WebAssembly BBQ function[%i] %s name %s", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
                    WTFMove(context.wasmEntrypointByproducts));

                for (auto& call : context.outgoingCalls) {
                    ASSERT(call.targetFunctionIndexSpace >= importFunctionCount);
                    const uint32_t targetFunctionIndex = call.targetFunctionIndexSpace - m_moduleInformation->importFunctionCount();
                    m_callees->at(targetFunctionIndex)->addCaller(locker, linkBuffer, call.unlinkedMoveAndCall);
                }
            }

            if (auto embedderToWasmInternalFunction = m_embedderToWasmInternalFunctions.get(functionIndex)) {
                LinkBuffer linkBuffer(*context.embedderEntrypointJIT, nullptr, JITCompilationCanFail);
                if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                    fail(locker, makeString("Out of executable memory in function entrypoint at index ", String::number(functionIndex)));
                    return;
                }

                embedderToWasmInternalFunction->entrypoint().compilation = makeUnique<B3::Compilation>(
                    FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "Embedder->WebAssembly entrypoint[%i] %s name %s", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
                    WTFMove(context.embedderEntrypointByproducts));

                if (context.embedderMoveAndCall)
                    callee.addCaller(locker, linkBuffer, *context.embedderMoveAndCall);
            }

        }

        for (auto& callee : *m_callees)
            callee->repatchCallers(locker, callee.get());
    }
    
    if (!isComplete()) {
        moveToState(State::Completed);
        runCompletionTasks(locker);
    }
}

void BBQPlan::work(CompilationEffort effort)
{
    switch (m_state) {
    case State::Initial:
        parseAndValidateModule(m_source.data(), m_source.size());
        if (!hasWork()) {
            ASSERT(isComplete());
            return;
        }
        FALLTHROUGH;
    case State::Validated:
        prepare();
        return;
    case State::Prepared:
        compileFunctions(effort);
        return;
    default:
        break;
    }
    return;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
