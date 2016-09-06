/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
#include "ModuleLoaderPrototype.h"
#include "Nodes.h"
#include "Parser.h"
#include "ParserError.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSModuleLoader);

const ClassInfo JSModuleLoader::s_info = { "ModuleLoader", &Base::s_info, nullptr, CREATE_METHOD_TABLE(JSModuleLoader) };

JSModuleLoader::JSModuleLoader(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void JSModuleLoader::finishCreation(ExecState* exec, VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSMap* map = JSMap::create(exec, vm, globalObject->mapStructure());
    RELEASE_ASSERT(!scope.exception());
    putDirect(vm, Identifier::fromString(&vm, "registry"), map);
}

// ------------------------------ Functions --------------------------------

static String printableModuleKey(ExecState* exec, JSValue key)
{
    if (key.isString() || key.isSymbol())
        return key.toPropertyKey(exec).impl();
    return exec->propertyNames().emptyIdentifier.impl();
}

JSValue JSModuleLoader::provide(ExecState* exec, JSValue key, Status status, const String& source)
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

JSInternalPromise* JSModuleLoader::loadAndEvaluateModule(ExecState* exec, JSValue moduleName, JSValue referrer, JSValue initiator)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().loadAndEvaluateModulePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(referrer);
    arguments.append(initiator);

    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSInternalPromise* JSModuleLoader::loadModule(ExecState* exec, JSValue moduleName, JSValue referrer, JSValue initiator)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().loadModulePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleName);
    arguments.append(referrer);
    arguments.append(initiator);

    return jsCast<JSInternalPromise*>(call(exec, function, callType, callData, this, arguments));
}

JSValue JSModuleLoader::linkAndEvaluateModule(ExecState* exec, JSValue moduleKey, JSValue initiator)
{
    JSObject* function = jsCast<JSObject*>(get(exec, exec->propertyNames().builtinNames().linkAndEvaluateModulePublicName()));
    CallData callData;
    CallType callType = JSC::getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(moduleKey);
    arguments.append(initiator);

    return call(exec, function, callType, callData, this, arguments);
}

JSInternalPromise* JSModuleLoader::resolve(ExecState* exec, JSValue name, JSValue referrer, JSValue initiator)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [resolve] ", printableModuleKey(exec, name), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderResolve)
        return globalObject->globalObjectMethodTable()->moduleLoaderResolve(globalObject, exec, this, name, referrer, initiator);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->resolve(exec, name);
    return deferred->promise();
}

JSInternalPromise* JSModuleLoader::fetch(ExecState* exec, JSValue key, JSValue initiator)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [fetch] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderFetch)
        return globalObject->globalObjectMethodTable()->moduleLoaderFetch(globalObject, exec, this, key, initiator);
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

JSInternalPromise* JSModuleLoader::translate(ExecState* exec, JSValue key, JSValue payload, JSValue initiator)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [translate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderTranslate)
        return globalObject->globalObjectMethodTable()->moduleLoaderTranslate(globalObject, exec, this, key, payload, initiator);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->resolve(exec, payload);
    return deferred->promise();
}

JSInternalPromise* JSModuleLoader::instantiate(ExecState* exec, JSValue key, JSValue source, JSValue initiator)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [instantiate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderInstantiate)
        return globalObject->globalObjectMethodTable()->moduleLoaderInstantiate(globalObject, exec, this, key, source, initiator);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->resolve(exec, jsUndefined());
    return deferred->promise();
}

JSValue JSModuleLoader::evaluate(ExecState* exec, JSValue key, JSValue moduleRecordValue, JSValue initiator)
{
    if (Options::dumpModuleLoadingState())
        dataLog("Loader [evaluate] ", printableModuleKey(exec, key), "\n");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (globalObject->globalObjectMethodTable()->moduleLoaderEvaluate)
        return globalObject->globalObjectMethodTable()->moduleLoaderEvaluate(globalObject, exec, this, key, moduleRecordValue, initiator);

    JSModuleRecord* moduleRecord = jsDynamicCast<JSModuleRecord*>(moduleRecordValue);
    if (!moduleRecord)
        return jsUndefined();
    return moduleRecord->evaluate(exec);
}

} // namespace JSC
