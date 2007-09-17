/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DumpRenderTree.h"
#include "GCController.h"

#include <wtf/Platform.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit/IWebJavaScriptCollector.h>
#include <WebKit/WebKit.h>

static JSValueRef collectCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    IWebJavaScriptCollector* collector;
    if (FAILED(::CoCreateInstance(CLSID_WebJavaScriptCollector, 0, CLSCTX_ALL, IID_IWebJavaScriptCollector, (void**)&collector)))
        return JSValueMakeUndefined(context);

    collector->collect();

    collector->Release();

    return JSValueMakeUndefined(context);
}

static JSValueRef collectOnAlternateThreadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    bool waitUntilDone = false;
    if (argumentCount > 0)
        waitUntilDone = JSValueToBoolean(context, arguments[0]);

    IWebJavaScriptCollector* collector;
    if (FAILED(::CoCreateInstance(CLSID_WebJavaScriptCollector, 0, CLSCTX_ALL, IID_IWebJavaScriptCollector, (void**)&collector)))
        return JSValueMakeUndefined(context);

    collector->collectOnAlternateThread(waitUntilDone ? TRUE : FALSE);

    collector->Release();

    return JSValueMakeUndefined(context);
}

static JSValueRef getJSObjectCountCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    IWebJavaScriptCollector* collector;
    if (FAILED(::CoCreateInstance(CLSID_WebJavaScriptCollector, 0, CLSCTX_ALL, IID_IWebJavaScriptCollector, (void**)&collector)))
        return JSValueMakeUndefined(context);

    UINT objects = 0;
    collector->objectCount(&objects);

    collector->Release();

    return JSValueMakeNumber(context, objects);
}

static JSStaticFunction staticFunctions[] = {
    { "collect", collectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "collectOnAlternateThread", collectOnAlternateThreadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "getJSObjectCount", getJSObjectCountCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef context) {
    static JSClassRef gcControllerClass = 0;

    if (!gcControllerClass) {
        JSClassDefinition classDefinition = { 0 };
        classDefinition.staticFunctions = staticFunctions;

        gcControllerClass = JSClassCreate(&classDefinition);
    }

    return gcControllerClass;
}

JSObjectRef makeGCController(JSContextRef context)
{
    return JSObjectMake(context, getClass(context), 0);
}
