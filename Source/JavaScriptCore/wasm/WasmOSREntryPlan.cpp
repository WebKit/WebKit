/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "WasmOSREntryPlan.h"

#if ENABLE(WEBASSEMBLY_OMGJIT)

#include "JITCompilation.h"
#include "LinkBuffer.h"
#include "NativeCalleeRegistry.h"
#include "WasmCallee.h"
#include "WasmFaultSignalHandler.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmMachineThreads.h"
#include "WasmNameSection.h"
#include "WasmOMGIRGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

namespace WasmOSREntryPlanInternal {
static constexpr bool verbose = false;
}

OSREntryPlan::OSREntryPlan(VM& vm, Ref<Module>&& module, Ref<Callee>&& callee, FunctionCodeIndex functionIndex, uint32_t loopIndex, MemoryMode mode, CompletionTask&& task)
    : Base(vm, const_cast<ModuleInformation&>(module->moduleInformation()), WTF::move(task))
    , m_module(WTF::move(module))
    , m_calleeGroup(*m_module->calleeGroupFor(mode))
    , m_callee(WTF::move(callee))
    , m_functionIndex(functionIndex)
    , m_loopIndex(loopIndex)
{
    ASSERT(Options::useOMGJIT());
    setMode(mode);
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(m_mode));
    Wasm::activateSignalingMemory();
    dataLogLnIf(WasmOSREntryPlanInternal::verbose, "Starting OMGForOSREntry plan for ", functionIndex, " of module: ", RawPointer(&m_module.get()));
}

void OSREntryPlan::dumpDisassembly(CompilationContext& context, LinkBuffer& linkBuffer, const TypeDefinition& signature, FunctionSpaceIndex functionIndexSpace)
{
    CompilationMode targetCompilationMode = CompilationMode::OMGForOSREntryMode;
    dataLogLnIf(context.procedure->shouldDumpIR() || shouldDumpDisassemblyFor(targetCompilationMode), "Generated OMGforOSREntry functionIndexSpace:(", functionIndexSpace, "),sig:(", signature.toString().ascii().data(), "),name:(", m_callee->nameWithHash(), "),wasmSize:(", m_moduleInformation->functionWasmSizeImportSpace(functionIndexSpace), ")");
    if (shouldDumpDisassemblyFor(targetCompilationMode)) [[unlikely]] {
        auto* disassembler = context.procedure->code().disassembler();

        const char* b3Prefix = "b3    ";
        const char* airPrefix = "Air        ";
        const char* asmPrefix = "asm              ";

        B3::Value* prevOrigin = nullptr;
        auto forEachInst = scopedLambda<void(B3::Air::Inst&)>([&] (B3::Air::Inst& inst) {
            if (inst.origin && inst.origin != prevOrigin && context.procedure->code().shouldPreserveB3Origins()) {
                if (String string = inst.origin->compilerConstructionSite(); !string.isNull())
                    dataLogLn(string);
                dataLog(b3Prefix);
                inst.origin->deepDump(context.procedure.get(), WTF::dataFile());
                dataLogLn();
                prevOrigin = inst.origin;
            }
        });

        disassembler->dump(context.procedure->code(), WTF::dataFile(), linkBuffer, airPrefix, asmPrefix, forEachInst);
        linkBuffer.didAlreadyDisassemble();
    }
}

void OSREntryPlan::work()
{
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
    const FunctionData& function = m_moduleInformation->functions[m_functionIndex];

    const FunctionSpaceIndex functionIndexSpace = m_module->moduleInformation().toSpaceIndex(m_functionIndex);
    ASSERT(functionIndexSpace < m_module->moduleInformation().functionIndexSpaceSize());

    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

    Ref<IPIntCallee> profiledCallee = m_calleeGroup->ipintCalleeFromFunctionIndexSpace(functionIndexSpace);
    Ref<OMGOSREntryCallee> callee = OMGOSREntryCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), Ref { profiledCallee }, m_loopIndex);

    beginCompilerSignpost(callee.get());
    Vector<UnlinkedWasmToWasmCall> unlinkedCalls;
    CompilationContext context;
    auto parseAndCompileResult = parseAndCompileOMG(context, profiledCallee.get(), callee.get(), function, signature, unlinkedCalls, m_module.get(), m_calleeGroup.get(), m_moduleInformation.get(), m_mode, CompilationMode::OMGForOSREntryMode, m_functionIndex, m_loopIndex);
    endCompilerSignpost(callee.get());

    if (!parseAndCompileResult) [[unlikely]] {
        Locker locker { m_lock };
        fail(makeString(parseAndCompileResult.error(), "when trying to tier up "_s, m_functionIndex.rawIndex()));
        return;
    }

    Entrypoint omgEntrypoint;
    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, callee.ptr(), LinkBuffer::Profile::WasmOMG, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) [[unlikely]] {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index "_s, m_functionIndex.rawIndex()));
        return;
    }

    InternalFunction* internalFunction = parseAndCompileResult->get();
    Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations;
    computeExceptionHandlerLocations(exceptionHandlerLocations, internalFunction, context, linkBuffer);

    auto samplingProfilerMap = callee->materializePCToOriginMap(context.procedure->releasePCToOriginMap(), linkBuffer);

    dumpDisassembly(context, linkBuffer, signature, functionIndexSpace);
    omgEntrypoint.compilation = makeUnique<Compilation>(
        FINALIZE_CODE_IF(context.procedure->shouldDumpIR(), linkBuffer, JITCompilationPtrTag, nullptr, "WebAssembly OMGForOSREntry function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), m_callee->nameWithHash().ascii().data()),
        WTF::move(context.wasmEntrypointByproducts));

    omgEntrypoint.calleeSaveRegisters = WTF::move(internalFunction->entrypoint.calleeSaveRegisters);

    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
    callee->setEntrypoint(WTF::move(omgEntrypoint), internalFunction->osrEntryScratchBufferSize, WTF::move(unlinkedCalls), WTF::move(internalFunction->stackmaps), WTF::move(internalFunction->exceptionHandlers), WTF::move(exceptionHandlerLocations));

    if (samplingProfilerMap)
        NativeCalleeRegistry::singleton().addPCToCodeOriginMap(callee.ptr(), WTF::move(samplingProfilerMap));

    {
        Locker locker { m_calleeGroup->m_lock };
        bool newlyInstalled = m_calleeGroup->recordOMGOSREntryCallee(locker, m_functionIndex, callee.get());
        if (newlyInstalled) {
            m_calleeGroup->reportCallees(locker, callee.ptr(), internalFunction->outgoingJITDirectCallees);

            for (auto& call : callee->wasmToWasmCallsites()) {
                CodePtr<WasmEntryPtrTag> entrypoint;
                if (call.functionIndexSpace < m_module->moduleInformation().importFunctionCount())
                    entrypoint = m_calleeGroup->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
                else {
                    Ref wasmCallee = m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace);
                    entrypoint = wasmCallee->entrypoint().retagged<WasmEntryPtrTag>();
                }

                MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
            }

            resetInstructionCacheOnAllThreads();
            WTF::storeStoreFence();

            {
                switch (m_callee->compilationMode()) {
                case CompilationMode::BBQMode: {
                    BBQCallee* bbqCallee = uncheckedDowncast<BBQCallee>(m_callee.ptr());
                    Locker locker { bbqCallee->tierUpCounter().getLock() };
                    bbqCallee->setOSREntryCallee(callee.copyRef(), mode());
                    bbqCallee->tierUpCounter().osrEntryTriggers()[m_loopIndex] = TierUpCount::TriggerReason::CompilationDone;
                    bbqCallee->tierUpCounter().setCompilationStatusForOMGForOSREntry(mode(), TierUpCount::CompilationStatus::Compiled);
                    break;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
        }
    }

    // We don't register our BBQCallee for deletion because this entrypoint isn't a general one and is only used for loop OSR.

    dataLogLnIf(WasmOSREntryPlanInternal::verbose, "Finished OMGForOSREntry ", m_functionIndex);
    Locker locker { m_lock };
    complete();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
