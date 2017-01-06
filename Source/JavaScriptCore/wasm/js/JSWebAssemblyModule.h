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
#include "JSWebAssemblyCallee.h"
#include "UnconditionalFinalizer.h"
#include "WasmFormat.h"
#include <wtf/Bag.h>
#include <wtf/Vector.h>

namespace JSC {

class SymbolTable;

class JSWebAssemblyModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyModule* create(VM&, Structure*, std::unique_ptr<Wasm::ModuleInformation>&&, Bag<CallLinkInfo>&&, Vector<Wasm::WasmExitStubs>&&, SymbolTable*, unsigned);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    const Wasm::ModuleInformation& moduleInformation() const { return *m_moduleInformation.get(); }
    SymbolTable* exportSymbolTable() const { return m_exportSymbolTable.get(); }
    Wasm::SignatureIndex signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
    {
        return m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace);
    }
    unsigned functionImportCount() const { return m_wasmExitStubs.size(); }

    JSWebAssemblyCallee* jsEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        return callees()[calleeIndex].get();
    }

    JSWebAssemblyCallee* wasmEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        return callees()[calleeIndex + m_calleeCount].get();
    }

    void setJSEntrypointCallee(VM& vm, unsigned calleeIndex, JSWebAssemblyCallee* callee)
    {
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        callees()[calleeIndex].set(vm, this, callee);
    }

    void setWasmEntrypointCallee(VM& vm, unsigned calleeIndex, JSWebAssemblyCallee* callee)
    {
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        callees()[calleeIndex + m_calleeCount].set(vm, this, callee);
    }

    WriteBarrier<JSWebAssemblyCallee>* callees()
    {
        return bitwise_cast<WriteBarrier<JSWebAssemblyCallee>*>(bitwise_cast<char*>(this) + offsetOfCallees());
    }

protected:
    JSWebAssemblyModule(VM&, Structure*, std::unique_ptr<Wasm::ModuleInformation>&&, Bag<CallLinkInfo>&&, Vector<Wasm::WasmExitStubs>&&, unsigned calleeCount);
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
        return offsetOfCallees() + sizeof(WriteBarrier<JSWebAssemblyCallee>) * numCallees * 2;
    }

    class UnconditionalFinalizer : public JSC::UnconditionalFinalizer { 
        void finalizeUnconditionally() override;
    };

    UnconditionalFinalizer m_unconditionalFinalizer;
    std::unique_ptr<Wasm::ModuleInformation> m_moduleInformation;
    Bag<CallLinkInfo> m_callLinkInfos;
    WriteBarrier<SymbolTable> m_exportSymbolTable;
    Vector<Wasm::WasmExitStubs> m_wasmExitStubs;
    unsigned m_calleeCount;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
