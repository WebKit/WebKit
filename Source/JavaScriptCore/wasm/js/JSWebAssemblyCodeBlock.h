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

#include "CallLinkInfo.h"
#include "JSCell.h"
#include "PromiseDeferredTimer.h"
#include "Structure.h"
#include "UnconditionalFinalizer.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmModule.h"
#include <wtf/Bag.h>
#include <wtf/Vector.h>

namespace JSC {

class JSWebAssemblyModule;
class JSWebAssemblyMemory;

namespace Wasm {
class Plan;
}

class JSWebAssemblyCodeBlock final : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static JSWebAssemblyCodeBlock* create(VM&, Ref<Wasm::CodeBlock>, JSWebAssemblyModule*);
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    template<typename CellType>
    static Subspace* subspaceFor(VM& vm)
    {
        return &vm.webAssemblyCodeBlockSpace;
    }

    unsigned functionImportCount() const { return m_codeBlock->functionImportCount(); }
    JSWebAssemblyModule* module() const { return m_module.get(); }

    bool isSafeToRun(JSWebAssemblyMemory*) const;

    void finishCreation(VM&, JSWebAssemblyModule*);

    // These two callee getters are only valid once the callees have been populated.

    Wasm::Callee& jsEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        ASSERT(runnable());
        return m_codeBlock->jsEntrypointCalleeFromFunctionIndexSpace(functionIndexSpace);
    }
    Wasm::WasmEntrypointLoadLocation wasmEntrypointLoadLocationFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        ASSERT(runnable());
        return m_codeBlock->wasmEntrypointLoadLocationFromFunctionIndexSpace(functionIndexSpace);
    }

    Wasm::WasmEntrypointLoadLocation wasmToJsCallStubForImport(unsigned importIndex)
    {
        ASSERT(runnable());
        return &importWasmToJSStub(importIndex);
    }

    static ptrdiff_t offsetOfImportWasmToJSStub(unsigned importIndex)
    {
        return offsetOfImportStubs() + sizeof(void*) * importIndex;
    }

    Wasm::CodeBlock& codeBlock() { return m_codeBlock.get(); }

    void clearJSCallICs(VM&);

    bool runnable() const { return !m_errorMessage; }

    String errorMessage()
    {
        ASSERT(!runnable());
        return m_errorMessage;
    }

private:
    JSWebAssemblyCodeBlock(VM&, Ref<Wasm::CodeBlock>&&, const Wasm::ModuleInformation&);
    DECLARE_EXPORT_INFO;
    static const bool needsDestruction = true;
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    static size_t offsetOfImportStubs()
    {
        return WTF::roundUpToMultipleOf<sizeof(void*)>(sizeof(JSWebAssemblyCodeBlock));
    }

    static size_t allocationSize(Checked<size_t> functionImportCount)
    {
        return (offsetOfImportStubs() + sizeof(void*) * functionImportCount).unsafeGet();
    }

    void*& importWasmToJSStub(unsigned importIndex)
    {
        return *bitwise_cast<void**>(bitwise_cast<char*>(this) + offsetOfImportWasmToJSStub(importIndex));
    }

    class UnconditionalFinalizer : public JSC::UnconditionalFinalizer {
        void finalizeUnconditionally() override;
    };

    Ref<Wasm::CodeBlock> m_codeBlock;
    WriteBarrier<JSWebAssemblyModule> m_module;
    Vector<MacroAssemblerCodeRef> m_wasmToJSExitStubs;
    UnconditionalFinalizer m_unconditionalFinalizer;
    Bag<CallLinkInfo> m_callLinkInfos;
    String m_errorMessage;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
