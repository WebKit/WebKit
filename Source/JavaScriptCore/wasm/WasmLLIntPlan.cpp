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
#include "LLIntData.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmLLIntGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/GraphNodeWorklist.h>

namespace JSC { namespace Wasm {

LLIntPlan::LLIntPlan(VM& vm, Vector<uint8_t>&& source, CompilerMode compilerMode, CompletionTask&& task)
    : Base(vm, WTFMove(source), compilerMode, WTFMove(task))
{
    if (parseAndValidateModule(m_source.span()))
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
    if (!tryReserveCapacity(m_wasmInternalFunctions, functions.size(), " WebAssembly functions"_s))
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
    auto parseAndCompileResult = parseAndCompileBytecode(function.data, signature, m_moduleInformation.get(), functionIndex);

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

#if USE(JSVALUE64)
bool LLIntPlan::makeInterpretedJSToWasmCallee(unsigned functionIndex)
{
    TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    const FunctionSignature& functionSignature = *signature.as<FunctionSignature>();
    if (!Options::useIPIntWrappers()
        || m_moduleInformation->memoryCount() > 1
        || m_moduleInformation->usesSIMD(functionIndex)
        || functionSignature.argumentCount() > 16
        || functionSignature.returnCount() > 16)
        return false;

    RegisterSet registersToSpill = RegisterSetBuilder::wasmPinnedRegisters();
    registersToSpill.add(GPRInfo::regCS1, IgnoreVectors);
    if (!isARM64()) {
        // We use some additional registers, see js_to_wasm_wrapper_entry
        registersToSpill.add(GPRInfo::regCS2, IgnoreVectors);
    }
#if CPU(ARM64) || CPU(ARMv7)
    const size_t JSEntrypointInterpreterCalleeSaveSpaceStackAligned = WTF::roundUpToMultipleOf(stackAlignmentBytes(), 4 * sizeof(CPURegister));
#else
    const size_t JSEntrypointInterpreterCalleeSaveSpaceStackAligned = WTF::roundUpToMultipleOf(stackAlignmentBytes(), 8 * sizeof(CPURegister));
#endif
    size_t totalFrameSize = JSEntrypointInterpreterCalleeSaveSpaceStackAligned;
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    RegisterAtOffsetList savedResultRegisters = wasmFrameConvention.computeResultsOffsetList();
    totalFrameSize += wasmFrameConvention.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.sizeOfAreaInBytes();
    totalFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), totalFrameSize);

    Vector<JSEntrypointInterpreterCalleeMetadata> metadata;
    metadata.append(JSEntrypointInterpreterCalleeMetadata::FrameSize);
    metadata.append(static_cast<JSEntrypointInterpreterCalleeMetadata>(safeCast<int8_t>((totalFrameSize - JSEntrypointInterpreterCalleeSaveSpaceStackAligned) / 8)));

    if (m_moduleInformation->memoryCount())
        metadata.append(JSEntrypointInterpreterCalleeMetadata::Memory);

    CallInformation jsFrameConvention = jsCallingConvention().callInformationFor(signature, CallRole::Callee);
    // This offset converts a caller-perspective SP-relative offset to a caller-perspective FP-relative offset.
    // To the js code, we are the callee. To the wasm code, we are the caller.
    intptr_t spToFP = -safeCast<int>(totalFrameSize);

    for (unsigned i = 0; i < functionSignature.argumentCount(); ++i) {
        RELEASE_ASSERT(jsFrameConvention.params[i].location.isStack());

        Type type = functionSignature.argumentType(i);
        auto jsParam = static_cast<JSEntrypointInterpreterCalleeMetadata>(safeCast<int8_t >(
            (jsFrameConvention.params[i].location.offsetFromFP() + static_cast<int>(PayloadOffset)) / 8));

        if (!type.isI32())
            return false; // TODO: eventually we should support this

        if (wasmFrameConvention.params[i].location.isStackArgument()) {
            auto wasmParam = static_cast<JSEntrypointInterpreterCalleeMetadata>(safeCast<int8_t>(
                (spToFP + wasmFrameConvention.params[i].location.offsetFromSP() + static_cast<int>(PayloadOffset)) / 8));
            auto wasmParamTag = static_cast<JSEntrypointInterpreterCalleeMetadata>(safeCast<int8_t>(
                (spToFP + wasmFrameConvention.params[i].location.offsetFromSP() + static_cast<int>(TagOffset)) / 8));
            auto loadType = (type.width() == Width32) ? JSEntrypointInterpreterCalleeMetadata::LoadI32 : JSEntrypointInterpreterCalleeMetadata::LoadI64;
            auto storeType = (type.width() == Width32) ? JSEntrypointInterpreterCalleeMetadata::StoreI32 : JSEntrypointInterpreterCalleeMetadata::StoreI64;
            metadata.append(loadType);
            metadata.append(jsParam);
            metadata.append(storeType);
            metadata.append(wasmParam);

            if (!is64Bit() && loadType == JSEntrypointInterpreterCalleeMetadata::LoadI32) {
                metadata.append(JSEntrypointInterpreterCalleeMetadata::Zero);
                metadata.append(JSEntrypointInterpreterCalleeMetadata::StoreI32);
                metadata.append(wasmParamTag);
            }
        } else {
            if (type.isF32() || type.isF64()) {
                metadata.append(type.isF64() ? JSEntrypointInterpreterCalleeMetadata::LoadF64 : JSEntrypointInterpreterCalleeMetadata::LoadF32);
                metadata.append(jsParam);
                metadata.append(jsEntrypointMetadataForFPR(wasmFrameConvention.params[i].location.fpr(), MetadataReadMode::Write));
            } else if (type.isI32() || type.isI64()) {
                metadata.append(type.isI64() ? JSEntrypointInterpreterCalleeMetadata::LoadI64 : JSEntrypointInterpreterCalleeMetadata::LoadI32);
                metadata.append(jsParam);
                metadata.append(jsEntrypointMetadataForGPR(wasmFrameConvention.params[i].location.jsr().payloadGPR(), MetadataReadMode::Write));

                if (!is64Bit() && type.isI32()) {
                    metadata.append(JSEntrypointInterpreterCalleeMetadata::Zero);
                    metadata.append(jsEntrypointMetadataForGPR(wasmFrameConvention.params[i].location.jsr().tagGPR(), MetadataReadMode::Write));
                }
            } else
                return false;
        }
    }

    metadata.append(JSEntrypointInterpreterCalleeMetadata::Call);

    if (functionSignature.returnsVoid()) {
        metadata.append(JSEntrypointInterpreterCalleeMetadata::Undefined);
        metadata.append(jsEntrypointMetadataForGPR(JSRInfo::returnValueJSR.payloadGPR(), MetadataReadMode::Write));
        if constexpr (!is64Bit()) {
            metadata.append(JSEntrypointInterpreterCalleeMetadata::ShiftTag);
            metadata.append(jsEntrypointMetadataForGPR(JSRInfo::returnValueJSR.tagGPR(), MetadataReadMode::Write));
        }
    } else if (functionSignature.returnCount() == 1) {
        if (!functionSignature.returnType(0).isI32())
            return false; // TODO: eventually we should support this

        JSValueRegs inputJSR = wasmFrameConvention.results[0].location.jsr();
        JSValueRegs outputJSR = jsFrameConvention.results[0].location.jsr();
        metadata.append(jsEntrypointMetadataForGPR(inputJSR.payloadGPR(), MetadataReadMode::Read));
        if (functionSignature.returnType(0).isI64())
            metadata.append(JSEntrypointInterpreterCalleeMetadata::BoxInt64);
        else if (functionSignature.returnType(0).isI32())
            metadata.append(JSEntrypointInterpreterCalleeMetadata::BoxInt32);
        else
            return false; // TODO
        metadata.append(jsEntrypointMetadataForGPR(outputJSR.payloadGPR(), MetadataReadMode::Write));
        if (!is64Bit()) {
            metadata.append(JSEntrypointInterpreterCalleeMetadata::ShiftTag);
            metadata.append(jsEntrypointMetadataForGPR(outputJSR.tagGPR(), MetadataReadMode::Write));
        }
    } else
        return false;
    metadata.append(JSEntrypointInterpreterCalleeMetadata::Done);

    if ((false))
        dumpJSEntrypointInterpreterCalleeMetadata(metadata);

    auto callee = JSEntrypointInterpreterCallee::create(WTFMove(metadata), &m_callees[functionIndex].get());
    auto result = m_jsEntrypointCallees.add(functionIndex, WTFMove(callee));
    ASSERT_UNUSED(result, result.isNewEntry);

    return true;
}
#else
bool LLIntPlan::makeInterpretedJSToWasmCallee(unsigned) { return false; }
#endif

void LLIntPlan::didCompleteCompilation()
{
    unsigned functionCount = m_wasmInternalFunctions.size();
    if (!m_callees && functionCount) {
        m_calleesVector.resize(functionCount);

        for (unsigned i = 0; i < functionCount; ++i) {
            size_t functionIndexSpace = i + m_moduleInformation->importFunctionCount();

            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                BytecodeDumper::dumpBlock(m_wasmInternalFunctions[i].get(), m_moduleInformation, WTF::dataFile());

            m_calleesVector[i] = LLIntCallee::create(*m_wasmInternalFunctions[i], functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace));
            ASSERT(!m_calleesVector[i]->entrypoint());
        }

#if ENABLE(JIT)
        // LLInt entrypoint thunks generation
        CCallHelpers jit;

        if (LIKELY(Options::useJIT())) {
            Vector<CCallHelpers::Label> entrypoints(functionCount);
            Vector<CCallHelpers::Jump> jumps(functionCount);
            for (unsigned i = 0; i < functionCount; ++i) {
                entrypoints[i] = jit.label();
                if (m_moduleInformation->usesSIMD(i))
                    JIT_COMMENT(jit, "SIMD function entrypoint");
                JIT_COMMENT(jit, "Entrypoint for function[", i, "]");
                {
                    CCallHelpers::Address calleeSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - prologueStackPointerDelta());
                    jit.loadPtr(calleeSlot.withOffset(PayloadOffset), GPRInfo::nonPreservedNonArgumentGPR0);
                    auto good = jit.branchPtr(MacroAssembler::Equal, GPRInfo::nonPreservedNonArgumentGPR0,
                        MacroAssembler::TrustedImmPtr(reinterpret_cast<uint64_t>(CalleeBits::boxNativeCallee(m_calleesVector[i].ptr()))));
                    jit.breakpoint();
                    good.link(&jit);
                }
                jumps[i] = jit.jump();
            }

            LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail("Out of executable memory in Wasm LLInt entry thunks"_s);
                return;
            }

            for (unsigned i = 0; i < functionCount; ++i) {
                m_calleesVector[i]->setEntrypoint(linkBuffer.locationOf<WasmEntryPtrTag>(entrypoints[i]));
                if (m_moduleInformation->usesSIMD(i))
                    linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(LLInt::wasmFunctionEntryThunkSIMD().code()));
                else
                    linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(LLInt::wasmFunctionEntryThunk().code()));
            }

            m_entryThunks = FINALIZE_WASM_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "Wasm LLInt entry thunks");
        } else
#endif
        {
            for (unsigned i = 0; i < functionCount; ++i)
                m_calleesVector[i]->setEntrypoint(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(wasm_function_prologue_trampoline));
        }
        m_callees = m_calleesVector.data();
    }

    if (!m_moduleInformation->clobberingTailCalls().isEmpty())
        computeTransitiveTailCalls();

    if (m_compilerMode == CompilerMode::Validation)
        return;

    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        const uint32_t functionIndexSpace = functionIndex + m_moduleInformation->importFunctionCount();
        if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->hasReferencedFunction(functionIndexSpace)) {
            if (makeInterpretedJSToWasmCallee(functionIndex))
                continue;

            if (!LIKELY(Options::useJIT())) {
                Base::fail(makeString("JIT is disabled, but the entrypoint for "_s, functionIndex, " requires JIT"_s));
                return;
            }

#if ENABLE(JIT)
            CCallHelpers jit;

            TypeIndex typeIndex = m_moduleInformation->internalFunctionTypeIndices[functionIndex];
            const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();

            // The LLInt always bounds checks
            MemoryMode mode = MemoryMode::BoundsChecking;

            auto callee = JSEntrypointJITCallee::create();
            std::unique_ptr<InternalFunction> function = createJSToWasmWrapper(jit, callee.get(), m_callees[functionIndex].ptr(), signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), mode, functionIndex);

            LinkBuffer linkBuffer(jit, nullptr, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail(makeString("Out of executable memory in function entrypoint at index "_s, functionIndex));
                return;
            }

            function->entrypoint.compilation = makeUnique<Compilation>(
                FINALIZE_WASM_CODE(linkBuffer, JITCompilationPtrTag, nullptr, "JS->WebAssembly entrypoint[%i] %s", functionIndex, signature.toString().ascii().data()),
                nullptr);

            callee->setEntrypoint(WTFMove(function->entrypoint));

            auto result = m_jsEntrypointCallees.add(functionIndex, WTFMove(callee));
            ASSERT_UNUSED(result, result.isNewEntry);
#endif
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
