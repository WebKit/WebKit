/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "JSCell.h"
#include "PromiseDeferredTimer.h"
#include "Structure.h"
#include "UnconditionalFinalizer.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include <wtf/Bag.h>
#include <wtf/Vector.h>

namespace JSC {

class JSWebAssemblyModule;
class JSWebAssemblyMemory;

namespace Wasm {
class Plan;
}

class JSWebAssemblyCodeBlock : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static JSWebAssemblyCodeBlock* create(VM& vm, JSWebAssemblyModule* owner, Wasm::MemoryMode mode, Ref<Wasm::Plan>&& plan, unsigned calleeCount, unsigned functionImportCount)
    {
        auto* result = new (NotNull, allocateCell<JSWebAssemblyCodeBlock>(vm.heap, allocationSize(functionImportCount))) JSWebAssemblyCodeBlock(vm, owner, mode, WTFMove(plan), calleeCount);
        result->finishCreation(vm);
        return result;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    unsigned functionImportCount() const { return m_wasmExitStubs.size(); }
    Wasm::MemoryMode mode() const { return m_mode; }
    JSWebAssemblyModule* module() const { return m_module.get(); }

    // Don't call intialize directly, this should be called for you, as needed, by JSWebAssemblyInstance::finalizeCreation.
    void initialize();
    bool initialized() const { return !m_plan; }

    Wasm::Plan& plan() const { ASSERT(!initialized()); return *m_plan; }

    bool runnable() const { return initialized() && !m_errorMessage; }
    String& errorMessage() { ASSERT(!runnable()); return m_errorMessage; }
    bool isSafeToRun(JSWebAssemblyMemory*) const;

    // These two callee getters are only valid once the callees have been populated.
    Wasm::Callee& jsEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        return *m_callees[calleeIndex].get();
    }
    Wasm::Callee& wasmEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        return *m_callees[calleeIndex + m_calleeCount].get();
    }

    void* wasmToJsCallStubForImport(unsigned importIndex)
    {
        RELEASE_ASSERT(importIndex < m_wasmExitStubs.size());
        return m_wasmExitStubs[importIndex].wasmToJs.code().executableAddress();
    }

    static ptrdiff_t offsetOfImportWasmToJSStub(unsigned importIndex)
    {
        return offsetOfImportStubs() + sizeof(void*) * importIndex;
    }

private:
    void setJSEntrypointCallee(unsigned calleeIndex, Ref<Wasm::Callee>&& callee)
    {
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        m_callees[calleeIndex] = WTFMove(callee);
    }
    void setWasmEntrypointCallee(unsigned calleeIndex, Ref<Wasm::Callee>&& callee)
    {
        RELEASE_ASSERT(calleeIndex < m_calleeCount);
        m_callees[calleeIndex + m_calleeCount] = WTFMove(callee);
    }

    JSWebAssemblyCodeBlock(VM&, JSWebAssemblyModule*, Wasm::MemoryMode, Ref<Wasm::Plan>&&, unsigned calleeCount);
    DECLARE_EXPORT_INFO;
    static const bool needsDestruction = true;
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    static ptrdiff_t offsetOfImportStubs()
    {
        return WTF::roundUpToMultipleOf<sizeof(void*)>(sizeof(JSWebAssemblyCodeBlock));
    }

    static size_t allocationSize(unsigned functionImportCount)
    {
        return offsetOfImportStubs() + sizeof(void*) * functionImportCount;
    }

    void*& importWasmToJSStub(unsigned importIndex)
    {
        return *bitwise_cast<void**>(bitwise_cast<char*>(this) + offsetOfImportWasmToJSStub(importIndex));
    }

    class UnconditionalFinalizer : public JSC::UnconditionalFinalizer {
        void finalizeUnconditionally() override;
    };

    WriteBarrier<JSWebAssemblyModule> m_module;
    UnconditionalFinalizer m_unconditionalFinalizer;
    Bag<CallLinkInfo> m_callLinkInfos;
    Vector<Wasm::WasmExitStubs> m_wasmExitStubs;
    Vector<RefPtr<Wasm::Callee>> m_callees;
    // The plan that is compiling this code block.
    RefPtr<Wasm::Plan> m_plan;
    String m_errorMessage;
    Wasm::MemoryMode m_mode;
    unsigned m_calleeCount;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
