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
#include "WasmEntryPlan.h"

#include "LLIntData.h"
#include "WasmBinding.h"
#include "WasmToJS.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/MakeString.h>

#if ENABLE(WEBASSEMBLY)

namespace JSC { namespace Wasm {

namespace WasmEntryPlanInternal {
static constexpr bool verbose = false;
}

EntryPlan::EntryPlan(VM& vm, Ref<ModuleInformation> info, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(info), WTFMove(task))
    , m_streamingParser(m_moduleInformation.get(), *this)
    , m_state(State::Validated)
    , m_compilerMode(compilerMode)
{
}

EntryPlan::EntryPlan(VM& vm, Vector<uint8_t>&& source, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(task))
    , m_source(WTFMove(source))
    , m_streamingParser(m_moduleInformation.get(), *this)
    , m_state(State::Initial)
    , m_compilerMode(compilerMode)
{
}

const char* EntryPlan::stateString(State state)
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

void EntryPlan::moveToState(State state)
{
    ASSERT(state >= m_state);
    dataLogLnIf(WasmEntryPlanInternal::verbose && state != m_state, "moving to state: ", stateString(state), " from state: ", stateString(m_state));
    m_state = state;
}

bool EntryPlan::parseAndValidateModule(std::span<const uint8_t> source)
{
    if (m_state != State::Initial)
        return true;

    dataLogLnIf(WasmEntryPlanInternal::verbose, "starting validation");
    MonotonicTime startTime;
    if (WasmEntryPlanInternal::verbose || Options::reportCompileTimes())
        startTime = MonotonicTime::now();

    m_streamingParser.addBytes(source);
    {
        Locker locker { m_lock };
        if (failed())
            return false;
    }

    if (m_streamingParser.finalize() != StreamingParser::State::Finished) {
        Locker locker { m_lock };
        fail(m_streamingParser.errorMessage());
        return false;
    }

    if (WasmEntryPlanInternal::verbose || Options::reportCompileTimes())
        dataLogLn("Took ", (MonotonicTime::now() - startTime).microseconds(), " us to validate module");

    moveToState(State::Validated);
    return true;
}

void EntryPlan::prepare()
{
    ASSERT(m_state == State::Validated);
    dataLogLnIf(WasmEntryPlanInternal::verbose, "Starting preparation"_s);

    const auto& functions = m_moduleInformation->functions;
    m_numberOfFunctions = functions.size();
    if (!tryReserveCapacity(m_wasmToWasmExitStubs, m_moduleInformation->importFunctionTypeIndices.size(), " WebAssembly to WebAssembly stubs"_s))
        return;
    if (!tryReserveCapacity(m_wasmToJSExitStubs, m_moduleInformation->importFunctionTypeIndices.size(), " WebAssembly to JavaScript stubs"_s))
        return;
    if (!tryReserveCapacity(m_unlinkedWasmToWasmCalls, functions.size(), " unlinked WebAssembly to WebAssembly calls"_s))
        return;

    m_unlinkedWasmToWasmCalls.resize(functions.size());

    const uint32_t importFunctionCount = m_moduleInformation->importFunctionCount();
    for (const auto& exp : m_moduleInformation->exports) {
        if (exp.kindIndex >= importFunctionCount)
            m_exportedFunctionIndices.add(exp.kindIndex - importFunctionCount);
    }

    for (const auto& element : m_moduleInformation->elements) {
        for (uint32_t i = 0; i < element.length(); i++) {
            if (element.initTypes[i] == Element::InitializationType::FromRefFunc && element.initialBitsOrIndices[i] >= importFunctionCount)
                m_exportedFunctionIndices.add(element.initialBitsOrIndices[i] - importFunctionCount);
        }
    }

    if (m_moduleInformation->startFunctionIndexSpace && m_moduleInformation->startFunctionIndexSpace >= importFunctionCount)
        m_exportedFunctionIndices.add(*m_moduleInformation->startFunctionIndexSpace - importFunctionCount);

    if (!prepareImpl())
        return;

    moveToState(State::Prepared);
}

// We don't have a semaphore class... and this does kinda interesting things.
class EntryPlan::ThreadCountHolder {
public:
    ThreadCountHolder(EntryPlan& plan)
        : m_plan(plan)
    {
        Locker locker { m_plan.m_lock };
        m_plan.m_numberOfActiveThreads++;
    }

    ~ThreadCountHolder()
    {
        Locker locker { m_plan.m_lock };
        m_plan.m_numberOfActiveThreads--;

        if (!m_plan.m_numberOfActiveThreads && !m_plan.hasWork())
            m_plan.complete();
    }

    EntryPlan& m_plan;
};


void EntryPlan::compileFunctions(CompilationEffort effort)
{
    ASSERT(m_state >= State::Prepared);
    dataLogLnIf(WasmEntryPlanInternal::verbose, "Starting compilation");

    if (!hasWork())
        return;

    std::optional<TraceScope> traceScope;
    if (Options::useTracePoints())
        traceScope.emplace(WebAssemblyCompileStart, WebAssemblyCompileEnd);
    ThreadCountHolder holder(*this);

    size_t bytesCompiled = 0;
    while (true) {
        if (effort == Partial && bytesCompiled >= Options::wasmPartialCompileLimit())
            return;

        uint32_t functionIndex;
        uint32_t functionIndexEnd;
        bool areWasmToWasmStubsCompiled = false;
        bool areWasmToJSStubsCompiled = false;
        {
            Locker locker { m_lock };
            if (m_currentIndex >= m_numberOfFunctions) {
                if (hasWork())
                    moveToState(State::Compiled);
                return;
            }
            functionIndex = m_currentIndex;
            functionIndexEnd = m_numberOfFunctions;
            for (uint32_t index = functionIndex; index < m_numberOfFunctions; ++index) {
                bytesCompiled += m_moduleInformation->functions[index].data.size();
                if (bytesCompiled >= Options::wasmPartialCompileLimit()) {
                    functionIndexEnd = index + 1;
                    break;
                }
            }
            m_currentIndex = functionIndexEnd;
            areWasmToWasmStubsCompiled = std::exchange(m_areWasmToWasmStubsCompiled, true);
            areWasmToJSStubsCompiled = std::exchange(m_areWasmToJSStubsCompiled, true);
        }

        for (uint32_t index = functionIndex; index < functionIndexEnd; ++index)
            compileFunction(index);

        if (!areWasmToWasmStubsCompiled) {
#if ENABLE(JIT)
            if (UNLIKELY(!generateWasmToWasmStubs())) {
                Locker locker { m_lock };
                fail(makeString("Out of executable memory at stub generation"_s));
                return;
            }
#endif
        }

        if (!areWasmToJSStubsCompiled) {
#if ENABLE(JIT)
            if (UNLIKELY(!generateWasmToJSStubs())) {
                Locker locker { m_lock };
                fail(makeString("Out of executable memory at stub generation"_s));
                return;
            }
#endif
        }
    }
}

void EntryPlan::complete()
{
    ASSERT(m_state != State::Compiled || m_currentIndex >= m_moduleInformation->functions.size());
    dataLogLnIf(WasmEntryPlanInternal::verbose, "Starting Completion");

    if (!failed() && m_state == State::Compiled)
        didCompleteCompilation();

    if (!isComplete()) {
        generateStubsIfNecessary();
        moveToState(State::Completed);
        runCompletionTasks();
    }
}

bool EntryPlan::completeSyncIfPossible()
{
    Locker locker { m_lock };
    if (m_currentIndex >= m_numberOfFunctions) {
        if (hasWork())
            moveToState(State::Compiled);

        if (!m_numberOfActiveThreads) {
            complete();
            return true;
        }
    }
    return false;
}

void EntryPlan::generateStubsIfNecessary()
{
    if (!std::exchange(m_areWasmToWasmStubsCompiled, true)) {
#if ENABLE(JIT)
        if (UNLIKELY(!generateWasmToWasmStubs())) {
            fail(makeString("Out of executable memory at stub generation"_s));
            return;
        }
#endif
    }

    if (!std::exchange(m_areWasmToJSStubsCompiled, true)) {
#if ENABLE(JIT)
        if (UNLIKELY(!generateWasmToJSStubs())) {
            fail(makeString("Out of executable memory at stub generation"_s));
            return;
        }
#endif
    }
}


bool EntryPlan::generateWasmToWasmStubs()
{
    m_wasmToWasmExitStubs.resize(m_moduleInformation->importFunctionTypeIndices.size());
    unsigned importFunctionIndex = 0;
    for (unsigned importIndex = 0; importIndex < m_moduleInformation->imports.size(); ++importIndex) {
        Import* import = &m_moduleInformation->imports[importIndex];
        if (import->kind != ExternalKind::Function)
            continue;
        dataLogLnIf(WasmEntryPlanInternal::verbose, "Processing import function number "_s, importFunctionIndex, ": "_s, makeString(import->module), ": "_s, makeString(import->field));
#if ENABLE(JIT)
        if (Options::useJIT()) {
            auto binding = wasmToWasm(importFunctionIndex);
            if (UNLIKELY(!binding))
                return false;
            m_wasmToWasmExitStubs[importFunctionIndex++] = binding.value();
        }
#else
        if (false);
#endif // ENABLE(JIT)
        else
            m_wasmToWasmExitStubs[importFunctionIndex++] = LLInt::getCodeRef<WasmEntryPtrTag>(wasm_to_wasm_wrapper_entry);
    }
    ASSERT(importFunctionIndex == m_wasmToWasmExitStubs.size());
    return true;
}


bool EntryPlan::generateWasmToJSStubs()
{
    m_wasmToJSExitStubs.resize(m_moduleInformation->importFunctionCount());
    for (unsigned importIndex = 0; importIndex < m_moduleInformation->importFunctionCount(); ++importIndex) {
#if ENABLE(JIT)
        Wasm::TypeIndex typeIndex = m_moduleInformation->importFunctionTypeIndices.at(importIndex);
        if (Options::useJIT()) {
            auto binding = wasmToJS(typeIndex, importIndex);
            if (UNLIKELY(!binding))
                return false;
            m_wasmToJSExitStubs[importIndex] = binding.value();
        }
#else
        if (false);
#endif // ENABLE(JIT)
        else
            m_wasmToJSExitStubs[importIndex] = LLInt::getCodeRef<WasmEntryPtrTag>(wasm_to_js_wrapper_entry);
    }
    return true;
}


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
