/*
 * Copyright (C) 2015-2017 Apple Inc. All Rights Reserved.
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
#include "JSModuleRecord.h"
#include "JSSourceCode.h"
#include "ModuleAnalyzer.h"
#include "ModuleLoaderPrototype.h"
#include "Nodes.h"
#include "ObjectConstructor.h"
#include "Parser.h"
#include "ParserError.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSModuleLoader);

const ClassInfo JSModuleLoader::s_info = { "ModuleLoader", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSModuleLoader) };

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
    putDirect(vm, Identifier::fromString(&vm, "registry"), map);
}

// ------------------------------ Functions --------------------------------

static String printableModuleKey(ExecState* exec, JSValue key)
{
    VM& vm = exec->vm();
    if (key.isString() || key.isSymbol())
        return key.toPropertyKey(exec).impl();
    return vm.propertyNames->emptyIdentifier.impl();
}

JSValue JSModuleLoader::provideFetch(ExecState* exec, JSValue key, const SourceCode& sourceCode)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().provideFetchPublicName()));
    RETURN_IF_EXCEPTION(scope, { });
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    SourceCode source { sourceCode };
    MarkedArgumentBuffer arguments;
    arguments.append(key);
    arguments.append(JSSourceCode::create(vm, WTFMove(source)));
    ASSERT(!arguments.hasOverflowed());

    scope.release();
    return call(exec, function, callType, callData, this, arguments);
}

JSInternalPromise* JSModuleLoader::loadAndEvaluateModule(ExecState* exec, JSValue moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().loadAndEvaluateModulePublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    scope.release();
    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::loadModule(ExecState* exec, JSValue moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().loadModulePublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    scope.release();
    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSValue JSModuleLoader::linkAndEvaluateModule(ExecState* exec, JSValue moduleKey, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().linkAndEvaluateModulePublicName()));
    RETURN_IF_EXCEPTION(scope, { });
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    scope.release();
    return call(exec, function, callType, callData, this, arguments);
}

JSInternalPromise* JSModuleLoader::requestImportModule(ExecState* exec, const Identifier& moduleKey, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* function = jsCast<JSObject*>(get(exec, vm.propertyNames->builtinNames().requestImportModulePublicName()));
    RETURN_IF_EXCEPTION(scope, nullptr);
    CallData callData;
    auto callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(jsString(exec, moduleKey.impl()));
    arguments.append(parameters);
    arguments.append(scriptFetcher);
    ASSERT(!arguments.hasOverflowed());

    scope.release();
    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::importModule(ExecState* exec, JSString* moduleName, JSValue parameters, const SourceOrigin& referrer)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [import] ", printableModuleKey(exec, moduleName), "\n");

    auto* globalObject = exec->lexicalGlobalObject();
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (globalObject->globalObjectMethodTable()->moduleLoaderImportModule)
        return globalObject->globalObjectMethodTable()->moduleLoaderImportModule(globalObject, exec, this, moduleName, parameters, referrer);

    auto* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    auto moduleNameString = moduleName->value(exec);
    if (UNLIKELY(scope.exception())) {
        JSValue exception = scope.exception()->value();
        scope.clearException();
        deferred->reject(exec, exception);
        return deferred->promise();
    }
    deferred->reject(exec, createError(exec, makeString("Could not import the module '", moduleNameString, "'.")));
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
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, exec->lexicalGlobalObject());
    scope.releaseAssertNoException();
    const Identifier moduleKey = resolveSync(exec, name, referrer, scriptFetcher);
    if (UNLIKELY(scope.exception())) {
        JSValue exception = scope.exception();
        scope.clearException();
        return deferred->reject(exec, exception);
    }
    auto result = deferred->resolve(exec, identifierToJSValue(vm, moduleKey));
    scope.releaseAssertNoException();
    return result;
}

JSInternalPromise* JSModuleLoader::fetch(ExecState* exec, JSValue key, JSValue parameters, JSValue scriptFetcher)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [fetch] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    if (globalObject->globalObjectMethodTable()->moduleLoaderFetch)
        return globalObject->globalObjectMethodTable()->moduleLoaderFetch(globalObject, exec, this, key, parameters, scriptFetcher);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    String moduleKey = key.toWTFString(exec);
    if (UNLIKELY(scope.exception())) {
        JSValue exception = scope.exception()->value();
        scope.clearException();
        deferred->reject(exec, exception);
        return deferred->promise();
    }
    deferred->reject(exec, createError(exec, makeString("Could not open the module '", moduleKey, "'.")));
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

    JSModuleRecord* moduleRecord = jsDynamicCast<JSModuleRecord*>(exec->vm(), moduleRecordValue);
    if (!moduleRecord)
        return jsUndefined();
    return moduleRecord->evaluate(exec);
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

    scope.release();
    return moduleRecord->getModuleNamespace(exec);
}

} // namespace JSC
