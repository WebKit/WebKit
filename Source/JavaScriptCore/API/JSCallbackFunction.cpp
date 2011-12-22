/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSCallbackFunction.h"

#include "APIShims.h"
#include "APICast.h"
#include "CodeBlock.h"
#include "ExceptionHelpers.h"
#include "JSCallbackObject.h"
#include "JSFunction.h"
#include "FunctionPrototype.h"
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include <wtf/Vector.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSCallbackFunction);

const ClassInfo JSCallbackFunction::s_info = { "CallbackFunction", &InternalFunction::s_info, 0, 0, CREATE_METHOD_TABLE(JSCallbackFunction) };

JSCallbackFunction::JSCallbackFunction(JSGlobalObject* globalObject, JSObjectCallAsFunctionCallback callback)
    : InternalFunction(globalObject, globalObject->callbackFunctionStructure())
    , m_callback(callback)
{
}

void JSCallbackFunction::finishCreation(JSGlobalData& globalData, const Identifier& name)
{
    Base::finishCreation(globalData, name);
    ASSERT(inherits(&s_info));
}

EncodedJSValue JSCallbackFunction::call(ExecState* exec)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef functionRef = toRef(exec->callee());
    JSObjectRef thisObjRef = toRef(exec->hostThisValue().toThisObject(exec));

    int argumentCount = static_cast<int>(exec->argumentCount());
    Vector<JSValueRef, 16> arguments(argumentCount);
    for (int i = 0; i < argumentCount; i++)
        arguments[i] = toRef(exec, exec->argument(i));

    JSValueRef exception = 0;
    JSValueRef result;
    {
        APICallbackShim callbackShim(exec);
        result = static_cast<JSCallbackFunction*>(toJS(functionRef))->m_callback(execRef, functionRef, thisObjRef, argumentCount, arguments.data(), &exception);
    }
    if (exception)
        throwError(exec, toJS(exec, exception));

    return JSValue::encode(toJS(exec, result));
}

CallType JSCallbackFunction::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = call;
    return CallTypeHost;
}

JSValueRef JSCallbackFunction::toStringCallback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    JSObject* object = toJS(thisObject);
    if (object->inherits(&JSCallbackObject<JSNonFinalObject>::s_info)) {
        for (JSClassRef jsClass = jsCast<JSCallbackObject<JSNonFinalObject>*>(object)->classRef(); jsClass; jsClass = jsClass->parentClass)
            if (jsClass->convertToType)
                return jsClass->convertToType(ctx, thisObject, kJSTypeString, exception);
    } else if (object->inherits(&JSCallbackObject<JSGlobalObject>::s_info)) {
        for (JSClassRef jsClass = jsCast<JSCallbackObject<JSGlobalObject>*>(object)->classRef(); jsClass; jsClass = jsClass->parentClass)
            if (jsClass->convertToType)
                return jsClass->convertToType(ctx, thisObject, kJSTypeString, exception);
    }
    return 0;
}

JSValueRef JSCallbackFunction::valueOfCallback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    JSObject* object = toJS(thisObject);
    if (object->inherits(&JSCallbackObject<JSNonFinalObject>::s_info)) {
        for (JSClassRef jsClass = jsCast<JSCallbackObject<JSNonFinalObject>*>(object)->classRef(); jsClass; jsClass = jsClass->parentClass)
            if (jsClass->convertToType)
                return jsClass->convertToType(ctx, thisObject, kJSTypeNumber, exception);
    } else if (object->inherits(&JSCallbackObject<JSGlobalObject>::s_info)) {
        for (JSClassRef jsClass = jsCast<JSCallbackObject<JSGlobalObject>*>(object)->classRef(); jsClass; jsClass = jsClass->parentClass)
            if (jsClass->convertToType)
                return jsClass->convertToType(ctx, thisObject, kJSTypeNumber, exception);
    }
    return 0;
}


} // namespace JSC
