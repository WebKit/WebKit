/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHAIP APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmIPIntPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "CalleeBits.h"
#include "JITCompilation.h"
#include "JITOpaqueByproducts.h"
#include "JSToWasm.h"
#include "LLIntData.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "NativeCalleeRegistry.h"
#include "WasmCallee.h"
#include "WasmFunctionIPIntMetadataGenerator.h"
#include "WasmIPIntGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

IPIntPlan::IPIntPlan(VM& vm, Vector<uint8_t>&& source, CompilerMode compilerMode, ValidationMode validationMode, CompletionTask&& task)
    : Base(vm, WTF::move(source), compilerMode, WTF::move(task))
    , m_lazyParsing(validationMode == ValidationMode::Lazy && Options::useWasmIPIntLazyParsing())
{
    if (parseAndValidateModule(m_moduleInformation->source()))
        prepare();
}

IPIntPlan::IPIntPlan(VM& vm, Ref<ModuleInformation> info, Ref<IPIntCallees> callees, CompletionTask&& task)
    : Base(vm, WTF::move(info), CompilerMode::FullCompile, WTF::move(task))
    , m_ipintCallees(WTF::move(callees))
    , m_calleesAlreadyRegistered(true)
{
    m_areWasmToJSStubsCompiled = true;
    prepare();
    m_currentIndex = m_moduleInformation->functions.size();
}

IPIntPlan::IPIntPlan(VM& vm, Ref<ModuleInformation> info, CompilerMode compilerMode, ValidationMode validationMode, CompletionTask&& task)
    : Base(vm, WTF::move(info), compilerMode, WTF::move(task))
    , m_lazyParsing(validationMode == ValidationMode::Lazy && Options::useWasmIPIntLazyParsing())
{
    prepare();
    m_currentIndex = m_moduleInformation->functions.size();
}

bool IPIntPlan::prepareImpl()
{
    const auto& functions = m_moduleInformation->functions;
    if (!m_ipintCallees)
        m_ipintCallees = IPIntCallees::create(functions.size());
    return true;
}

void IPIntPlan::compileFunction(FunctionCodeIndex functionIndex)
{
    auto functionIndexSpace = m_moduleInformation->toSpaceIndex(functionIndex);
    const auto& function = m_moduleInformation->functions[functionIndex];
    const uint8_t* bytecode = function.data.data();
    const uint8_t* bytecodeEnd = bytecode + function.data.size() - 1;
    ASSERT_UNUSED(functionIndexSpace, &m_moduleInformation->rtt(functionIndexSpace) == &m_moduleInformation->rtt(m_moduleInformation->internalFunctionTypeSignatureIndices[functionIndex]));

    auto callee = IPIntCallee::create(functionIndex, functionIndexSpace, m_moduleInformation->rtt(functionIndexSpace), m_moduleInformation->nameSection().get(functionIndexSpace), bytecode, bytecodeEnd);

    // Install the normal IPInt entrypoint regardless of lazy vs. eager. The
    // entrypoint itself checks m_data and calls a slow path on first entry
    // when lazy parsing is enabled.
    CodePtr<WasmEntryPtrTag> entrypoint;
#if ENABLE(JIT)
    if (Options::useJIT())
        entrypoint = LLInt::inPlaceInterpreterEntryThunk().retaggedCode<WasmEntryPtrTag>();
#endif
    if (!entrypoint)
        entrypoint = LLInt::getCodeFunctionPtr<CFunctionPtrTag>(ipint_trampoline);
    callee->setEntrypointWithoutRegistration(entrypoint);

    if (m_lazyParsing) {
        m_ipintCallees->at(functionIndex) = WTF::move(callee);
        return;
    }

    // Eager path: parse and initialize metadata now.
    beginCompilerSignpost(CompilationMode::IPIntMode, functionIndexSpace);
    auto entrypointResult = parseAndInitializeIPIntCallee(callee.get(), m_moduleInformation.get());
    endCompilerSignpost(CompilationMode::IPIntMode, functionIndexSpace);

    if (!entrypointResult) [[unlikely]] {
        Locker locker { m_lock };
        failFunctionCompilation(functionIndex, makeString(entrypointResult.error(), ", in function at index "_s, functionIndex.rawIndex()));
        return;
    }

    m_ipintCallees->at(functionIndex) = WTF::move(callee);
}

void IPIntPlan::didCompleteCompilation()
{
    generateStubsIfNecessary();

    unsigned functionCount = m_ipintCallees->size();
    if (!m_calleesAlreadyRegistered && functionCount) {
        // Set names here rather than at IPIntCallee creation: during streaming the name section
        // (which follows the code section) has not been parsed yet when a function is compiled.
        auto& nameSection = m_moduleInformation->nameSection();
        for (auto& callee : *m_ipintCallees)
            callee->setName(nameSection.get(callee->index()));

        NativeCalleeRegistry::singleton().registerCallees(*m_ipintCallees);
        if (Options::useWasmTailCalls())
            RestoreFrameCallee::singleton();
    }
}

void IPIntPlan::completeInStreaming()
{
    Locker locker { m_lock };
    if (failIfMixedExceptionHandlingProposals())
        return;
    complete();
}

void IPIntPlan::didCompileFunctionInStreaming()
{
    Locker locker { m_lock };
    if (hasWork())
        moveToState(EntryPlan::State::Compiled);
}

void IPIntPlan::didFailInStreaming(String&& message)
{
    Locker locker { m_lock };
    if (!m_errorMessage)
        fail(WTF::move(message));
}

void IPIntPlan::work()
{
    switch (m_state) {
    case State::Prepared:
        compileFunctions();
        break;
    case State::Compiled:
        break;
    default:
        break;
    }
}

bool IPIntPlan::didReceiveFunctionData(FunctionCodeIndex, const FunctionData&)
{
    // Validation is done inline by the parser
    return true;
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
