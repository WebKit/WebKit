/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "ServiceWorkerPageProtocol.h"
#import <JavaScriptCore/JSContextRef.h>
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInFramePrivate.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RunLoop.h>

@interface ServiceWorkerPagePlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation ServiceWorkerPagePlugIn {
    RetainPtr<id <ServiceWorkerPageProtocol>> _remoteObject;
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController*)controller serviceWorkerGlobalObjectIsAvailableForFrame:(WKWebProcessPlugInFrame *)frame inScriptWorld:(WKWebProcessPlugInScriptWorld *)scriptWorld
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(frame);
    JSContext *serviceWorkerJSContext = [frame jsContextForServiceWorkerWorld:scriptWorld];
    RELEASE_ASSERT(serviceWorkerJSContext);
    RELEASE_ASSERT([WKWebProcessPlugInFrame lookUpFrameFromJSContext:serviceWorkerJSContext] == frame);

    JSContext *mainFrameJSContext = [frame jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]];
    RELEASE_ASSERT(mainFrameJSContext);

    // The main frame and the service worker should have different JSContexts but should use the same VM.
    RELEASE_ASSERT(mainFrameJSContext != serviceWorkerJSContext);
    RELEASE_ASSERT(JSContextGetGroup(mainFrameJSContext.JSGlobalContextRef) == JSContextGetGroup(serviceWorkerJSContext.JSGlobalContextRef));

    RELEASE_ASSERT(scriptWorld == [WKWebProcessPlugInScriptWorld normalWorld]);

    JSValue *globalIsServiceWorkerGlobalScope = [serviceWorkerJSContext evaluateScript:@"self.__proto__ === ServiceWorkerGlobalScope.prototype"];
    RELEASE_ASSERT(!!globalIsServiceWorkerGlobalScope);
    RELEASE_ASSERT([globalIsServiceWorkerGlobalScope toBool]);

    auto remoteObjectRegistry = frame._browserContextController._remoteObjectRegistry;
    RELEASE_ASSERT(remoteObjectRegistry);

    [_remoteObject serviceWorkerGlobalObjectIsAvailable];
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
    _remoteObject = [browserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:interface];

    browserContextController.loadDelegate = self;
}

@end

