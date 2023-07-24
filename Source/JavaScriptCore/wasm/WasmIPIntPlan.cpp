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
    if (parseAndValidateModule(m_source.data(), m_source.size()))
        prepare();
}

IPIntPlan::IPIntPlan(VM& vm, Ref<ModuleInformation> info, const Ref<IPIntCallee>* callees, CompletionTask&& task)
    : Base(vm, WTFMove(info), CompilerMode::FullCompile, WTFMove(task))
    , m_callees(callees)
{
    ASSERT(m_callees || !m_moduleInformation->functions.size());
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
    if (!tryReserveCapacity(m_wasmInternalFunctions, functions.size(), "WebAssembly functions"))
        return false;

    m_wasmInternalFunctions.resize(functions.size());
    return true;
}

void IPIntPlan::compileFunction(uint32_t functionIndex)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex);
    unsigned functionIndexSpace = m_wasmToWasmExitStubs.size() + functionIndex;
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace) == typeIndex);

    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
    Expected<std::unique_ptr<FunctionIPIntMetadataGenerator>, String> parseAndCompileResult = parseAndCompileMetadata(function.data.data(), function.data.size(), signature, m_moduleInformation.get(), functionIndex);

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(makeString(parseAndCompileResult.error(), ", in function at index "_s, functionIndex)); // FIXME make this an Expected.
        }
        m_currentIndex = m_moduleInformation->functions.size();
        return;
    }

    Locker locker { m_lock };

    for (auto successor : parseAndCompileResult->get()->tailCallSuccessors())
        addTailCallEdge(m_moduleInformation->importFunctionCount() + parseAndCompileResult->get()->functionIndex(), successor);

    if (parseAndCompileResult->get()->tailCallClobbersInstance())
        m_moduleInformation->addClobberingTailCall(m_moduleInformation->importFunctionCount() + parseAndCompileResult->get()->functionIndex());

    m_wasmInternalFunctions[functionIndex] = WTFMove(*parseAndCompileResult);
}

void IPIntPlan::didCompleteCompilation()
{
    unsigned functionCount = m_wasmInternalFunctions.size();
    if (!m_callees && functionCount) {
        // IPInt entrypoint thunks generation
        CCallHelpers jit;
        m_calleesVector.resize(functionCount);
        Vector<CCallHelpers::Label> entrypoints(functionCount);
        Vector<CCallHelpers::Jump> jumps(functionCount);
        for (unsigned i = 0; i < functionCount; ++i) {
            size_t functionIndexSpace = i + m_moduleInformation->importFunctionCount();

            // TODO: dump option

            m_calleesVector[i] = IPIntCallee::create(*m_wasmInternalFunctions[i], functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace));
            ASSERT(!m_calleesVector[i]->entrypoint());
            entrypoints[i] = jit.label();
            // if (m_moduleInformation->usesSIMD(i))
            //     JIT_COMMENT(jit, "SIMD function entrypoint");
            JIT_COMMENT(jit, "Entrypoint for function[", i, "]");
            CCallHelpers::Address calleeSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - prologueStackPointerDelta());
            jit.storePtr(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(m_calleesVector[i].ptr())), calleeSlot.withOffset(PayloadOffset));
#if USE(JSVALUE_32_64)
            jit.store32(CCallHelpers::TrustedImm32(JSValue::WasmTag), calleeSlot.withOffset(TagOffset));
#endif
            jumps[i] = jit.jump();
        }

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate())) {
            Base::fail("Out of executable memory in Wasm IPInt entry thunks"_s);
            return;
        }

        for (unsigned i = 0; i < functionCount; ++i) {
            m_calleesVector[i]->setEntrypoint(linkBuffer.locationOf<WasmEntryPtrTag>(entrypoints[i]));
        //     if (m_moduleInformation->usesSIMD(i))
        //         linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(IPInt::wasmFunctionEntryThunkSIMD().code()));
        //     else
            linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(LLInt::inPlaceInterpreterEntryThunk().code()));
        }

        m_entryThunks = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "Wasm IPInt entry thunks");
        m_callees = m_calleesVector.data();
        if (!m_moduleInformation->clobberingTailCalls().isEmpty())
            computeTransitiveTailCalls();
    }
    if (m_compilerMode == CompilerMode::Validation)
        return;

    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        const uint32_t functionIndexSpace = functionIndex + m_moduleInformation->importFunctionCount();
        if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->hasReferencedFunction(functionIndexSpace)) {
            TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
            const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
            CCallHelpers jit;
            // The LLInt always bounds checks
            MemoryMode mode = MemoryMode::BoundsChecking;
            Ref<JSEntrypointCallee> callee = JSEntrypointCallee::create();
            std::unique_ptr<InternalFunction> function = createJSToWasmWrapper(jit, callee.get(), signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), mode, functionIndex);

            LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail(makeString("Out of executable memory in function entrypoint at index "_s, functionIndex));
                return;
            }

            function->entrypoint.compilation = makeUnique<Compilation>(
                FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "JS->WebAssembly entrypoint[%i] %s", functionIndex, signature.toString().ascii().data()),
                nullptr);

            callee->setEntrypoint(WTFMove(function->entrypoint));

            auto result = m_jsEntrypointCallees.add(functionIndex, WTFMove(callee));
            ASSERT_UNUSED(result, result.isNewEntry);
        }
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
