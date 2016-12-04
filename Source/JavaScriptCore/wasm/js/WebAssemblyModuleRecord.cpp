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
#include "WebAssemblyModuleRecord.h"

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "JSCInlines.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "WasmFormat.h"
#include "WebAssemblyFunction.h"

namespace JSC {

const ClassInfo WebAssemblyModuleRecord::s_info = { "WebAssemblyModuleRecord", &Base::s_info, nullptr, CREATE_METHOD_TABLE(WebAssemblyModuleRecord) };

Structure* WebAssemblyModuleRecord::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

WebAssemblyModuleRecord* WebAssemblyModuleRecord::create(ExecState* exec, VM& vm, Structure* structure, const Identifier& moduleKey, const Wasm::ModuleInformation& moduleInformation)
{
    WebAssemblyModuleRecord* instance = new (NotNull, allocateCell<WebAssemblyModuleRecord>(vm.heap)) WebAssemblyModuleRecord(vm, structure, moduleKey);
    instance->finishCreation(exec, vm, moduleInformation);
    return instance;
}

WebAssemblyModuleRecord::WebAssemblyModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey)
    : Base(vm, structure, moduleKey)
{
}

void WebAssemblyModuleRecord::destroy(JSCell* cell)
{
    WebAssemblyModuleRecord* thisObject = jsCast<WebAssemblyModuleRecord*>(cell);
    thisObject->WebAssemblyModuleRecord::~WebAssemblyModuleRecord();
}

void WebAssemblyModuleRecord::finishCreation(ExecState* exec, VM& vm, const Wasm::ModuleInformation& moduleInformation)
{
    Base::finishCreation(exec, vm);
    ASSERT(inherits(info()));
    for (const auto& exp : moduleInformation.exports) {
        switch (exp.kind) {
        case Wasm::External::Function: {
            addExportEntry(ExportEntry::createLocal(exp.field, exp.field));
            break;
        }
        case Wasm::External::Table: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164135
            break;
        }
        case Wasm::External::Memory: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164134
            break;
        }
        case Wasm::External::Global: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
            // In the MVP, only immutable global variables can be exported.
            break;
        }
        }
    }
}

void WebAssemblyModuleRecord::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyModuleRecord* thisObject = jsCast<WebAssemblyModuleRecord*>(cell);
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_instance);
}

void WebAssemblyModuleRecord::link(ExecState* state, JSWebAssemblyInstance* instance)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);
    auto* globalObject = state->lexicalGlobalObject();

    const Wasm::ModuleInformation& moduleInformation = instance->module()->moduleInformation();
    SymbolTable* exportSymbolTable = instance->module()->exportSymbolTable();

    // FIXME wire up the imports. https://bugs.webkit.org/show_bug.cgi?id=165118

    // Let exports be a list of (string, JS value) pairs that is mapped from each external value e in instance.exports as follows:
    JSModuleEnvironment* moduleEnvironment = JSModuleEnvironment::create(vm, globalObject, nullptr, exportSymbolTable, JSValue(), this);
    for (const auto& exp : moduleInformation.exports) {
        JSValue exportedValue;
        switch (exp.kind) {
        case Wasm::External::Function: {
            // 1. If e is a closure c:
            //   i. If there is an Exported Function Exotic Object func in funcs whose func.[[Closure]] equals c, then return func.
            //   ii. (Note: At most one wrapper is created for any closure, so func is unique, even if there are multiple occurrances in the list. Moreover, if the item was an import that is already an Exported Function Exotic Object, then the original function object will be found. For imports that are regular JS functions, a new wrapper will be created.)
            //   iii. Otherwise:
            //     a. Let func be an Exported Function Exotic Object created from c.
            //     b. Append func to funcs.
            //     c. Return func.
            JSWebAssemblyCallee* wasmCallee = instance->module()->callee(exp.functionIndex);
            Wasm::Signature* signature = moduleInformation.functions.at(exp.functionIndex).signature;
            WebAssemblyFunction* function = WebAssemblyFunction::create(vm, globalObject, signature->arguments.size(), exp.field.string(), instance, wasmCallee, signature);
            exportedValue = function;
            break;
        }
        case Wasm::External::Table: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164135
            break;
        }
        case Wasm::External::Memory: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164134
            break;
        }
        case Wasm::External::Global: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
            // In the MVP, only immutable global variables can be exported.
            break;
        }
        }

        bool shouldThrowReadOnlyError = false;
        bool ignoreReadOnlyErrors = true;
        bool putResult = false;
        symbolTablePutTouchWatchpointSet(moduleEnvironment, state, exp.field, exportedValue, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
        RELEASE_ASSERT(putResult);
    }

    RELEASE_ASSERT(!m_instance);
    m_instance.set(vm, this, instance);
    m_moduleEnvironment.set(vm, this, moduleEnvironment);
}

JSValue WebAssemblyModuleRecord::evaluate(ExecState* state)
{
    // FIXME this should call the module's `start` function, if any. https://bugs.webkit.org/show_bug.cgi?id=165150
    // https://github.com/WebAssembly/design/blob/master/Modules.md#module-start-function
    // The start function must not take any arguments or return anything
    UNUSED_PARAM(state);
    return jsUndefined();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
