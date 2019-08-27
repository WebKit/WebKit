/*
 * Copyright (C) 2015-2019 Apple Inc. All Rights Reserved.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "JSModuleLoader.h"

#include "BuiltinNames.h"
#include "CatchScope.h"
#include "CodeProfiling.h"
#include "Error.h"
#include "Exception.h"
#include "JSCInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSMap.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSSourceCode.h"
#include "JSWebAssembly.h"
#include "ModuleAnalyzer.h"
#include "Nodes.h"
#include "ObjectConstructor.h"
#include "Parser.h"
#include "ParserError.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL moduleLoaderParseModule(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderRequestedModules(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderEvaluate(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderModuleDeclarationInstantiation(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderResolve(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderResolveSync(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderFetch(ExecState*);
static EncodedJSValue JSC_HOST_CALL moduleLoaderGetModuleNamespaceObject(ExecState*);

}

#include "JSModuleLoader.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSModuleLoader);

const ClassInfo JSModuleLoader::s_info = { "ModuleLoader", &Base::s_info, &moduleLoaderTable, nullptr, CREATE_METHOD_TABLE(JSModuleLoader) };

/* Source for JSModuleLoader.lut.h
@begin moduleLoaderTable
    ensureRegistered               JSBuiltin                                  DontEnum|Function 1
    forceFulfillPromise            JSBuiltin                                  DontEnum|Function 2
    fulfillFetch                   JSBuiltin                                  DontEnum|Function 2
    requestFetch                   JSBuiltin                                  DontEnum|Function 3
    requestInstantiate             JSBuiltin                                  DontEnum|Function 3
    requestSatisfy                 JSBuiltin                                  DontEnum|Function 3
    link                           JSBuiltin                                  DontEnum|Function 2
    moduleDeclarationInstantiation moduleLoaderModuleDeclarationInstantiation DontEnum|Function 2
    moduleEvaluation               JSBuiltin                                  DontEnum|Function 2
    evaluate                       moduleLoaderEvaluate                       DontEnum|Function 3
    provideFetch                   JSBuiltin                                  DontEnum|Function 2
    loadAndEvaluateModule          JSBuiltin                                  DontEnum|Function 3
    loadModule                     JSBuiltin                                  DontEnum|Function 3
    linkAndEvaluateModule          JSBuiltin                                  DontEnum|Function 2
    requestImportModule            JSBuiltin                                  DontEnum|Function 3
    dependencyKeysIfEvaluated      JSBuiltin                                  DontEnum|Function 1
    getModuleNamespaceObject       moduleLoaderGetModuleNamespaceObject       DontEnum|Function 1
    parseModule                    moduleLoaderParseModule                    DontEnum|Function 2
    requestedModules               moduleLoaderRequestedModules               DontEnum|Function 1
    resolve                        moduleLoaderResolve                        DontEnum|Function 2
    resolveSync                    moduleLoaderResolveSync                    DontEnum|Function 2
    fetch                          moduleLoaderFetch                          DontEnum|Function 3
@end
*/

JSModuleLoader::JSModuleLoader(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void JSModuleLoader::finishCreation(ExecState* exec, VM& vm, JSGlobalObject* globalObject)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);

    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSMap* map = JSMap::create(exec, vm, globalObject->mapStructure());
    scope.releaseAssertNoException();
    putDirect(vm, Identifier::fromString(vm, "registry"), map);
}

// ------------------------------ Functions --------------------------------

static String printableModuleKey(ExecState* exec, JSValue key)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (key.isString() || key.isSymbol()) {
        auto propertyName = key.toPropertyKey(exec);
        scope.assertNoException(); // This is OK since this function is just for debugging purpose.
        return propertyName.impl();
    }
    return vm.propertyNames->emptyIdentifier.impl();
}

JSArray* JSModuleLoader::dependencyKeysIfEvaluated(ExecState* exec, JSValue key)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().dependencyKeysIfEvaluatedPublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    CallType callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(key);

    JSValue result = call(exec, function, callType, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return jsDynamicCast<JSArray*>(vm, result);
}

JSValue JSModuleLoader::provideFetch(ExecState* exec, JSValue key, const SourceCode& sourceCode)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().provideFetchPublicName()));
    RETURN_IF_EXCEPTION(scope, { });
    CallData callData;
    CallType callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    SourceCode source { sourceCode };
    MarkedArgumentBuffer arguments;
    arguments.append(key);
    arguments.append(JSSourceCode::create(vm, WTFMove(source)));
    ASSERT(!arguments.hasOverflowed());

    RELEASE_AND_RETURN(scope, call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::loadAndEvaluateModule(ExecState* exec, JSValue moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().loadAndEvaluateModulePublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    CallType callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    JSValue promise = call(exec, function, callType, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsCast<JSInternalPromise*>(promise);
}

JSInternalPromise* JSModuleLoader::loadModule(ExecState* exec, JSValue moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().loadModulePublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    CallType callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    JSValue promise = call(exec, function, callType, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsCast<JSInternalPromise*>(promise);
}

JSValue JSModuleLoader::linkAndEvaluateModule(ExecState* exec, JSValue moduleKey, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().linkAndEvaluateModulePublicName()));
    RETURN_IF_EXCEPTION(scope, { });
    CallData callData;
    CallType callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    RELEASE_AND_RETURN(scope, call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::requestImportModule(ExecState* exec, const Identifier& moduleKey, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().requestImportModulePublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    auto callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(jsString(vm, moduleKey.impl()));
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    JSValue promise = call(exec, function, callType, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsCast<JSInternalPromise*>(promise);
}

JSInternalPromise* JSModuleLoader::importModule(ExecState* exec, JSString* moduleName, JSValue parameters, const SourceOrigin& referrer)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [import] ", printableModuleKey(exec, moduleName), "\n");

    auto* globalObject = exec->lexicalGlobalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (globalObject->globalObjectMethodTable()->moduleLoaderImportModule)
        RELEASE_AND_RETURN(throwScope, globalObject->globalObjectMethodTable()->moduleLoaderImportModule(globalObject, exec, this, moduleName, parameters, referrer));

    auto* deferred = JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto moduleNameString = moduleName->value(exec);
    if (UNLIKELY(catchScope.exception())) {
        JSValue exception = catchScope.exception()->value();
        catchScope.clearException();
        deferred->reject(exec, exception);
        catchScope.clearException();
        return deferred->promise();
    }
    deferred->reject(exec, createError(exec, makeString("Could not import the module '", moduleNameString, "'.")));
    catchScope.clearException();
    return deferred->promise();
}

Identifier JSModuleLoader::resolveSync(ExecState* exec, JSValue name, JSValue referrer, JSValue scriptFetcher)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [resolve] ", printableModuleKey(exec, name), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderResolve)
        return globalObject->globalObjectMethodTable()->moduleLoaderResolve(globalObject, exec, this, name, referrer, scriptFetcher);
    return name.toPropertyKey(exec);
}

JSInternalPromise* JSModuleLoader::resolve(ExecState* exec, JSValue name, JSValue referrer, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::tryCreate(exec, exec->lexicalGlobalObject());
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    const Identifier moduleKey = resolveSync(exec, name, referrer, scriptFetcher);
    if (UNLIKELY(catchScope.exception())) {
        JSValue exception = catchScope.exception();
        catchScope.clearException();
        auto result = deferred->reject(exec, exception);
        catchScope.clearException();
        return result;
    }
    auto result = deferred->resolve(exec, identifierToJSValue(vm, moduleKey));
    catchScope.clearException();
    return result;
}

JSInternalPromise* JSModuleLoader::fetch(ExecState* exec, JSValue key, JSValue parameters, JSValue scriptFetcher)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [fetch] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (globalObject->globalObjectMethodTable()->moduleLoaderFetch)
        RELEASE_AND_RETURN(throwScope, globalObject->globalObjectMethodTable()->moduleLoaderFetch(globalObject, exec, this, key, parameters, scriptFetcher));

    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    String moduleKey = key.toWTFString(exec);
    if (UNLIKELY(catchScope.exception())) {
        JSValue exception = catchScope.exception()->value();
        catchScope.clearException();
        deferred->reject(exec, exception);
        catchScope.clearException();
        return deferred->promise();
    }
    deferred->reject(exec, createError(exec, makeString("Could not open the module '", moduleKey, "'.")));
    catchScope.clearException();
    return deferred->promise();
}

JSObject* JSModuleLoader::createImportMetaProperties(ExecState* exec, JSValue key, JSModuleRecord* moduleRecord, JSValue scriptFetcher)
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderCreateImportMetaProperties)
        return globalObject->globalObjectMethodTable()->moduleLoaderCreateImportMetaProperties(globalObject, exec, this, key, moduleRecord, scriptFetcher);
    return constructEmptyObject(exec, exec->lexicalGlobalObject()->nullPrototypeObjectStructure());
}

JSValue JSModuleLoader::evaluate(ExecState* exec, JSValue key, JSValue moduleRecordValue, JSValue scriptFetcher)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [evaluate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderEvaluate)
        return globalObject->globalObjectMethodTable()->moduleLoaderEvaluate(globalObject, exec, this, key, moduleRecordValue, scriptFetcher);

    return evaluateNonVirtual(exec, key, moduleRecordValue, scriptFetcher);
}

JSValue JSModuleLoader::evaluateNonVirtual(ExecState* exec, JSValue, JSValue moduleRecordValue, JSValue)
{
    if (auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(exec->vm(), moduleRecordValue))
        return moduleRecord->evaluate(exec);
    return jsUndefined();
}

JSModuleNamespaceObject* JSModuleLoader::getModuleNamespaceObject(ExecState* exec, JSValue moduleRecordValue)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(vm, moduleRecordValue);
    if (!moduleRecord) {
        throwTypeError(exec, scope);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, moduleRecord->getModuleNamespace(exec));
}

// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL moduleLoaderParseModule(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::tryCreate(exec, exec->lexicalGlobalObject());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto reject = [&] (JSValue rejectionReason) {
        catchScope.clearException();
        auto result = deferred->reject(exec, rejectionReason);
        catchScope.clearException();
        return JSValue::encode(result);
    };

    const Identifier moduleKey = exec->argument(0).toPropertyKey(exec);
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception());

    JSValue source = exec->argument(1);
    auto* jsSourceCode = jsCast<JSSourceCode*>(source);
    SourceCode sourceCode = jsSourceCode->sourceCode();

#if ENABLE(WEBASSEMBLY)
    if (sourceCode.provider()->sourceType() == SourceProviderSourceType::WebAssembly)
        return JSValue::encode(JSWebAssembly::instantiate(exec, deferred, moduleKey, jsSourceCode));
#endif

    CodeProfiling profile(sourceCode);

    ParserError error;
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        vm, sourceCode, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);
    if (error.isValid())
        return reject(error.toErrorObject(exec->lexicalGlobalObject(), sourceCode));
    ASSERT(moduleProgramNode);

    ModuleAnalyzer moduleAnalyzer(exec, moduleKey, sourceCode, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables());
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception());

    auto result = deferred->resolve(exec, moduleAnalyzer.analyze(*moduleProgramNode));
    catchScope.clearException();
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL moduleLoaderRequestedModules(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(vm, exec->argument(0));
    if (!moduleRecord) 
        RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyArray(exec, nullptr)));

    JSArray* result = constructEmptyArray(exec, nullptr, moduleRecord->requestedModules().size());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t i = 0;
    for (auto& key : moduleRecord->requestedModules()) {
        result->putDirectIndex(exec, i++, jsString(vm, key.get()));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL moduleLoaderModuleDeclarationInstantiation(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(vm, exec->argument(0));
    if (!moduleRecord)
        return JSValue::encode(jsUndefined());

    if (Options::dumpModuleLoadingState())
        dataLog("Loader [link] ", moduleRecord->moduleKey(), "\n");

    moduleRecord->link(exec, exec->argument(1));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(jsUndefined());
}

// ------------------------------ Hook Functions ---------------------------

EncodedJSValue JSC_HOST_CALL moduleLoaderResolve(ExecState* exec)
{
    VM& vm = exec->vm();
    // Hook point, Loader.resolve.
    // https://whatwg.github.io/loader/#browser-resolve
    // Take the name and resolve it to the unique identifier for the resource location.
    // For example, take the "jquery" and return the URL for the resource.
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->resolve(exec, exec->argument(0), exec->argument(1), exec->argument(2)));
}

EncodedJSValue JSC_HOST_CALL moduleLoaderResolveSync(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    auto result = loader->resolveSync(exec, exec->argument(0), exec->argument(1), exec->argument(2));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(identifierToJSValue(vm, result));
}

EncodedJSValue JSC_HOST_CALL moduleLoaderFetch(ExecState* exec)
{
    VM& vm = exec->vm();
    // Hook point, Loader.fetch
    // https://whatwg.github.io/loader/#browser-fetch
    // Take the key and fetch the resource actually.
    // For example, JavaScriptCore shell can provide the hook fetching the resource
    // from the local file system.
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->fetch(exec, exec->argument(0), exec->argument(1), exec->argument(2)));
}

EncodedJSValue JSC_HOST_CALL moduleLoaderGetModuleNamespaceObject(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* loader = jsDynamicCast<JSModuleLoader*>(vm, exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    auto* moduleNamespaceObject = loader->getModuleNamespaceObject(exec, exec->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(moduleNamespaceObject);
}

// ------------------- Additional Hook Functions ---------------------------

EncodedJSValue JSC_HOST_CALL moduleLoaderEvaluate(ExecState* exec)
{
    // To instrument and retrieve the errors raised from the module execution,
    // we inserted the hook point here.

    VM& vm = exec->vm();
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, exec->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->evaluate(exec, exec->argument(0), exec->argument(1), exec->argument(2)));
}

} // namespace JSC
