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
#include "JSObject.h"
#include "JSWebAssemblyCodeBlock.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyTable.h"
#include "WasmCreationMode.h"
#include "WasmInstance.h"
#include <wtf/Ref.h>

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;
class WebAssemblyToJSCallee;

namespace Wasm {
class CodeBlock;
}

class JSWebAssemblyInstance final : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static Identifier createPrivateModuleKey();

    static JSWebAssemblyInstance* create(VM&, ExecState*, const Identifier& moduleKey, JSWebAssemblyModule*, JSObject* importObject, Structure*, Ref<Wasm::Module>&&, Wasm::CreationMode);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    void finalizeCreation(VM&, ExecState*, Ref<Wasm::CodeBlock>&&, JSObject* importObject, Wasm::CreationMode);
    
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

    JSWebAssemblyModule* module() const { return m_module.get(); }

    static size_t offsetOfInstance() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_instance); }
    static size_t offsetOfCallee() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_callee); }

protected:
    JSWebAssemblyInstance(VM&, Structure*, Ref<Wasm::Instance>&&);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

private:
    Ref<Wasm::Instance> m_instance;

    WriteBarrier<JSWebAssemblyModule> m_module;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlock;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    WriteBarrier<JSWebAssemblyMemory> m_memory;
    WriteBarrier<JSWebAssemblyTable> m_table;
    WriteBarrier<WebAssemblyToJSCallee> m_callee;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
