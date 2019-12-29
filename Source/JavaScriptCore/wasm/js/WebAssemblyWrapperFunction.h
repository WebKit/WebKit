/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "JSWebAssemblyCodeBlock.h"
#include "WebAssemblyFunctionBase.h"

namespace JSC {

using Wasm::WasmToWasmImportableFunction;

class WebAssemblyWrapperFunction final : public WebAssemblyFunctionBase {
public:
    using Base = WebAssemblyFunctionBase;

    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyWrapperFunctionSpace<mode>();
    }

    DECLARE_INFO;

    static WebAssemblyWrapperFunction* create(VM&, JSGlobalObject*, Structure*, JSObject*, unsigned importIndex, JSWebAssemblyInstance*, Wasm::SignatureIndex);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    Wasm::SignatureIndex signatureIndex() const { return m_importableFunction.signatureIndex; }
    WasmToWasmImportableFunction::LoadLocation  entrypointLoadLocation() const { return m_importableFunction.entrypointLoadLocation; }
    WasmToWasmImportableFunction importableFunction() const { return m_importableFunction; }
    JSObject* function() { return m_function.get(); }

protected:
    static void visitChildren(JSCell*, SlotVisitor&);

    void finishCreation(VM&, NativeExecutable*, unsigned length, const String& name, JSObject*, JSWebAssemblyInstance*);

private:
    WebAssemblyWrapperFunction(VM&, NativeExecutable*, JSGlobalObject*, Structure*, WasmToWasmImportableFunction);

    WriteBarrier<JSObject> m_function;
    // It's safe to just hold the raw WasmToWasmImportableFunction because we have a reference
    // to our Instance, which points to the CodeBlock, which points to the Module
    // that exported us, which ensures that the actual Signature/code doesn't get deallocated.
    WasmToWasmImportableFunction m_importableFunction;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
