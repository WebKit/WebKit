/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "GetComputedStyleAfterIframeRemovalProtocol.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

// FIXME(rdar://171294437): Disabling the back/forward cache explicitly is (for some reason) not available
// on iOS, so we can only run this test on macOS at present.
#if PLATFORM(MAC)

static bool didReceiveResult;

@interface GetComputedStyleAfterIframeRemovalObject : NSObject <GetComputedStyleAfterIframeRemovalProtocol>
@end

@implementation GetComputedStyleAfterIframeRemovalObject

- (void)didNotCrashWithResult:(NSString *)result
{
    // After disconnectContentFrame() completes, the iframe's render tree should have been torn down.
    // Viewport factors should resolve to zero, and width:10vw should compute "0px".
    EXPECT_WK_STREQ(result, @"0px");
    didReceiveResult = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit, GetComputedStyleAfterIframeOwnerDestructionDoesNotCrash)
{
    RetainPtr configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"GetComputedStyleAfterIframeRemovalPlugIn"]);
    // Disabling the back/forward cache explicitly is (for some reason) not available on iOS.
    // Disable the Back-Forward Cache so that navigating away from the first page immediately
    // triggers HTMLFrameOwnerElement::disconnectContentFrame() on its subframes. Otherwise we can't
    // test the frame teardown in this test.
    [[configuration preferences] _setUsesPageCache:NO];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr object = adoptNS([[GetComputedStyleAfterIframeRemovalObject alloc] init]);
    RetainPtr interface = retainPtr([_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(GetComputedStyleAfterIframeRemovalProtocol)]);
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface.get()];

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    NSURL *pageURL = [NSBundle.test_resourcesBundle URLForResource:@"GetComputedStyleAfterIframeRemoval" withExtension:@"html"];
    [webView loadFileURL:pageURL allowingReadAccessToURL:pageURL.URLByDeletingLastPathComponent];
    [navigationDelegate waitForDidFinishNavigation];

    // Navigate to a second page to trigger HTMLFrameOwnerElement::disconnectContentFrame()
    [webView loadHTMLString:@"<html><body>second page</body></html>" baseURL:nil];

    TestWebKitAPI::Util::run(&didReceiveResult);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
