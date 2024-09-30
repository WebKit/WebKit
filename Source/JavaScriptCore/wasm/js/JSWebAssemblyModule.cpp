/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyModule.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyLinkError.h"
#include "WasmBinding.h"
#include "WasmFormat.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC {

const ClassInfo JSWebAssemblyModule::s_info = { "WebAssembly.Module"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyModule) };

JSWebAssemblyModule* JSWebAssemblyModule::create(VM& vm, Structure* structure, Ref<Wasm::Module>&& result)
{
    auto* module = new (NotNull, allocateCell<JSWebAssemblyModule>(vm)) JSWebAssemblyModule(vm, structure, WTFMove(result));
    module->finishCreation(vm);
    return module;
}

Structure* JSWebAssemblyModule::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(WebAssemblyModuleType, StructureFlags), info());
}


JSWebAssemblyModule::JSWebAssemblyModule(VM& vm, Structure* structure, Ref<Wasm::Module>&& module)
    : Base(vm, structure)
    , m_module(WTFMove(module))
{
}

void JSWebAssemblyModule::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    // On success, a new WebAssembly.Module object is returned with [[Module]] set to the validated Ast.module.
    SymbolTable* exportSymbolTable = SymbolTable::create(vm);
    const Wasm::ModuleInformation& moduleInformation = m_module->moduleInformation();
    {
        auto offset = exportSymbolTable->takeNextScopeOffset(NoLockingNecessary);
        exportSymbolTable->set(NoLockingNecessary, vm.propertyNames->starNamespacePrivateName.impl(), SymbolTableEntry(VarOffset(offset)));
    }
    for (auto& exp : moduleInformation.exports) {
        auto offset = exportSymbolTable->takeNextScopeOffset(NoLockingNecessary);
        exportSymbolTable->set(NoLockingNecessary, makeAtomString(exp.field).impl(), SymbolTableEntry(VarOffset(offset)));
    }

    m_exportSymbolTable.set(vm, this, exportSymbolTable);
}

void JSWebAssemblyModule::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyModule*>(cell)->JSWebAssemblyModule::~JSWebAssemblyModule();
    Wasm::TypeInformation::tryCleanup();
}

const Wasm::ModuleInformation& JSWebAssemblyModule::moduleInformation() const
{
    return m_module->moduleInformation();
}

SymbolTable* JSWebAssemblyModule::exportSymbolTable() const
{
    return m_exportSymbolTable.get();
}

Wasm::TypeIndex JSWebAssemblyModule::typeIndexFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace) const
{
    return m_module->typeIndexFromFunctionIndexSpace(functionIndexSpace);
}

Wasm::Module& JSWebAssemblyModule::module()
{
    return m_module.get();
}

template<typename Visitor>
void JSWebAssemblyModule::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyModule* thisObject = jsCast<JSWebAssemblyModule*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_exportSymbolTable);
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyModule);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
