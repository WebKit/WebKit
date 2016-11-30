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
#include "JSWebAssemblyModule.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "WasmFormat.h"
#include "WasmMemory.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

JSWebAssemblyModule* JSWebAssemblyModule::create(VM& vm, Structure* structure, std::unique_ptr<Wasm::ModuleInformation>& moduleInformation, Wasm::CompiledFunctions& compiledFunctions, SymbolTable* exportSymbolTable)
{
    auto* instance = new (NotNull, allocateCell<JSWebAssemblyModule>(vm.heap)) JSWebAssemblyModule(vm, structure, moduleInformation, compiledFunctions);
    instance->finishCreation(vm, exportSymbolTable);
    return instance;
}

Structure* JSWebAssemblyModule::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyModule::JSWebAssemblyModule(VM& vm, Structure* structure, std::unique_ptr<Wasm::ModuleInformation>& moduleInformation, Wasm::CompiledFunctions& compiledFunctions)
    : Base(vm, structure)
    , m_moduleInformation(WTFMove(moduleInformation))
    , m_compiledFunctions(WTFMove(compiledFunctions))
{
}

void JSWebAssemblyModule::finishCreation(VM& vm, SymbolTable* exportSymbolTable)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_exportSymbolTable.set(vm, this, exportSymbolTable);
}

void JSWebAssemblyModule::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyModule*>(cell)->JSWebAssemblyModule::~JSWebAssemblyModule();
}

void JSWebAssemblyModule::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyModule*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_exportSymbolTable);
}

const ClassInfo JSWebAssemblyModule::s_info = { "WebAssembly.Module", &Base::s_info, 0, CREATE_METHOD_TABLE(JSWebAssemblyModule) };

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
