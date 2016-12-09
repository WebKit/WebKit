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
#include "JSWebAssemblyMemory.h"

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;

class JSWebAssemblyInstance : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;


    static JSWebAssemblyInstance* create(VM&, Structure*, JSWebAssemblyModule*, JSModuleNamespaceObject*, unsigned);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    JSWebAssemblyModule* module()
    {
        ASSERT(m_module);
        return m_module.get();
    }

    WriteBarrier<JSCell>* importFunction(unsigned idx)
    {
        RELEASE_ASSERT(idx < m_numImportFunctions);
        return &importFunctions()[idx];
    }

    WriteBarrier<JSCell>* importFunctions()
    {
        return bitwise_cast<WriteBarrier<JSCell>*>(bitwise_cast<char*>(this) + offsetOfImportFunctions());
    }

    void setImportFunction(VM& vm, JSCell* value, unsigned idx)
    {
        importFunction(idx)->set(vm, this, value);
    }

    JSWebAssemblyMemory* memory() { return m_memory.get(); }
    void setMemory(VM& vm, JSWebAssemblyMemory* memory) { m_memory.set(vm, this, memory); }

    static size_t offsetOfImportFunction(unsigned idx)
    {
        return offsetOfImportFunctions() + sizeof(WriteBarrier<JSCell>) * idx;
    }

protected:
    JSWebAssemblyInstance(VM&, Structure*, unsigned);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    static size_t offsetOfImportFunctions()
    {
        return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<JSCell>)>(sizeof(JSWebAssemblyInstance));
    }

    static size_t allocationSize(unsigned numImportFunctions)
    {
        return offsetOfImportFunctions() + sizeof(WriteBarrier<JSCell>) * numImportFunctions;
    }

private:
    WriteBarrier<JSWebAssemblyModule> m_module;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    WriteBarrier<JSWebAssemblyMemory> m_memory;
    unsigned m_numImportFunctions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
