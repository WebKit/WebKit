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
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyTable.h"
#include "WasmContext.h"
#include "WasmInstance.h"

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;
class WebAssemblyToJSCallee;

namespace Wasm {
class CodeBlock;
class Table; // FIXME remove this after refactoring. https://webkit.org/b/177472
}

class JSWebAssemblyInstance : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyInstance* create(VM&, ExecState*, JSWebAssemblyModule*, JSObject* importObject, Structure*, Ref<Wasm::Instance>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    void finalizeCreation(VM&, ExecState*, Ref<Wasm::CodeBlock>&&);
    
    Wasm::Instance& instance() { return m_instance.get(); }
    Wasm::Context* context() const { return &m_vm->wasmContext; }
    JSModuleNamespaceObject* moduleNamespaceObject() { return m_moduleNamespaceObject.get(); }
    JSWebAssemblyTable* table() { return m_table.get(); }
    WebAssemblyToJSCallee* webAssemblyToJSCallee() { return m_callee.get(); }

    JSWebAssemblyMemory* memory() { return m_memory.get(); }
    void setMemory(VM& vm, JSWebAssemblyMemory* value) { ASSERT(!memory()); m_memory.set(vm, this, value); m_wasmMemory = &memory()->memory(); }
    Wasm::MemoryMode memoryMode() { return memory()->memory().mode(); }

    // Tail accessors.
    static size_t offsetOfTail() { return WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(JSWebAssemblyInstance)); }
    struct ImportFunctionInfo {
        // Target instance and entrypoint are only set for wasm->wasm calls, and are otherwise nullptr. The embedder-specific logic occurs through import function.
        JSWebAssemblyInstance* targetInstance { nullptr };
        Wasm::WasmEntrypointLoadLocation wasmEntrypoint { nullptr };
        WriteBarrier<JSObject> importFunction { WriteBarrier<JSObject>() };
    };
    ImportFunctionInfo* importFunctionInfo(size_t importFunctionNum) { return &bitwise_cast<ImportFunctionInfo*>(bitwise_cast<char*>(this) + offsetOfTail())[importFunctionNum]; }
    static size_t offsetOfTargetInstance(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, targetInstance); }
    static size_t offsetOfWasmEntrypoint(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, wasmEntrypoint); }
    static size_t offsetOfImportFunction(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, importFunction); }
    JSObject* importFunction(unsigned importFunctionNum) { RELEASE_ASSERT(importFunctionNum < m_numImportFunctions); return importFunctionInfo(importFunctionNum)->importFunction.get(); }
    
    // FIXME remove these after refactoring them out. https://webkit.org/b/177472
    Wasm::Memory& internalMemory() { return memory()->memory(); }
    Wasm::CodeBlock& wasmCodeBlock() const { return *m_instance->codeBlock(); }
    static ptrdiff_t offsetOfWasmTable() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_wasmTable); }
    static ptrdiff_t offsetOfCallee() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_callee); }
    static ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_vm); }
    static ptrdiff_t offsetOfGlobals() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_globals); }
    static ptrdiff_t offsetOfCodeBlock() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_codeBlock); }
    static ptrdiff_t offsetOfWasmCodeBlock() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_wasmCodeBlock); }
    static ptrdiff_t offsetOfCachedStackLimit() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedStackLimit); }
    static ptrdiff_t offsetOfWasmMemory() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_wasmMemory); }
    void* cachedStackLimit() const { RELEASE_ASSERT(m_instance->cachedStackLimit() == m_cachedStackLimit); return m_cachedStackLimit; }
    void setCachedStackLimit(void* limit) { m_instance->setCachedStackLimit(limit); m_cachedStackLimit = limit; }
    Wasm::Memory* wasmMemory() { return m_wasmMemory; }
    Wasm::Module& wasmModule() { return m_wasmModule; }

protected:
    JSWebAssemblyInstance(VM&, Structure*, unsigned numImportFunctions, Ref<Wasm::Instance>&&);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    static size_t allocationSize(Checked<size_t> numImportFunctions)
    {
        return (offsetOfTail() + sizeof(ImportFunctionInfo) * numImportFunctions).unsafeGet();
    }

private:
    JSWebAssemblyModule* module() const { return m_module.get(); }

    Ref<Wasm::Instance> m_instance;

    VM* m_vm;
    WriteBarrier<JSWebAssemblyModule> m_module;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlock;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    WriteBarrier<JSWebAssemblyMemory> m_memory;
    WriteBarrier<JSWebAssemblyTable> m_table;
    WriteBarrier<WebAssemblyToJSCallee> m_callee;
    
    // FIXME remove these after refactoring them out. https://webkit.org/b/177472
    void* m_cachedStackLimit { bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()) };
    Wasm::CodeBlock* m_wasmCodeBlock { nullptr };
    Wasm::Module& m_wasmModule;
    Wasm::Memory* m_wasmMemory { nullptr };
    Wasm::Table* m_wasmTable { nullptr };
    uint64_t* m_globals { nullptr };

    unsigned m_numImportFunctions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
