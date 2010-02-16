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
#include "JavaScriptProfileNode.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "JSDOMBinding.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <profiler/ProfileNode.h>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <wtf/StdLibExtras.h>

using namespace JSC;

namespace WebCore {

// Cache

typedef HashMap<ProfileNode*, JSObject*> ProfileNodeMap;

static ProfileNodeMap& profileNodeCache()
{
    DEFINE_STATIC_LOCAL(ProfileNodeMap, staticProfileNodes, ());
    return staticProfileNodes;
}

static JSValueRef getFunctionName(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> functionNameString(Adopt, JSStringCreateWithCharacters(profileNode->functionName().data(), profileNode->functionName().size()));
    return JSValueMakeString(ctx, functionNameString.get());
}

static JSValueRef getURL(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> urlString(Adopt, JSStringCreateWithCharacters(profileNode->url().data(), profileNode->url().size()));
    return JSValueMakeString(ctx, urlString.get());
}

static JSValueRef getLineNumber(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, profileNode->lineNumber());
}

static JSValueRef getTotalTime(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    JSC::JSLock lock(SilenceAssertionsOnly);

    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, profileNode->totalTime());
}

static JSValueRef getSelfTime(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    JSC::JSLock lock(SilenceAssertionsOnly);

    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, profileNode->selfTime());
}

static JSValueRef getNumberOfCalls(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    JSC::JSLock lock(SilenceAssertionsOnly);

    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, profileNode->numberOfCalls());
}

static JSValueRef getChildren(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    JSC::JSLock lock(SilenceAssertionsOnly);

    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    const Vector<RefPtr<ProfileNode> >& children = profileNode->children();

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

    ExecState* exec = toJS(ctx);
    for (Vector<RefPtr<ProfileNode> >::const_iterator it = children.begin(); it != children.end(); ++it) {
        JSValueRef arg0 = toRef(exec, toJS(exec, (*it).get() ));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}

static JSValueRef getVisible(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    JSC::JSLock lock(SilenceAssertionsOnly);

    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(ctx, profileNode->visible());
}

static JSValueRef getCallUID(JSContextRef ctx, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    JSC::JSLock lock(SilenceAssertionsOnly);

    if (!JSValueIsObjectOfClass(ctx, thisObject, ProfileNodeClass()))
        return JSValueMakeUndefined(ctx);

    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(ctx, profileNode->callIdentifier().hash());
}

static void finalize(JSObjectRef object)
{
    ProfileNode* profileNode = static_cast<ProfileNode*>(JSObjectGetPrivate(object));
    profileNodeCache().remove(profileNode);
    profileNode->deref();
}

JSClassRef ProfileNodeClass()
{
    static JSStaticValue staticValues[] = {
        { "functionName", getFunctionName, 0, kJSPropertyAttributeNone },
        { "url", getURL, 0, kJSPropertyAttributeNone },
        { "lineNumber", getLineNumber, 0, kJSPropertyAttributeNone },
        { "totalTime", getTotalTime, 0, kJSPropertyAttributeNone },
        { "selfTime", getSelfTime, 0, kJSPropertyAttributeNone },
        { "numberOfCalls", getNumberOfCalls, 0, kJSPropertyAttributeNone },
        { "children", getChildren, 0, kJSPropertyAttributeNone },
        { "visible", getVisible, 0, kJSPropertyAttributeNone },
        { "callUID", getCallUID, 0, kJSPropertyAttributeNone },
        { 0, 0, 0, 0 }
    };

    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "ProfileNode", 0, staticValues, 0,
        0, finalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef profileNodeClass = JSClassCreate(&classDefinition);
    return profileNodeClass;
}

JSValue toJS(ExecState* exec, ProfileNode* profileNode)
{
    if (!profileNode)
        return jsNull();

    JSObject* profileNodeWrapper = profileNodeCache().get(profileNode);
    if (profileNodeWrapper)
        return profileNodeWrapper;

    profileNode->ref();

    profileNodeWrapper = toJS(JSObjectMake(toRef(exec), ProfileNodeClass(), static_cast<void*>(profileNode)));
    profileNodeCache().set(profileNode, profileNodeWrapper);
    return profileNodeWrapper;
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
