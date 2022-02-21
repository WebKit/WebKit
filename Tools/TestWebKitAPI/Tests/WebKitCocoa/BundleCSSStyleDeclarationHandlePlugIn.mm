/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "BundleCSSStyleDeclarationHandleProtocol.h"
#import "PlatformUtilities.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInCSSStyleDeclarationHandle.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface BundleCSSStyleDeclarationHandlePlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation BundleCSSStyleDeclarationHandlePlugIn {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<WKWebProcessPlugInController> _plugInController;
    RetainPtr<id <BundleCSSStyleDeclarationHandleProtocol>> _remoteObject;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    ASSERT(!_browserContextController);
    ASSERT(!_plugInController);
    _browserContextController = browserContextController;
    _plugInController = plugInController;

    auto interface = retainPtr([_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleCSSStyleDeclarationHandleProtocol)]);
    _remoteObject = [browserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:interface.get()];

    [_browserContextController setLoadDelegate:self];
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didFinishDocumentLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    auto context = [frame jsContextForWorld:WKWebProcessPlugInScriptWorld.normalWorld];
    auto jsCSSStyleDeclaration = [context evaluateScript:@"document.body.style"];
    auto cssStyleDeclarationHandle = [WKWebProcessPlugInCSSStyleDeclarationHandle cssStyleDeclarationHandleWithJSValue:jsCSSStyleDeclaration inContext:context];
    auto jsCSSStyleDeclarationForCSSStyleDeclarationHandle = [frame jsCSSStyleDeclarationForCSSStyleDeclarationHandle:cssStyleDeclarationHandle inWorld:WKWebProcessPlugInScriptWorld.normalWorld];
    if (jsCSSStyleDeclaration != jsCSSStyleDeclarationForCSSStyleDeclarationHandle)
        [_remoteObject verifyStyle:NO];
    else {
        auto jsVerifyStyle = [context evaluateScript:@"verifyStyle"];
        [_remoteObject verifyStyle:[jsVerifyStyle callWithArguments:@[jsCSSStyleDeclarationForCSSStyleDeclarationHandle]].toBool];
    }
}

@end
