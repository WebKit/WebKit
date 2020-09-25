/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
#include "JSJavaScriptCallFramePrototype.h"

#include "JSCInlines.h"
#include "JSJavaScriptCallFrame.h"

namespace Inspector {

using namespace JSC;

// Functions.
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFramePrototypeFunctionEvaluateWithScopeExtension);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFramePrototypeFunctionScopeDescriptions);

// Attributes.
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeCaller);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeSourceID);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeLine);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeColumn);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeFunctionName);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeScopeChain);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeThisObject);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeType);
static JSC_DECLARE_HOST_FUNCTION(jsJavaScriptCallFrameIsTailDeleted);

const ClassInfo JSJavaScriptCallFramePrototype::s_info = { "JavaScriptCallFrame", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSJavaScriptCallFramePrototype) };

void JSJavaScriptCallFramePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("evaluateWithScopeExtension", jsJavaScriptCallFramePrototypeFunctionEvaluateWithScopeExtension, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("scopeDescriptions", jsJavaScriptCallFramePrototypeFunctionScopeDescriptions, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("caller", jsJavaScriptCallFrameAttributeCaller, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("sourceID", jsJavaScriptCallFrameAttributeSourceID, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("line", jsJavaScriptCallFrameAttributeLine, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("column", jsJavaScriptCallFrameAttributeColumn, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("functionName", jsJavaScriptCallFrameAttributeFunctionName, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("scopeChain", jsJavaScriptCallFrameAttributeScopeChain, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("thisObject", jsJavaScriptCallFrameAttributeThisObject, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("type", jsJavaScriptCallFrameAttributeType, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("isTailDeleted", jsJavaScriptCallFrameIsTailDeleted, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFramePrototypeFunctionEvaluateWithScopeExtension, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->evaluateWithScopeExtension(globalObject, callFrame));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFramePrototypeFunctionScopeDescriptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->scopeDescriptions(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeCaller, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->caller(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeSourceID, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->sourceID(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeLine, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->line(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeColumn, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->column(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeFunctionName, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->functionName(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeScopeChain, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->scopeChain(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeThisObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->thisObject(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameAttributeType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->type(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(jsJavaScriptCallFrameIsTailDeleted, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(vm, thisValue);
    if (!castedThis)
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(castedThis->isTailDeleted(globalObject));
}

} // namespace Inspector
