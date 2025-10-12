/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ArityCheckMode.h>
#include <JavaScriptCore/MacroAssemblerCodeRef.h>
#include <JavaScriptCore/WasmCallee.h>
#include <JavaScriptCore/WebAssemblyFunctionBase.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class JSGlobalObject;
struct ProtoCallFrame;
class WebAssemblyInstance;

class WebAssemblyFunction final : public WebAssemblyFunctionBase {
    friend JSC::LLIntOffsetsExtractor;

public:
    using Base = WebAssemblyFunctionBase;

    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyFunctionSpace<mode>();
    }

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

    JS_EXPORT_PRIVATE static WebAssemblyFunction* create(VM&, JSGlobalObject*, Structure*, unsigned, const String&, JSWebAssemblyInstance*, Wasm::JSToWasmCallee&, Wasm::IPIntCallee&, WasmToWasmImportableFunction::LoadLocation, Wasm::TypeIndex, Ref<const Wasm::RTT>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    Wasm::JSToWasmCallee* jsToWasmCallee() const { return m_boxedJSToWasmCallee.ptr(); }
    CodePtr<WasmEntryPtrTag> jsToWasm(ArityCheckMode arity)
    {
        ASSERT_UNUSED(arity, arity == ArityCheckMode::ArityCheckNotRequired || arity == ArityCheckMode::MustCheckArity);
        return m_boxedJSToWasmCallee->entrypoint();
    }

    CodePtr<JSEntryPtrTag> jsCallICEntrypoint()
    {
#if ENABLE(JIT)
        if (m_taintedness >= SourceTaintedOrigin::IndirectlyTainted)
            return nullptr;

        // Prep the entrypoint for the slow path.
        executable()->entrypointFor(CodeSpecializationKind::CodeForCall, ArityCheckMode::MustCheckArity);
        if (!m_jsToWasmICJITCode)
            m_jsToWasmICJITCode = signature().jsToWasmICEntrypoint();
        return m_jsToWasmICJITCode;
#else
        return nullptr;
#endif
    }

    SourceTaintedOrigin taintedness() const { return m_taintedness; }

    static constexpr ptrdiff_t offsetOfBoxedJSToWasmCallee() { return OBJECT_OFFSETOF(WebAssemblyFunction, m_boxedJSToWasmCallee); }
    static constexpr ptrdiff_t offsetOfFrameSize() { return OBJECT_OFFSETOF(WebAssemblyFunction, m_frameSize); }

private:
    WebAssemblyFunction(VM&, NativeExecutable*, JSGlobalObject*, Structure*, JSWebAssemblyInstance*, Wasm::JSToWasmCallee&, Wasm::IPIntCallee&, WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation, Wasm::TypeIndex, Ref<const Wasm::RTT>&&);

    CodePtr<JSEntryPtrTag> jsCallEntrypointSlow();

    // This let's the JS->Wasm interpreter find its metadata
    Ref<Wasm::JSToWasmCallee, BoxedNativeCalleePtrTraits<Wasm::JSToWasmCallee>> m_boxedJSToWasmCallee;
    uint32_t m_frameSize;
    SourceTaintedOrigin m_taintedness;

#if ENABLE(JIT)
    CodePtr<JSEntryPtrTag> m_jsToWasmICJITCode;
#endif
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
