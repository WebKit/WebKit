/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

BBQPlan::BBQPlan(VM& vm, Ref<ModuleInformation> moduleInformation, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, CalleeGroup* calleeGroup, CompletionTask&& completionTask)
    : EntryPlan(vm, WTFMove(moduleInformation), CompilerMode::FullCompile, WTFMove(completionTask))
    , m_calleeGroup(calleeGroup)
    , m_functionIndex(functionIndex)
    , m_hasExceptionHandlers(hasExceptionHandlers)
{
    ASSERT(Options::useBBQJIT());
    setMode(m_calleeGroup->mode());
    dataLogLnIf(WasmBBQPlanInternal::verbose, "Starting BBQ plan for ", functionIndex);
    m_areWasmToJSStubsCompiled = true;
    m_areWasmToWasmStubsCompiled = true;
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

bool BBQPlan::prepareImpl()
{
    const auto& functions = m_moduleInformation->functions;
    if (!tryReserveCapacity(m_wasmInternalFunctions, functions.size(), " WebAssembly functions"_s)
        || !tryReserveCapacity(m_wasmInternalFunctionLinkBuffers, functions.size(), " compilation contexts"_s)
        || !tryReserveCapacity(m_compilationContexts, functions.size(), " compilation contexts"_s)
        || !tryReserveCapacity(m_callees, functions.size(), " BBQ callees"_s)
        || !tryReserveCapacity(m_allLoopEntrypoints, functions.size(), " loop entrypoints"_s))
        return false;

    m_wasmInternalFunctions.resize(functions.size());
    m_wasmInternalFunctionLinkBuffers.resize(functions.size());
    m_exceptionHandlerLocations.resize(functions.size());
    m_compilationContexts.resize(functions.size());
    m_callees.resize(functions.size());
    m_allLoopEntrypoints.resize(functions.size());

    return true;
}

bool BBQPlan::dumpDisassembly(CompilationContext& context, LinkBuffer& linkBuffer, unsigned functionIndex, const TypeDefinition& signature, unsigned functionIndexSpace)
{
    if (UNLIKELY(shouldDumpDisassemblyFor(CompilationMode::BBQMode))) {
        dataLogF("Generated BBQ code for WebAssembly BBQ function[%i] %s name %s\n", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data());
        if (context.bbqDisassembler)
            context.bbqDisassembler->dump(linkBuffer);
        linkBuffer.didAlreadyDisassemble();
        return true;
    }
    return false;
}

void BBQPlan::work(CompilationEffort effort)
{
    if (!m_calleeGroup) {
        switch (m_state) {
        case State::Initial:
            parseAndValidateModule();
            if (!hasWork()) {
                ASSERT(m_state == State::Validated);
                Locker locker { m_lock };
                complete();
                break;
            }
            FALLTHROUGH;
        case State::Validated:
            prepare();
            break;
        case State::Prepared:
            compileFunctions(effort);
            break;
        default:
            break;
        }
        return;
    }

    CompilationContext context;
    Vector<UnlinkedWasmToWasmCall> unlinkedWasmToWasmCalls;
    std::unique_ptr<TierUpCount> tierUp;
    tierUp = makeUnique<TierUpCount>();
    size_t functionIndexSpace = m_functionIndex + m_moduleInformation->importFunctionCount();
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

    bool usesSIMD = m_moduleInformation->usesSIMD(m_functionIndex);
    SavedFPWidth savedFPWidth = usesSIMD ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors;
    auto* tierUpPointer = tierUp.get();
    Ref<BBQCallee> callee = BBQCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), WTFMove(tierUp), savedFPWidth);
    std::unique_ptr<InternalFunction> function = compileFunction(m_functionIndex, callee.get(), context, unlinkedWasmToWasmCalls, tierUpPointer);

    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, callee.ptr(), LinkBuffer::Profile::WasmBBQ, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index "_s, m_functionIndex), Plan::Error::OutOfMemory);
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

        m_calleeGroup->callsiteCollection().addCallsites(locker, *m_calleeGroup, callee->wasmToWasmCallsites());
        m_calleeGroup->callsiteCollection().updateCallsitesToCallUs(locker, *m_calleeGroup, CodeLocationLabel<WasmEntryPtrTag>(entrypoint), m_functionIndex, functionIndexSpace);

        {
            if (Options::useWasmIPInt()) {
                IPIntCallee& ipintCallee = m_calleeGroup->m_ipintCallees->at(m_functionIndex).get();
                Locker locker { ipintCallee.tierUpCounter().m_lock };
                ipintCallee.setReplacement(callee.copyRef(), mode());
                ipintCallee.tierUpCounter().setCompilationStatus(mode(), IPIntTierUpCounter::CompilationStatus::Compiled);
            } else {
                LLIntCallee& llintCallee = m_calleeGroup->m_llintCallees->at(m_functionIndex).get();
                Locker locker { llintCallee.tierUpCounter().m_lock };
                llintCallee.setReplacement(callee.copyRef(), mode());
                llintCallee.tierUpCounter().setCompilationStatus(mode(), LLIntTierUpCounter::CompilationStatus::Compiled);
            }
        }
    }

    // Replace the LLInt interpreted entry callee. Note that we can do this after we publish our
    // callee because calling into the LLInt should still work.
    auto* jsEntrypointCallee = m_calleeGroup->m_jsEntrypointCallees.get(m_functionIndex);
    if (jsEntrypointCallee && jsEntrypointCallee->compilationMode() == CompilationMode::JITLessJSEntrypointMode && !static_cast<JITLessJSEntrypointCallee*>(jsEntrypointCallee)->hasReplacement()) {
        Locker locker { m_lock };
        TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
        const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

        auto callee = JSEntrypointJITCallee::create();
        context.jsEntrypointJIT = makeUnique<CCallHelpers>();
        Vector<UnlinkedWasmToWasmCall> newCall;
        auto jsToWasmInternalFunction = createJSToWasmWrapper(*context.jsEntrypointJIT, callee.get(), nullptr, signature, &newCall, m_moduleInformation.get(), m_mode, m_functionIndex);
        auto linkBuffer = makeUnique<LinkBuffer>(*context.jsEntrypointJIT, &callee.get(), LinkBuffer::Profile::WasmBBQ, JITCompilationCanFail);

        if (linkBuffer->isValid()) {
            jsToWasmInternalFunction->entrypoint.compilation = makeUnique<Compilation>(
                FINALIZE_WASM_CODE(*linkBuffer, JITCompilationPtrTag, nullptr, "(ipint upgrade edition) JS->WebAssembly entrypoint[%i] %s", m_functionIndex, signature.toString().ascii().data()),
                nullptr);

            for (auto& call : newCall) {
                CodePtr<WasmEntryPtrTag> entrypoint;
                if (call.functionIndexSpace < m_moduleInformation->importFunctionCount())
                    entrypoint = m_calleeGroup->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
                else
                    entrypoint = m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace).entrypoint().retagged<WasmEntryPtrTag>();

                MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
            }

            callee->setEntrypoint(WTFMove(jsToWasmInternalFunction->entrypoint));
            Locker locker { m_calleeGroup->m_lock };
            // Note that we can compile the same function with multiple memory modes, which can cause this
            // race. That's fine, both stubs should do the same thing.
            static_cast<JITLessJSEntrypointCallee*>(jsEntrypointCallee)->setReplacement(callee.ptr());

            auto result = m_jsToWasmInternalFunctions.add(m_functionIndex, std::tuple { WTFMove(callee), WTFMove(linkBuffer), WTFMove(jsToWasmInternalFunction) });
            ASSERT_UNUSED(result, result.isNewEntry);
        }
    }

    dataLogLnIf(WasmBBQPlanInternal::verbose, "Finished BBQ ", m_functionIndex);

    Locker locker { m_lock };
    moveToState(State::Completed);
    runCompletionTasks();
}

void BBQPlan::compileFunction(uint32_t functionIndex)
{
    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();

    std::unique_ptr<TierUpCount> tierUp;
    if (Options::useBBQTierUpChecks())
        tierUp = makeUnique<TierUpCount>();

    unsigned functionIndexSpace = m_moduleInformation->importFunctionCount() + functionIndex;
    bool usesSIMD = m_moduleInformation->usesSIMD(functionIndex);
    SavedFPWidth savedFPWidth = usesSIMD ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors;
    auto* tierUpPointer = tierUp.get();
    auto& context = m_compilationContexts[functionIndex];
    Ref<BBQCallee> bbqCallee = BBQCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), WTFMove(tierUp), savedFPWidth);
    auto* calleePtr = bbqCallee.ptr();
    m_wasmInternalFunctions[functionIndex] = compileFunction(functionIndex, bbqCallee.get(), context, m_unlinkedWasmToWasmCalls[functionIndex], tierUpPointer);
    {
        auto linkBuffer = makeUnique<LinkBuffer>(*context.wasmEntrypointJIT, calleePtr, LinkBuffer::Profile::WasmBBQ, JITCompilationCanFail);
        if (linkBuffer->isValid())
            m_wasmInternalFunctionLinkBuffers[functionIndex] = WTFMove(linkBuffer);
    }
    m_callees[functionIndex] = WTFMove(bbqCallee);

    if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->hasReferencedFunction(functionIndexSpace)) {
        Locker locker { m_lock };
        TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
        const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

        auto callee = JSEntrypointJITCallee::create();
        context.jsEntrypointJIT = makeUnique<CCallHelpers>();
        auto jsToWasmInternalFunction = createJSToWasmWrapper(*context.jsEntrypointJIT, callee.get(), nullptr, signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), m_mode, functionIndex);
        auto linkBuffer = makeUnique<LinkBuffer>(*context.jsEntrypointJIT, calleePtr, LinkBuffer::Profile::WasmBBQ, JITCompilationCanFail);

        auto result = m_jsToWasmInternalFunctions.add(functionIndex, std::tuple { WTFMove(callee), WTFMove(linkBuffer), WTFMove(jsToWasmInternalFunction) });
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}

std::unique_ptr<InternalFunction> BBQPlan::compileFunction(uint32_t functionIndex, BBQCallee& callee, CompilationContext& context, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, TierUpCount* tierUp)
{
    const auto& function = m_moduleInformation->functions[functionIndex];
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    unsigned functionIndexSpace = m_moduleInformation->importFunctionCount() + functionIndex;
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace) == typeIndex);
    Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileResult;

#if ENABLE(WEBASSEMBLY_BBQJIT)
    parseAndCompileResult = parseAndCompileBBQ(context, callee, function, signature, unlinkedWasmToWasmCalls, m_moduleInformation.get(), m_mode, functionIndex, m_hasExceptionHandlers, UINT32_MAX, tierUp);
#endif

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(makeString(parseAndCompileResult.error(), ", in function at index "_s, functionIndex)); // FIXME make this an Expected.
        }
        m_currentIndex = m_moduleInformation->functions.size();
        return nullptr;
    }

    return WTFMove(*parseAndCompileResult);
}

void BBQPlan::didCompleteCompilation()
{
    generateStubsIfNecessary();

    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        CompilationContext& context = m_compilationContexts[functionIndex];
        TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
        const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
        const uint32_t functionIndexSpace = functionIndex + m_moduleInformation->importFunctionCount();
        ASSERT(functionIndexSpace < m_moduleInformation->functionIndexSpaceSize());
        {
            InternalFunction* function = m_wasmInternalFunctions[functionIndex].get();
            if (!m_wasmInternalFunctionLinkBuffers[functionIndex]) {
                Base::fail(makeString("Out of executable memory in function at index "_s, functionIndex));
                return;
            }
            
            auto& linkBuffer = *m_wasmInternalFunctionLinkBuffers[functionIndex];

            computeExceptionHandlerAndLoopEntrypointLocations(m_exceptionHandlerLocations[functionIndex], m_allLoopEntrypoints[functionIndex], function, context, linkBuffer);

            computePCToCodeOriginMap(context, linkBuffer);

            bool alreadyDumped = dumpDisassembly(context, linkBuffer, functionIndex, signature, functionIndexSpace);
            function->entrypoint.compilation = makeUnique<Compilation>(
                FINALIZE_CODE_IF((!alreadyDumped && shouldDumpDisassemblyFor(CompilationMode::BBQMode)), linkBuffer, JITCompilationPtrTag, nullptr, "WebAssembly BBQ function[%i] %s name %s", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
                WTFMove(context.wasmEntrypointByproducts));
        }

        {
            auto iter = m_jsToWasmInternalFunctions.find(functionIndex);
            if (iter != m_jsToWasmInternalFunctions.end()) {
                LinkBuffer& linkBuffer = *std::get<1>(iter->value);
                const auto& jsToWasmInternalFunction = std::get<2>(iter->value);

                if (linkBuffer.didFailToAllocate()) {
                    Base::fail(makeString("Out of executable memory in function entrypoint at index "_s, functionIndex));
                    return;
                }

                jsToWasmInternalFunction->entrypoint.compilation = makeUnique<Compilation>(
                    FINALIZE_WASM_CODE_FOR_MODE(CompilationMode::BBQMode, linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly entrypoint[%i] %s name %s", functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()), nullptr);
            }
        }
    }

    for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
        for (auto& call : unlinked) {
            CodePtr<WasmEntryPtrTag> executableAddress;
            if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndexSpace)) {
                // FIXME imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT. https://bugs.webkit.org/show_bug.cgi?id=166462
                executableAddress = m_wasmToWasmExitStubs.at(call.functionIndexSpace).code();
            } else
                executableAddress = m_wasmInternalFunctions.at(call.functionIndexSpace - m_moduleInformation->importFunctionCount())->entrypoint.compilation->code().retagged<WasmEntryPtrTag>();
            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(executableAddress));
            if (call.calleeLocation)
                MacroAssembler::repatchPointer(call.calleeLocation, 0);
        }
    }
}

void BBQPlan::initializeCallees(const CalleeInitializer& callback)
{
    ASSERT(!failed());
    for (unsigned internalFunctionIndex = 0; internalFunctionIndex < m_wasmInternalFunctions.size(); ++internalFunctionIndex) {
        RefPtr<JSEntrypointCallee> jsEntrypointCallee;
        RefPtr<BBQCallee> wasmEntrypointCallee = m_callees[internalFunctionIndex];
        {
            auto iter = m_jsToWasmInternalFunctions.find(internalFunctionIndex);
            if (iter != m_jsToWasmInternalFunctions.end()) {
                const auto& jsToWasmFunction = std::get<2>(iter->value);
                jsEntrypointCallee = std::get<0>(iter->value);
                if (jsEntrypointCallee->compilationMode() == CompilationMode::JSEntrypointJITMode)
                    static_cast<JSEntrypointJITCallee*>(jsEntrypointCallee.get())->setEntrypoint(WTFMove(jsToWasmFunction->entrypoint));
            }
        }

        InternalFunction* function = m_wasmInternalFunctions[internalFunctionIndex].get();
        wasmEntrypointCallee->setEntrypoint(WTFMove(function->entrypoint), WTFMove(m_unlinkedWasmToWasmCalls[internalFunctionIndex]), WTFMove(function->stackmaps), WTFMove(function->exceptionHandlers), WTFMove(m_exceptionHandlerLocations[internalFunctionIndex]), WTFMove(m_allLoopEntrypoints[internalFunctionIndex]), { }, function->osrEntryScratchBufferSize);

        if (m_compilationContexts[internalFunctionIndex].pcToCodeOriginMap)
            NativeCalleeRegistry::singleton().addPCToCodeOriginMap(wasmEntrypointCallee.get(), WTFMove(m_compilationContexts[internalFunctionIndex].pcToCodeOriginMap));

        callback(internalFunctionIndex, WTFMove(jsEntrypointCallee), wasmEntrypointCallee.releaseNonNull());
    }
}

bool BBQPlan::didReceiveFunctionData(unsigned, const FunctionData&)
{
    return true;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_BBQJIT)
