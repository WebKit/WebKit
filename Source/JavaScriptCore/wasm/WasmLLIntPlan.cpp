/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "B3Compilation.h"
#include "BytecodeDumper.h"
#include "CalleeBits.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "WasmCallee.h"
#include "WasmLLIntGenerator.h"
#include "WasmSignatureInlines.h"
#include "WasmValidate.h"

namespace JSC { namespace Wasm {

namespace WasmLLIntPlanInternal {
static const bool verbose = false;
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
    SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
    const Signature& signature = SignatureInformation::get(signatureIndex);
    unsigned functionIndexSpace = m_wasmToWasmExitStubs.size() + functionIndex;
    ASSERT_UNUSED(functionIndexSpace, m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace) == signatureIndex);
    ASSERT(validateFunction(function, signature, m_moduleInformation.get()));

    m_unlinkedWasmToWasmCalls[functionIndex] = Vector<UnlinkedWasmToWasmCall>();
    Expected<std::unique_ptr<FunctionCodeBlock>, String> parseAndCompileResult = parseAndCompileBytecode(function.data.data(), function.data.size(), signature, m_moduleInformation.get(), functionIndex, m_throwWasmException);

    if (UNLIKELY(!parseAndCompileResult)) {
        auto locker = holdLock(m_lock);
        if (!m_errorMessage) {
            // Multiple compiles could fail simultaneously. We arbitrarily choose the first.
            fail(locker, makeString(parseAndCompileResult.error(), ", in function at index ", String::number(functionIndex))); // FIXME make this an Expected.
        }
        m_currentIndex = m_moduleInformation->functions.size();
        return;
    }

    m_wasmInternalFunctions[functionIndex] = WTFMove(*parseAndCompileResult);

    if (m_exportedFunctionIndices.contains(functionIndex) || m_moduleInformation->referencedFunctions().contains(functionIndex)) {
        EmbederToWasmFunction entry;
        entry.jit = makeUnique<CCallHelpers>();
        entry.function = m_createEmbedderWrapper(*entry.jit, signature, &m_unlinkedWasmToWasmCalls[functionIndex], m_moduleInformation.get(), m_mode, functionIndex);
        auto locker = holdLock(m_lock);
        auto result = m_embedderToWasmInternalFunctions.add(functionIndex, WTFMove(entry));
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}

void LLIntPlan::didCompleteCompilation(const AbstractLocker& locker)
{
    for (uint32_t functionIndex = 0; functionIndex < m_moduleInformation->functions.size(); functionIndex++) {
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature& signature = SignatureInformation::get(signatureIndex);

        if (auto it = m_embedderToWasmInternalFunctions.find(functionIndex); it != m_embedderToWasmInternalFunctions.end()) {
            LinkBuffer linkBuffer(*it->value.jit, nullptr, JITCompilationCanFail);
            if (UNLIKELY(linkBuffer.didFailToAllocate())) {
                Base::fail(locker, makeString("Out of executable memory in function entrypoint at index ", String::number(functionIndex)));
                return;
            }

            it->value.function->entrypoint.compilation = makeUnique<B3::Compilation>(
                FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "Embedder->WebAssembly entrypoint[%i] %s", functionIndex, signature.toString().ascii().data()),
                nullptr);
        }
    }

    unsigned functionCount = m_wasmInternalFunctions.size();
    if (functionCount) {
        // LLInt entrypoint thunks generation
        CCallHelpers jit;
        m_callees.resize(functionCount);
        Vector<CCallHelpers::Label> entrypoints(functionCount);
        Vector<CCallHelpers::Jump> jumps(functionCount);
        for (unsigned i = 0; i < functionCount; ++i) {
            size_t functionIndexSpace = i + m_moduleInformation->importFunctionCount();

            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                BytecodeDumper::dumpBlock(m_wasmInternalFunctions[i].get(), m_moduleInformation, WTF::dataFile());

            m_callees[i] = Wasm::LLIntCallee::create(WTFMove(m_wasmInternalFunctions[i]), functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace));
            entrypoints[i] = jit.label();
#if CPU(X86_64)
            CCallHelpers::Address calleeSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CPURegister));
#elif CPU(ARM64)
            CCallHelpers::Address calleeSlot(CCallHelpers::stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC));
#else
#error Unsupported architecture.
#endif
            jit.storePtr(CCallHelpers::TrustedImmPtr(CalleeBits::boxWasm(m_callees[i].ptr())), calleeSlot);
            jumps[i] = jit.jump();
        }

        LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, JITCompilationCanFail);
        if (UNLIKELY(linkBuffer.didFailToAllocate())) {
            Base::fail(locker, "Out of executable memory in Wasm LLInt entry thunks");
            return;
        }

        for (unsigned i = 0; i < functionCount; ++i) {
            m_callees[i]->setEntrypoint(linkBuffer.locationOf<WasmEntryPtrTag>(entrypoints[i]));
            linkBuffer.link<JITThunkPtrTag>(jumps[i], CodeLocationLabel<JITThunkPtrTag>(LLInt::wasmFunctionEntryThunk().code()));
        }

        m_entryThunks = FINALIZE_CODE(linkBuffer, B3CompilationPtrTag, "Wasm LLInt entry thunks");
    }

    for (auto& unlinked : m_unlinkedWasmToWasmCalls) {
        for (auto& call : unlinked) {
            MacroAssemblerCodePtr<WasmEntryPtrTag> executableAddress;
            if (m_moduleInformation->isImportedFunctionFromFunctionIndexSpace(call.functionIndexSpace)) {
                // FIXME imports could have been linked in B3, instead of generating a patchpoint. This condition should be replaced by a RELEASE_ASSERT. https://bugs.webkit.org/show_bug.cgi?id=166462
                executableAddress = m_wasmToWasmExitStubs.at(call.functionIndexSpace).code();
            } else
                executableAddress = m_callees[call.functionIndexSpace - m_moduleInformation->importFunctionCount()]->entrypoint();
            MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(executableAddress));
        }
    }
}

void LLIntPlan::initializeCallees(const CalleeInitializer& callback)
{
    ASSERT(!failed());
    for (unsigned internalFunctionIndex = 0; internalFunctionIndex < m_wasmInternalFunctions.size(); ++internalFunctionIndex) {
        RefPtr<Wasm::Callee> embedderEntrypointCallee;
        if (auto it = m_embedderToWasmInternalFunctions.find(internalFunctionIndex); it != m_embedderToWasmInternalFunctions.end()) {
            embedderEntrypointCallee = Wasm::EmbedderEntrypointCallee::create(WTFMove(it->value.function->entrypoint));
            if (it->value.function->calleeMoveLocation)
                MacroAssembler::repatchPointer(it->value.function->calleeMoveLocation, CalleeBits::boxWasm(embedderEntrypointCallee.get()));
        }

        callback(internalFunctionIndex, WTFMove(embedderEntrypointCallee), WTFMove(m_callees[internalFunctionIndex]));
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
