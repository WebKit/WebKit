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
#include "WebAssemblyModuleConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "ArrayBuffer.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyModule.h"
#include "ObjectConstructor.h"
#include "SymbolTable.h"
#include "WasmCallee.h"
#include "WasmModuleInformation.h"
#include "WasmPlan.h"
#include "WebAssemblyModulePrototype.h"
#include <wtf/StdLibExtras.h>

namespace JSC {
static EncodedJSValue JSC_HOST_CALL webAssemblyModuleCustomSections(ExecState*);
static EncodedJSValue JSC_HOST_CALL webAssemblyModuleImports(ExecState*);
static EncodedJSValue JSC_HOST_CALL webAssemblyModuleExports(ExecState*);
}

#include "WebAssemblyModuleConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyModuleConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyModule, nullptr, CREATE_METHOD_TABLE(WebAssemblyModuleConstructor) };

/* Source for WebAssemblyModuleConstructor.lut.h
 @begin constructorTableWebAssemblyModule
 customSections webAssemblyModuleCustomSections DontEnum|Function 2
 imports        webAssemblyModuleImports        DontEnum|Function 1
 exports        webAssemblyModuleExports        DontEnum|Function 1
 @end
 */

EncodedJSValue JSC_HOST_CALL webAssemblyModuleCustomSections(ExecState* exec)
{
    VM& vm = exec->vm();
    auto* globalObject = exec->lexicalGlobalObject();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, exec->argument(0));
    if (!module)
        return JSValue::encode(throwException(exec, throwScope, createTypeError(exec, ASCIILiteral("WebAssembly.Module.customSections called with non WebAssembly.Module argument"))));

    const String sectionNameString = exec->argument(1).getString(exec);
    RETURN_IF_EXCEPTION(throwScope, { });

    JSArray* result = constructEmptyArray(exec, nullptr, globalObject);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& customSections = module->moduleInformation().customSections;
    for (const Wasm::CustomSection& section : customSections) {
        if (String::fromUTF8(section.name) == sectionNameString) {
            auto buffer = ArrayBuffer::tryCreate(section.payload.data(), section.payload.size());
            if (!buffer)
                return JSValue::encode(throwException(exec, throwScope, createOutOfMemoryError(exec)));

            result->push(exec, JSArrayBuffer::create(vm, globalObject->m_arrayBufferStructure.get(), WTFMove(buffer)));
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL webAssemblyModuleImports(ExecState* exec)
{
    VM& vm = exec->vm();
    auto* globalObject = exec->lexicalGlobalObject();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, exec->argument(0));
    if (!module)
        return JSValue::encode(throwException(exec, throwScope, createTypeError(exec, ASCIILiteral("WebAssembly.Module.imports called with non WebAssembly.Module argument"))));

    JSArray* result = constructEmptyArray(exec, nullptr, globalObject);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& imports = module->moduleInformation().imports;
    if (imports.size()) {
        Identifier module = Identifier::fromString(exec, "module");
        Identifier name = Identifier::fromString(exec, "name");
        Identifier kind = Identifier::fromString(exec, "kind");
        for (const Wasm::Import& imp : imports) {
            JSObject* obj = constructEmptyObject(exec);
            RETURN_IF_EXCEPTION(throwScope, { });
            obj->putDirect(vm, module, jsString(exec, String::fromUTF8(imp.module)));
            obj->putDirect(vm, name, jsString(exec, String::fromUTF8(imp.field)));
            obj->putDirect(vm, kind, jsString(exec, String(makeString(imp.kind))));
            result->push(exec, obj);
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL webAssemblyModuleExports(ExecState* exec)
{
    VM& vm = exec->vm();
    auto* globalObject = exec->lexicalGlobalObject();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, exec->argument(0));
    if (!module)
        return JSValue::encode(throwException(exec, throwScope, createTypeError(exec, ASCIILiteral("WebAssembly.Module.exports called with non WebAssembly.Module argument"))));

    JSArray* result = constructEmptyArray(exec, nullptr, globalObject);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& exports = module->moduleInformation().exports;
    if (exports.size()) {
        Identifier name = Identifier::fromString(exec, "name");
        Identifier kind = Identifier::fromString(exec, "kind");
        for (const Wasm::Export& exp : exports) {
            JSObject* obj = constructEmptyObject(exec);
            RETURN_IF_EXCEPTION(throwScope, { });
            obj->putDirect(vm, name, jsString(exec, String::fromUTF8(exp.field)));
            obj->putDirect(vm, kind, jsString(exec, String(makeString(exp.kind))));
            result->push(exec, obj);
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyModule(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* structure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), exec->lexicalGlobalObject()->WebAssemblyModuleStructure());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    throwScope.release();
    return JSValue::encode(WebAssemblyModuleConstructor::createModule(exec, exec->argument(0), structure));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyModule(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(exec, scope, "WebAssembly.Module"));
}

JSValue WebAssemblyModuleConstructor::createModule(ExecState* exec, JSValue buffer, Structure* structure)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<uint8_t> source = createSourceBufferFromValue(vm, exec, buffer);
    RETURN_IF_EXCEPTION(scope, { });

    return JSWebAssemblyModule::createStub(vm, exec, structure, Wasm::Module::validateSync(vm, WTFMove(source)));
}

WebAssemblyModuleConstructor* WebAssemblyModuleConstructor::create(VM& vm, Structure* structure, WebAssemblyModulePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyModuleConstructor>(vm.heap)) WebAssemblyModuleConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyModuleConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyModuleConstructor::finishCreation(VM& vm, WebAssemblyModulePrototype* prototype)
{
    Base::finishCreation(vm, ASCIILiteral("Module"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

WebAssemblyModuleConstructor::WebAssemblyModuleConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

ConstructType WebAssemblyModuleConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructJSWebAssemblyModule;
    return ConstructType::Host;
}

CallType WebAssemblyModuleConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callJSWebAssemblyModule;
    return CallType::Host;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

