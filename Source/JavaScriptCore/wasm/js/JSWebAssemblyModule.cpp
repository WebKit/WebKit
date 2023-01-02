/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "WasmFormat.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmToJS.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo JSWebAssemblyModule::s_info = { "WebAssembly.Module"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyModule) };

JSWebAssemblyModule* JSWebAssemblyModule::createStub(VM& vm, JSGlobalObject* globalObject, Structure* structure, Wasm::Module::ValidationResult&& result)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!result.has_value()) {
        throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, result.error()));
        return nullptr;
    }

    auto* module = new (NotNull, allocateCell<JSWebAssemblyModule>(vm)) JSWebAssemblyModule(vm, structure, result.value().releaseNonNull());
    module->finishCreation(vm);

    auto error = module->generateWasmToJSStubs(vm);
    if (UNLIKELY(!error)) {
        switch (error.error()) {
        case Wasm::BindingFailure::OutOfMemory:
            throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, "Out of executable memory"_s));
            return nullptr;
        }
        ASSERT_NOT_REACHED();
    }
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
        String field = String::fromUTF8(exp.field);
        exportSymbolTable->set(NoLockingNecessary, AtomString(field).impl(), SymbolTableEntry(VarOffset(offset)));
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

Wasm::TypeIndex JSWebAssemblyModule::typeIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
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

void JSWebAssemblyModule::clearJSCallICs(VM& vm)
{
    for (auto& callLinkInfo : m_callLinkInfos)
        callLinkInfo.unlink(vm);
}

void JSWebAssemblyModule::finalizeUnconditionally(VM& vm)
{
    for (auto& callLinkInfo : m_callLinkInfos)
        callLinkInfo.visitWeak(vm);
}

Expected<void, Wasm::BindingFailure> JSWebAssemblyModule::generateWasmToJSStubs(VM& vm)
{
    const Wasm::ModuleInformation& moduleInformation = m_module->moduleInformation();
    if (moduleInformation.importFunctionCount()) {
        FixedVector<OptimizingCallLinkInfo> callLinkInfos(moduleInformation.importFunctionCount());
        FixedVector<MacroAssemblerCodeRef<WasmEntryPtrTag>> stubs(moduleInformation.importFunctionCount());
        for (unsigned importIndex = 0; importIndex < moduleInformation.importFunctionCount(); ++importIndex) {
            Wasm::TypeIndex typeIndex = moduleInformation.importFunctionTypeIndices.at(importIndex);
            auto binding = Wasm::wasmToJS(vm, callLinkInfos[importIndex], typeIndex, importIndex);
            if (UNLIKELY(!binding))
                return makeUnexpected(binding.error());
            stubs[importIndex] = binding.value();
        }
        m_wasmToJSExitStubs = WTFMove(stubs);
        m_callLinkInfos = WTFMove(callLinkInfos);
    }
    return { };
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
