/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
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
#include "JSInjectedScriptHostPrototype.h"

#include "Error.h"
#include "GetterSetter.h"
#include "Identifier.h"
#include "InjectedScriptHost.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSInjectedScriptHost.h"

namespace Inspector {

using namespace JSC;

static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionSubtype(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionFunctionDetails(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionGetInternalProperties(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionInternalConstructorName(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionProxyTargetValue(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapSize(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapEntries(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetSize(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetEntries(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIteratorEntries(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryInstances(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryHolders(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension(JSGlobalObject*, CallFrame*);

static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeEvaluate(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeSavedResultAlias(JSGlobalObject*, CallFrame*);

const ClassInfo JSInjectedScriptHostPrototype::s_info = { "InjectedScriptHost", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSInjectedScriptHostPrototype) };

void JSInjectedScriptHostPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("subtype", jsInjectedScriptHostPrototypeFunctionSubtype, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("functionDetails", jsInjectedScriptHostPrototypeFunctionFunctionDetails, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("getInternalProperties", jsInjectedScriptHostPrototypeFunctionGetInternalProperties, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("internalConstructorName", jsInjectedScriptHostPrototypeFunctionInternalConstructorName, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("isHTMLAllCollection", jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("isPromiseRejectedWithNativeGetterTypeError", jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("proxyTargetValue", jsInjectedScriptHostPrototypeFunctionProxyTargetValue, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("weakMapSize", jsInjectedScriptHostPrototypeFunctionWeakMapSize, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("weakMapEntries", jsInjectedScriptHostPrototypeFunctionWeakMapEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("weakSetSize", jsInjectedScriptHostPrototypeFunctionWeakSetSize, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("weakSetEntries", jsInjectedScriptHostPrototypeFunctionWeakSetEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("iteratorEntries", jsInjectedScriptHostPrototypeFunctionIteratorEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("queryInstances", jsInjectedScriptHostPrototypeFunctionQueryInstances, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("queryHolders", jsInjectedScriptHostPrototypeFunctionQueryHolders, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("evaluateWithScopeExtension", jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("evaluate", jsInjectedScriptHostPrototypeAttributeEvaluate, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("savedResultAlias", jsInjectedScriptHostPrototypeAttributeSavedResultAlias, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeEvaluate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->evaluate(globalObject));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeSavedResultAlias(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->savedResultAlias(globalObject));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionInternalConstructorName(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->internalConstructorName(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->isHTMLAllCollection(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->isPromiseRejectedWithNativeGetterTypeError(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionProxyTargetValue(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->proxyTargetValue(vm, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapSize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakMapSize(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakMapEntries(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetSize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakSetSize(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakSetEntries(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIteratorEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->iteratorEntries(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryInstances(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->queryInstances(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryHolders(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->queryHolders(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->evaluateWithScopeExtension(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionSubtype(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->subtype(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionFunctionDetails(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->functionDetails(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionGetInternalProperties(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->getInternalProperties(globalObject, callFrame));
}

} // namespace Inspector

