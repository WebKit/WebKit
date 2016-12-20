/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WasmPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSWebAssemblyCallee.h"
#include "WasmB3IRGenerator.h"
#include "WasmBinding.h"
#include "WasmCallingConvention.h"
#include "WasmMemory.h"
#include "WasmModuleParser.h"
#include "WasmValidate.h"
#include <wtf/DataLog.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;
    
Plan::Plan(VM* vm, Vector<uint8_t> source)
    : Plan(vm, source.data(), source.size())
{
}

Plan::Plan(VM* vm, const uint8_t* source, size_t sourceLength)
    : m_vm(vm)
    , m_source(source)
    , m_sourceLength(sourceLength)
{
}

bool Plan::parseAndValidateModule()
{
    {
        ModuleParser moduleParser(m_vm, m_source, m_sourceLength);
        auto parseResult = moduleParser.parse();
        if (!parseResult) {
            m_errorMessage = parseResult.error();
            return false;
        }
        m_moduleInformation = WTFMove(parseResult->module);
        m_functionLocationInBinary = WTFMove(parseResult->functionLocationInBinary);
        m_functionIndexSpace.size = parseResult->functionIndexSpace.size();
        m_functionIndexSpace.buffer = parseResult->functionIndexSpace.releaseBuffer();
    }

    for (unsigned functionIndex = 0; functionIndex < m_functionLocationInBinary.size(); ++functionIndex) {
        if (verbose)
            dataLogLn("Processing function starting at: ", m_functionLocationInBinary[functionIndex].start, " and ending at: ", m_functionLocationInBinary[functionIndex].end);
        const uint8_t* functionStart = m_source + m_functionLocationInBinary[functionIndex].start;
        size_t functionLength = m_functionLocationInBinary[functionIndex].end - m_functionLocationInBinary[functionIndex].start;
        ASSERT(Checked<uintptr_t>(bitwise_cast<uintptr_t>(functionStart)) + functionLength <= Checked<uintptr_t>(bitwise_cast<uintptr_t>(m_source)) + m_sourceLength);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature* signature = SignatureInformation::get(m_vm, signatureIndex);

        auto validationResult = validateFunction(m_vm, functionStart, functionLength, signature, m_functionIndexSpace, *m_moduleInformation);
        if (!validationResult) {
            if (verbose) {
                for (unsigned i = 0; i < functionLength; ++i)
                    dataLog(RawPointer(reinterpret_cast<void*>(functionStart[i])), ", ");
                dataLogLn();
            }
            m_errorMessage = validationResult.error(); // FIXME make this an Expected.
            return false;
        }
    }

    return true;
}

void Plan::run()
{
    if (!parseAndValidateModule())
        return;

    auto tryReserveCapacity = [this] (auto& vector, size_t size, const char* what) {
        if (UNLIKELY(!vector.tryReserveCapacity(size))) {
            StringBuilder builder;
            builder.appendLiteral("Failed allocating enough space for ");
            builder.appendNumber(size);
            builder.append(what);
            m_errorMessage = builder.toString();
            return false;
        }
        return true;
    };

    Vector<Vector<UnlinkedWasmToWasmCall>> unlinkedWasmToWasmCalls;
    if (!tryReserveCapacity(m_wasmToJSStubs, m_moduleInformation->importFunctionSignatureIndices.size(), " WebAssembly to JavaScript stubs")
        || !tryReserveCapacity(unlinkedWasmToWasmCalls, m_functionLocationInBinary.size(), " unlinked WebAssembly to WebAssembly calls")
        || !tryReserveCapacity(m_wasmInternalFunctions, m_functionLocationInBinary.size(), " WebAssembly functions"))
        return;

    for (unsigned importIndex = 0; importIndex < m_moduleInformation->imports.size(); ++importIndex) {
        Import* import = &m_moduleInformation->imports[importIndex];
        if (import->kind != ExternalKind::Function)
            continue;
        unsigned importFunctionIndex = m_wasmToJSStubs.size();
        if (verbose)
            dataLogLn("Processing import function number ", importFunctionIndex, ": ", import->module, ": ", import->field);
        SignatureIndex signatureIndex = m_moduleInformation->importFunctionSignatureIndices.at(import->kindIndex);
        m_wasmToJSStubs.uncheckedAppend(importStubGenerator(m_vm, m_callLinkInfos, signatureIndex, importFunctionIndex));
        m_functionIndexSpace.buffer.get()[importFunctionIndex].code = m_wasmToJSStubs[importFunctionIndex].code().executableAddress();
    }

    for (unsigned functionIndex = 0; functionIndex < m_functionLocationInBinary.size(); ++functionIndex) {
        if (verbose)
            dataLogLn("Processing function starting at: ", m_functionLocationInBinary[functionIndex].start, " and ending at: ", m_functionLocationInBinary[functionIndex].end);
        const uint8_t* functionStart = m_source + m_functionLocationInBinary[functionIndex].start;
        size_t functionLength = m_functionLocationInBinary[functionIndex].end - m_functionLocationInBinary[functionIndex].start;
        ASSERT(functionLength <= m_sourceLength);
        SignatureIndex signatureIndex = m_moduleInformation->internalFunctionSignatureIndices[functionIndex];
        const Signature* signature = SignatureInformation::get(m_vm, signatureIndex);
        unsigned functionIndexSpace = m_wasmToJSStubs.size() + functionIndex;
        ASSERT(m_functionIndexSpace.buffer.get()[functionIndexSpace].signatureIndex == signatureIndex);

        ASSERT(validateFunction(m_vm, functionStart, functionLength, signature, m_functionIndexSpace, *m_moduleInformation));

        unlinkedWasmToWasmCalls.uncheckedAppend(Vector<UnlinkedWasmToWasmCall>());
        auto parseAndCompileResult = parseAndCompile(*m_vm, functionStart, functionLength, signature, unlinkedWasmToWasmCalls.at(functionIndex), m_functionIndexSpace, *m_moduleInformation);
        if (UNLIKELY(!parseAndCompileResult)) {
            m_errorMessage = parseAndCompileResult.error();
            return; // FIXME make this an Expected.
        }
        m_wasmInternalFunctions.uncheckedAppend(WTFMove(*parseAndCompileResult));
        m_functionIndexSpace.buffer.get()[functionIndexSpace].code = m_wasmInternalFunctions[functionIndex]->wasmEntrypoint.compilation->code().executableAddress();
    }

    // Patch the call sites for each WebAssembly function.
    for (auto& unlinked : unlinkedWasmToWasmCalls) {
        for (auto& call : unlinked)
            MacroAssembler::repatchCall(call.callLocation, CodeLocationLabel(m_functionIndexSpace.buffer.get()[call.functionIndex].code));
    }

    m_failed = false;
}

void Plan::initializeCallees(JSGlobalObject* globalObject, std::function<void(unsigned, JSWebAssemblyCallee*, JSWebAssemblyCallee*)> callback)
{
    ASSERT(!failed());
    for (unsigned internalFunctionIndex = 0; internalFunctionIndex < m_wasmInternalFunctions.size(); ++internalFunctionIndex) {
        WasmInternalFunction* function = m_wasmInternalFunctions[internalFunctionIndex].get();

        JSWebAssemblyCallee* jsEntrypointCallee = JSWebAssemblyCallee::create(globalObject->vm(), WTFMove(function->jsToWasmEntrypoint));
        MacroAssembler::repatchPointer(function->jsToWasmCalleeMoveLocation, jsEntrypointCallee);

        JSWebAssemblyCallee* wasmEntrypointCallee = JSWebAssemblyCallee::create(globalObject->vm(), WTFMove(function->wasmEntrypoint));
        MacroAssembler::repatchPointer(function->wasmCalleeMoveLocation, wasmEntrypointCallee);

        callback(internalFunctionIndex, jsEntrypointCallee, wasmEntrypointCallee);
    }
}

Plan::~Plan() { }

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
