/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSObjectGetProxyTargetTest.h"

#include "APICast.h"
#include "InitializeThreading.h"
#include "JSCInlines.h"
#include "JSObjectRefPrivate.h"
#include "JSProxy.h"
#include "JavaScript.h"
#include "Options.h"
#include "ProxyObject.h"

using namespace JSC;

int testJSObjectGetProxyTarget()
{
    bool overallResult = true;
    
    printf("JSObjectGetProxyTargetTest:\n");
    
    auto test = [&] (const char* description, bool currentResult) {
        printf("    %s: %s\n", description, currentResult ? "PASS" : "FAIL");
        overallResult &= currentResult;
    };
    
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef context = JSGlobalContextCreateInGroup(group, nullptr);
    
    VM& vm = *toJS(group);
    JSObjectRef globalObjectProxy = JSContextGetGlobalObject(context);

    JSGlobalObject* globalObjectObject;
    JSObjectRef globalObjectRef;
    JSProxy* jsProxyObject;

    {
        JSLockHolder locker(vm);
        JSProxy* globalObjectProxyObject = jsCast<JSProxy*>(toJS(globalObjectProxy));
        globalObjectObject = jsCast<JSGlobalObject*>(globalObjectProxyObject->target());
        Structure* proxyStructure = JSProxy::createStructure(vm, globalObjectObject, globalObjectObject->objectPrototype(), PureForwardingProxyType);
        globalObjectRef = toRef(jsCast<JSObject*>(globalObjectObject));
        jsProxyObject = JSProxy::create(vm, proxyStructure);
    }
    
    JSObjectRef array = JSObjectMakeArray(context, 0, nullptr, nullptr);

    ProxyObject* proxyObjectObject;

    {
        JSLockHolder locker(vm);
        Structure* emptyObjectStructure = JSFinalObject::createStructure(vm, globalObjectObject, globalObjectObject->objectPrototype(), 0);
        JSObject* handler = JSFinalObject::create(vm, emptyObjectStructure);
        proxyObjectObject = ProxyObject::create(globalObjectObject, toJS(array), handler);
    }

    JSObjectRef jsProxy = toRef(jsProxyObject);
    JSObjectRef proxyObject = toRef(proxyObjectObject);
    
    test("proxy target of null is null", !JSObjectGetProxyTarget(nullptr));
    test("proxy target of non-proxy is null", !JSObjectGetProxyTarget(array));
    test("proxy target of uninitialized JSProxy is null", !JSObjectGetProxyTarget(jsProxy));
    
    {
        JSLockHolder locker(vm);
        jsProxyObject->setTarget(vm, globalObjectObject);
    }
    
    test("proxy target of initialized JSProxy works", JSObjectGetProxyTarget(jsProxy) == globalObjectRef);
    
    test("proxy target of ProxyObject works", JSObjectGetProxyTarget(proxyObject) == array);
    
    test("proxy target of GlobalObject is the globalObject", JSObjectGetProxyTarget(globalObjectProxy) == globalObjectRef);

    JSGlobalContextRelease(context);
    JSContextGroupRelease(group);

    printf("JSObjectGetProxyTargetTest: %s\n", overallResult ? "PASS" : "FAIL");
    return !overallResult;
}

