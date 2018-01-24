/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "JSCPoison.h"
#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "JSWebAssemblyCodeBlock.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyTable.h"
#include "WasmInstance.h"
#include <wtf/Ref.h>

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;
class WebAssemblyToJSCallee;

namespace Wasm {
class CodeBlock;
}

class JSWebAssemblyInstance : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyInstance* create(VM&, ExecState*, JSWebAssemblyModule*, JSObject* importObject, Structure*, Ref<Wasm::Module>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    void finalizeCreation(VM&, ExecState*, Ref<Wasm::CodeBlock>&&);
    
    Wasm::Instance& instance() { return m_instance.get(); }
    JSModuleNamespaceObject* moduleNamespaceObject() { return m_moduleNamespaceObject.get(); }
    WebAssemblyToJSCallee* webAssemblyToJSCallee() { return m_callee.get(); }

    JSWebAssemblyMemory* memory() { return m_memory.get(); }
    void setMemory(VM& vm, JSWebAssemblyMemory* value) {
        ASSERT(!memory());
        m_memory.set(vm, this, value);
        instance().setMemory(makeRef(memory()->memory()));
    }
    Wasm::MemoryMode memoryMode() { return memory()->memory().mode(); }

    JSWebAssemblyTable* table() { return m_table.get(); }
    void setTable(VM& vm, JSWebAssemblyTable* value) {
        ASSERT(!table());
        m_table.set(vm, this, value);
        instance().setTable(makeRef(*table()->table()));
    }

    static size_t offsetOfPoisonedInstance() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_instance); }
    static size_t offsetOfPoisonedCallee() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_callee); }

    template<typename T>
    using PoisonedBarrier = PoisonedWriteBarrier<JSWebAssemblyInstancePoison, T>;

protected:
    JSWebAssemblyInstance(VM&, Structure*, Ref<Wasm::Instance>&&);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

private:
    JSWebAssemblyModule* module() const { return m_module.get(); }

    PoisonedRef<JSWebAssemblyInstancePoison, Wasm::Instance> m_instance;

    PoisonedBarrier<JSWebAssemblyModule> m_module;
    PoisonedBarrier<JSWebAssemblyCodeBlock> m_codeBlock;
    PoisonedBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    PoisonedBarrier<JSWebAssemblyMemory> m_memory;
    PoisonedBarrier<JSWebAssemblyTable> m_table;
    PoisonedBarrier<WebAssemblyToJSCallee> m_callee;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
