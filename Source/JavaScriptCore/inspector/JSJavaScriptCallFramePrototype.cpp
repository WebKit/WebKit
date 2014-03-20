/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "Error.h"
#include "GetterSetter.h"
#include "Identifier.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSJavaScriptCallFrame.h"

using namespace JSC;

namespace Inspector {

// Functions.
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFramePrototypeFunctionEvaluate(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFramePrototypeFunctionScopeType(ExecState*);

// Attributes.
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeCaller(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeSourceID(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeLine(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeColumn(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeFunctionName(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeScopeChain(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeThisObject(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeType(ExecState*);

// Constants.
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantGLOBAL_SCOPE(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantLOCAL_SCOPE(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantWITH_SCOPE(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantCLOSURE_SCOPE(ExecState*);
static EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantCATCH_SCOPE(ExecState*);

const ClassInfo JSJavaScriptCallFramePrototype::s_info = { "JavaScriptCallFrame", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSJavaScriptCallFramePrototype) };

#define JSC_NATIVE_NON_INDEX_ACCESSOR(jsName, cppName, attributes) \
    { \
        Identifier identifier(&vm, jsName); \
        GetterSetter* accessor = GetterSetter::create(vm); \
        JSFunction* function = JSFunction::create(vm, globalObject, 0, identifier.string(), cppName); \
        accessor->setGetter(vm, function); \
        putDirectNonIndexAccessor(vm, identifier, accessor, (attributes)); \
    }

void JSJavaScriptCallFramePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.prototypeMap.addPrototype(this);

    JSC_NATIVE_FUNCTION("evaluate", jsJavaScriptCallFramePrototypeFunctionEvaluate, DontEnum, 1);
    JSC_NATIVE_FUNCTION("scopeType", jsJavaScriptCallFramePrototypeFunctionScopeType, DontEnum, 1);

    JSC_NATIVE_NON_INDEX_ACCESSOR("caller", jsJavaScriptCallFrameAttributeCaller, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("sourceID", jsJavaScriptCallFrameAttributeSourceID, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("line", jsJavaScriptCallFrameAttributeLine, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("column", jsJavaScriptCallFrameAttributeColumn, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("functionName", jsJavaScriptCallFrameAttributeFunctionName, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("scopeChain", jsJavaScriptCallFrameAttributeScopeChain, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("thisObject", jsJavaScriptCallFrameAttributeThisObject, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("type", jsJavaScriptCallFrameAttributeType, DontEnum | Accessor);

    JSC_NATIVE_NON_INDEX_ACCESSOR("GLOBAL_SCOPE", jsJavaScriptCallFrameConstantGLOBAL_SCOPE, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("LOCAL_SCOPE", jsJavaScriptCallFrameConstantLOCAL_SCOPE, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("WITH_SCOPE", jsJavaScriptCallFrameConstantWITH_SCOPE, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("CLOSURE_SCOPE", jsJavaScriptCallFrameConstantCLOSURE_SCOPE, DontEnum | Accessor);
    JSC_NATIVE_NON_INDEX_ACCESSOR("CATCH_SCOPE", jsJavaScriptCallFrameConstantCATCH_SCOPE, DontEnum | Accessor);
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFramePrototypeFunctionEvaluate(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->evaluate(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFramePrototypeFunctionScopeType(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->scopeType(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeCaller(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->caller(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeSourceID(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->sourceID(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeLine(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->line(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeColumn(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->column(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeFunctionName(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->functionName(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeScopeChain(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->scopeChain(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeThisObject(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->thisObject(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameAttributeType(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    JSJavaScriptCallFrame* castedThis = jsDynamicCast<JSJavaScriptCallFrame*>(thisValue);
    if (!castedThis)
        return throwVMTypeError(exec);

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSJavaScriptCallFrame::info());
    return JSValue::encode(castedThis->type(exec));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantGLOBAL_SCOPE(ExecState*)
{
    return JSValue::encode(jsNumber(JSJavaScriptCallFrame::GLOBAL_SCOPE));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantLOCAL_SCOPE(ExecState*)
{
    return JSValue::encode(jsNumber(JSJavaScriptCallFrame::LOCAL_SCOPE));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantWITH_SCOPE(ExecState*)
{
    return JSValue::encode(jsNumber(JSJavaScriptCallFrame::WITH_SCOPE));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantCLOSURE_SCOPE(ExecState*)
{
    return JSValue::encode(jsNumber(JSJavaScriptCallFrame::CLOSURE_SCOPE));
}

EncodedJSValue JSC_HOST_CALL jsJavaScriptCallFrameConstantCATCH_SCOPE(ExecState*)
{
    return JSValue::encode(jsNumber(JSJavaScriptCallFrame::CATCH_SCOPE));
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
