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
#include "WasmOSREntryPlan.h"

#if ENABLE(WEBASSEMBLY_B3JIT)

#include "JITCompilation.h"
#include "LinkBuffer.h"
#include "WasmB3IRGenerator.h"
#include "WasmCallee.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmMachineThreads.h"
#include "WasmNameSection.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

namespace WasmOSREntryPlanInternal {
static constexpr bool verbose = false;
}

OSREntryPlan::OSREntryPlan(VM& vm, Ref<Module>&& module, Ref<Callee>&& callee, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, uint32_t loopIndex, MemoryMode mode, CompletionTask&& task)
    : Base(vm, const_cast<ModuleInformation&>(module->moduleInformation()), WTFMove(task))
    , m_module(WTFMove(module))
    , m_calleeGroup(*m_module->calleeGroupFor(mode))
    , m_callee(WTFMove(callee))
    , m_hasExceptionHandlers(hasExceptionHandlers)
    , m_functionIndex(functionIndex)
    , m_loopIndex(loopIndex)
{
    ASSERT(Options::useOMGJIT());
    setMode(mode);
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(m_mode));
    dataLogLnIf(WasmOSREntryPlanInternal::verbose, "Starting OMGForOSREntry plan for ", functionIndex, " of module: ", RawPointer(&m_module.get()));
}

void OSREntryPlan::dumpDisassembly(CompilationContext& context, LinkBuffer& linkBuffer)
{
    CompilationMode targetCompilationMode = m_callee->compilationMode() == CompilationMode::LLIntMode ? CompilationMode::BBQForOSREntryMode : CompilationMode::OMGForOSREntryMode;
    if (UNLIKELY(shouldDumpDisassemblyFor(targetCompilationMode))) {
        auto* disassembler = context.procedure->code().disassembler();

        const char* b3Prefix = "b3    ";
        const char* airPrefix = "Air        ";
        const char* asmPrefix = "asm              ";

        auto forEachInst = scopedLambda<void(B3::Air::Inst&)>([&] (B3::Air::Inst& inst) {
            if (inst.origin && context.procedure->code().shouldPreserveB3Origins()) {
                if (String string = inst.origin->compilerConstructionSite(); !string.isNull())
                    dataLogLn("\033[1;37m", string, "\033[0m");
                dataLog(b3Prefix);
                inst.origin->deepDump(context.procedure.get(), WTF::dataFile());
                dataLogLn();
            }
        });

        disassembler->dump(context.procedure->code(), WTF::dataFile(), linkBuffer, airPrefix, asmPrefix, forEachInst);
    }
}

void OSREntryPlan::work(CompilationEffort)
{
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
    const FunctionData& function = m_moduleInformation->functions[m_functionIndex];

    const uint32_t functionIndexSpace = m_functionIndex + m_module->moduleInformation().importFunctionCount();
    ASSERT(functionIndexSpace < m_module->moduleInformation().functionIndexSpaceSize());

    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex);

    CompilationMode targetCompilationMode = m_callee->compilationMode() == CompilationMode::LLIntMode ? CompilationMode::BBQForOSREntryMode : CompilationMode::OMGForOSREntryMode;
    Ref<OSREntryCallee> callee = OSREntryCallee::create(targetCompilationMode, functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), m_loopIndex);

    Vector<UnlinkedWasmToWasmCall> unlinkedCalls;
    CompilationContext context;
    auto parseAndCompileResult = parseAndCompileB3(context, callee.get(), function, signature, unlinkedCalls, m_moduleInformation.get(), m_mode, targetCompilationMode, m_functionIndex, m_hasExceptionHandlers, m_loopIndex);

    if (UNLIKELY(!parseAndCompileResult)) {
        Locker locker { m_lock };
        fail(makeString(parseAndCompileResult.error(), "when trying to tier up ", String::number(m_functionIndex)));
        return;
    }

    Entrypoint omgEntrypoint;
    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, nullptr, LinkBuffer::Profile::Wasm, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate())) {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index ", String::number(m_functionIndex)));
        return;
    }

    InternalFunction* internalFunction = parseAndCompileResult->get();
    Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations;
    computeExceptionHandlerLocations(exceptionHandlerLocations, internalFunction, context, linkBuffer);

    dataLogLnIf(context.procedure->shouldDumpIR() || shouldDumpDisassemblyFor(targetCompilationMode), "Generated OMG code for WebAssembly OMGforOSREntry function[", m_functionIndex, "] ", signature.toString().ascii().data(), " name ", makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data());
    dumpDisassembly(context, linkBuffer);
    bool dumpDisassemblyAgain = false;
    omgEntrypoint.compilation = makeUnique<Compilation>(
        FINALIZE_CODE_IF(context.procedure->shouldDumpIR() || dumpDisassemblyAgain, linkBuffer, JITCompilationPtrTag, "WebAssembly OMGForOSREntry function[%i] %s name %s", m_functionIndex, signature.toString().ascii().data(), makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data()),
        WTFMove(context.wasmEntrypointByproducts));

    omgEntrypoint.calleeSaveRegisters = WTFMove(internalFunction->entrypoint.calleeSaveRegisters);

    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
    callee->setEntrypoint(WTFMove(omgEntrypoint), internalFunction->osrEntryScratchBufferSize, WTFMove(unlinkedCalls), WTFMove(internalFunction->stackmaps), WTFMove(internalFunction->exceptionHandlers), WTFMove(exceptionHandlerLocations));
    {
        Locker locker { m_calleeGroup->m_lock };
        for (auto& call : callee->wasmToWasmCallsites()) {
            CodePtr<WasmEntryPtrTag> entrypoint;
            if (call.functionIndexSpace < m_module->moduleInformation().importFunctionCount())
                entrypoint = m_calleeGroup->m_wasmToWasmExitStubs[call.functionIndexSpace].code();
            else
                entrypoint = m_calleeGroup->wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace).entrypoint().retagged<WasmEntryPtrTag>();

            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
        }

        resetInstructionCacheOnAllThreads();
        WTF::storeStoreFence();

        {
            switch (m_callee->compilationMode()) {
            case CompilationMode::LLIntMode: {
                LLIntCallee* llintCallee = static_cast<LLIntCallee*>(m_callee.ptr());
                Locker locker { llintCallee->tierUpCounter().m_lock };
                llintCallee->setOSREntryCallee(callee.copyRef(), mode());
                llintCallee->tierUpCounter().m_loopCompilationStatus = LLIntTierUpCounter::CompilationStatus::Compiled;
                break;
            }
            case CompilationMode::BBQMode: {
                BBQCallee* bbqCallee = static_cast<BBQCallee*>(m_callee.ptr());
                Locker locker { bbqCallee->tierUpCount()->getLock() };
                bbqCallee->setOSREntryCallee(callee.copyRef(), mode());
                bbqCallee->tierUpCount()->osrEntryTriggers()[m_loopIndex] = TierUpCount::TriggerReason::CompilationDone;
                bbqCallee->tierUpCount()->m_compilationStatusForOMGForOSREntry = TierUpCount::CompilationStatus::Compiled;
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }
    dataLogLnIf(WasmOSREntryPlanInternal::verbose, "Finished OMGForOSREntry ", m_functionIndex);
    Locker locker { m_lock };
    complete();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
