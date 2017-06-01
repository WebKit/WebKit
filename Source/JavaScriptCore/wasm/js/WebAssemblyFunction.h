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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WebAssemblyFunctionBase.h"
#include <wtf/Noncopyable.h>

namespace JSC {

class JSGlobalObject;
struct ProtoCallFrame;
class WebAssemblyInstance;

namespace B3 {
class Compilation;
}

class WebAssemblyFunction : public WebAssemblyFunctionBase {
public:
    using Base = WebAssemblyFunctionBase;

    const static unsigned StructureFlags = Base::StructureFlags;

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static WebAssemblyFunction* create(VM&, JSGlobalObject*, unsigned, const String&, JSWebAssemblyInstance*, Wasm::Callee& jsEntrypoint, Wasm::WasmEntrypointLoadLocation, Wasm::SignatureIndex);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    Wasm::SignatureIndex signatureIndex() const { return m_wasmFunction.signatureIndex; }
    Wasm::WasmEntrypointLoadLocation wasmEntrypointLoadLocation() const { return m_wasmFunction.code; }
    Wasm::CallableFunction callableFunction() const { return m_wasmFunction; }

    void* jsEntrypoint() { return m_jsEntrypoint; }

    static ptrdiff_t offsetOfWasmEntrypointLoadLocation() { return OBJECT_OFFSETOF(WebAssemblyFunction, m_wasmFunction) + Wasm::CallableFunction::offsetOfWasmEntrypointLoadLocation(); }

private:
    WebAssemblyFunction(VM&, JSGlobalObject*, Structure*, Wasm::Callee& jsEntrypoint, Wasm::WasmEntrypointLoadLocation, Wasm::SignatureIndex);

    // It's safe to just hold the raw CallableFunction/jsEntrypoint because we have a reference
    // to our Instance, which points to the Module that exported us, which
    // ensures that the actual Signature/code doesn't get deallocated.
    void* m_jsEntrypoint;
    Wasm::CallableFunction m_wasmFunction;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
