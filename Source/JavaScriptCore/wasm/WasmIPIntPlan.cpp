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

IPIntPlan::IPIntPlan(VM& vm, Vector<uint8_t>&& source, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTF::move(source), compilerMode, WTF::move(task))
{
    if (parseAndValidateModule(m_source.span()))
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

IPIntPlan::IPIntPlan(VM& vm, Ref<ModuleInformation> info, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTF::move(info), compilerMode, WTF::move(task))
{
    prepare();
    m_currentIndex = m_moduleInformation->functions.size();
}

bool IPIntPlan::prepareImpl()
{
    const auto& functions = m_moduleInformation->functions;
    if (!tryReserveCapacity(m_wasmInternalFunctions, functions.size(), "WebAssembly functions"_s))
        return false;
    m_wasmInternalFunctions.resize(functions.size());

    if (!m_ipintCallees)
        m_ipintCallees = IPIntCallees::create(functions.size());
    return true;
}

void IPIntPlan::compileFunction(FunctionCodeIndex functionIndex)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    TypeSignatureIndex typeSignatureIndex = m_moduleInformation->internalFunctionTypeSignatureIndices[functionIndex];
    const RTT& signature = m_moduleInformation->rtt(typeSignatureIndex);
    auto functionIndexSpace = m_moduleInformation->toSpaceIndex(functionIndex);
    ASSERT_UNUSED(functionIndexSpace, &m_moduleInformation->rtt(functionIndexSpace) == &m_moduleInformation->rtt(typeSignatureIndex));

    beginCompilerSignpost(CompilationMode::IPIntMode, functionIndexSpace);
    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
    auto parseAndCompileResult = parseAndCompileMetadata(function.data, signature, m_moduleInformation.get(), functionIndex);
    endCompilerSignpost(CompilationMode::IPIntMode, functionIndexSpace);

    if (!parseAndCompileResult) [[unlikely]] {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(makeString(parseAndCompileResult.error(), ", in function at index "_s, functionIndex.rawIndex())); // FIXME make this an Expected.
        }
        m_currentIndex = m_moduleInformation->functions.size();
        return;
    }

    m_wasmInternalFunctions[functionIndex] = WTF::move(*parseAndCompileResult);

    {
        auto callee = IPIntCallee::create(*m_wasmInternalFunctions[functionIndex], functionIndexSpace, signature, m_moduleInformation->nameSection().get(functionIndexSpace));
        ASSERT(!callee->entrypoint());
        bool usesSIMD = m_moduleInformation->usesSIMD(functionIndex);
        // Immediately tier up to BBQ for SIMD, if necesary.
        if (usesSIMD && !Options::useWasmIPIntSIMD())
            callee->tierUpCounter().setNewThreshold(0);

        if (usesSIMD && !Options::useBBQJIT() && !Options::useWasmIPIntSIMD()) {
            Locker locker { m_lock };
            Base::fail(makeString("JIT is disabled, but the entrypoint for "_s, functionIndex.rawIndex(), " requires JIT"_s));
            return;
        }

        CodePtr<WasmEntryPtrTag> entrypoint { };
#if ENABLE(JIT)
        if (Options::useJIT())
            entrypoint = LLInt::inPlaceInterpreterEntryThunk().retaggedCode<WasmEntryPtrTag>();
#endif
        if (!entrypoint)
            entrypoint = LLInt::getCodeFunctionPtr<CFunctionPtrTag>(ipint_trampoline);

        callee->setEntrypointWithoutRegistration(entrypoint);
        m_ipintCallees->at(functionIndex) = WTF::move(callee);
    }
}

void IPIntPlan::didCompleteCompilation()
{
    generateStubsIfNecessary();

    unsigned functionCount = m_wasmInternalFunctions.size();
    if (!m_calleesAlreadyRegistered && functionCount) {
        NativeCalleeRegistry::singleton().registerCallees(*m_ipintCallees);
        if (Options::useWasmTailCalls())
            RestoreFrameCallee::singleton();
    }

    if (m_compilerMode == CompilerMode::Validation)
        return;

    for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
        for (auto& call : unlinked) {
            CodePtr<WasmEntryPtrTag> executableAddress;
            if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndexSpace)) {
                // FIXME: imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT.
                // https://bugs.webkit.org/show_bug.cgi?id=166462
                executableAddress = m_wasmToWasmExitStubs.at(call.functionIndexSpace).code();
            } else
                executableAddress = m_ipintCallees->at(call.functionIndexSpace - m_moduleInformation->importFunctionCount())->entrypoint();
            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(executableAddress));
        }
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
