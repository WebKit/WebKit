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

#include "config.h"
#include "JSWebAssemblyModule.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyCodeBlock.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyMemory.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmMemory.h"
#include "WasmModule.h"
#include "WasmPlan.h"
#include "WebAssemblyToJSCallee.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo JSWebAssemblyModule::s_info = { "WebAssembly.Module", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyModule) };

JSWebAssemblyModule* JSWebAssemblyModule::createStub(VM& vm, ExecState* exec, Structure* structure, Wasm::Module::ValidationResult&& result)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!result.has_value()) {
        auto* error = JSWebAssemblyCompileError::create(exec, vm, structure->globalObject()->webAssemblyCompileErrorStructure(), result.error());
        RETURN_IF_EXCEPTION(scope, nullptr);
        throwException(exec, scope, error);
        return nullptr;
    }

    auto* module = new (NotNull, allocateCell<JSWebAssemblyModule>(vm.heap)) JSWebAssemblyModule(vm, structure, result.value().releaseNonNull());
    module->finishCreation(vm);
    return module;
}

Structure* JSWebAssemblyModule::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}


JSWebAssemblyModule::JSWebAssemblyModule(VM& vm, Structure* structure, Ref<Wasm::Module>&& module)
    : Base(vm, structure)
    , m_module(WTFMove(module))
{
}

void JSWebAssemblyModule::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    // On success, a new WebAssembly.Module object is returned with [[Module]] set to the validated Ast.module.
    SymbolTable* exportSymbolTable = SymbolTable::create(vm);
    const Wasm::ModuleInformation& moduleInformation = m_module->moduleInformation();
    for (auto& exp : moduleInformation.exports) {
        auto offset = exportSymbolTable->takeNextScopeOffset(NoLockingNecessary);
        String field = String::fromUTF8(exp.field);
        exportSymbolTable->set(NoLockingNecessary, AtomString(field).impl(), SymbolTableEntry(VarOffset(offset)));
    }

    m_exportSymbolTable.set(vm, this, exportSymbolTable);
    m_callee.set(vm, this, WebAssemblyToJSCallee::create(vm, globalObject(vm)->webAssemblyToJSCalleeStructure(), this));
}

void JSWebAssemblyModule::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyModule*>(cell)->JSWebAssemblyModule::~JSWebAssemblyModule();
    Wasm::SignatureInformation::tryCleanup();
}

const Wasm::ModuleInformation& JSWebAssemblyModule::moduleInformation() const
{
    return m_module->moduleInformation();
}

SymbolTable* JSWebAssemblyModule::exportSymbolTable() const
{
    return m_exportSymbolTable.get();
}

Wasm::SignatureIndex JSWebAssemblyModule::signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
{
    return m_module->signatureIndexFromFunctionIndexSpace(functionIndexSpace);
}

WebAssemblyToJSCallee* JSWebAssemblyModule::callee() const
{
    return m_callee.get();
}

JSWebAssemblyCodeBlock* JSWebAssemblyModule::codeBlock(Wasm::MemoryMode mode)
{
    return m_codeBlocks[static_cast<size_t>(mode)].get();
}

Wasm::Module& JSWebAssemblyModule::module()
{
    return m_module.get();
}

void JSWebAssemblyModule::setCodeBlock(VM& vm, Wasm::MemoryMode mode, JSWebAssemblyCodeBlock* codeBlock)
{
    m_codeBlocks[static_cast<size_t>(mode)].set(vm, this, codeBlock);
}

void JSWebAssemblyModule::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSWebAssemblyModule* thisObject = jsCast<JSWebAssemblyModule*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_exportSymbolTable);
    visitor.append(thisObject->m_callee);
    for (unsigned i = 0; i < Wasm::NumberOfMemoryModes; ++i)
        visitor.append(thisObject->m_codeBlocks[i]);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
