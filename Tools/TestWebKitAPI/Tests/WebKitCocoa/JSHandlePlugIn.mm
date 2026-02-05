/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#import "JSHandlePlugInProtocol.h"
#import "PlatformUtilities.h"
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageOverlay.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFramePrivate.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKJSHandle.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static WKWebProcessPlugInBrowserContextController *globalBrowserContextController;

@interface JSHandlePlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation JSHandlePlugIn

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    ASSERT(!globalBrowserContextController);
    globalBrowserContextController = browserContextController;

    browserContextController.loadDelegate = self;

    static RetainPtr<WKWebProcessPlugInScriptWorld> world { [WKWebProcessPlugInScriptWorld world] };
    [world allowJSHandleCreation];
}

static JSValueRef javaScriptFunction(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount == 1) {
        JSContext *jsContext = [JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(context)];
        JSValue *jsValue = [JSValue valueWithJSValueRef:arguments[0] inContext:jsContext];
        if (_WKJSHandle *handle = [WKWebProcessPlugInFrame jsHandleFromValue:jsValue withContext:jsContext]) {
            _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(JSHandlePlugInProtocol)];

            id<JSHandlePlugInProtocol> remoteObject = [globalBrowserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:interface];
            [remoteObject receiveDictionaryFromWebProcess:@{
                @"testkey" : handle,
                @"testdatakey" : [NSKeyedArchiver archivedDataWithRootObject:handle requiringSecureCoding:YES error:nullptr]
            }];
        }
    }
    return JSValueMakeUndefined(context);
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController*)controller globalObjectIsAvailableForFrame:(WKWebProcessPlugInFrame *)frame inScriptWorld:(WKWebProcessPlugInScriptWorld *)scriptWorld
{
    if (scriptWorld == WKWebProcessPlugInScriptWorld.normalWorld)
        return;

    JSContext *context = [frame jsContextForWorld:scriptWorld];

    JSValue *value = [JSValue valueWithNewObjectInContext:context];

    JSValueRef nativeImplementationValueRef = JSObjectMakeFunctionWithCallback((JSContextRef)context.JSGlobalContextRef, adopt(JSStringCreateWithCharacters((const JSChar *)L"testFunction", 12)).get(), javaScriptFunction);
    JSValue *nativeImplementationValue = [JSValue valueWithJSValueRef:nativeImplementationValueRef inContext:context];
    [value setValue:nativeImplementationValue forProperty:@"testFunction"];

    [context.globalObject setValue:value forProperty:@"testProperty"];

    [context evaluateScript:@"onload = () => { window.testProperty.testFunction(window.webkit.createJSHandle(document.getElementById('testelement'))) }"];
}

@end
