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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "JSFunction.h"
#include "WasmFormat.h"

namespace JSC {

class JSGlobalObject;
class JSWebAssemblyInstance;
using Wasm::WasmToWasmImportableFunction;
using Wasm::WasmOrJSImportableFunctionCallLinkInfo;

class WebAssemblyFunctionBase : public JSFunction {
public:
    using Base = JSFunction;

    static constexpr unsigned StructureFlags = Base::StructureFlags;

    DECLARE_INFO;

    JSWebAssemblyInstance* instance() const { return m_instance.get(); }

    Wasm::TypeIndex typeIndex() const { return m_importableFunction.typeIndex; }
    Wasm::Type type() const { return { Wasm::TypeKind::Ref, typeIndex() }; }
    WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation() const { return m_importableFunction.entrypointLoadLocation; }
    const uintptr_t* boxedWasmCalleeLoadLocation() const { return m_importableFunction.boxedWasmCalleeLoadLocation; }
    const Wasm::WasmOrJSImportableFunction& importableFunction() const { return m_importableFunction; }
    const Wasm::RTT* rtt() const { return m_importableFunction.rtt; }
    const Wasm::FunctionSignature& signature() const;
    WasmOrJSImportableFunctionCallLinkInfo* callLinkInfo() const { return m_callLinkInfo; }

    static constexpr ptrdiff_t offsetOfInstance() { return OBJECT_OFFSETOF(WebAssemblyFunctionBase, m_instance); }

    static constexpr ptrdiff_t offsetOfSignatureIndex() { return OBJECT_OFFSETOF(WebAssemblyFunctionBase, m_importableFunction) + WasmToWasmImportableFunction::offsetOfSignatureIndex(); }

    static constexpr ptrdiff_t offsetOfEntrypointLoadLocation() { return OBJECT_OFFSETOF(WebAssemblyFunctionBase, m_importableFunction) + WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation(); }
    static constexpr ptrdiff_t offsetOfBoxedWasmCalleeLoadLocation() { return OBJECT_OFFSETOF(WebAssemblyFunctionBase, m_importableFunction) + WasmToWasmImportableFunction::offsetOfBoxedWasmCalleeLoadLocation(); }

    static constexpr ptrdiff_t offsetOfRTT() { return OBJECT_OFFSETOF(WebAssemblyFunctionBase, m_importableFunction) + WasmToWasmImportableFunction::offsetOfRTT(); }

protected:
    DECLARE_VISIT_CHILDREN;
    void finishCreation(VM&, NativeExecutable*, unsigned length, const String& name);
    WebAssemblyFunctionBase(VM&, NativeExecutable*, JSGlobalObject*, Structure*, JSWebAssemblyInstance*, Wasm::WasmOrJSImportableFunction&&, Wasm::WasmOrJSImportableFunctionCallLinkInfo*);

    Wasm::WasmOrJSImportableFunction m_importableFunction;
    // It's safe to just hold the raw WasmToWasmImportableFunctionCallLinkInfo because we have a reference
    // to our Instance, which points to the CodeBlock, which points to the Module
    // that exported us, which ensures that the actual Signature/RTT/code doesn't get deallocated.
    Wasm::WasmOrJSImportableFunctionCallLinkInfo* m_callLinkInfo;
    WriteBarrier<JSWebAssemblyInstance> m_instance;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
