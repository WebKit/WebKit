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
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyInstancePrototype.h"
#include "WebAssemblyModuleRecord.h"

#include "WebAssemblyInstanceConstructor.lut.h"

namespace JSC {

static const bool verbose = false;

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
    JSWebAssemblyModule* jsModule = jsDynamicCast<JSWebAssemblyModule*>(state->argument(0));
    if (!jsModule)
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("first argument to WebAssembly.Instance must be a WebAssembly.Module"), defaultSourceAppender, runtimeTypeForValue(state->argument(0)))));
    const Wasm::ModuleInformation& moduleInformation = jsModule->moduleInformation();

    // If the importObject parameter is not undefined and Type(importObject) is not Object, a TypeError is thrown.
    JSValue importArgument = state->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (!importArgument.isUndefined() && !importObject)
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("second argument to WebAssembly.Instance must be undefined or an Object"), defaultSourceAppender, runtimeTypeForValue(importArgument))));

    // If the list of module.imports is not empty and Type(importObject) is not Object, a TypeError is thrown.
    if (moduleInformation.imports.size() && !importObject)
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("second argument to WebAssembly.Instance must be Object because the WebAssembly.Module has imports"), defaultSourceAppender, runtimeTypeForValue(importArgument))));

    Identifier moduleKey = Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyInstance"));
    WebAssemblyModuleRecord* moduleRecord = WebAssemblyModuleRecord::create(state, vm, globalObject->webAssemblyModuleRecordStructure(), moduleKey, moduleInformation);
    RETURN_IF_EXCEPTION(scope, { });

    Structure* instanceStructure = InternalFunction::createSubclassStructure(state, state->newTarget(), globalObject->WebAssemblyInstanceStructure());
    RETURN_IF_EXCEPTION(scope, { });

    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::create(vm, instanceStructure, jsModule, moduleRecord->getModuleNamespace(state), moduleInformation.imports.size());
    RETURN_IF_EXCEPTION(scope, { });

    // Let funcs, memories and tables be initially-empty lists of callable JavaScript objects, WebAssembly.Memory objects and WebAssembly.Table objects, respectively.
    // Let imports be an initially-empty list of external values.
    unsigned numImportFunctions = 0;

    // FIXME implement Table https://bugs.webkit.org/show_bug.cgi?id=164135
    // FIXME implement Memory https://bugs.webkit.org/show_bug.cgi?id=164134
    // FIXME implement Global https://bugs.webkit.org/show_bug.cgi?id=164133

    // For each import i in module.imports:
    for (auto& import : moduleInformation.imports) {
        // 1. Let o be the resultant value of performing Get(importObject, i.module_name).
        JSValue importModuleValue = importObject->get(state, import.module);
        RETURN_IF_EXCEPTION(scope, { });
        // 2. If Type(o) is not Object, throw a TypeError.
        if (!importModuleValue.isObject())
            return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("import must be an object"), defaultSourceAppender, runtimeTypeForValue(importModuleValue))));
        // 3. Let v be the value of performing Get(o, i.item_name)
        JSObject* object = jsCast<JSObject*>(importModuleValue);
        JSValue value = object->get(state, import.field);
        RETURN_IF_EXCEPTION(scope, { });
        switch (import.kind) {
        case Wasm::External::Function: {
            // 4. If i is a function import:
            // i. If IsCallable(v) is false, throw a TypeError.
            if (!value.isFunction())
                return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("import function must be callable"), defaultSourceAppender, runtimeTypeForValue(value))));
            JSCell* cell = value.asCell();
            // ii. If v is an Exported Function Exotic Object:
            if (WebAssemblyFunction* importedExports = jsDynamicCast<WebAssemblyFunction*>(object)) {
                // FIXME handle Function Exotic Object properly. https://bugs.webkit.org/show_bug.cgi?id=165282
                // a. If the signature of v does not match the signature of i, throw a TypeError.
                // b. Let closure be v.[[Closure]].
                RELEASE_ASSERT_NOT_REACHED();
                UNUSED_PARAM(importedExports);
                break;
            }
            // iii. Otherwise:
            // a. Let closure be a new host function of the given signature which calls v by coercing WebAssembly arguments to JavaScript arguments via ToJSValue and returns the result, if any, by coercing via ToWebAssemblyValue.
            // Note: done as part of Plan compilation.
            // iv. Append v to funcs.
            instance->setImportFunction(vm, cell, numImportFunctions++);
            // v. Append closure to imports.
            break;
        }
        case Wasm::External::Table: {
            // 7. Otherwise (i is a table import):
            // FIXME implement Table https://bugs.webkit.org/show_bug.cgi?id=164135
            // i. If v is not a WebAssembly.Table object, throw a TypeError.
            // ii. Append v to tables.
            // iii. Append v.[[Table]] to imports.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        case Wasm::External::Memory: {
            // 6. If i is a memory import:
            // FIXME implement Memory https://bugs.webkit.org/show_bug.cgi?id=164134
            // i. If v is not a WebAssembly.Memory object, throw a TypeError.
            // ii. Append v to memories.
            // iii. Append v.[[Memory]] to imports.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        case Wasm::External::Global: {
            // 5. If i is a global import:
            // FIXME implement Global https://bugs.webkit.org/show_bug.cgi?id=164133
            // i. If i is not an immutable global, throw a TypeError.
            // ii. If Type(v) is not Number, throw a TypeError.
            // iii. Append ToWebAssemblyValue(v) to imports.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        }
    }

    moduleRecord->link(state, instance);
    RETURN_IF_EXCEPTION(scope, { });
    if (verbose)
        moduleRecord->dump();
    JSValue startResult = moduleRecord->evaluate(state);
    UNUSED_PARAM(startResult);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(instance);
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

