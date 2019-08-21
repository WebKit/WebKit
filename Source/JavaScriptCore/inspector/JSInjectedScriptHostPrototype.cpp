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

using namespace JSC;

namespace Inspector {

static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionSubtype(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionFunctionDetails(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionGetInternalProperties(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionInternalConstructorName(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionProxyTargetValue(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapSize(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapEntries(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetSize(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetEntries(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIteratorEntries(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryInstances(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryHolders(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension(ExecState*);

static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeEvaluate(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeSavedResultAlias(ExecState*);

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

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeEvaluate(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->evaluate(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeAttributeSavedResultAlias(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->savedResultAlias(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionInternalConstructorName(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->internalConstructorName(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsHTMLAllCollection(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->isHTMLAllCollection(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIsPromiseRejectedWithNativeGetterTypeError(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->isPromiseRejectedWithNativeGetterTypeError(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionProxyTargetValue(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->proxyTargetValue(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapSize(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->weakMapSize(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakMapEntries(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->weakMapEntries(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetSize(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->weakSetSize(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionWeakSetEntries(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->weakSetEntries(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionIteratorEntries(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->iteratorEntries(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryInstances(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->queryInstances(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionQueryHolders(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->queryHolders(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionEvaluateWithScopeExtension(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->evaluateWithScopeExtension(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionSubtype(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->subtype(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionFunctionDetails(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->functionDetails(exec));
}

EncodedJSValue JSC_HOST_CALL jsInjectedScriptHostPrototypeFunctionGetInternalProperties(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    JSInjectedScriptHost* castedThis = jsDynamicCast<JSInjectedScriptHost*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(exec, scope);

    return JSValue::encode(castedThis->getInternalProperties(exec));
}

} // namespace Inspector

