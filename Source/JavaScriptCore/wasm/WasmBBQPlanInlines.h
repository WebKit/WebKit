/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "CalleeBits.h"
#include "WasmBBQPlan.h"
#include "WasmCallee.h"
#include "WasmNameSection.h"

namespace JSC { namespace Wasm {

template<typename Functor>
void BBQPlan::initializeCallees(const Functor& callback)
{
    ASSERT(!failed());
    for (unsigned internalFunctionIndex = 0; internalFunctionIndex < m_wasmInternalFunctions.size(); ++internalFunctionIndex) {

        RefPtr<Wasm::Callee> embedderEntrypointCallee;
        if (auto embedderToWasmFunction = m_embedderToWasmInternalFunctions.get(internalFunctionIndex)) {
            embedderEntrypointCallee = Wasm::EmbedderEntrypointCallee::create(WTFMove(embedderToWasmFunction->entrypoint));
            MacroAssembler::repatchPointer(embedderToWasmFunction->calleeMoveLocation, CalleeBits::boxWasm(embedderEntrypointCallee.get()));
        }

        InternalFunction* function = m_wasmInternalFunctions[internalFunctionIndex].get();
        size_t functionIndexSpace = internalFunctionIndex + m_moduleInformation->importFunctionCount();
        Ref<Wasm::Callee> wasmEntrypointCallee = Wasm::BBQCallee::create(WTFMove(function->entrypoint), functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace), WTFMove(m_tierUpCounts[internalFunctionIndex]));
        MacroAssembler::repatchPointer(function->calleeMoveLocation, CalleeBits::boxWasm(wasmEntrypointCallee.ptr()));

        callback(internalFunctionIndex, WTFMove(embedderEntrypointCallee), WTFMove(wasmEntrypointCallee));
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
