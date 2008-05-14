/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "JavaScriptFunctionCallProfile.h"

#include "kjs_binding.h"
#include <profiler/FunctionCallProfile.h>
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/Value.h>

using namespace KJS;

namespace WebCore {

// Cache

typedef HashMap<FunctionCallProfile*, JSValue*> FunctionCallProfileMap;

static FunctionCallProfileMap& functionCallProfileCache()
{ 
    static FunctionCallProfileMap staticFunctionCallProfiles;
    return staticFunctionCallProfiles;
}

// Static Values

static JSClassRef functionCallProfileClass();

static JSValueRef getFunctionName(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, functionCallProfileClass()))
        return JSValueMakeUndefined(ctx);

    FunctionCallProfile* functionCallProfile = static_cast<FunctionCallProfile*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeString(ctx, JSStringCreateWithCharacters(functionCallProfile->functionName().data(), functionCallProfile->functionName().size()));
}

static JSValueRef getTotalTime(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    KJS::JSLock lock;

    if (!JSValueIsObjectOfClass(ctx, thisObject, functionCallProfileClass()))
        return JSValueMakeUndefined(ctx);

    FunctionCallProfile* functionCallProfile = static_cast<FunctionCallProfile*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, functionCallProfile->milliSecs());
}

static JSValueRef getSelfTime(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    KJS::JSLock lock;

    if (!JSValueIsObjectOfClass(ctx, thisObject, functionCallProfileClass()))
        return JSValueMakeUndefined(ctx);

    FunctionCallProfile* functionCallProfile = static_cast<FunctionCallProfile*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, functionCallProfile->milliSecs());
}

static JSValueRef getNumberOfCalls(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    KJS::JSLock lock;

    if (!JSValueIsObjectOfClass(ctx, thisObject, functionCallProfileClass()))
        return JSValueMakeUndefined(ctx);

    FunctionCallProfile* functionCallProfile = static_cast<FunctionCallProfile*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, functionCallProfile->numberOfCalls());
}

static JSValueRef getChildren(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    KJS::JSLock lock;

    if (!JSValueIsObjectOfClass(ctx, thisObject, functionCallProfileClass()))
        return JSValueMakeUndefined(ctx);

    FunctionCallProfile* functionCallProfile = static_cast<FunctionCallProfile*>(JSObjectGetPrivate(thisObject));
    const Deque<RefPtr<FunctionCallProfile> >& children = functionCallProfile->children();

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSRetainPtr<JSStringRef> arrayString(Adopt, JSStringCreateWithUTF8CString("Array"));

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, arrayString.get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSRetainPtr<JSStringRef> pushString(Adopt, JSStringCreateWithUTF8CString("push"));
    
    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, pushString.get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    for (Deque<RefPtr<FunctionCallProfile> >::const_iterator it = children.begin(); it != children.end(); ++it) {
        JSValueRef arg0 = toRef(toJS(toJS(ctx), (*it).get() ));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}

static void finalize(JSObjectRef object)
{
    FunctionCallProfile* functionCallProfile = static_cast<FunctionCallProfile*>(JSObjectGetPrivate(object));
    functionCallProfileCache().remove(functionCallProfile);
    functionCallProfile->deref();
}

JSClassRef functionCallProfileClass()
{
    static JSStaticValue staticValues[] = {
        { "functionName", getFunctionName, 0, kJSPropertyAttributeNone },
        { "totalTime", getTotalTime, 0, kJSPropertyAttributeNone },
        { "selfTime", getSelfTime, 0, kJSPropertyAttributeNone },
        { "numberOfCalls", getNumberOfCalls, 0, kJSPropertyAttributeNone },
        { "children", getChildren, 0, kJSPropertyAttributeNone },
        { 0, 0, 0, 0 }
    };

    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "FunctionCallProfile", 0, staticValues, 0,
        0, finalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef functionCallProfileClass = JSClassCreate(&classDefinition);
    return functionCallProfileClass;
}

JSValue* toJS(ExecState* exec, FunctionCallProfile* functionCallProfile)
{
    if (!functionCallProfile)
        return jsNull();

    JSValue* functionCallProfileWrapper = functionCallProfileCache().get(functionCallProfile);
    if (functionCallProfileWrapper)
        return functionCallProfileWrapper;

    functionCallProfile->ref();

    functionCallProfileWrapper = toJS(JSObjectMake(toRef(exec), functionCallProfileClass(), static_cast<void*>(functionCallProfile)));
    functionCallProfileCache().set(functionCallProfile, functionCallProfileWrapper);
    return functionCallProfileWrapper;

}


} // namespace WebCore
