/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY_BBQJIT)

#include "JITCompilation.h"
#include "JSToWasm.h"
#include "LinkBuffer.h"
#include "NativeCalleeRegistry.h"
#include "WasmBBQJIT.h"
#include "WasmCallee.h"
#include "WasmCalleeGroup.h"
#include "WasmCompilationContext.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmTierUpCount.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

namespace WasmBBQPlanInternal {
static constexpr bool verbose = false;
}

BBQPlan::BBQPlan(VM& vm, Ref<ModuleInformation>&& moduleInformation, FunctionCodeIndex functionIndex, std::optional<bool> hasExceptionHandlers, Ref<CalleeGroup>&& calleeGroup, CompletionTask&& completionTask)
    : Plan(vm, WTFMove(moduleInformation), WTFMove(completionTask))
    , m_calleeGroup(WTFMove(calleeGroup))
    , m_functionIndex(functionIndex)
    , m_hasExceptionHandlers(hasExceptionHandlers)
{
    ASSERT(Options::useBBQJIT());
    setMode(m_calleeGroup->mode());
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Starting BBQ plan for ", functionIndex);
}

FunctionAllowlist& BBQPlan::ensureGlobalBBQAllowlist()
{
    static LazyNeverDestroyed<FunctionAllowlist> bbqAllowlist;
    static std::once_flag initializeAllowlistFlag;
    std::call_once(initializeAllowlistFlag, [] {
        const char* functionAllowlistFile = Options::bbqAllowlist();
        bbqAllowlist.construct(functionAllowlistFile);
    });
    return bbqAllowlist;
}

bool BBQPlan::dumpDisassembly(CompilationContext& context, LinkBuffer& linkBuffer, FunctionCodeIndex functionIndex, const TypeDefinition& signature, FunctionSpaceIndex functionIndexSpace)
{
    if (UNLIKELY(shouldDumpDisassemblyFor(CompilationMode::BBQMode))) {
        dataLogF("Generated BBQ code for WebAssembly BBQ function[%zu] %s name %s\n", functionIndex.rawIndex(), signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data());
        if (context.bbqDisassembler)
            context.bbqDisassembler->dump(linkBuffer);
        linkBuffer.didAlreadyDisassemble();
        return true;
    }
    return false;
}

void BBQPlan::work(CompilationEffort)
{
    ASSERT(m_calleeGroup->runnable());
    CompilationContext context;
    Vector<UnlinkedWasmToWasmCall> unlinkedWasmToWasmCalls;
    FunctionSpaceIndex functionIndexSpace = m_moduleInformation->toSpaceIndex(m_functionIndex);
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

    bool usesSIMD = m_moduleInformation->usesSIMD(m_functionIndex);
    SavedFPWidth savedFPWidth = usesSIMD ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors;
    Ref<BBQCallee> callee = BBQCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), savedFPWidth);
    std::unique_ptr<InternalFunction> function = compileFunction(m_functionIndex, callee.get(), context, unlinkedWasmToWasmCalls);

    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, callee.ptr(), LinkBuffer::Profile::WasmBBQ, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index "_s, m_functionIndex.rawIndex()), Plan::Error::OutOfMemory);
        return;
    }

    Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations;
    Vector<CodeLocationLabel<WasmEntryPtrTag>> loopEntrypointLocations;
    computeExceptionHandlerAndLoopEntrypointLocations(exceptionHandlerLocations, loopEntrypointLocations, function.get(), context, linkBuffer);

    computePCToCodeOriginMap(context, linkBuffer);

    bool alreadyDumped = dumpDisassembly(context, linkBuffer, m_functionIndex, signature, functionIndexSpace);
    function->entrypoint.compilation = makeUnique<Compilation>(
        FINALIZE_CODE_IF((!alreadyDumped && shouldDumpDisassemblyFor(CompilationMode::BBQMode)), linkBuffer, JITCompilationPtrTag, nullptr, "WebAssembly BBQ function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
        WTFMove(context.wasmEntrypointByproducts));

    CodePtr<WasmEntryPtrTag> entrypoint;
    std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint;
    if (function->bbqSharedLoopEntrypoint)
        sharedLoopEntrypoint = linkBuffer.locationOf<WasmEntryPtrTag>(*function->bbqSharedLoopEntrypoint);

    {
        callee->setEntrypoint(WTFMove(function->entrypoint), WTFMove(unlinkedWasmToWasmCalls), WTFMove(function->stackmaps), WTFMove(function->exceptionHandlers), WTFMove(exceptionHandlerLocations), WTFMove(loopEntrypointLocations), sharedLoopEntrypoint, function->osrEntryScratchBufferSize);
        entrypoint = callee->entrypoint();

        if (context.pcToCodeOriginMap)
            NativeCalleeRegistry::singleton().addPCToCodeOriginMap(callee.ptr(), WTFMove(context.pcToCodeOriginMap));

        // We want to make sure we publish our callee at the same time as we link our callsites. This enables us to ensure we
        // always call the fastest code. Any function linked after us will see our new code and the new callsites, which they
        // will update. It's also ok if they publish their code before we reset the instruction caches because after we release
        // the lock our code is ready to be published too.
        Locker locker { m_calleeGroup->m_lock };

        m_calleeGroup->setBBQCallee(locker, m_functionIndex, callee.copyRef());
        ASSERT(m_calleeGroup->replacement(locker, callee->index()) == callee.ptr());
        m_calleeGroup->reportCallees(locker, callee.ptr(), function->outgoingJITDirectCallees);

        for (auto& call : callee->wasmToWasmCallsites()) {
            CodePtr<WasmEntryPtrTag> entrypoint;
            Wasm::Callee* calleeCallee = nullptr;
            if (call.functionIndexSpace < m_moduleInformation->importFunctionCount())
                entrypoint = m_calleeGroup->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
            else {
                calleeCallee = &m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace);
                entrypoint = m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace).entrypoint().retagged<WasmEntryPtrTag>();
            }

            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
            MacroAssembler::repatchPointer(call.calleeLocation, CalleeBits::boxNativeCalleeIfExists(calleeCallee));
        }

        m_calleeGroup->updateCallsitesToCallUs(locker, CodeLocationLabel<WasmEntryPtrTag>(entrypoint), m_functionIndex);

        {
            // These locks store barrier the set of OMG callee above.
            if (Options::useWasmIPInt()) {
                IPIntCallee& ipintCallee = m_calleeGroup->m_ipintCallees->at(m_functionIndex).get();
                Locker locker { ipintCallee.tierUpCounter().m_lock };
                ipintCallee.tierUpCounter().setCompilationStatus(mode(), IPIntTierUpCounter::CompilationStatus::Compiled);
            } else {
                LLIntCallee& llintCallee = m_calleeGroup->m_llintCallees->at(m_functionIndex).get();
                Locker locker { llintCallee.tierUpCounter().m_lock };
                llintCallee.tierUpCounter().setCompilationStatus(mode(), LLIntTierUpCounter::CompilationStatus::Compiled);
            }
        }
    }

    dataLogLnIf(WasmBBQPlanInternal::verbose, "Finished BBQ ", m_functionIndex);

    Locker locker { m_lock };
    complete();
}

std::unique_ptr<InternalFunction> BBQPlan::compileFunction(FunctionCodeIndex functionIndex, BBQCallee& callee, CompilationContext& context, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    FunctionSpaceIndex functionIndexSpace = m_moduleInformation->toSpaceIndex(functionIndex);
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace) == typeIndex);
    Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileResult;

    beginCompilerSignpost(callee);
    parseAndCompileResult = parseAndCompileBBQ(context, callee, function, signature, unlinkedWasmToWasmCalls, m_moduleInformation.get(), m_mode, functionIndex, m_hasExceptionHandlers, UINT32_MAX);
    endCompilerSignpost(callee);

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(makeString(parseAndCompileResult.error(), ", in function at index "_s, functionIndex.rawIndex())); // FIXME: make this an Expected.
        }
        return nullptr;
    }

    return WTFMove(*parseAndCompileResult);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_BBQJIT)
