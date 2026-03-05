/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "WasmCallee.h"
#include "WasmFaultSignalHandler.h"
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

OMGPlan::OMGPlan(VM& vm, Ref<Module>&& module, FunctionCodeIndex functionIndex, MemoryMode mode, CompletionTask&& task)
    : Base(vm, const_cast<ModuleInformation&>(module->moduleInformation()), WTF::move(task))
    , m_module(WTF::move(module))
    , m_calleeGroup(*m_module->calleeGroupFor(mode))
    , m_functionIndex(functionIndex)
{
    ASSERT(Options::useOMGJIT());
    setMode(mode);
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(m_mode));
    Wasm::activateSignalingMemory();
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

void OMGPlan::dumpDisassembly(const Callee& callee, CompilationContext& context, LinkBuffer& linkBuffer, const TypeDefinition& signature, FunctionSpaceIndex functionIndexSpace)
{
    dataLogLnIf(context.procedure->shouldDumpIR() || shouldDumpDisassemblyFor(CompilationMode::OMGMode), "Generated OMG functionIndexSpace:(", functionIndexSpace, "),sig:(", signature.toString().ascii().data(), "),name:(", callee.nameWithHash(), "),wasmSize:(", m_moduleInformation->functionWasmSizeImportSpace(functionIndexSpace), ")");
    if (shouldDumpDisassemblyFor(CompilationMode::OMGMode)) [[unlikely]] {
        ScopedPrintStream out;
        UncheckedKeyHashSet<B3::Value*> printedValues;
        auto* disassembler = context.procedure->code().disassembler();

        const char* b3Prefix = "b3    ";
        const char* airPrefix = "Air        ";
        const char* asmPrefix = "asm              ";

        B3::Value* prevOrigin = nullptr;
        auto forEachInst = scopedLambda<void(B3::Air::Inst&)>([&] (B3::Air::Inst& inst) {
            if (inst.origin && inst.origin != prevOrigin && context.procedure->code().shouldPreserveB3Origins()) {
                if (String string = inst.origin->compilerConstructionSite(); !string.isNull())
                    out.println(string);
                Vector<B3::Value*> children;
                Vector<B3::Value*> worklist;
                children.append(inst.origin);
                worklist.append(inst.origin);
                while (!worklist.isEmpty()) {
                    B3::Value* current = worklist.takeLast();
                    for (B3::Value* child : current->children()) {
                        if (printedValues.add(child).isNewEntry) {
                            children.append(child);
                            worklist.append(child);
                        }
                    }
                }
                for (size_t i = children.size(); i--;) {
                    out.print(b3Prefix);
                    children[i]->deepDump(context.procedure.get(), out);
                    out.println();
                }

                prevOrigin = inst.origin;
            }
        });

        disassembler->dump(context.procedure->code(), out, linkBuffer, airPrefix, asmPrefix, forEachInst);
        linkBuffer.didAlreadyDisassemble();
    }
}

void OMGPlan::work()
{
    ASSERT(m_calleeGroup->runnable());
    ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
    const FunctionData& function = m_moduleInformation->functions[m_functionIndex];

    const FunctionSpaceIndex functionIndexSpace = m_moduleInformation->toSpaceIndex(m_functionIndex);
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[m_functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

    Ref<IPIntCallee> profiledCallee = m_calleeGroup->ipintCalleeFromFunctionIndexSpace(functionIndexSpace);
    Ref<OMGCallee> callee = OMGCallee::create(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), Ref { profiledCallee });

    beginCompilerSignpost(callee.get());
    Vector<UnlinkedWasmToWasmCall> unlinkedCalls;
    CompilationContext context;
    auto parseAndCompileResult = parseAndCompileOMG(context, profiledCallee.get(), callee.get(), function, signature, unlinkedCalls, m_module.get(), m_calleeGroup.get(), m_moduleInformation.get(), m_mode, CompilationMode::OMGMode, m_functionIndex, UINT32_MAX);
    endCompilerSignpost(callee.get());

    if (!parseAndCompileResult) [[unlikely]] {
        Locker locker { m_lock };
        fail(makeString(parseAndCompileResult.error(), "when trying to tier up "_s, m_functionIndex.rawIndex()), CompilationError::Parse);
        return;
    }

    Entrypoint omgEntrypoint;
    LinkBuffer linkBuffer(*context.wasmEntrypointJIT, callee.ptr(), LinkBuffer::Profile::WasmOMG, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) [[unlikely]] {
        Locker locker { m_lock };
        Base::fail(makeString("Out of executable memory while tiering up function at index "_s, m_functionIndex.rawIndex()), CompilationError::OutOfMemory);
        return;
    }

    InternalFunction* internalFunction = parseAndCompileResult->get();
    Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations;
    computeExceptionHandlerLocations(exceptionHandlerLocations, internalFunction, context, linkBuffer);

    auto samplingProfilerMap = callee->materializePCToOriginMap(context.procedure->releasePCToOriginMap(), linkBuffer);
    {
        ScopedPrintStream out;
        dumpDisassembly(callee.get(), context, linkBuffer, signature, functionIndexSpace);
        omgEntrypoint.compilation = makeUnique<Compilation>(
            FINALIZE_CODE_IF(context.procedure->shouldDumpIR(), linkBuffer, JITCompilationPtrTag, nullptr, "OMG functionIndexSpace:(", functionIndexSpace, "),sig:(", signature.toString().ascii().data(), "),name:(", makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))).ascii().data(), "),wasmSize:(", m_moduleInformation->functionWasmSizeImportSpace(functionIndexSpace), ")"),
            WTF::move(context.wasmEntrypointByproducts));
    }

    omgEntrypoint.calleeSaveRegisters = WTF::move(internalFunction->entrypoint.calleeSaveRegisters);

    bool newlyInstalled = false;
    CodePtr<WasmEntryPtrTag> entrypoint;
    {
        ASSERT(m_calleeGroup.ptr() == m_module->calleeGroupFor(mode()));
        callee->setEntrypoint(WTF::move(omgEntrypoint), WTF::move(unlinkedCalls), WTF::move(internalFunction->stackmaps), WTF::move(internalFunction->exceptionHandlers), WTF::move(exceptionHandlerLocations));
        entrypoint = callee->entrypoint();

        if (samplingProfilerMap)
            NativeCalleeRegistry::singleton().addPCToCodeOriginMap(callee.ptr(), WTF::move(samplingProfilerMap));

        Locker locker { m_calleeGroup->m_lock };
        newlyInstalled = m_calleeGroup->installOptimizedCallee(locker, m_moduleInformation, m_functionIndex, callee.copyRef(), internalFunction->outgoingJITDirectCallees);

        if (newlyInstalled) {
            if (RefPtr bbqCallee = m_calleeGroup->bbqCallee(locker, m_functionIndex)) {
                Locker locker { bbqCallee->tierUpCounter().getLock() };
                bbqCallee->tierUpCounter().setCompilationStatusForOMG(mode(), TierUpCount::CompilationStatus::Compiled);
            }
            IPIntCallee& ipintCallee = m_calleeGroup->m_ipintCallees->at(m_functionIndex).get();
            Locker locker { ipintCallee.tierUpCounter().m_lock };
            ipintCallee.tierUpCounter().setCompilationStatus(mode(), IPIntTierUpCounter::CompilationStatus::Compiled);
        }
    }

    if (newlyInstalled) {
        if (Options::freeRetiredWasmCode()) {
            WTF::storeStoreFence();
            Locker locker { m_calleeGroup->m_lock };
            m_calleeGroup->releaseBBQCallee(locker, m_functionIndex);
        }
    }

    dataLogLnIf(WasmOMGPlanInternal::verbose, "Finished OMG ", m_functionIndex);
    Locker locker { m_lock };
    complete();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
