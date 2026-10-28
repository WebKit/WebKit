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

#import "Helpers/PlatformUtilities.h"
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageOverlay.h>
#import <WebKit/WKBundlePagePrivate.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>

// Installs a page overlay that handles right mouse downs where x < 200, like a data detector highlight that
// presents its own menu. Also exposes `copyContextMenuAtPoint(x, y)` to the page, which simulates a context
// click via WKBundlePageCopyContextMenuAtPointInWindow and returns whether a context menu was produced.
@interface PageOverlayPlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation PageOverlayPlugIn

static bool overlayMouseDown(WKBundlePageOverlayRef, WKPoint position, WKEventMouseButton mouseButton, const void*)
{
    return mouseButton == kWKEventMouseButtonRightButton && position.x < 200;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    browserContextController.loadDelegate = self;

    WKBundlePageOverlayClientV1 overlayClient { };
    overlayClient.base.version = 1;
    overlayClient.mouseDown = overlayMouseDown;

    WKBundlePageOverlayRef pageOverlay = WKBundlePageOverlayCreate((WKBundlePageOverlayClientBase*)&overlayClient);
    WKBundlePageInstallPageOverlay(browserContextController._bundlePageRef, pageOverlay);
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller globalObjectIsAvailableForFrame:(WKWebProcessPlugInFrame *)frame inScriptWorld:(WKWebProcessPlugInScriptWorld *)scriptWorld
{
    WKBundlePageRef page = controller._bundlePageRef;
    [[frame jsContextForWorld:scriptWorld] setObject:^BOOL(double x, double y) {
        WKArrayRef menuItems = WKBundlePageCopyContextMenuAtPointInWindow(page, WKPointMake(x, y));
        if (!menuItems)
            return NO;
        WKRelease(menuItems);
        return YES;
    } forKeyedSubscript:@"copyContextMenuAtPoint"];
}

@end
