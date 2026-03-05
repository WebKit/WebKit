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
#include "WasmFaultSignalHandler.h"
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

BBQPlan::BBQPlan(VM& vm, Ref<ModuleInformation>&& moduleInformation, FunctionCodeIndex functionIndex, Ref<IPIntCallee>&& profiledCallee, Ref<Module>&& module, Ref<CalleeGroup>&& calleeGroup, CompletionTask&& completionTask)
    : Plan(vm, WTF::move(moduleInformation), WTF::move(completionTask))
    , m_profiledCallee(WTF::move(profiledCallee))
    , m_module(WTF::move(module))
    , m_calleeGroup(WTF::move(calleeGroup))
    , m_functionIndex(functionIndex)
{
    ASSERT(Options::useBBQJIT());
    setMode(m_calleeGroup->mode());
    Wasm::activateSignalingMemory();
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

bool BBQPlan::dumpDisassembly(CompilationContext& context, LinkBuffer& linkBuffer, const TypeDefinition& signature, FunctionSpaceIndex functionIndexSpace)
{
    if (shouldDumpDisassemblyFor(CompilationMode::BBQMode)) [[unlikely]] {
        dataLogLn("Generated BBQ functionIndexSpace:(", functionIndexSpace, "),sig:(", signature.toString().ascii().data(), "),name:(", m_profiledCallee->nameWithHash(), "),wasmSize:(", m_moduleInformation->functionWasmSizeImportSpace(functionIndexSpace), ")");
        if (context.bbqDisassembler)
            context.bbqDisassembler->dump(linkBuffer);
        linkBuffer.didAlreadyDisassemble();
        return true;
    }
    return false;
}

void BBQPlan::work()
{
    ASSERT(m_calleeGroup->runnable());
    CompilationContext context;
    Vector<UnlinkedWasmToWasmCall> unlinkedWasmToWasmCalls;
    FunctionSpaceIndex functionIndexSpace = m_moduleInformation->toSpaceIndex(m_functionIndex);
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

    Ref<BBQCallee> callee = BBQCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), Ref { m_profiledCallee });
    std::unique_ptr<InternalFunction> function = compileFunction(m_functionIndex, callee.get(), context, unlinkedWasmToWasmCalls);

    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, callee.ptr(), LinkBuffer::Profile::WasmBBQ, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) [[unlikely]] {
        fail(makeString("Out of executable memory while tiering up function at index "_s, m_functionIndex.rawIndex()), CompilationError::OutOfMemory);
        return;
    }

    Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations;
    Vector<CodeLocationLabel<WasmEntryPtrTag>> loopEntrypointLocations;
    computeExceptionHandlerAndLoopEntrypointLocations(exceptionHandlerLocations, loopEntrypointLocations, function.get(), context, linkBuffer);

    if (context.pcToCodeOriginMapBuilder)
        context.pcToCodeOriginMap = Box<PCToCodeOriginMap>::create(WTF::move(*context.pcToCodeOriginMapBuilder), linkBuffer);

    bool alreadyDumped = dumpDisassembly(context, linkBuffer, signature, functionIndexSpace);
    function->entrypoint.compilation = makeUnique<Compilation>(
        FINALIZE_CODE_IF((!alreadyDumped && shouldDumpDisassemblyFor(CompilationMode::BBQMode)), linkBuffer, JITCompilationPtrTag, nullptr, "BBQ functionIndexSpace:(", functionIndexSpace, "),sig:(", signature.toString().ascii().data(), "),name:(", callee->nameWithHash().ascii().data(), "),wasmSize:(", m_moduleInformation->functionWasmSizeImportSpace(functionIndexSpace), ")"),
        WTF::move(context.wasmEntrypointByproducts));

    CodePtr<WasmEntryPtrTag> entrypoint;
    std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint;
    if (function->bbqSharedLoopEntrypoint)
        sharedLoopEntrypoint = linkBuffer.locationOf<WasmEntryPtrTag>(*function->bbqSharedLoopEntrypoint);

    {
        callee->setEntrypoint(WTF::move(function->entrypoint), WTF::move(unlinkedWasmToWasmCalls), WTF::move(function->stackmaps), WTF::move(function->exceptionHandlers), WTF::move(exceptionHandlerLocations), WTF::move(loopEntrypointLocations), sharedLoopEntrypoint, function->osrEntryScratchBufferSize);
        entrypoint = callee->entrypoint();

        if (context.pcToCodeOriginMap)
            NativeCalleeRegistry::singleton().addPCToCodeOriginMap(callee.ptr(), WTF::move(context.pcToCodeOriginMap));

        {
            Locker locker { m_calleeGroup->m_lock };
            m_calleeGroup->installOptimizedCallee(locker, m_moduleInformation, m_functionIndex, callee.copyRef(), function->outgoingJITDirectCallees);
        }
        {
            Locker locker { m_profiledCallee->tierUpCounter().m_lock };
            m_profiledCallee->tierUpCounter().setCompilationStatus(mode(), IPIntTierUpCounter::CompilationStatus::Compiled);
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
    RELEASE_ASSERT(mode() == m_calleeGroup->mode());
    parseAndCompileResult = parseAndCompileBBQ(context, m_profiledCallee.get(), callee, function, signature, unlinkedWasmToWasmCalls, m_module.get(), m_calleeGroup.get(), m_moduleInformation.get(), m_mode, functionIndex);
    endCompilerSignpost(callee);

    if (!parseAndCompileResult) [[unlikely]] {
        fail(makeString(parseAndCompileResult.error(), ", in function at index "_s, functionIndex.rawIndex()), CompilationError::Parse); // FIXME: make this an Expected.
        return nullptr;
    }

    return WTF::move(*parseAndCompileResult);
}

void BBQPlan::fail(String&& errorMessage, CompilationError error)
{
    {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            Base::fail(WTF::move(errorMessage), error);
        }
    }
    {
        Locker locker { m_profiledCallee->tierUpCounter().m_lock };
        m_profiledCallee->tierUpCounter().setCompilationStatus(mode(), IPIntTierUpCounter::CompilationStatus::Failed);
        m_profiledCallee->tierUpCounter().setCompilationError(mode(), error);
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_BBQJIT)
