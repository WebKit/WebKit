/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "AccessibilityTestSupportProtocol.h"
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageResourceLoadClient.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface AccessibilityTestPlugin : NSObject<WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate, AccessibilityTestSupportProtocol>
@end

@implementation AccessibilityTestPlugin {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<WKWebProcessPlugInController> _plugInController;
    RetainPtr<_WKRemoteObjectInterface> _interface;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    ASSERT(!_browserContextController);
    ASSERT(!_plugInController);
    _browserContextController = browserContextController;
    _plugInController = plugInController;

    _interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(AccessibilityTestSupportProtocol)];
    [[browserContextController _remoteObjectRegistry] registerExportedObject:self interface:_interface.get()];
}

- (void)checkAccessibilityWebProcessLoaderBundleIsLoaded:(void (^)(BOOL bundleLoaded, NSString *loadedPath))completionHandler
{
    BOOL accessibilityBundleLoaded = NO;
    NSString *loadedPath = nil;
    NSArray<NSBundle *> *allBundles = NSBundle.allBundles;
    for (NSBundle *bundle in allBundles) {
        if ([bundle.bundleIdentifier isEqualToString:@"com.apple.WebProcessLoader.axbundle"]) {
            accessibilityBundleLoaded = [bundle isLoaded];
            loadedPath = [bundle bundlePath];
            break;
        }
    }
    completionHandler(accessibilityBundleLoaded, loadedPath);
}

- (void)dealloc
{
    [[_browserContextController _remoteObjectRegistry] unregisterExportedObject:self interface:_interface.get()];
    [super dealloc];
}

@end

