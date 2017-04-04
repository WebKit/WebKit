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

#include "JSFunction.h"
#include "WasmCallee.h"
#include <wtf/Noncopyable.h>

namespace JSC {

class JSGlobalObject;
struct ProtoCallFrame;
class WebAssemblyInstance;

namespace B3 {
class Compilation;
}

class WebAssemblyFunction : public JSFunction {
public:
    typedef JSFunction Base;

    const static unsigned StructureFlags = Base::StructureFlags;

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static WebAssemblyFunction* create(VM&, JSGlobalObject*, unsigned, const String&, JSWebAssemblyInstance*, Wasm::Callee& jsEntrypoint, Wasm::Callee& wasmEntrypoint, Wasm::SignatureIndex);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    JSWebAssemblyInstance* instance() const { return m_instance.get(); }
    Wasm::SignatureIndex signatureIndex() const { return m_signatureIndex; }
    void* wasmEntrypoint() { return m_wasmEntrypoint; }
    void* jsEntrypoint() { return m_jsEntrypoint; }

    static ptrdiff_t offsetOfInstance() { return OBJECT_OFFSETOF(WebAssemblyFunction, m_instance); }
    static ptrdiff_t offsetOfWasmEntrypoint() { return OBJECT_OFFSETOF(WebAssemblyFunction, m_wasmEntrypoint); }

protected:
    static void visitChildren(JSCell*, SlotVisitor&);

    void finishCreation(VM&, NativeExecutable*, unsigned length, const String& name, JSWebAssemblyInstance*);

private:
    WebAssemblyFunction(VM&, JSGlobalObject*, Structure*, Wasm::Callee& jsEntrypoint, Wasm::Callee& wasmEntrypoint, Wasm::SignatureIndex);

    WriteBarrier<JSWebAssemblyInstance> m_instance;
    // We can hold raw pointers to these executable address pointers since
    // we have a GC reference to our instance, which keeps alive the CodeBlock,
    // which owns this pointer.
    void* m_jsEntrypoint;
    void* m_wasmEntrypoint;
    // It's safe to just hold the raw signatureIndex because we have a reference
    // to our Instance, which points to the Module that exported us, which
    // ensures that the actual Signature doesn't get deallocated.
    Wasm::SignatureIndex m_signatureIndex;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
