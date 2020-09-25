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

#include "JSCInlines.h"
#include "JSInjectedScriptHost.h"

namespace Inspector {

using namespace JSC;

static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionSubtype);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionFunctionDetails);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionGetInternalProperties);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionInternalConstructorName);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionProxyTargetValue);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakMapSize);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakMapEntries);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakSetSize);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakSetEntries);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionIteratorEntries);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionQueryInstances);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionQueryHolders);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension);

static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeAttributeEvaluate);
static JSC_DECLARE_HOST_FUNCTION(jsInjectedScriptHostPrototypeAttributeSavedResultAlias);

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

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeAttributeEvaluate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->evaluate(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeAttributeSavedResultAlias, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->savedResultAlias(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionInternalConstructorName, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->internalConstructorName(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->isHTMLAllCollection(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->isPromiseRejectedWithNativeGetterTypeError(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionProxyTargetValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->proxyTargetValue(vm, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakMapSize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakMapSize(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakMapEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakMapEntries(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakSetSize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakSetSize(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionWeakSetEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->weakSetEntries(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionIteratorEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->iteratorEntries(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionQueryInstances, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->queryInstances(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionQueryHolders, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->queryHolders(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->evaluateWithScopeExtension(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionSubtype, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->subtype(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionFunctionDetails, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->functionDetails(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsInjectedScriptHostPrototypeFunctionGetInternalProperties, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

