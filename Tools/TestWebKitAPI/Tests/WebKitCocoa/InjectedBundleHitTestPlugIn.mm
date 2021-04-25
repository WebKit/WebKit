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
#import "InjectedBundleHitTestProtocol.h"
#import "WebCoreTestSupport.h"
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageResourceLoadClient.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInHitTestResult.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInNodeHandle.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface InjectedBundleHitTestPlugIn : NSObject<WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate, InjectedBundleHitTestProtocol>
@end

@implementation InjectedBundleHitTestPlugIn {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<_WKRemoteObjectInterface> _interface;
    RetainPtr<WKWebProcessPlugInFrame> _mainFrame;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    _browserContextController = browserContextController;
    _interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(InjectedBundleHitTestProtocol)];
    [browserContextController._remoteObjectRegistry registerExportedObject:self interface:_interface.get()];
    browserContextController.loadDelegate = self;
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didClearWindowObjectForFrame:(WKWebProcessPlugInFrame *)frame inScriptWorld:(WKWebProcessPlugInScriptWorld *)scriptWorld
{
    WebCoreTestSupport::injectInternalsObject([frame jsContextForWorld:scriptWorld].JSGlobalContextRef);
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController*)controller didFinishLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    if (frame.isMainFrame)
        _mainFrame = frame;
}

- (void)hasSelectableTextAt:(NSValue *)locationValue completionHandler:(void(^)(BOOL))completionHandler
{
    auto result = [_mainFrame hitTest:locationValue.pointValue options:WKHitTestOptionAllowUserAgentShadowRootContent];
    completionHandler(result.nodeHandle.isSelectableTextNode);
}

- (void)dealloc
{
    [[_browserContextController _remoteObjectRegistry] unregisterExportedObject:self interface:_interface.get()];
    [super dealloc];
}

@end

