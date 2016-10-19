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
#include "WebAssemblyObject.h"

#include "FunctionPrototype.h"
#include "JSCInlines.h"
#include "js/JSWebAssemblyCompileError.h"
#include "js/JSWebAssemblyInstance.h"
#include "js/JSWebAssemblyMemory.h"
#include "js/JSWebAssemblyModule.h"
#include "js/JSWebAssemblyRuntimeError.h"
#include "js/JSWebAssemblyTable.h"
#include "js/WebAssemblyCompileErrorConstructor.h"
#include "js/WebAssemblyCompileErrorPrototype.h"
#include "js/WebAssemblyInstanceConstructor.h"
#include "js/WebAssemblyInstancePrototype.h"
#include "js/WebAssemblyMemoryConstructor.h"
#include "js/WebAssemblyMemoryPrototype.h"
#include "js/WebAssemblyModuleConstructor.h"
#include "js/WebAssemblyModulePrototype.h"
#include "js/WebAssemblyRuntimeErrorConstructor.h"
#include "js/WebAssemblyRuntimeErrorPrototype.h"
#include "js/WebAssemblyTableConstructor.h"
#include "js/WebAssemblyTablePrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(WebAssemblyObject);

#define DECLARE_WASM_OBJECT_FUNCTION(NAME, ...) EncodedJSValue JSC_HOST_CALL wasmObjectFunc ## NAME(ExecState*);
FOR_EACH_WASM_FUNCTION_PROPERTY(DECLARE_WASM_OBJECT_FUNCTION)

const ClassInfo WebAssemblyObject::s_info = { "WebAssembly", &Base::s_info, 0, CREATE_METHOD_TABLE(WebAssemblyObject) };

WebAssemblyObject* WebAssemblyObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyObject>(vm.heap)) WebAssemblyObject(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* WebAssemblyObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

#define SET_UP_WASM_CONSTRUCTOR_PROPERTY(NAME, ...) \
    auto* prototype ## NAME = WebAssembly ## NAME ## Prototype::create(vm, globalObject, WebAssembly ## NAME ## Prototype::createStructure(vm, globalObject, globalObject->objectPrototype())); \
    auto* structure ## NAME = JSWebAssembly ## NAME::createStructure(vm, globalObject, prototype ## NAME); \
    auto* constructor ## NAME = WebAssembly ## NAME ## Constructor::create(vm, WebAssembly ## NAME ## Constructor::createStructure(vm, globalObject, globalObject->functionPrototype()), prototype ## NAME, structure ## NAME); \
    prototype ## NAME->putDirectWithoutTransition(vm, vm.propertyNames->constructor, constructor ## NAME, DontEnum);
    FOR_EACH_WASM_CONSTRUCTOR_PROPERTY(SET_UP_WASM_CONSTRUCTOR_PROPERTY)

#define REGISTER_WASM_CONSTRUCTOR_PROPERTY(NAME, ...) \
    putDirectWithoutTransition(vm, Identifier::fromString(globalObject->globalExec(), #NAME), constructor ## NAME, DontEnum);
    FOR_EACH_WASM_CONSTRUCTOR_PROPERTY(REGISTER_WASM_CONSTRUCTOR_PROPERTY)

#define REGISTER_WASM_FUNCTION_PROPERTY(NAME, LENGTH) \
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, #NAME), LENGTH, wasmObjectFunc ## NAME, NoIntrinsic, DontEnum);
    FOR_EACH_WASM_FUNCTION_PROPERTY(REGISTER_WASM_FUNCTION_PROPERTY)
}

WebAssemblyObject::WebAssemblyObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL wasmObjectFuncvalidate(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the validate function property"))));
}

EncodedJSValue JSC_HOST_CALL wasmObjectFunccompile(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the compile function property"))));
}

} // namespace JSC
