/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "WasmLLIntPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "BytecodeDumper.h"
#include "CCallHelpers.h"
#include "CalleeBits.h"
#include "JITCompilation.h"
#include "JITOpaqueByproducts.h"
#include "JSToWasm.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "WasmCallee.h"
#include "WasmLLIntGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/GraphNodeWorklist.h>

namespace JSC { namespace Wasm {

LLIntPlan::LLIntPlan(VM& vm, Vector<uint8_t>&& source, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(source), compilerMode, WTFMove(task))
{
    if (parseAndValidateModule(m_source.data(), m_source.size()))
        prepare();
}

LLIntPlan::LLIntPlan(VM& vm, Ref<ModuleInformation> info, const Ref<LLIntCallee>* callees, CompletionTask&& task)
    : Base(vm, WTFMove(info), CompilerMode::FullCompile, WTFMove(task))
    , m_callees(callees)
{
    ASSERT(m_callees || !m_moduleInformation->functions.size());
    prepare();
    m_currentIndex = m_moduleInformation->functions.size();
}

LLIntPlan::LLIntPlan(VM& vm, Ref<ModuleInformation> info, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(info), compilerMode, WTFMove(task))
{
    prepare();
    m_currentIndex = m_moduleInformation->functions.size();
}

bool LLIntPlan::prepareImpl()
{
    const auto& functions = m_moduleInformation->functions;
    if (!tryReserveCapacity(m_wasmInternalFunctions, functions.size(), " WebAssembly functions"))
        return false;

    m_wasmInternalFunctions.resize(functions.size());

    return true;
}

void LLIntPlan::compileFunction(uint32_t functionIndex)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex);
    unsigned functionIndexSpace = m_wasmToWasmExitStubs.size() + functionIndex;
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace) == typeIndex);

    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
    Expected<std::unique_ptr<FunctionCodeBlockGenerator>, String> parseAndCompileResult = parseAndCompileBytecode(function.data.data(), function.data.size(), signature, m_moduleInformation.get(), functionIndex);

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(makeString(parseAndCompileResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
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

void LLIntPlan::didCompleteCompilation()
{
    unsigned functionCount = m_wasmInternalFunctions.size();
    if (!m_callees && functionCount) {
        // LLInt entrypoint thunks generation
        CCallHelpers jit;
        m_calleesVector.resize(functionCount);
        Vector<CCallHelpers::Label> entrypoints(functionCount);
        Vector<CCallHelpers::Jump> jumps(functionCount);
        for (unsigned i = 0; i < functionCount; ++i) {
            size_t functionIndexSpace = i + m_moduleInformation->importFunctionCount();

            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                BytecodeDumper::dumpBlock(m_wasmInternalFunctions[i].get(), m_moduleInformation, WTF::dataFile());

            m_calleesVector[i] = LLIntCallee::create(*m_wasmInternalFunctions[i], functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace));
            ASSERT(!m_calleesVector[i]->entrypoint());
            entrypoints[i] = jit.label();
            if (m_moduleInformation->isSIMDFunction(i))
                JIT_COMMENT(jit, "SIMD function entrypoint");
            JIT_COMMENT(jit, "Entrypoint for function[", i, "]");
#if CPU(X86_64)
            CCallHelpers::Address calleeSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CPURegister));
#elif CPU(ARM64) || CPU(RISCV64) || CPU(ARM)
            CCallHelpers::Address calleeSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC));
#else
#error Unsupported architecture.
#endif
            jit.storePtr(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(m_calleesVector[i].ptr())), calleeSlot.withOffset(PayloadOffset));
#if USE(JSVALUE32_64)
            jit.store32(CCallHelpers::TrustedImm32(JSValue::WasmTag), calleeSlot.withOffset(TagOffset));
#endif
            jumps[i] = jit.jump();
        }

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Wasm, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate())) {
            Base::fail("Out of executable memory in Wasm LLInt entry thunks"_s);
            return;
        }

        for (unsigned i = 0; i < functionCount; ++i) {
            m_calleesVector[i]->setEntrypoint(linkBuffer.locationOf<WasmEntryPtrTag>(entrypoints[i]));
            if (m_moduleInformation->isSIMDFunction(i))
                linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(LLInt::wasmFunctionEntryThunkSIMD().code()));
            else
                linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(LLInt::wasmFunctionEntryThunk().code()));
        }

        m_entryThunks = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "Wasm LLInt entry thunks");
        m_callees = m_calleesVector.data();
        if (!m_moduleInformation->clobberingTailCalls().isEmpty())
            computeTransitiveTailCalls();
    }

    if (m_compilerMode == CompilerMode::Validation)
        return;

    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->referencedFunctions().contains(functionIndex)) {
            TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
            const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
            CCallHelpers jit;
            // The LLInt always bounds checks
            MemoryMode mode = MemoryMode::BoundsChecking;
            Ref<EmbedderEntrypointCallee> callee = EmbedderEntrypointCallee::create();
            std::unique_ptr<InternalFunction> function = createJSToWasmWrapper(jit, callee.get(), signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), mode, functionIndex);

            LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::Wasm, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail(makeString("Out of executable memory in function entrypoint at index ", String::number(functionIndex)));
                return;
            }

            function->entrypoint.compilation = makeUnique<Compilation>(
                FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "Embedder->WebAssembly entrypoint[%i] %s", functionIndex, signature.toString().ascii().data()),
                nullptr);

            callee->setEntrypoint(WTFMove(function->entrypoint));

            auto result = m_embedderCallees.add(functionIndex, WTFMove(callee));
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

void LLIntPlan::completeInStreaming()
{
    Locker locker { m_lock };
    complete();
}

void LLIntPlan::didCompileFunctionInStreaming()
{
    Locker locker { m_lock };
    moveToState(EntryPlan::State::Compiled);
}

void LLIntPlan::didFailInStreaming(String&& message)
{
    Locker locker { m_lock };
    if (!m_errorMessage)
        fail(WTFMove(message));
}

void LLIntPlan::work(CompilationEffort effort)
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

bool LLIntPlan::didReceiveFunctionData(unsigned, const FunctionData&)
{
    // Validation is done inline by the parser
    return true;
}

void LLIntPlan::addTailCallEdge(uint32_t callerIndex, uint32_t calleeIndex)
{
    auto it = m_tailCallGraph.find(calleeIndex);
    if (it == m_tailCallGraph.end())
        it = m_tailCallGraph.add(calleeIndex, TailCallGraph::MappedType()).iterator;
    it->value.add(callerIndex);
}

void LLIntPlan::computeTransitiveTailCalls() const
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
