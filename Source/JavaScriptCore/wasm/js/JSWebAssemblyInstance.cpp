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

#include "config.h"
#include "JSWebAssemblyInstance.h"

#if ENABLE(WEBASSEMBLY)

#include "AbstractModuleRecord.h"
#include "JSCInlines.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssemblyModule.h"

namespace JSC {

JSWebAssemblyInstance* JSWebAssemblyInstance::create(VM& vm, Structure* structure, JSWebAssemblyModule* module, JSModuleNamespaceObject* moduleNamespaceObject)
{
    auto* instance = new (NotNull, allocateCell<JSWebAssemblyInstance>(vm.heap)) JSWebAssemblyInstance(vm, structure);
    instance->finishCreation(vm, module, moduleNamespaceObject);
    return instance;
}

Structure* JSWebAssemblyInstance::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyInstance::JSWebAssemblyInstance(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSWebAssemblyInstance::finishCreation(VM& vm, JSWebAssemblyModule* module, JSModuleNamespaceObject* moduleNamespaceObject)
{
    Base::finishCreation(vm);
    m_module.set(vm, this, module);
    m_moduleNamespaceObject.set(vm, this, moduleNamespaceObject);
    // FIXME this should put the module namespace object onto the exports object, instead of moduleEnvironment in WebAssemblyInstanceConstructor. https://bugs.webkit.org/show_bug.cgi?id=165121
    ASSERT(inherits(info()));
}

void JSWebAssemblyInstance::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyInstance*>(cell)->JSWebAssemblyInstance::~JSWebAssemblyInstance();
}

void JSWebAssemblyInstance::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyInstance*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_module);
    visitor.append(&thisObject->m_moduleNamespaceObject);
}

const ClassInfo JSWebAssemblyInstance::s_info = { "WebAssembly.Instance", &Base::s_info, 0, CREATE_METHOD_TABLE(JSWebAssemblyInstance) };

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
