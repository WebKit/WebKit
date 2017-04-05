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
#include "JSWebAssemblyCodeBlock.h"
#include "UnconditionalFinalizer.h"
#include "WasmModuleInformation.h"
#include <wtf/Bag.h>
#include <wtf/Vector.h>

namespace JSC {

namespace Wasm {
class Plan;
}

class SymbolTable;
class JSWebAssemblyMemory;
class WebAssemblyToJSCallee;

class JSWebAssemblyModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyModule* createStub(VM&, ExecState*, Structure*, RefPtr<Wasm::Plan>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    const Wasm::ModuleInformation& moduleInformation() const { return m_moduleInformation.get(); }
    SymbolTable* exportSymbolTable() const { return m_exportSymbolTable.get(); }
    Wasm::SignatureIndex signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
    {
        return m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace);
    }
    WebAssemblyToJSCallee* callee() const { return m_callee.get(); }

    JSWebAssemblyCodeBlock* codeBlock(Wasm::MemoryMode mode) { return m_codeBlocks[static_cast<size_t>(mode)].get(); }

    const Vector<uint8_t>& source() const { return moduleInformation().source; }

private:
    friend class JSWebAssemblyCodeBlock;

    void setCodeBlock(VM&, Wasm::MemoryMode, JSWebAssemblyCodeBlock*);

    JSWebAssemblyModule(VM&, Structure*, Wasm::Plan&);
    void finishCreation(VM&);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    Ref<Wasm::ModuleInformation> m_moduleInformation;
    WriteBarrier<SymbolTable> m_exportSymbolTable;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlocks[Wasm::NumberOfMemoryModes];
    WriteBarrier<WebAssemblyToJSCallee> m_callee;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
