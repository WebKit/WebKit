/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "WasmOMGPlan.h"
#include "JSToWasm.h"

#if ENABLE(WEBASSEMBLY_OMGJIT)

#include "JITCompilation.h"
#include "LinkBuffer.h"
#include "NativeCalleeRegistry.h"
#include "VMInspector.h"
#include "WasmCallee.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmNameSection.h"
#include "WasmOMGIRGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/ScopedPrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

namespace WasmOMGPlanInternal {
static constexpr bool verbose = false;
}

OMGPlan::OMGPlan(VM& vm, Ref<Module>&& module, FunctionCodeIndex functionIndex, std::optional<bool> hasExceptionHandlers, MemoryMode mode, CompletionTask&& task)
    : Base(vm, const_cast<ModuleInformation&>(module->moduleInformation()), WTFMove(task))
    , m_module(WTFMove(module))
    , m_calleeGroup(*m_module->calleeGroupFor(mode))
    , m_hasExceptionHandlers(hasExceptionHandlers)
    , m_functionIndex(functionIndex)
{
    ASSERT(Options::useOMGJIT());
    setMode(mode);
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(m_mode));
    dataLogLnIf(WasmOMGPlanInternal::verbose, "[", m_moduleInformation->toSpaceIndex(m_functionIndex), "]: Starting OMG plan for ", functionIndex, " of module: ", RawPointer(&m_module.get()));
}

FunctionAllowlist& OMGPlan::ensureGlobalOMGAllowlist()
{
    static LazyNeverDestroyed<FunctionAllowlist> omgAllowlist;
    static std::once_flag initializeAllowlistFlag;
    std::call_once(initializeAllowlistFlag, [] {
        const char* functionAllowlistFile = Options::omgAllowlist();
        omgAllowlist.construct(functionAllowlistFile);
    });
    return omgAllowlist;
}

void OMGPlan::dumpDisassembly(CompilationContext& context, LinkBuffer& linkBuffer, FunctionCodeIndex functionIndex, const TypeDefinition& signature, FunctionSpaceIndex functionIndexSpace)
{
    dataLogLnIf(context.procedure->shouldDumpIR() || shouldDumpDisassemblyFor(CompilationMode::OMGMode), "Generated OMG code for WebAssembly OMG function[", functionIndex, "] ", signature.toString().ascii().data(), " name ", makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data());
    if (UNLIKELY(shouldDumpDisassemblyFor(CompilationMode::OMGMode))) {
        ScopedPrintStream out;
        auto* disassembler = context.procedure->code().disassembler();

        const char* b3Prefix = "b3    ";
        const char* airPrefix = "Air        ";
        const char* asmPrefix = "asm              ";

        B3::Value* prevOrigin = nullptr;
        auto forEachInst = scopedLambda<void(B3::Air::Inst&)>([&] (B3::Air::Inst& inst) {
            if (inst.origin && inst.origin != prevOrigin && context.procedure->code().shouldPreserveB3Origins()) {
                if (String string = inst.origin->compilerConstructionSite(); !string.isNull())
                    out.println("\033[1;37m", string, "\033[0m");
                out.print(b3Prefix);
                inst.origin->deepDump(context.procedure.get(), out);
                out.println();
                prevOrigin = inst.origin;
            }
        });

        disassembler->dump(context.procedure->code(), out, linkBuffer, airPrefix, asmPrefix, forEachInst);
        linkBuffer.didAlreadyDisassemble();
    }
}

void OMGPlan::work(CompilationEffort)
{
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
    const FunctionData& function = m_moduleInformation->functions[m_functionIndex];

    const FunctionSpaceIndex functionIndexSpace = m_moduleInformation->toSpaceIndex(m_functionIndex);
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

    Ref<OMGCallee> callee = OMGCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace));

    beginCompilerSignpost(callee.get());
    Vector<UnlinkedWasmToWasmCall> unlinkedCalls;
    CompilationContext context;
    auto parseAndCompileResult = parseAndCompileOMG(context, callee.get(), function, signature, unlinkedCalls, m_calleeGroup.get(), m_moduleInformation.get(), m_mode, CompilationMode::OMGMode, m_functionIndex, m_hasExceptionHandlers, UINT32_MAX);
    endCompilerSignpost(callee.get());

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        fail(makeString(parseAndCompileResult.error(), "when trying to tier up "_s, m_functionIndex.rawIndex()), Plan::Error::Parse);
        return;
    }

    Entrypoint omgEntrypoint;
    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, callee.ptr(), LinkBuffer::Profile::WasmOMG, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index "_s, m_functionIndex.rawIndex()), Plan::Error::OutOfMemory);
        return;
    }

    InternalFunction* internalFunction = parseAndCompileResult->get();
    Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations;
    computeExceptionHandlerLocations(exceptionHandlerLocations, internalFunction, context, linkBuffer);

    computePCToCodeOriginMap(context, linkBuffer);

    {
        ScopedPrintStream out;
        dumpDisassembly(context, linkBuffer, m_functionIndex, signature, functionIndexSpace);
        omgEntrypoint.compilation = makeUnique<Compilation>(
            FINALIZE_CODE_IF(context.procedure->shouldDumpIR(), linkBuffer, JITCompilationPtrTag, nullptr, "WebAssembly OMG function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
            WTFMove(context.wasmEntrypointByproducts));
    }

    omgEntrypoint.calleeSaveRegisters = WTFMove(internalFunction->entrypoint.calleeSaveRegisters);

    CodePtr<WasmEntryPtrTag> entrypoint;
    {
        ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
        callee->setEntrypoint(WTFMove(omgEntrypoint), WTFMove(unlinkedCalls), WTFMove(internalFunction->stackmaps), WTFMove(internalFunction->exceptionHandlers), WTFMove(exceptionHandlerLocations));
        entrypoint = callee->entrypoint();

        if (context.pcToCodeOriginMap)
            NativeCalleeRegistry::singleton().addPCToCodeOriginMap(callee.ptr(), WTFMove(context.pcToCodeOriginMap));

        // We want to make sure we publish our callee at the same time as we link our callsites. This enables us to ensure we
        // always call the fastest code. Any function linked after us will see our new code and the new callsites, which they
        // will update. It's also ok if they publish their code before we reset the instruction caches because after we release
        // the lock our code is ready to be published too.
        Locker locker { m_calleeGroup->m_lock };

        m_calleeGroup->setOMGCallee(locker, m_functionIndex, callee.copyRef());
        ASSERT(m_calleeGroup->replacement(locker, callee->index()) == callee.ptr());
        m_calleeGroup->reportCallees(locker, callee.ptr(), internalFunction->outgoingJITDirectCallees);

        for (auto& call : callee->wasmToWasmCallsites()) {
            CodePtr<WasmEntryPtrTag> entrypoint;
            Wasm::Callee* calleeCallee = nullptr;
            if (call.functionIndexSpace < m_module->moduleInformation().importFunctionCount())
                entrypoint = m_calleeGroup->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
            else {
                calleeCallee = &m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace);
                entrypoint = m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace).entrypoint().retagged<WasmEntryPtrTag>();
            }

            // FIXME: This does an icache flush for each of these... which doesn't make any sense since this code isn't runnable here
            // and any stale cache will be evicted when updateCallsitesToCallUs is called.
            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
            MacroAssembler::repatchPointer(call.calleeLocation, CalleeBits::boxNativeCalleeIfExists(calleeCallee));
        }

        m_calleeGroup->updateCallsitesToCallUs(locker, CodeLocationLabel<WasmEntryPtrTag>(entrypoint), m_functionIndex);
        ASSERT(*m_calleeGroup->entrypointLoadLocationFromFunctionIndexSpace(functionIndexSpace) == entrypoint);

        {
            // These locks store barrier the set of OMG callee above.
            if (BBQCallee* bbqCallee = m_calleeGroup->bbqCallee(locker, m_functionIndex)) {
                Locker locker { bbqCallee->tierUpCounter().getLock() };
                bbqCallee->tierUpCounter().setCompilationStatusForOMG(mode(), TierUpCount::CompilationStatus::Compiled);
            }
            if (Options::useWasmIPInt() && m_calleeGroup->m_ipintCallees) {
                IPIntCallee& ipintCallee = m_calleeGroup->m_ipintCallees->at(m_functionIndex).get();
                Locker locker { ipintCallee.tierUpCounter().m_lock };
                ipintCallee.tierUpCounter().setCompilationStatus(mode(), IPIntTierUpCounter::CompilationStatus::Compiled);
            }
            if (!Options::useWasmIPInt() && m_calleeGroup->m_llintCallees) {
                LLIntCallee& llintCallee = m_calleeGroup->m_llintCallees->at(m_functionIndex).get();
                Locker locker { llintCallee.tierUpCounter().m_lock };
                llintCallee.tierUpCounter().setCompilationStatus(mode(), LLIntTierUpCounter::CompilationStatus::Compiled);
            }
        }
    }

    auto* jsEntrypointCallee = m_calleeGroup->m_jsEntrypointCallees.get(m_functionIndex);
    if (jsEntrypointCallee)
        jsEntrypointCallee->setReplacementTarget(entrypoint);

    if (Options::freeRetiredWasmCode()) {
        // Taking the lock here fences all the stores above.
        Locker locker { m_calleeGroup->m_lock };
        m_calleeGroup->releaseBBQCallee(locker, m_functionIndex);
    }

    dataLogLnIf(WasmOMGPlanInternal::verbose, "Finished OMG ", m_functionIndex);
    Locker locker { m_lock };
    complete();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
