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
#include "WasmCallee.h"
#include "WasmFunctionIPIntMetadataGenerator.h"
#include "WasmIPIntGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/GraphNodeWorklist.h>

namespace JSC { namespace Wasm {

IPIntPlan::IPIntPlan(VM& vm, Vector<uint8_t>&& source, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(source), compilerMode, WTFMove(task))
{
    if (parseAndValidateModule(m_source.span()))
        prepare();
}

IPIntPlan::IPIntPlan(VM& vm, Ref<ModuleInformation> info, const Ref<IPIntCallee>* callees, CompletionTask&& task)
    : Base(vm, WTFMove(info), CompilerMode::FullCompile, WTFMove(task))
    , m_callees(callees)
{
    ASSERT(m_callees || !m_moduleInformation->functions.size());
    m_areWasmToJSStubsCompiled = true;
    prepare();
    m_currentIndex = m_moduleInformation->functions.size();
}

IPIntPlan::IPIntPlan(VM& vm, Ref<ModuleInformation> info, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(info), compilerMode, WTFMove(task))
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

    if (!tryReserveCapacity(m_entrypoints, functions.size(), " WebAssembly functions"_s))
        return false;
    m_entrypoints.resize(functions.size());

    if (!m_callees) {
        if (!tryReserveCapacity(m_calleesVector, functions.size(), " WebAssembly functions"_s))
            return false;
        m_calleesVector.resize(functions.size());
    }
    return true;
}

void IPIntPlan::compileFunction(uint32_t functionIndex)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    unsigned functionIndexSpace = m_moduleInformation->importFunctionTypeIndices.size() + functionIndex;
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace) == typeIndex);

    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
    auto parseAndCompileResult = parseAndCompileMetadata(function.data, signature, m_moduleInformation.get(), functionIndex);

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(makeString(parseAndCompileResult.error(), ", in function at index "_s, functionIndex)); // FIXME make this an Expected.
        }
        m_currentIndex = m_moduleInformation->functions.size();
        return;
    }

    if (Options::useWebAssemblyTailCalls()) {
        Locker locker { m_lock };

        for (auto successor : parseAndCompileResult->get()->tailCallSuccessors())
            addTailCallEdge(m_moduleInformation->importFunctionCount() + parseAndCompileResult->get()->functionIndex(), successor);

        if (parseAndCompileResult->get()->tailCallClobbersInstance())
            m_moduleInformation->addClobberingTailCall(m_moduleInformation->importFunctionCount() + parseAndCompileResult->get()->functionIndex());
    }

    m_wasmInternalFunctions[functionIndex] = WTFMove(*parseAndCompileResult);

    IPIntCallee* ipintCallee = nullptr;
    if (!m_callees) {
        auto callee = IPIntCallee::create(*m_wasmInternalFunctions[functionIndex], functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace));
        ASSERT(!callee->entrypoint());

        if (Options::useJIT()) {
#if ENABLE(JIT)
            if (m_moduleInformation->usesSIMD(functionIndex))
                callee->setEntrypoint(LLInt::inPlaceInterpreterEntryThunkSIMD().retaggedCode<WasmEntryPtrTag>());
            else
                callee->setEntrypoint(LLInt::inPlaceInterpreterEntryThunk().retaggedCode<WasmEntryPtrTag>());
#endif
        } else
            callee->setEntrypoint(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(wasm_function_prologue_trampoline));
        ipintCallee = callee.ptr();
        m_calleesVector[functionIndex] = WTFMove(callee);
    } else
        ipintCallee = m_callees[functionIndex].ptr();

    // If the function is exported via module, then we ensure JSToWasm entrypoint.
    if (m_compilerMode != CompilerMode::Validation) {
        if (m_exportedFunctionIndices.contains(functionIndex)) {
            if (!ensureEntrypoint(*ipintCallee, functionIndex)) {
                Locker locker { m_lock };
                Base::fail(makeString("JIT is disabled, but the entrypoint for "_s, functionIndex, " requires JIT"_s));
                return;
            }
        }
    }

}

bool IPIntPlan::ensureEntrypoint(IPIntCallee& ipintCallee, unsigned functionIndex)
{
    if (m_entrypoints[functionIndex])
        return true;

    if (!LIKELY(Options::useJIT()))
        return false;

#if ENABLE(JIT)
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    CCallHelpers jit;
    // The LLInt always bounds checks
    MemoryMode mode = MemoryMode::BoundsChecking;
    auto callee = JSEntrypointJITCallee::create();
    std::unique_ptr<InternalFunction> function = createJSToWasmWrapper(jit, callee.get(), &ipintCallee, signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), mode, functionIndex);

    LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate()))
        return false;

    function->entrypoint.compilation = makeUnique<Compilation>(
        FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly entrypoint[%i] %s", functionIndex, signature.toString().ascii().data()),
        nullptr);

    callee->setEntrypoint(WTFMove(function->entrypoint));
    m_entrypoints[functionIndex] = WTFMove(callee);
    return true;
#else
    UNUSED_PARAM(ipintCallee);
    UNUSED_PARAM(functionIndex);
    return false;
#endif
}

void IPIntPlan::didCompleteCompilation()
{
    generateStubsIfNecessary();

#if ENABLE(JIT)
    unsigned functionCount = m_wasmInternalFunctions.size();
    if (!m_callees && functionCount) {
        m_callees = m_calleesVector.data();
        if (!m_moduleInformation->clobberingTailCalls().isEmpty())
            computeTransitiveTailCalls();
    }
#endif

    if (m_compilerMode == CompilerMode::Validation)
        return;

    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        if (!m_entrypoints[functionIndex]) {
            const uint32_t functionIndexSpace = functionIndex + m_moduleInformation->importFunctionCount();
            if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->hasReferencedFunction(functionIndexSpace)) {
                if (!ensureEntrypoint(m_callees[functionIndex].get(), functionIndex)) {
                    Base::fail(makeString("JIT is disabled, but the entrypoint for "_s, functionIndex, " requires JIT"_s));
                    return;
                }
            }
        }
        if (auto& callee = m_entrypoints[functionIndex])
            m_jsEntrypointCallees.add(functionIndex, callee);
    }

    for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
        for (auto& call : unlinked) {
            CodePtr<WasmEntryPtrTag> executableAddress;
            if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndexSpace)) {
                // FIXME: imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT.
                // https://bugs.webkit.org/show_bug.cgi?id=166462
                executableAddress = m_wasmToWasmExitStubs.at(call.functionIndexSpace).code();
            } else
                executableAddress = m_callees[call.functionIndexSpace - m_moduleInformation->importFunctionCount()]->entrypoint();
            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(executableAddress));
        }
    }
}

void IPIntPlan::completeInStreaming()
{
    Locker locker { m_lock };
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
        fail(WTFMove(message));
}

void IPIntPlan::work(CompilationEffort effort)
{
    switch (m_state) {
    case State::Prepared:
        compileFunctions(effort);
        break;
    case State::Compiled:
        break;
    default:
        break;
    }
}

bool IPIntPlan::didReceiveFunctionData(unsigned, const FunctionData&)
{
    // Validation is done inline by the parser
    return true;
}

void IPIntPlan::addTailCallEdge(uint32_t callerIndex, uint32_t calleeIndex)
{
    auto it = m_tailCallGraph.find(calleeIndex);
    if (it == m_tailCallGraph.end())
        it = m_tailCallGraph.add(calleeIndex, TailCallGraph::MappedType()).iterator;
    it->value.add(callerIndex);
}

void IPIntPlan::computeTransitiveTailCalls() const
{
    GraphNodeWorklist<uint32_t, HashSet<uint32_t, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>> worklist;

    for (auto clobberingTailCall : m_moduleInformation->clobberingTailCalls())
        worklist.push(clobberingTailCall);

    while (worklist.notEmpty()) {
        auto node = worklist.pop();
        auto it = m_tailCallGraph.find(node);
        if (it == m_tailCallGraph.end())
            continue;
        for (const auto &successor : it->value) {
            if (worklist.saw(successor))
                continue;
            m_moduleInformation->addClobberingTailCall(successor);
            worklist.push(successor);
        }
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
