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

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;
class WebAssemblyToJSCallee;

class JSWebAssemblyInstance : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyInstance* create(VM&, ExecState*, JSWebAssemblyModule*, JSObject* importObject, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    JSWebAssemblyCodeBlock* codeBlock() const { return m_codeBlock.get(); }
    void finalizeCreation(VM&, ExecState*, Ref<Wasm::CodeBlock>&&);

    JSWebAssemblyModule* module() const { return m_module.get(); }

    JSObject* importFunction(unsigned idx) { RELEASE_ASSERT(idx < m_numImportFunctions); return importFunctions()[idx].get(); }

    JSModuleNamespaceObject* moduleNamespaceObject() { return m_moduleNamespaceObject.get(); }

    JSWebAssemblyMemory* memory() { return m_memory.get(); }
    void setMemory(VM& vm, JSWebAssemblyMemory* value) { ASSERT(!memory()); m_memory.set(vm, this, value); }
    Wasm::MemoryMode memoryMode() { return memory()->memory().mode(); }

    JSWebAssemblyTable* table() { return m_table.get(); }

    int32_t loadI32Global(unsigned i) const { return m_globals.get()[i]; }
    int64_t loadI64Global(unsigned i) const { return m_globals.get()[i]; }
    float loadF32Global(unsigned i) const { return bitwise_cast<float>(loadI32Global(i)); }
    double loadF64Global(unsigned i) const { return bitwise_cast<double>(loadI64Global(i)); }
    void setGlobal(unsigned i, int64_t bits) { m_globals.get()[i] = bits; }

    static ptrdiff_t offsetOfMemory() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_memory); }
    static ptrdiff_t offsetOfTable() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_table); }
    static ptrdiff_t offsetOfCallee() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_callee); }
    static ptrdiff_t offsetOfGlobals() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_globals); }
    static ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_vm); }
    static ptrdiff_t offsetOfCodeBlock() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_codeBlock); }
    static ptrdiff_t offsetOfCachedStackLimit() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedStackLimit); }
    static size_t offsetOfImportFunctions() { return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<JSCell>)>(sizeof(JSWebAssemblyInstance)); }
    static size_t offsetOfImportFunction(size_t importFunctionNum) { return offsetOfImportFunctions() + importFunctionNum * sizeof(sizeof(WriteBarrier<JSCell>)); }

    WebAssemblyToJSCallee* webAssemblyToJSCallee() { return m_callee.get(); }

    void* cachedStackLimit() const { return m_cachedStackLimit; }
    void setCachedStackLimit(void* limit) { m_cachedStackLimit = limit; }

protected:
    JSWebAssemblyInstance(VM&, Structure*, unsigned numImportFunctions);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    static size_t allocationSize(Checked<size_t> numImportFunctions)
    {
        return (offsetOfImportFunctions() + sizeof(WriteBarrier<JSCell>) * numImportFunctions).unsafeGet();
    }

private:
    VM* m_vm;
    WriteBarrier<JSObject>* importFunctions() { return bitwise_cast<WriteBarrier<JSObject>*>(bitwise_cast<char*>(this) + offsetOfImportFunctions()); }
    size_t globalMemoryByteSize() const;

    WriteBarrier<JSWebAssemblyModule> m_module;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlock;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    WriteBarrier<JSWebAssemblyMemory> m_memory;
    WriteBarrier<JSWebAssemblyTable> m_table;
    WriteBarrier<WebAssemblyToJSCallee> m_callee;
    MallocPtr<uint64_t> m_globals;
    void* m_cachedStackLimit { bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()) };
    unsigned m_numImportFunctions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
