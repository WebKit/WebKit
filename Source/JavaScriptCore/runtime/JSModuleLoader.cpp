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
#include "JSScriptFetchParameters.h"
#include "JSSourceCode.h"
#include "JSWebAssembly.h"
#include "ModuleAnalyzer.h"
#include "Nodes.h"
#include "ObjectConstructor.h"
#include "Parser.h"
#include "ParserError.h"
#include "SyntheticModuleRecord.h"
#include "VMTrapsInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(moduleLoaderParseModule);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderRequestedModules);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderRequestedModuleParameters);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderEvaluate);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderModuleDeclarationInstantiation);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderResolve);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderFetch);
static JSC_DECLARE_HOST_FUNCTION(moduleLoaderGetModuleNamespaceObject);

}

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSModuleLoader);

const ClassInfo JSModuleLoader::s_info = { "ModuleLoader"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSModuleLoader) };

JSModuleLoader::JSModuleLoader(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void JSModuleLoader::finishCreation(JSGlobalObject* globalObject, VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSMap* map = JSMap::create(vm, globalObject->mapStructure());
    putDirect(vm, Identifier::fromString(vm, "registry"_s), map);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("getModuleNamespaceObject"_s, moduleLoaderGetModuleNamespaceObject, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("parseModule"_s, moduleLoaderParseModule, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("requestedModules"_s, moduleLoaderRequestedModules, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("requestedModuleParameters"_s, moduleLoaderRequestedModuleParameters, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("resolve"_s, moduleLoaderResolve, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("fetch"_s, moduleLoaderFetch, static_cast<unsigned>(PropertyAttribute::DontEnum), 3, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("moduleDeclarationInstantiation"_s, moduleLoaderModuleDeclarationInstantiation, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Private);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("evaluate"_s, moduleLoaderEvaluate, static_cast<unsigned>(PropertyAttribute::DontEnum), 3, ImplementationVisibility::Private);

    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().ensureRegisteredPublicName(), moduleLoaderEnsureRegisteredCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().requestFetchPublicName(), moduleLoaderRequestFetchCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().requestInstantiatePublicName(), moduleLoaderRequestInstantiateCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().requestSatisfyPublicName(), moduleLoaderRequestSatisfyCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().linkPublicName(), moduleLoaderLinkCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().moduleEvaluationPublicName(), moduleLoaderModuleEvaluationCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().asyncModuleEvaluationPublicName(), moduleLoaderAsyncModuleEvaluationCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().provideFetchPublicName(), moduleLoaderProvideFetchCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().loadAndEvaluateModulePublicName(), moduleLoaderLoadAndEvaluateModuleCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().loadModulePublicName(), moduleLoaderLoadModuleCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().linkAndEvaluateModulePublicName(), moduleLoaderLinkAndEvaluateModuleCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().requestImportModulePublicName(), moduleLoaderRequestImportModuleCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().dependencyKeysIfEvaluatedPublicName(), moduleLoaderDependencyKeysIfEvaluatedCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
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
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(key);
    ASSERT(!arguments.hasOverflowed());

    JSValue result = call(globalObject, function, callData, this, arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return jsDynamicCast<JSArray*>(result);
}

JSValue JSModuleLoader::provideFetch(JSGlobalObject* globalObject, JSValue key, const SourceCode& sourceCode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().provideFetchPublicName());
    RETURN_IF_EXCEPTION(scope, { });
    auto callData = JSC::getCallData(function);
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
    auto callData = JSC::getCallData(function);
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

JSInternalPromise* JSModuleLoader::loadModule(JSGlobalObject* globalObject, JSValue moduleKey, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().loadModulePublicName());
    RETURN_IF_EXCEPTION(scope, nullptr);
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);
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
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    RELEASE_AND_RETURN(scope, call(globalObject, function, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::requestImportModule(JSGlobalObject* globalObject, const Identifier& moduleName, JSValue referrer, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* function = getAs<JSObject*>(globalObject, vm.propertyNames->builtinNames().requestImportModulePublicName());
    RETURN_IF_EXCEPTION(scope, nullptr);
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(jsString(vm, moduleName.string()));
    arguments.append(referrer);
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
    promise->reject(globalObject, createError(globalObject, makeString("Could not import the module '", WTFMove(moduleNameString), "'.")));
    return promise;
}

Identifier JSModuleLoader::resolve(JSGlobalObject* globalObject, JSValue name, JSValue referrer, JSValue scriptFetcher)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [resolve] ", printableModuleKey(globalObject, name));

    if (globalObject->globalObjectMethodTable()->moduleLoaderResolve)
        return globalObject->globalObjectMethodTable()->moduleLoaderResolve(globalObject, this, name, referrer, scriptFetcher);
    return name.toPropertyKey(globalObject);
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
    if (auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(moduleRecordValue))
        return moduleRecord->evaluate(globalObject, sentValue, resumeMode);
    return jsUndefined();
}

JSModuleNamespaceObject* JSModuleLoader::getModuleNamespaceObject(JSGlobalObject* globalObject, JSValue moduleRecordValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(moduleRecordValue);
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

    dataLogLnIf(Options::dumpModuleLoadingState(), "loader [parsing] ", moduleKey);

    JSValue source = callFrame->argument(1);
    auto* jsSourceCode = jsCast<JSSourceCode*>(source);
    SourceCode sourceCode = jsSourceCode->sourceCode();
    SourceProviderSourceType sourceType = sourceCode.provider()->sourceType();

    switch (sourceType) {
#if ENABLE(WEBASSEMBLY)
        case SourceProviderSourceType::WebAssembly:
            RELEASE_AND_RETURN(scope, JSValue::encode(JSWebAssembly::instantiate(globalObject, promise, moduleKey, jsSourceCode)));
#endif
        case SourceProviderSourceType::JSON: {
            // https://tc39.es/proposal-json-modules/#sec-parse-json-module
            auto* moduleRecord = SyntheticModuleRecord::parseJSONModule(globalObject, moduleKey, WTFMove(sourceCode));
            RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));
            scope.release();
            promise->resolve(globalObject, moduleRecord);
            return JSValue::encode(promise);
        }
        case SourceProviderSourceType::Synthetic: {
            SyntheticSourceProvider* syntheticSourceProvider = reinterpret_cast<SyntheticSourceProvider*>(sourceCode.provider());
            MarkedArgumentBuffer args;
            Vector<Identifier, 4> exportNames;
            syntheticSourceProvider->generate(globalObject, moduleKey, exportNames, args);
            RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));
            
            auto* moduleRecord = SyntheticModuleRecord::tryCreateWithExportNamesAndValues(globalObject, moduleKey, exportNames, args);
            RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

            scope.release();
            promise->resolve(globalObject, moduleRecord);
            return JSValue::encode(promise);
        }
        default: {
            break;
        }
    }

    ParserError error;
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        vm, sourceCode, Identifier(), ImplementationVisibility::Public, JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);
    if (error.isValid())
        RELEASE_AND_RETURN(scope, JSValue::encode(rejectWithError(error.toErrorObject(globalObject, sourceCode))));
    ASSERT(moduleProgramNode);

    ModuleAnalyzer moduleAnalyzer(globalObject, moduleKey, sourceCode, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables(), moduleProgramNode->features());
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    auto result = moduleAnalyzer.analyze(*moduleProgramNode);
    if (!result)
        RELEASE_AND_RETURN(scope, JSValue::encode(rejectWithError(createTypeError(globalObject, result.error()))));

    scope.release();
    promise->resolve(globalObject, result.value());
    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderRequestedModules, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(callFrame->argument(0));
    if (!moduleRecord) 
        RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyArray(globalObject, nullptr)));

    JSArray* result = constructEmptyArray(globalObject, nullptr, moduleRecord->requestedModules().size());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t i = 0;
    for (auto& request : moduleRecord->requestedModules()) {
        result->putDirectIndex(globalObject, i++, jsString(vm, String { request.m_specifier.get() }));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderRequestedModuleParameters, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(callFrame->argument(0));
    if (!moduleRecord)
        RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyArray(globalObject, nullptr)));

    JSArray* result = constructEmptyArray(globalObject, nullptr, moduleRecord->requestedModules().size());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t i = 0;
    for (auto& request : moduleRecord->requestedModules()) {
        if (request.m_assertions)
            result->putDirectIndex(globalObject, i++, JSScriptFetchParameters::create(vm, vm.scriptFetchParametersStructure.get(), *request.m_assertions));
        else
            result->putDirectIndex(globalObject, i++, jsUndefined());
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderModuleDeclarationInstantiation, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* moduleRecord = jsDynamicCast<AbstractModuleRecord*>(callFrame->argument(0));
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
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    auto result = loader->resolve(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(identifierToJSValue(vm, result));
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderFetch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // Hook point, Loader.fetch
    // https://whatwg.github.io/loader/#browser-fetch
    // Take the key and fetch the resource actually.
    // For example, JavaScriptCore shell can provide the hook fetching the resource
    // from the local file system.
    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->fetch(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2)));
}

JSC_DEFINE_HOST_FUNCTION(moduleLoaderGetModuleNamespaceObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* loader = jsDynamicCast<JSModuleLoader*>(callFrame->thisValue());
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

    JSModuleLoader* loader = jsDynamicCast<JSModuleLoader*>(callFrame->thisValue());
    if (!loader)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(loader->evaluate(globalObject, callFrame->argument(0), callFrame->argument(1), callFrame->argument(2), callFrame->argument(3), callFrame->argument(4)));
}

} // namespace JSC
