/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
#include "ModuleLoaderObject.h"

#include "BuiltinNames.h"
#include "CodeProfiling.h"
#include "Error.h"
#include "Exception.h"
#include "JSCInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSMap.h"
#include "JSModuleEnvironment.h"
#include "JSModuleRecord.h"
#include "ModuleAnalyzer.h"
#include "Nodes.h"
#include "Parser.h"
#include "ParserError.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectParseModule(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectRequestedModules(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectEvaluate(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectModuleDeclarationInstantiation(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectResolve(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectFetch(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectTranslate(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderObjectInstantiate(ExecState*);

}

#include "ModuleLoaderObject.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ModuleLoaderObject);

const ClassInfo ModuleLoaderObject::s_info = { "ModuleLoader", &Base::s_info, &moduleLoaderObjectTable, CREATE_METHOD_TABLE(ModuleLoaderObject) };

/* Source for ModuleLoaderObject.lut.h
@begin moduleLoaderObjectTable
    setStateToMax                  JSBuiltin                                        DontEnum|Function 2
    newRegistryEntry               JSBuiltin                                        DontEnum|Function 1
    ensureRegistered               JSBuiltin                                        DontEnum|Function 1
    forceFulfillPromise            JSBuiltin                                        DontEnum|Function 2
    fulfillFetch                   JSBuiltin                                        DontEnum|Function 2
    fulfillTranslate               JSBuiltin                                        DontEnum|Function 2
    fulfillInstantiate             JSBuiltin                                        DontEnum|Function 2
    commitInstantiated             JSBuiltin                                        DontEnum|Function 3
    instantiation                  JSBuiltin                                        DontEnum|Function 3
    requestFetch                   JSBuiltin                                        DontEnum|Function 1
    requestTranslate               JSBuiltin                                        DontEnum|Function 1
    requestInstantiate             JSBuiltin                                        DontEnum|Function 1
    requestResolveDependencies     JSBuiltin                                        DontEnum|Function 1
    requestInstantiateAll          JSBuiltin                                        DontEnum|Function 1
    requestLink                    JSBuiltin                                        DontEnum|Function 1
    requestReady                   JSBuiltin                                        DontEnum|Function 1
    link                           JSBuiltin                                        DontEnum|Function 1
    moduleDeclarationInstantiation moduleLoaderObjectModuleDeclarationInstantiation DontEnum|Function 2
    moduleEvaluation               JSBuiltin                                        DontEnum|Function 2
    evaluate                       moduleLoaderObjectEvaluate                       DontEnum|Function 2
    provide                        JSBuiltin                                        DontEnum|Function 3
    loadAndEvaluateModule          JSBuiltin                                        DontEnum|Function 2
    loadModule                     JSBuiltin                                        DontEnum|Function 2
    linkAndEvaluateModule          JSBuiltin                                        DontEnum|Function 1
    parseModule                    moduleLoaderObjectParseModule                    DontEnum|Function 2
    requestedModules               moduleLoaderObjectRequestedModules               DontEnum|Function 1
    resolve                        moduleLoaderObjectResolve                        DontEnum|Function 1
    fetch                          moduleLoaderObjectFetch                          DontEnum|Function 1
    translate                      moduleLoaderObjectTranslate                      DontEnum|Function 2
    instantiate                    moduleLoaderObjectInstantiate                    DontEnum|Function 2
@end
*/

ModuleLoaderObject::ModuleLoaderObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void ModuleLoaderObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    // Constant values for the state.
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "Fetch"), jsNumber(Status::Fetch));
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "Translate"), jsNumber(Status::Translate));
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "Instantiate"), jsNumber(Status::Instantiate));
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "ResolveDependencies"), jsNumber(Status::ResolveDependencies));
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "Link"), jsNumber(Status::Link));
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "Ready"), jsNumber(Status::Ready));

    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "registry"), JSMap::create(vm, globalObject->mapStructure()));
}

bool ModuleLoaderObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<Base>(exec, moduleLoaderObjectTable, jsCast<ModuleLoaderObject*>(object), propertyName, slot);
}

// ------------------------------ Functions --------------------------------

static String printableModuleKey(ExecState* exec, JSValue key)
{
    if (key.isString() || key.isSymbol())
        return key.toPropertyKey(exec).impl();
    return exec->propertyNames().emptyIdentifier.impl();
}

JSValue ModuleLoaderObject::provide(ExecState* exec, JSValue key, Status status, const String& source)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().providePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(key);
    arguments.append(jsNumber(status));
    arguments.append(jsString(exec, source));

    return call(exec, function, callType, callData, this, arguments);
}

JSInternalPromise* ModuleLoaderObject::loadAndEvaluateModule(ExecState* exec, JSValue moduleName, JSValue referrer)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().loadAndEvaluateModulePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(referrer);

    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* ModuleLoaderObject::loadModule(ExecState* exec, JSValue moduleName, JSValue referrer)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().loadModulePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(referrer);

    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* ModuleLoaderObject::linkAndEvaluateModule(ExecState* exec, JSValue moduleKey)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().linkAndEvaluateModulePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);

    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* ModuleLoaderObject::resolve(ExecState* exec, JSValue name, JSValue referrer)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [resolve] ", printableModuleKey(exec, name), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderResolve)
        return globalObject->globalObjectMethodTable()->moduleLoaderResolve(globalObject, exec, name, referrer);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->resolve(exec, name);
    return deferred->promise();
}

JSInternalPromise* ModuleLoaderObject::fetch(ExecState* exec, JSValue key)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [fetch] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderFetch)
        return globalObject->globalObjectMethodTable()->moduleLoaderFetch(globalObject, exec, key);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    String moduleKey = key.toString(exec)->value(exec);
    if (exec->hadException()) {
        JSValue exception = exec->exception()->value();
        exec->clearException();
        deferred->reject(exec, exception);
        return deferred->promise();
    }
    deferred->reject(exec, createError(exec, makeString("Could not open the module '", moduleKey, "'.")));
    return deferred->promise();
}

JSInternalPromise* ModuleLoaderObject::translate(ExecState* exec, JSValue key, JSValue payload)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [translate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderTranslate)
        return globalObject->globalObjectMethodTable()->moduleLoaderTranslate(globalObject, exec, key, payload);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->resolve(exec, payload);
    return deferred->promise();
}

JSInternalPromise* ModuleLoaderObject::instantiate(ExecState* exec, JSValue key, JSValue source)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [instantiate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderInstantiate)
        return globalObject->globalObjectMethodTable()->moduleLoaderInstantiate(globalObject, exec, key, source);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->resolve(exec, jsUndefined());
    return deferred->promise();
}

JSValue ModuleLoaderObject::evaluate(ExecState* exec, JSValue key, JSValue moduleRecordValue)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [evaluate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderEvaluate)
        return globalObject->globalObjectMethodTable()->moduleLoaderEvaluate(globalObject, exec, key, moduleRecordValue);

    JSModuleRecord* moduleRecord = jsDynamicCast<JSModuleRecord*>(moduleRecordValue);
    if (!moduleRecord)
        return jsUndefined();
    return moduleRecord->evaluate(exec);
}

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectParseModule(ExecState* exec)
{
    VM& vm = exec->vm();
    const Identifier moduleKey = exec->argument(0).toPropertyKey(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    String source = exec->argument(1).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    SourceCode sourceCode = makeSource(source, moduleKey.impl());

    CodeProfiling profile(sourceCode);

    ParserError error;
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        &vm, sourceCode, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);

    if (error.isValid()) {
        throwVMError(exec, error.toErrorObject(exec->lexicalGlobalObject(), sourceCode));
        return JSValue::encode(jsUndefined());
    }
    ASSERT(moduleProgramNode);

    ModuleAnalyzer moduleAnalyzer(exec, moduleKey, sourceCode, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables());
    JSModuleRecord* moduleRecord = moduleAnalyzer.analyze(*moduleProgramNode);

    return JSValue::encode(moduleRecord);
}

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectRequestedModules(ExecState* exec)
{
    JSModuleRecord* moduleRecord = jsDynamicCast<JSModuleRecord*>(exec->argument(0));
    if (!moduleRecord)
        return JSValue::encode(constructEmptyArray(exec, nullptr));

    JSArray* result = constructEmptyArray(exec, nullptr, moduleRecord->requestedModules().size());
    size_t i = 0;
    for (auto& key : moduleRecord->requestedModules())
        result->putDirectIndex(exec, i++, jsString(exec, key.get()));

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectModuleDeclarationInstantiation(ExecState* exec)
{
    JSModuleRecord* moduleRecord = jsDynamicCast<JSModuleRecord*>(exec->argument(0));
    if (!moduleRecord)
        return JSValue::encode(jsUndefined());

    if (Options::dumpModuleLoadingState())
        dataLog("Loader [link] ", moduleRecord->moduleKey(), "\n");

    moduleRecord->link(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    return JSValue::encode(jsUndefined());
}

// ------------------------------ Hook Functions ---------------------------

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectResolve(ExecState* exec)
{
    // Hook point, Loader.resolve.
    // https://whatwg.github.io/loader/#browser-resolve
    // Take the name and resolve it to the unique identifier for the resource location.
    // For example, take the "jquery" and return the URL for the resource.
    ModuleLoaderObject* loader = jsDynamicCast<ModuleLoaderObject*>(exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->resolve(exec, exec->argument(0), exec->argument(1)));
}

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectFetch(ExecState* exec)
{
    // Hook point, Loader.fetch
    // https://whatwg.github.io/loader/#browser-fetch
    // Take the key and fetch the resource actually.
    // For example, JavaScriptCore shell can provide the hook fetching the resource
    // from the local file system.
    ModuleLoaderObject* loader = jsDynamicCast<ModuleLoaderObject*>(exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->fetch(exec, exec->argument(0)));
}

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectTranslate(ExecState* exec)
{
    // Hook point, Loader.translate
    // https://whatwg.github.io/loader/#browser-translate
    // Take the key and the fetched source code and translate it to the ES6 source code.
    // Typically it is used by the transpilers.
    ModuleLoaderObject* loader = jsDynamicCast<ModuleLoaderObject*>(exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->translate(exec, exec->argument(0), exec->argument(1)));
}

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectInstantiate(ExecState* exec)
{
    // Hook point, Loader.instantiate
    // https://whatwg.github.io/loader/#browser-instantiate
    // Take the key and the translated source code, and instantiate the module record
    // by parsing the module source code.
    // It has the chance to provide the optional module instance that is different from
    // the ordinary one.
    ModuleLoaderObject* loader = jsDynamicCast<ModuleLoaderObject*>(exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->instantiate(exec, exec->argument(0), exec->argument(1)));
}

// ------------------- Additional Hook Functions ---------------------------

EncodedJSValue JSC_HOST_CALL moduleLoaderObjectEvaluate(ExecState* exec)
{
    // To instrument and retrieve the errors raised from the module execution,
    // we inserted the hook point here.

    ModuleLoaderObject* loader = jsDynamicCast<ModuleLoaderObject*>(exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->evaluate(exec, exec->argument(0), exec->argument(1)));
}

} // namespace JSC
