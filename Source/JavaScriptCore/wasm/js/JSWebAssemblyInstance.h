/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyCodeBlock.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyTable.h"
#include "WasmCreationMode.h"
#include "WasmInstance.h"
#include <wtf/Ref.h>

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;

namespace Wasm {
class CodeBlock;
}

class JSWebAssemblyInstance final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyInstanceSpace<mode>();
    }

    static Identifier createPrivateModuleKey();

    static JSWebAssemblyInstance* tryCreate(VM&, JSGlobalObject*, const Identifier& moduleKey, JSWebAssemblyModule*, JSObject* importObject, Structure*, Ref<Wasm::Module>&&, Wasm::CreationMode);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    void finalizeCreation(VM&, JSGlobalObject*, Ref<Wasm::CodeBlock>&&, JSObject* importObject, Wasm::CreationMode);
    
    Wasm::Instance& instance() { return m_instance.get(); }
    JSModuleNamespaceObject* moduleNamespaceObject() { return m_moduleNamespaceObject.get(); }

    JSWebAssemblyMemory* memory() { return m_memory.get(); }
    void setMemory(VM& vm, JSWebAssemblyMemory* value) {
        ASSERT(!memory());
        m_memory.set(vm, this, value);
        instance().setMemory(makeRef(memory()->memory()));
    }
    Wasm::MemoryMode memoryMode() { return memory()->memory().mode(); }

    JSWebAssemblyTable* table(unsigned i) { return m_tables[i].get(); }
    void setTable(VM& vm, uint32_t index, JSWebAssemblyTable* value)
    {
        ASSERT(index < m_tables.size());
        ASSERT(!table(index));
        m_tables[index].set(vm, this, value);
        instance().setTable(index, makeRef(*table(index)->table()));
    }

    void linkGlobal(VM& vm, uint32_t index, JSWebAssemblyGlobal* value)
    {
        ASSERT(value == value->global()->owner<JSWebAssemblyGlobal>());
        instance().linkGlobal(index, makeRef(*value->global()));
        vm.heap.writeBarrier(this, value);
    }

    JSGlobalObject* globalObject() const { return m_globalObject.get(); }
    JSWebAssemblyModule* module() const { return m_module.get(); }

    static size_t offsetOfInstance() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_instance); }
    static size_t offsetOfModule() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_module); }
    static size_t offsetOfGlobalObject() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_globalObject); }
    static size_t offsetOfVM() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_vm); }

protected:
    JSWebAssemblyInstance(VM&, Structure*, Ref<Wasm::Instance>&&);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*);
    static void visitChildren(JSCell*, SlotVisitor&);

private:
    Ref<Wasm::Instance> m_instance;
    VM* m_vm;

    WriteBarrier<JSGlobalObject> m_globalObject;
    WriteBarrier<JSWebAssemblyModule> m_module;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlock;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    WriteBarrier<JSWebAssemblyMemory> m_memory;
    Vector<WriteBarrier<JSWebAssemblyTable>> m_tables;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
