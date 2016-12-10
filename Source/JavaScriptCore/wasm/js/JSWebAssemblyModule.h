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

#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "WasmFormat.h"
#include <wtf/Bag.h>
#include <wtf/Vector.h>

namespace JSC {

class JSWebAssemblyCallee;
class SymbolTable;

class JSWebAssemblyModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyModule* create(VM&, Structure*, std::unique_ptr<Wasm::ModuleInformation>&&, Bag<CallLinkInfo>&&, Vector<Wasm::WasmToJSStub>&&, Wasm::ImmutableFunctionIndexSpace&&, SymbolTable*, unsigned);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    const Wasm::ModuleInformation& moduleInformation() const { return *m_moduleInformation.get(); }
    SymbolTable* exportSymbolTable() const { return m_exportSymbolTable.get(); }
    Wasm::Signature* signatureForFunctionIndexSpace(unsigned functionIndexSpace) const { ASSERT(functionIndexSpace < m_functionIndexSpace.size); return m_functionIndexSpace.buffer.get()[functionIndexSpace].signature; }
    unsigned importCount() const { return m_wasmToJSStubs.size(); }

    JSWebAssemblyCallee* calleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= importCount());
        unsigned calleeIndex = functionIndexSpace - importCount();
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        return callees()[calleeIndex].get();
    }

    WriteBarrier<JSWebAssemblyCallee>* callees()
    {
        return bitwise_cast<WriteBarrier<JSWebAssemblyCallee>*>(bitwise_cast<char*>(this) + offsetOfCallees());
    }

    static ptrdiff_t offsetOfFunctionIndexSpace() { return OBJECT_OFFSETOF(JSWebAssemblyModule, m_functionIndexSpace); }

protected:
    JSWebAssemblyModule(VM&, Structure*, std::unique_ptr<Wasm::ModuleInformation>&&, Bag<CallLinkInfo>&&, Vector<Wasm::WasmToJSStub>&&, Wasm::ImmutableFunctionIndexSpace&&, unsigned calleeCount);
    void finishCreation(VM&, SymbolTable*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

private:
    static size_t offsetOfCallees()
    {
        return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<JSWebAssemblyCallee>)>(sizeof(JSWebAssemblyModule));
    }

    static size_t allocationSize(unsigned numCallees)
    {
        return offsetOfCallees() + sizeof(WriteBarrier<JSWebAssemblyCallee>) * numCallees;
    }

    std::unique_ptr<Wasm::ModuleInformation> m_moduleInformation;
    Bag<CallLinkInfo> m_callLinkInfos;
    WriteBarrier<SymbolTable> m_exportSymbolTable;
    Vector<Wasm::WasmToJSStub> m_wasmToJSStubs;
    const Wasm::ImmutableFunctionIndexSpace m_functionIndexSpace;
    unsigned m_calleeCount;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
