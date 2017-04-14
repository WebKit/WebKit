/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WasmPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "WasmB3IRGenerator.h"
#include "WasmBinding.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmFaultSignalHandler.h"
#include "WasmMemory.h"
#include "WasmModuleParser.h"
#include "WasmValidate.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringBuilder.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;

Plan::Plan(VM& vm, Ref<ModuleInformation> info, AsyncWork work, CompletionTask&& task)
    : m_moduleInformation(WTFMove(info))
    , m_source(m_moduleInformation->source.data())
    , m_sourceLength(m_moduleInformation->source.size())
    , m_state(State::Validated)
    , m_asyncWork(work)
{
    m_completionTasks.append(std::make_pair(&vm, WTFMove(task)));
}

Plan::Plan(VM& vm, Vector<uint8_t>&& source, AsyncWork work, CompletionTask&& task)
    : Plan(vm, makeRef(*new ModuleInformation(WTFMove(source))), work, WTFMove(task))
{
    m_state = State::Initial;
}

Plan::Plan(VM& vm, const uint8_t* source, size_t sourceLength, AsyncWork work, CompletionTask&& task)
    : m_moduleInformation(makeRef(*new ModuleInformation(Vector<uint8_t>())))
    , m_source(source)
    , m_sourceLength(sourceLength)
    , m_state(State::Initial)
    , m_asyncWork(work)
{
    m_completionTasks.append(std::make_pair(&vm, WTFMove(task)));
}

const char* Plan::stateString(State state)
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

void Plan::moveToState(State state)
{
    ASSERT(state >= m_state);
    dataLogLnIf(verbose && state != m_state, "moving to state: ", stateString(state), " from state: ", stateString(m_state));
    m_state = state;
}

void Plan::fail(const AbstractLocker& locker, String&& errorMessage)
{
    dataLogLnIf(verbose, "failing with message: ", errorMessage);
    m_errorMessage = WTFMove(errorMessage);
    complete(locker);
}

void Plan::addCompletionTask(VM& vm, CompletionTask&& task)
{
    LockHolder locker(m_lock);
    if (m_state != State::Completed)
        m_completionTasks.append(std::make_pair(&vm, WTFMove(task)));
    else
        task->run(vm, *this);
}

bool Plan::parseAndValidateModule()
{
    if (m_state != State::Initial)
        return true;

    dataLogLnIf(verbose, "starting validation");
    MonotonicTime startTime;
    if (verbose || Options::reportCompileTimes())
        startTime = MonotonicTime::now();

    {
        ModuleParser moduleParser(m_source, m_sourceLength, m_moduleInformation);
        auto parseResult = moduleParser.parse();
        if (!parseResult) {
            fail(holdLock(m_lock), WTFMove(parseResult.error()));
            return false;
        }
    }

    const auto& functionLocations = m_moduleInformation->functionLocationInBinary;
    for (unsigned functionIndex = 0; functionIndex < functionLocations.size(); ++functionIndex) {
        dataLogLnIf(verbose, "Processing function starting at: ", functionLocations[functionIndex].start, " and ending at: ", functionLocations[functionIndex].end);
        const uint8_t* functionStart = m_source + functionLocations[functionIndex].start;
        size_t functionLength = functionLocations[functionIndex].end - functionLocations[functionIndex].start;
        ASSERT(functionLength <= m_sourceLength);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);

        auto validationResult = validateFunction(functionStart, functionLength, signature, m_moduleInformation.get());
        if (!validationResult) {
            if (verbose) {
                for (unsigned i = 0; i < functionLength; ++i)
                    dataLog(RawPointer(reinterpret_cast<void*>(functionStart[i])), ", ");
                dataLogLn();
            }
            fail(holdLock(m_lock), makeString(validationResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
            return false;
        }
    }

    if (verbose || Options::reportCompileTimes())
        dataLogLn("Took ", (MonotonicTime::now() - startTime).microseconds(), " us to validate module");
    if (m_asyncWork == Validation)
        complete(holdLock(m_lock));
    else
        moveToState(State::Validated);
    return true;
}

void Plan::prepare()
{
    ASSERT(m_state == State::Validated);
    dataLogLnIf(verbose, "Starting preparation");

    TraceScope traceScope(WebAssemblyCompileStart, WebAssemblyCompileEnd);

    auto tryReserveCapacity = [this] (auto& vector, size_t size, const char* what) {
        if (UNLIKELY(!vector.tryReserveCapacity(size))) {
            StringBuilder builder;
            builder.appendLiteral("Failed allocating enough space for ");
            builder.appendNumber(size);
            builder.append(what);
            fail(holdLock(m_lock), builder.toString());
            return false;
        }
        return true;
    };

    const auto& functionLocations = m_moduleInformation->functionLocationInBinary;
    if (!tryReserveCapacity(m_wasmToWasmExitStubs, m_moduleInformation->importFunctionSignatureIndices.size(), " WebAssembly to JavaScript stubs")
        || !tryReserveCapacity(m_unlinkedWasmToWasmCalls, functionLocations.size(), " unlinked WebAssembly to WebAssembly calls")
        || !tryReserveCapacity(m_wasmInternalFunctions, functionLocations.size(), " WebAssembly functions")
        || !tryReserveCapacity(m_compilationContexts, functionLocations.size(), " compilation contexts"))
        return;

    m_unlinkedWasmToWasmCalls.resize(functionLocations.size());
    m_wasmInternalFunctions.resize(functionLocations.size());
    m_compilationContexts.resize(functionLocations.size());

    for (unsigned importIndex = 0; importIndex < m_moduleInformation->imports.size(); ++importIndex) {
        Import* import = &m_moduleInformation->imports[importIndex];
        if (import->kind != ExternalKind::Function)
            continue;
        unsigned importFunctionIndex = m_wasmToWasmExitStubs.size();
        dataLogLnIf(verbose, "Processing import function number ", importFunctionIndex, ": ", makeString(import->module), ": ", makeString(import->field));
        m_wasmToWasmExitStubs.uncheckedAppend(wasmToWasm(importFunctionIndex));
    }

    moveToState(State::Prepared);
}

// We don't have a semaphore class... and this does kinda interesting things.
class Plan::ThreadCountHolder {
public:
    ThreadCountHolder(Plan& plan)
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

    Plan& m_plan;
};

void Plan::compileFunctions(CompilationEffort effort)
{
    ASSERT(m_state >= State::Prepared);
    dataLogLnIf(verbose, "Starting compilation");

    if (!hasWork())
        return;

    ThreadCountHolder holder(*this);

    size_t bytesCompiled = 0;
    const auto& functionLocations = m_moduleInformation->functionLocationInBinary;
    while (true) {
        if (effort == Partial && bytesCompiled >= Options::webAssemblyPartialCompileLimit())
            return;

        uint32_t functionIndex;
        {
            auto locker = holdLock(m_lock);
            if (m_currentIndex >= functionLocations.size()) {
                if (hasWork())
                    moveToState(State::Compiled);
                return;
            }
            functionIndex = m_currentIndex;
            ++m_currentIndex;
        }

        const uint8_t* functionStart = m_source + functionLocations[functionIndex].start;
        size_t functionLength = functionLocations[functionIndex].end - functionLocations[functionIndex].start;
        ASSERT(functionLength <= m_sourceLength);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);
        unsigned functionIndexSpace = m_wasmToWasmExitStubs.size() + functionIndex;
        ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace) == signatureIndex);
        ASSERT(validateFunction(functionStart, functionLength, signature, m_moduleInformation.get()));

        m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
        auto parseAndCompileResult = parseAndCompile(m_compilationContexts[functionIndex], functionStart, functionLength, signature, m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), m_mode, Options::webAssemblyB3OptimizationLevel());

        if (UNLIKELY(!parseAndCompileResult)) {
            auto locker = holdLock(m_lock);
            if (!m_errorMessage) {
                // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
                fail(locker, makeString(parseAndCompileResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
            }
            m_currentIndex = functionLocations.size();
            return;
        }

        m_wasmInternalFunctions[functionIndex] = WTFMove(*parseAndCompileResult);
        bytesCompiled += functionLength;
    }
}

void Plan::complete(const AbstractLocker&)
{
    ASSERT(m_state != State::Compiled || m_currentIndex >= m_moduleInformation->functionLocationInBinary.size());
    dataLogLnIf(verbose, "Starting Completion");

    if (m_state == State::Compiled) {
        for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functionLocationInBinary.size(); functionIndex++) {
            {
                CompilationContext& context = m_compilationContexts[functionIndex];
                SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
                {
                    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr);
                    m_wasmInternalFunctions[functionIndex]->wasmEntrypoint.compilation = std::make_unique<B3::Compilation>(
                        FINALIZE_CODE(linkBuffer, ("WebAssembly function[%i] %s", functionIndex, SignatureInformation::get(signatureIndex).toString().ascii().data())),
                        WTFMove(context.wasmEntrypointByproducts));
                }

                {
                    LinkBuffer linkBuffer(*context.jsEntrypointJIT, nullptr);
                    linkBuffer.link(context.jsEntrypointToWasmEntrypointCall, FunctionPtr(m_wasmInternalFunctions[functionIndex]->wasmEntrypoint.compilation->code().executableAddress()));

                    m_wasmInternalFunctions[functionIndex]->jsToWasmEntrypoint.compilation = std::make_unique<B3::Compilation>(
                        FINALIZE_CODE(linkBuffer, ("JavaScript->WebAssembly entrypoint[%i] %s", functionIndex, SignatureInformation::get(signatureIndex).toString().ascii().data())),
                        WTFMove(context.jsEntrypointByproducts));
                }
            }

        }

        for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
            for (auto& call : unlinked) {
                void* executableAddress;
                if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndex)) {
                    // FIXME imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT. https://bugs.webkit.org/show_bug.cgi?id=166462
                    executableAddress = m_wasmToWasmExitStubs.at(call.functionIndex).code().executableAddress();
                } else
                    executableAddress = m_wasmInternalFunctions.at(call.functionIndex - m_wasmToWasmExitStubs.size())->wasmEntrypoint.compilation->code().executableAddress();
                MacroAssembler::repatchCall(call.callLocation, CodeLocationLabel(executableAddress));
            }
        }
    }

    if (m_state != State::Completed) {
        moveToState(State::Completed);
        for (auto& task : m_completionTasks)
            task.second->run(*task.first, *this);
        m_completionTasks.clear();
        m_completed.notifyAll();
    }
}

void Plan::waitForCompletion()
{
    LockHolder locker(m_lock);
    if (m_state != State::Completed) {
        // FIXME: We should have a wait conditionally so we don't have to hold the lock to complete / fail.
        m_completed.wait(m_lock);
    }
}

bool Plan::tryRemoveVMAndCancelIfLast(VM& vm)
{
    LockHolder locker(m_lock);

    bool removedAnyTasks = false;
    m_completionTasks.removeAllMatching([&] (const std::pair<VM*, CompletionTask>& pair) {
        bool shouldRemove = pair.first == &vm;
        removedAnyTasks |= shouldRemove;
        return shouldRemove;
    });

    if (!removedAnyTasks)
        return false;

    if (m_state == State::Completed) {
        // We trivially cancel anything that's completed.
        return true;
    }

    if (m_completionTasks.isEmpty()) {
        m_currentIndex = m_moduleInformation->functionLocationInBinary.size();
        fail(locker, ASCIILiteral("WebAssembly Plan was cancelled. If you see this error message please file a bug at bugs.webkit.org!"));
        return true;
    }

    return false;
}

Plan::~Plan() { }

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
