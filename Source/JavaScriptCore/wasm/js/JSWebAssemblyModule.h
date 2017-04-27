/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "UnconditionalFinalizer.h"
#include "WasmModule.h"
#include <wtf/Bag.h>
#include <wtf/Vector.h>

namespace JSC {

namespace Wasm {
class Module;
class Plan;
}

class SymbolTable;
class JSWebAssemblyCodeBlock;
class JSWebAssemblyMemory;
class WebAssemblyToJSCallee;

class JSWebAssemblyModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static JSWebAssemblyModule* createStub(VM&, ExecState*, Structure*, Wasm::Module::ValidationResult&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    const Wasm::ModuleInformation& moduleInformation() const { return m_module->moduleInformation(); }
    SymbolTable* exportSymbolTable() const { return m_exportSymbolTable.get(); }
    Wasm::SignatureIndex signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
    {
        return m_module->signatureIndexFromFunctionIndexSpace(functionIndexSpace);
    }
    WebAssemblyToJSCallee* callee() const { return m_callee.get(); }

    JSWebAssemblyCodeBlock* codeBlock(Wasm::MemoryMode mode) { return m_codeBlocks[static_cast<size_t>(mode)].get(); }

    const Vector<uint8_t>& source() const;

    Wasm::Module& module() { return m_module.get(); }
    void setCodeBlock(VM&, Wasm::MemoryMode, JSWebAssemblyCodeBlock*);

private:
    friend class JSWebAssemblyCodeBlock;

    JSWebAssemblyModule(VM&, Structure*, Ref<Wasm::Module>&&);
    void finishCreation(VM&);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    Ref<Wasm::Module> m_module;
    WriteBarrier<SymbolTable> m_exportSymbolTable;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlocks[Wasm::NumberOfMemoryModes];
    WriteBarrier<WebAssemblyToJSCallee> m_callee;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
