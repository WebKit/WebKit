/*
 * Copyright (C) 2015-2021 Apple Inc. All Rights Reserved.
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
#include "JSCInlines.h"
#include "JSInternalPromise.h"
#include "JSMap.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSSourceCode.h"
#include "JSWebAssembly.h"
#include "ModuleAnalyzer.h"
#include "Nodes.h"
#include "ObjectConstructor.h"
#include "Parser.h"
#include "ParserError.h"
#include "VMTrapsInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(moduleLoaderParseModule);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderRequestedModules);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderEvaluate);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderModuleDeclarationInstantiation);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderResolve);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderResolveSync);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderFetch);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderGetModuleNamespaceObject);

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
    asyncModuleEvaluation          JSBuiltin                                  DontEnum|Function 3
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

void JSModuleLoader::finishCreation(JSGlobalObject* globalObject, VM& vm)
{
    DeferTerminationForAWhile deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSMap* map = JSMap::create(globalObject, vm, globalObject->mapStructure());
    scope.releaseAssertNoException();
    putDirect(vm, Identifier::fromString(vm, "registry"), map);
}

// ------------------------------ Functions --------------------------------

static String printableModuleKey(JSGlobalObject* globalObject, JSValue key)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    if (key.isString() || key.isSymbol()) {
        auto propertyName = key.toPropertyKey(globalObject);
        scope.assertNoExceptionExceptTermination(); // This is OK since this function is just for debugging purpose.
        return propertyName.impl();
    }
    return vm.propertyNames->emptyIdentifier.impl();
}

JSArray* JSModuleLoader::dependencyKeysIfEvaluated(JSGlobalObject* globalObject, JSValue key)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().dependencyKeysIfEvaluatedPublicName());
    RETURN_IF_EXCEPTION(scope, nullptr);
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(key);

    JSValue result = call(globalObject, function, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return jsDynamicCast<JSArray*>(vm, result);
}

JSValue JSModuleLoader::provideFetch(JSGlobalObject* globalObject, JSValue key, const SourceCode& sourceCode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().provideFetchPublicName());
    RETURN_IF_EXCEPTION(scope, { });
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != CallData::Type::None);

    SourceCode source { sourceCode };
    MarkedArgumentBuffer arguments;
    arguments.append(key);
    arguments.append(JSSourceCode::create(vm, WTFMove(source)));
    ASSERT(!arguments.hasOverflowed());

    RELEASE_AND_RETURN(scope, call(globalObject, function, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::loadAndEvaluateModule(JSGlobalObject* globalObject, JSValue moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().loadAndEvaluateModulePublicName());
    RETURN_IF_EXCEPTION(scope, nullptr);
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    JSValue promise = call(globalObject, function, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsCast<JSInternalPromise*>(promise);
}

JSInternalPromise* JSModuleLoader::loadModule(JSGlobalObject* globalObject, JSValue moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().loadModulePublicName());
    RETURN_IF_EXCEPTION(scope, nullptr);
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    JSValue promise = call(globalObject, function, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsCast<JSInternalPromise*>(promise);
}

JSValue JSModuleLoader::linkAndEvaluateModule(JSGlobalObject* globalObject, JSValue moduleKey, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().linkAndEvaluateModulePublicName());
    RETURN_IF_EXCEPTION(scope, { });
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    RELEASE_AND_RETURN(scope, call(globalObject, function, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::requestImportModule(JSGlobalObject* globalObject, const Identifier& moduleKey, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().requestImportModulePublicName());
    RETURN_IF_EXCEPTION(scope, nullptr);
    auto callData = JSC::getCallData(vm, function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(jsString(vm, moduleKey.impl()));
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    JSValue promise = call(globalObject, function, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsCast<JSInternalPromise*>(promise);
}

JSInternalPromise* JSModuleLoader::importModule(JSGlobalObject* globalObject, JSString* moduleName, JSValue parameters, const SourceOrigin& referrer)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [import] ", printableModuleKey(globalObject, moduleName));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (globalObject->globalObjectMethodTable()->moduleLoaderImportModule)
        RELEASE_AND_RETURN(scope, globalObject->globalObjectMethodTable()->moduleLoaderImportModule(globalObject, this, moduleName, parameters, referrer));

    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    String moduleNameString = moduleName->value(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    scope.release();
    promise->reject(globalObject, createError(globalObject, makeString("Could not import the module '", moduleNameString, "'.")));
    return promise;
}

Identifier JSModuleLoader::resolveSync(JSGlobalObject* globalObject, JSValue name, JSValue referrer, JSValue scriptFetcher)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [resolve] ", printableModuleKey(globalObject, name));

    if (globalObject->globalObjectMethodTable()->moduleLoaderResolve)
        return globalObject->globalObjectMethodTable()->moduleLoaderResolve(globalObject, this, name, referrer, scriptFetcher);
    return name.toPropertyKey(globalObject);
}

JSInternalPromise* JSModuleLoader::resolve(JSGlobalObject* globalObject, JSValue name, JSValue referrer, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    const Identifier moduleKey = resolveSync(globalObject, name, referrer, scriptFetcher);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    scope.release();
    promise->resolve(globalObject, identifierToJSValue(vm, moduleKey));
    return promise;
}

JSInternalPromise* JSModuleLoader::fetch(JSGlobalObject* globalObject, JSValue key, JSValue parameters, JSValue scriptFetcher)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [fetch] ", printableModuleKey(globalObject, key));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (globalObject->globalObjectMethodTable()->moduleLoaderFetch)
        RELEASE_AND_RETURN(scope, globalObject->globalObjectMethodTable()->moduleLoaderFetch(globalObject, this, key, parameters, scriptFetcher));

    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    String moduleKey = key.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    scope.release();
    promise->reject(globalObject, createError(globalObject, makeString("Could not open the module '", moduleKey, "'.")));
    return promise;
}

JSObject* JSModuleLoader::createImportMetaProperties(JSGlobalObject* globalObject, JSValue key, JSModuleRecord* moduleRecord, JSValue scriptFetcher)
{
    if (globalObject->globalObjectMethodTable()->moduleLoaderCreateImportMetaProperties)
        return globalObject->globalObjectMethodTable()->moduleLoaderCreateImportMetaProperties(globalObject, this, key, moduleRecord, scriptFetcher);
    return constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
}

JSValue JSModuleLoader::evaluate(JSGlobalObject* globalObject, JSValue key, JSValue moduleRecordValue, JSValue scriptFetcher, JSValue sentValue, JSValue resumeMode)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [evaluate] ", printableModuleKey(globalObject, key));

    if (globalObject->globalObjectMethodTable()->moduleLoaderEvaluate)
        return globalObject->globalObjectMethodTable()->moduleLoaderEvaluate(globalObject, this, key, moduleRecordValue, scriptFetcher, sentValue, resumeMode);

    return evaluateNonVirtual(globalObject, key, moduleRecordValue, scriptFetcher, sentValue, resumeMode);
}

JSValue JSModuleLoader::evaluateNonVirtual(JSGlobalObject* globalObject, JSValue, JSValue moduleRecordValue, JSValue, JSValue sentValue, JSValue resumeMode)
{
    if (auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(globalObject->vm(), moduleRecordValue))
        return moduleRecord->evaluate(globalObject, sentValue, resumeMode);
    return jsUndefined();
}

JSModuleNamespaceObject* JSModuleLoader::getModuleNamespaceObject(JSGlobalObject* globalObject, JSValue moduleRecordValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(vm, moduleRecordValue);
    if (!moduleRecord) {
        throwTypeError(globalObject, scope);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, moduleRecord->getModuleNamespace(globalObject));
}

// ------------------------------ Functions --------------------------------

JSC_DEFINE_HOST_FUNCTION(moduleLoaderParseModule, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto rejectWithError = [&](JSValue error) {
        promise->reject(globalObject, error);
        return promise;
    };

    const Identifier moduleKey = callFrame->argument(0).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    JSValue source = callFrame->argument(1);
    auto* jsSourceCode = jsCast<JSSourceCode*>(source);
    SourceCode sourceCode = jsSourceCode->sourceCode();

#if ENABLE(WEBASSEMBLY)
    if (sourceCode.provider()->sourceType() == SourceProviderSourceType::WebAssembly)
        RELEASE_AND_RETURN(scope, JSValue::encode(JSWebAssembly::instantiate(globalObject, promise, moduleKey, jsSourceCode)));
#endif

    dataLogLnIf(Options::dumpModuleLoadingState(), "loader [parsing] ", moduleKey);

    ParserError error;
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        vm, sourceCode, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);
    if (error.isValid())
        RELEASE_AND_RETURN(scope, JSValue::encode(rejectWithError(error.toErrorObject(globalObject, sourceCode))));
    ASSERT(moduleProgramNode);

    ModuleAnalyzer moduleAnalyzer(globalObject, moduleKey, sourceCode, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables());
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    scope.release();
    promise->resolve(globalObject, moduleAnalyzer.analyze(*moduleProgramNode));
    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderRequestedModules, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(vm, callFrame->argument(0));
    if (!moduleRecord) 
        RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyArray(globalObject, nullptr)));

    JSArray* result = constructEmptyArray(globalObject, nullptr, moduleRecord->requestedModules().size());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t i = 0;
    for (auto& key : moduleRecord->requestedModules()) {
        result->putDirectIndex(globalObject, i++, jsString(vm, key.get()));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderModuleDeclarationInstantiation, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(vm, callFrame->argument(0));
    if (!moduleRecord)
        return JSValue::encode(jsUndefined());

    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [link] ", moduleRecord->moduleKey());

    auto sync = moduleRecord->link(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(jsBoolean(sync == Synchronousness::Async));
}

// ------------------------------ Hook Functions ---------------------------

JSC_DEFINE_HOST_FUNCTION(moduleLoaderResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    // Hook point, Loader.resolve.
    // https://whatwg.github.io/loader/#browser-resolve
    // Take the name and resolve it to the unique identifier for the resource location.
    // For example, take the "jquery" and return the URL for the resource.
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->resolve(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2)));
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderResolveSync, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    auto result = loader->resolveSync(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(identifierToJSValue(vm, result));
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderFetch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    // Hook point, Loader.fetch
    // https://whatwg.github.io/loader/#browser-fetch
    // Take the key and fetch the resource actually.
    // For example, JavaScriptCore shell can provide the hook fetching the resource
    // from the local file system.
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->fetch(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2)));
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderGetModuleNamespaceObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* loader = jsDynamicCast<JSModuleLoader*>(vm, callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    auto* moduleNamespaceObject = loader->getModuleNamespaceObject(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(moduleNamespaceObject);
}

// ------------------- Additional Hook Functions ---------------------------

JSC_DEFINE_HOST_FUNCTION(moduleLoaderEvaluate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // To instrument and retrieve the errors raised from the module execution,
    // we inserted the hook point here.

    VM& vm = globalObject->vm();
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(vm, callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->evaluate(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2), callFrame->argument(3), callFrame->argument(4)));
}

} // namespace JSC
