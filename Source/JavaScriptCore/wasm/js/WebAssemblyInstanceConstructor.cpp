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
#include "WebAssemblyInstanceConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "FunctionPrototype.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "WebAssemblyInstancePrototype.h"

#include "WebAssemblyInstanceConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyInstanceConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyInstance, CREATE_METHOD_TABLE(WebAssemblyInstanceConstructor) };

/* Source for WebAssemblyInstanceConstructor.lut.h
 @begin constructorTableWebAssemblyInstance
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyInstance(ExecState* state)
{
    auto& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = state->lexicalGlobalObject();

    // If moduleObject is not a WebAssembly.Module instance, a TypeError is thrown.
    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(state->argument(0));
    if (!module)
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("first argument to WebAssembly.Instance must be a WebAssembly.Module"), defaultSourceAppender, runtimeTypeForValue(state->argument(0)))));

    // If the importObject parameter is not undefined and Type(importObject) is not Object, a TypeError is thrown.
    JSValue importArgument = state->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (!importArgument.isUndefined() && !importObject)
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("second argument to WebAssembly.Instance must be undefined or an Object"), defaultSourceAppender, runtimeTypeForValue(importArgument))));

    // If the list of module.imports is not empty and Type(importObject) is not Object, a TypeError is thrown.
    if (module->moduleInformation()->imports.size() && !importObject)
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("second argument to WebAssembly.Instance must be Object because the WebAssembly.Module has imports"), defaultSourceAppender, runtimeTypeForValue(importArgument))));

    // FIXME String things from https://bugs.webkit.org/show_bug.cgi?id=164023
    // Let exports be a list of (string, JS value) pairs that is mapped from each external value e in instance.exports as follows:
    IdentifierSet instanceExports;
    for (const auto& name : instanceExports) {
        // FIXME validate according to Module.Instance spec.
        (void)name;
    }
    Identifier moduleKey;
    SourceCode sourceCode;
    VariableEnvironment declaredVariables;
    VariableEnvironment lexicalVariables;
    auto* moduleRecord = JSModuleRecord::create(state, vm, globalObject->moduleRecordStructure(), moduleKey, sourceCode, declaredVariables, lexicalVariables);
    auto* moduleNamespaceObject = JSModuleNamespaceObject::create(state, globalObject, globalObject->moduleNamespaceObjectStructure(), moduleRecord, instanceExports);

    auto* structure = InternalFunction::createSubclassStructure(state, state->newTarget(), globalObject->WebAssemblyInstanceStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(JSWebAssemblyInstance::create(vm, structure, moduleNamespaceObject));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyInstance(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(state, scope, "WebAssembly.Instance"));
}

WebAssemblyInstanceConstructor* WebAssemblyInstanceConstructor::create(VM& vm, Structure* structure, WebAssemblyInstancePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyInstanceConstructor>(vm.heap)) WebAssemblyInstanceConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyInstanceConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyInstanceConstructor::finishCreation(VM& vm, WebAssemblyInstancePrototype* prototype)
{
    Base::finishCreation(vm, ASCIILiteral("Instance"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

WebAssemblyInstanceConstructor::WebAssemblyInstanceConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

ConstructType WebAssemblyInstanceConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructJSWebAssemblyInstance;
    return ConstructType::Host;
}

CallType WebAssemblyInstanceConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callJSWebAssemblyInstance;
    return CallType::Host;
}

void WebAssemblyInstanceConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<WebAssemblyInstanceConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

