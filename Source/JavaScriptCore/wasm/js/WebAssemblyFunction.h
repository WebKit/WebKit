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
#include <wtf/Noncopyable.h>

namespace JSC {

class JSGlobalObject;
class JSWebAssemblyCallee;
class WebAssemblyInstance;

namespace B3 {
class Compilation;
}

namespace Wasm {
struct Signature;
}

class WebAssemblyFunction : public JSFunction {
public:
    typedef JSFunction Base;

    const static unsigned StructureFlags = Base::StructureFlags;

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static WebAssemblyFunction* create(VM&, JSGlobalObject*, unsigned, const String&, JSWebAssemblyInstance*, JSWebAssemblyCallee*, Wasm::Signature*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    JSWebAssemblyCallee* webAssemblyCallee() const { return m_wasmCallee.get(); }
    JSWebAssemblyInstance* instance() const { return m_instance.get(); }
    const Wasm::Signature* signature()
    { 
        ASSERT(m_signature);
        return m_signature;
    }

protected:
    static void visitChildren(JSCell*, SlotVisitor&);

    void finishCreation(VM&, NativeExecutable*, unsigned length, const String& name, JSWebAssemblyInstance*, JSWebAssemblyCallee*, Wasm::Signature*);

private:
    WebAssemblyFunction(VM&, JSGlobalObject*, Structure*);

    WriteBarrier<JSWebAssemblyInstance> m_instance;
    WriteBarrier<JSWebAssemblyCallee> m_wasmCallee;
    Wasm::Signature* m_signature;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
