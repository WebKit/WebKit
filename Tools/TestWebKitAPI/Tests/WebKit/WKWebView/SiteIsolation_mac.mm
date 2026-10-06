/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

// Split from SiteIsolation.mm. This file is unified with SiteIsolation.mm
// and its neighbors, so it must not rely on their file-scope helpers or reuse their names.
// Helpers shared between these files live in Helpers/cocoa/SiteIsolationTestUtilities.h.

#import "config.h"

#if PLATFORM(MAC)

#import "FrameTreeChecks.h"
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/DragAndDropSimulator.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "Helpers/mac/AppKitSPI.h"
#import "Helpers/mac/LocalEventMonitorSwizzler.h"
#import "Helpers/mac/WKWebViewForTestingImmediateActions.h"
#import "InstanceMethodSwizzler.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/IntRect.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/SQLiteDatabase.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivateForTesting.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKHitTestResult.h>
#import <WebKit/_WKUserInitiatedAction.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/MakeString.h>

@interface NSMenu ()
- (id)_menuImpl;
@end

@interface WKWebView ()
- (void)paste:(id)sender;
- (WKPageRef)_pageForTesting;
- (void)toggleContinuousSpellChecking:(id)sender;
@end

@interface SiteIsolationPageScrollCounter : NSObject<WKUIDelegatePrivate>
@property (nonatomic) NSUInteger pageScrollCount;
@end

@implementation SiteIsolationPageScrollCounter
- (void)_webViewDidScroll:(WKWebView *)webView
{
    ++_pageScrollCount;
}
@end

// Stands in for the font panel's attribute converter, and always adds a single underline.
@interface SiteIsolationUnderlineAttributeConverter : NSObject
- (NSDictionary *)convertAttributes:(NSDictionary *)attributes;
@end

@implementation SiteIsolationUnderlineAttributeConverter
- (NSDictionary *)convertAttributes:(NSDictionary *)attributes
{
    RetainPtr convertedAttributes = adoptNS([attributes mutableCopy]);
    [convertedAttributes setObject:@(NSUnderlineStyleSingle) forKey:NSUnderlineStyleAttributeName];
    return convertedAttributes.autorelease();
}
@end

@interface SiteIsolationMouseMoveOverElementDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic, copy) void (^mouseDidMoveOverElement)(_WKHitTestResult *, NSEventModifierFlags);
@end

@implementation SiteIsolationMouseMoveOverElementDelegate
- (void)_webView:(WKWebView *)webView mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(NSEventModifierFlags)flags userInfo:(id<NSSecureCoding>)userInfo
{
    if (_mouseDidMoveOverElement)
        _mouseDidMoveOverElement(hitTestResult, flags);
}
@end

namespace TestWebKitAPI {

TEST(SiteIsolation, ObscuredContentInsetsSurviveWindowOpenProcessSwap)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/example_opened_after_navigation"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration])];
    enableSiteIsolation(configuration);
    configuration.get().preferences.javaScriptCanOpenWindowsAutomatically = YES;

    RetainPtr openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    RetainPtr opener = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [opener setNavigationDelegate:openerNavigationDelegate];

    RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    __block RetainPtr<TestWKWebView> opened;
    RetainPtr openerUIDelegate = adoptNS([TestUIDelegate new]);
    openerUIDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedNavigationDelegate;
        return opened.get();
    };
    [opener setUIDelegate:openerUIDelegate];

    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!opened)
        Util::spinRunLoop();
    [openedNavigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opened, { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [opened waitForNextPresentationUpdate];

    // Navigating the opened window's main frame to example.com causes it to swap into the
    // opener's already-running example.com process, reusing the WebPage that process already
    // held for this page (as a RemotePageProxy placeholder), rather than creating a new one.
    [opened evaluateJavaScript:@"window.location = 'https://example.com/example_opened_after_navigation'" completionHandler:nil];
    [openedNavigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(opened, { { "https://example.com"_s } });

    // If the reused process's WebCore::Page never learned about the inset, this reads back 0.
    RetainPtr insetTop = [opened objectByEvaluatingJavaScript:@"internals.obscuredContentInsetTop()"];
    EXPECT_EQ([insetTop doubleValue], 100);
}

TEST(SiteIsolation, WindowFeaturesOnlyAppliedInMainFrameProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script>w = window.open('https://webkit.org/opened', '_blank', 'left=50,top=60,width=300,height=200')</script>"_s } },
        { "/opened"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    RetainPtr openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    RetainPtr opener = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [opener setNavigationDelegate:openerNavigationDelegate.get()];

    __block Vector<CGRect> windowFrames;
    RetainPtr openedUIDelegate = adoptNS([TestUIDelegate new]);
    openedUIDelegate.get().getWindowFrameWithCompletionHandler = ^(WKWebView *, void (^completionHandler)(CGRect)) {
        completionHandler(CGRectMake(0, 0, 800, 600));
    };
    openedUIDelegate.get().setWindowFrame = ^(WKWebView *, CGRect frame) {
        windowFrames.append(frame);
    };

    RetainPtr openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    __block RetainPtr<TestWKWebView> opened;
    RetainPtr openerUIDelegate = adoptNS([TestUIDelegate new]);
    openerUIDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = openedNavigationDelegate.get();
        opened.get().UIDelegate = openedUIDelegate.get();
        // Like Safari, size the web view after creating it, so pages for the opened window created in
        // other processes during initialization have an empty view size.
        [opened setFrame:NSMakeRect(0, 0, 800, 600)];
        return opened.get();
    };
    [opener setUIDelegate:openerUIDelegate.get()];

    [opener loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!opened)
        Util::spinRunLoop();
    [openedNavigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opened.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    // Make sure any window frame messages sent by either process have been received.
    EXPECT_EQ([[opener objectByEvaluatingJavaScript:@"1"] intValue], 1);
    EXPECT_EQ([[opener objectByEvaluatingJavaScript:@"1" inFrame:[opener firstChildFrame]] intValue], 1);

    // Only the process with the local main frame can compute the window frame from the features.
    // The example.com process, where the opened window's main frame is remote, would compute a frame
    // based on an empty viewport size.
    EXPECT_EQ(windowFrames.size(), 1u);
    for (auto& frame : windowFrames) {
        EXPECT_EQ(frame.size.width, 300);
        EXPECT_EQ(frame.size.height, 200);
    }
}

TEST(SiteIsolation, GeneratesPageLoadTimingAfterOnUnloadFetch)
{
    // FIXME: this is a bit more convoluted than it needs to be due to didGeneratePageLoadTiming not working unless the page has at least one subresource load.
    HTTPServer server({
        { "/1"_s, { "<iframe src='https://example.com/iframe'></iframe><script src='/script'></script> 1"_s } },
        { "/2"_s, { "<iframe src='https://example.com/iframe'></iframe><script src='/script'></script> 2"_s } },
        { "/iframe"_s, { "<script>window.addEventListener('unload', () => fetch('/foobar', { keepalive: true }))</script> <script src='/script'></script>"_s } },
        { "/foobar"_s, { "foobar"_s } },
        { "/script"_s, { "console.log('hello from script')"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool didGeneratePageLoadTiming = false;

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 400));
    navigationDelegate.get().didGeneratePageLoadTiming = ^(_WKPageLoadTiming *timing) {
        if (timing)
            didGeneratePageLoadTiming = true;
    };

    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:0 backing:NSBackingStoreBuffered defer:NO]);
    [window.get().contentView addSubview:webView.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/1"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://example.com"_s } } }
    });

    for (int i = 0; i < 20 && !didGeneratePageLoadTiming; i++)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE(didGeneratePageLoadTiming);
    didGeneratePageLoadTiming = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/2"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://example.com"_s } } }
    });

    for (int i = 0; i < 20 && !didGeneratePageLoadTiming; i++)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE(didGeneratePageLoadTiming);
}

TEST(SiteIsolation, PropagateMouseEventsToSubframe)
{
    auto mainframeHTML = "<script>"
    "    window.eventTypes = [];"
    "    window.addEventListener('message', function(event) {"
    "        window.eventTypes.push(event.data);"
    "    });"
    "</script>"
    "<iframe src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<script>"
    "    addEventListener('mousemove', (event) => { window.parent.postMessage('mousemove', '*') });"
    "    addEventListener('mousedown', (event) => { window.parent.postMessage('mousedown,' + event.pageX + ',' + event.pageY, '*') });"
    "    addEventListener('mouseup', (event) => { window.parent.postMessage('mouseup,' + event.pageX + ',' + event.pageY, '*') });"
    "    alert('iframe loaded');"
    "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    EXPECT_WK_STREQ("iframe loaded", [webView _test_waitForAlert]);

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];

    NSArray<NSString *> *eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    while (eventTypes.count != 3u)
        eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    EXPECT_WK_STREQ("mousemove", eventTypes[0]);
    EXPECT_WK_STREQ("mousedown,40,40", eventTypes[1]);
    EXPECT_WK_STREQ("mouseup,40,40", eventTypes[2]);
}

TEST(SiteIsolation, MouseCaptureOutsideSubframeDuringDrag)
{
    auto mainframeHTML = "<script>"
    "    window.eventTypes = [];"
    "    window.addEventListener('message', function(event) {"
    "        window.eventTypes.push(event.data);"
    "    });"
    "</script>"
    "<iframe src='https://domain2.com/subframe' style='position: absolute; left: 0; top: 0; width: 100px; height: 100px; border: none;'></iframe>"_s;

    auto subframeHTML = "<script>"
    "    addEventListener('mousedown', (event) => { window.parent.postMessage('mousedown', '*') });"
    "    addEventListener('mousemove', (event) => { window.parent.postMessage('mousemove', '*') });"
    "    addEventListener('mouseup', (event) => { window.parent.postMessage('mouseup', '*') });"
    "    alert('iframe loaded');"
    "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    EXPECT_WK_STREQ("iframe loaded", [webView _test_waitForAlert]);

    // Press down inside the 100x100 subframe, then drag to and release at a point well
    // outside its bounds (in the main frame's own area). Without mouse capture spanning the
    // RemoteFrame boundary, the main frame re-hit-tests on every move and stops forwarding
    // events to the subframe's process as soon as the point leaves its on-screen rect, so the
    // subframe would never see the mousemove/mouseup below.
    CGPoint insideSubframe = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    CGPoint outsideSubframe = [webView convertPoint:CGPointMake(400, 400) toView:nil];
    [webView mouseDownAtPoint:insideSubframe simulatePressure:NO];
    [webView mouseDragToPoint:outsideSubframe];
    [webView mouseUpAtPoint:outsideSubframe];
    [webView waitForPendingMouseEvents];

    NSArray<NSString *> *eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    while (eventTypes.count != 3u)
        eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    EXPECT_WK_STREQ("mousedown", eventTypes[0]);
    EXPECT_WK_STREQ("mousemove", eventTypes[1]);
    EXPECT_WK_STREQ("mouseup", eventTypes[2]);
}

TEST(SiteIsolation, RunOpenPanel)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><input style='width: 100vw; height: 100vh;' type='file'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    __block bool fileSelected = false;
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        fileSelected = true;
        completionHandler(@[ [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"test"]] ]);
    }];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(100, 100) toView:nil];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];
    Util::run(&fileSelected);

    NSString *js = @"function f() { try { return document.getElementsByTagName('input')[0].files[0].name } catch (e) { return 'exception: ' + e; } }; f()";
    while (![[webView objectByEvaluatingJavaScript:js inFrame:[webView firstChildFrame]] isEqualToString:@"test"])
        Util::spinRunLoop();
}

TEST(SiteIsolation, CancelOpenPanel)
{
    auto subframeHTML = "<!DOCTYPE html><input style='width: 100vw; height: 100vh;' id='file' type='file'>"
        "<script>"
        "document.getElementById('file').addEventListener('cancel', () => { alert('cancel'); });"
        "</script>"_s;
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        completionHandler(nil);
    }];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(100, 100) toView:nil];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "cancel");
}

TEST(SiteIsolation, OpenPanelTranscodedImageReachesCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><input type='file' accept='image/jpeg'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        completionHandler(@[ [NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-400px" withExtension:@"gif"] ]);
    }];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('input').showPicker()" inFrame:childFrame.get()];

    // The input only accepts JPEG, so the GIF is transcoded, and the result must reach the iframe's process.
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.querySelector('input').files[0]?.type ?? ''" inFrame:childFrame.get()] isEqualToString:@"image/jpeg"];
    }));
}

TEST(SiteIsolation, DragEvents)
{
    auto mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue;'></div>"
    "<script>"
    "    draggable.addEventListener('dragstart', (event) => { window.parent.postMessage('dragstart', '*') });"
    "    draggable.addEventListener('dragend', (event) => { window.parent.postMessage('dragend', '*') });"
    "    draggable.addEventListener('dragenter', (event) => { window.parent.postMessage('dragenter:' + event.clientX + ',' + event.clientY, '*') });"
    "    draggable.addEventListener('dragleave', (event) => { window.parent.postMessage('dragleave', '*') });"
    "    addEventListener('dragover', (event) => { window.parent.postMessage('dragover:' + event.clientX + ',' + event.clientY, '*') });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);
    RetainPtr webView = [simulator webView];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 150)];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_GT(events.count, 4U);

    bool foundDragLeave = false;
    for (NSString *event in events) {
        if ([event hasPrefix:@"dragleave"]) {
            foundDragLeave = true;
            break;
        }
    }
    EXPECT_WK_STREQ("dragstart", events[0]);
    EXPECT_TRUE(foundDragLeave);
    EXPECT_WK_STREQ("dragend", events[events.count - 1]);

    NSString *dragenterEvent = events[1];
    EXPECT_TRUE([dragenterEvent hasPrefix:@"dragenter:"]);
    NSString *dragenterCoords = [dragenterEvent substringFromIndex:[@"dragenter:" length]];
    NSArray *dragenterComponents = [dragenterCoords componentsSeparatedByString:@","];
    if (dragenterComponents.count == 2) {
        int x = [dragenterComponents[0] intValue];
        int y = [dragenterComponents[1] intValue];
        EXPECT_TRUE(x >= 65 && x <= 75) << "Expected dragenter x coordinate around 71, got " << x;
        EXPECT_TRUE(y >= 65 && y <= 75) << "Expected dragenter y coordinate around 71, got " << y;
    }

    NSString *lastDragOverEvent = nil;
    for (NSString *event in events) {
        if ([event hasPrefix:@"dragover:"])
            lastDragOverEvent = event;
    }
    EXPECT_NOT_NULL(lastDragOverEvent);
    if (lastDragOverEvent) {
        NSString *dragoverCoords = [lastDragOverEvent substringFromIndex:[@"dragover:" length]];
        NSArray *dragoverComponents = [dragoverCoords componentsSeparatedByString:@","];
        if (dragoverComponents.count == 2) {
            int x = [dragoverComponents[0] intValue];
            int y = [dragoverComponents[1] intValue];
            EXPECT_TRUE(x >= 135 && x <= 145) << "Expected final dragover x coordinate around 140, got " << x;
            EXPECT_TRUE(y >= 135 && y <= 145) << "Expected final dragover y coordinate around 140, got " << y;
        }
    }
}

TEST(SiteIsolation, FrameMetrics)
{
    auto mainframeHTML = "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div>"
    "Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc."
    "</div>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr frame = [webView mainFrame];
    WKFrameInfo *info = frame.get().info;

    EXPECT_EQ(info._isScrollable, NO);
    EXPECT_EQ(info._contentSize.width, 800);
    EXPECT_EQ(info._contentSize.height, 600);
    EXPECT_TRUE(CGSizeEqualToSize(info._contentSize, info._visibleContentSize));
    EXPECT_TRUE(CGSizeEqualToSize(info._contentSize, info._visibleContentSizeExcludingScrollbars));

    frame = frame.get().childFrames.firstObject;
    info = frame.get().info;

    EXPECT_EQ(info._isScrollable, YES);
    EXPECT_EQ(info._visibleContentSize.width, 300);
    EXPECT_EQ(info._visibleContentSize.height, 300);
    EXPECT_EQ(info._visibleContentSize.height, info._visibleContentSizeExcludingScrollbars.height);
    EXPECT_EQ(info._visibleContentSizeExcludingScrollbars.width, info._contentSize.width);
    EXPECT_TRUE(info._visibleContentSizeExcludingScrollbars.width < info._visibleContentSize.width);
}

void siteIsolationWriteImageDataToPasteboard(NSString *type, NSData *data)
{
    [NSPasteboard.generalPasteboard declareTypes:@[type] owner:nil];
    [NSPasteboard.generalPasteboard setData:data forType:type];
}

TEST(SiteIsolation, PasteGIF)
{
    auto mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div id='editor' contenteditable style=\"height:100%; width: 100%;\"></div>"
    "<script>"
    "const editor = document.getElementById('editor');"
    "editor.focus();"
    "editor.addEventListener('paste', (event) => { window.parent.postMessage(event.clipboardData.files[0].name, '*'); });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(20, 20) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    auto *data = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"sunset-in-cupertino-400px" ofType:@"gif"]];
    siteIsolationWriteImageDataToPasteboard(UTTypeGIF.identifier, data);
    [webView paste:nil];

    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_EQ(1U, events.count);
    EXPECT_WK_STREQ("image.gif", events[0]);
}

TEST(SiteIsolation, AppKitText)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<html><body><input id='input' value='test'></input></body></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];
    auto textLocation = NSMakePoint(23, 564);
    while ("TEST"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView uppercaseWord:nil];
        Util::runFor(10_ms);
    }
    while ("test"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView lowercaseWord:nil];
        Util::runFor(10_ms);
    }
    while ("Test"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView capitalizeWord:nil];
        Util::runFor(10_ms);
    }
}

TEST(SiteIsolation, HandleAcceptedCandidateInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body><input id='input' value='a'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrameInfo = [webView firstChildFrame];
    RetainPtr candidate = [NSTextCheckingResult replacementCheckingResultWithRange:NSMakeRange(0, 0) replacementString:@"b"];

    // Accept a candidate while the cross-origin iframe's input is focused with its text selected.
    // The IPC must reach the iframe's process; if it is routed to the main frame the candidate is
    // dropped (the main process has no local focused frame) and the input is never updated.
    // Use WithUserGesture because Element::focus() is a no-op for cross-origin non-main-frame
    // iframes without a user gesture.
    while ("b"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView objectByEvaluatingJavaScriptWithUserGesture:@"input.focus(); input.select()" inFrame:childFrameInfo.get()];
        Util::runFor(10_ms);
        [webView _forceRequestCandidates];
        Util::runFor(10_ms);
        [webView _handleAcceptedCandidate:candidate.get()];
        Util::runFor(10_ms);
    }
}

static NSArray<NSTextCheckingResult *> *swizzledCheckStringForMisspelledWord(id, SEL, NSString *stringToCheck, NSRange, NSTextCheckingTypes types, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    NSRange range = [stringToCheck rangeOfString:@"zzr"];
    if (range.location == NSNotFound || !(types & NSTextCheckingTypeSpelling))
        return @[ ];
    return @[ [NSTextCheckingResult spellCheckingResultWithRange:range] ];
}

TEST(SiteIsolation, ToggleContinuousSpellCheckingInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringForMisspelledWord)
    };

    [webView _setContinuousSpellCheckingEnabledForTesting:NO];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    RetainPtr childFrame = [webView firstChildFrame];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];
    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body)" inFrame:childFrame.get()];

    auto spellingMarkerCount = [&] {
        return [[webView objectByEvaluatingJavaScript:@"internals.markerCountForNode(document.body.firstChild, 'spelling')" inFrame:childFrame.get()] intValue];
    };

    [webView toggleContinuousSpellChecking:nil];
    [webView insertText:@"zzr " replacementRange:NSMakeRange(0, 0)];
    EXPECT_TRUE(Util::waitFor([&] {
        return spellingMarkerCount() > 0;
    }));

    [webView toggleContinuousSpellChecking:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return !spellingMarkerCount();
    }));
}

TEST(SiteIsolation, SetMarkedTextInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body><input id='input'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    // setMarkedText:selectedRange:replacementRange: and unmarkText are NSTextInputClient methods;
    // parameterize the type so the compiler sees them (WKWebView conforms via a private category).
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrameInfo = [webView firstChildFrame];

    // Set marked (composition) text while the cross-origin iframe's input is focused.
    // setMarkedText: routes to WebViewImpl::setMarkedText -> WebPageProxy::setCompositionAsync.
    // The IPC must reach the iframe's process; if it is routed to the main frame the composition
    // is dropped (the main process has no local focused frame) and the input is never updated.
    // Use WithUserGesture because Element::focus() is a no-op for cross-origin non-main-frame
    // iframes without a user gesture. The loop is bounded so a regression fails via the assertion
    // below rather than hanging forever.
    bool markedTextLanded = false;
    for (unsigned attempt = 0; attempt < 500 && !markedTextLanded; ++attempt) {
        [webView objectByEvaluatingJavaScriptWithUserGesture:@"input.focus(); input.select()" inFrame:childFrameInfo.get()];
        Util::runFor(10_ms);
        [webView setMarkedText:@"hello" selectedRange:NSMakeRange(5, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
        Util::runFor(10_ms);
        markedTextLanded = "hello"_s == String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()]);
    }
    EXPECT_TRUE(markedTextLanded);

    // Confirm the composition. unmarkText routes to WebViewImpl::unmarkText ->
    // WebPageProxy::confirmCompositionAsync, which must also reach the focused (sub)frame's
    // process; the committed value must remain in the iframe's input.
    [webView unmarkText];
    Util::runFor(10_ms);
    EXPECT_WK_STREQ("hello", [webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()]);
}

static void checkFirstRectForCharacterRangeInCrossOriginIframe(const String& mainframeHTML, const String& subframeHTML, void (^prepareBeforeFocusing)(TestWKWebView *, WKFrameInfo *) = nil)
{
    HTTPServer server({
        { "/control"_s, { "<body style='margin: 0'><input id='input' style='position: absolute; left: 120px; top: 130px;' value='test'></body>"_s } },
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    auto firstRectForRange = [webView] {
        __block bool done = false;
        __block NSRect result = NSZeroRect;
        [static_cast<id<NSTextInputClient_Async>>(webView.get()) firstRectForCharacterRange:NSMakeRange(0, 1) completionHandler:^(NSRect rect, NSRange) {
            result = rect;
            done = true;
        }];
        Util::run(&done);
        return result;
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/control"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView stringByEvaluatingJavaScript:@"input.focus()"];
    [webView waitForNextPresentationUpdate];
    NSRect controlRect = firstRectForRange();
    EXPECT_FALSE(NSIsEmptyRect(controlRect));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];

    if (prepareBeforeFocusing)
        prepareBeforeFocusing(webView, childFrameInfo);

    // Focus is a no-op for cross-origin non-main-frame iframes without a user gesture; retry until
    // it lands (bounded, so a regression fails the assertion below rather than hanging).
    // If the query is routed to the wrong process (the main frame's, which has no focused element),
    // it silently returns an empty rect forever.
    NSRect rect = NSZeroRect;
    EXPECT_TRUE(Util::waitFor([&] {
        [webView objectByEvaluatingJavaScriptWithUserGesture:@"input.focus()" inFrame:childFrameInfo.get()];
        [webView waitForNextPresentationUpdate];
        rect = firstRectForRange();
        return !NSIsEmptyRect(rect);
    }, 500));

    EXPECT_NEAR(rect.origin.x, controlRect.origin.x, 2);
    EXPECT_NEAR(rect.origin.y, controlRect.origin.y, 2);
}

static ASCIILiteral defaultCrossOriginIframeInputHTML = "<body style='margin: 0'><input id='input' style='position: absolute; left: 20px; top: 30px;' value='test'></body>"_s;
static ASCIILiteral tallCrossOriginIframeInputHTML = "<body style='margin: 0; min-height: 1000px'><input id='input' style='position: absolute; left: 20px; top: 530px;' value='test'></body>"_s;

TEST(SiteIsolation, FirstRectForCharacterRangeInCrossOriginIframe)
{
    checkFirstRectForCharacterRangeInCrossOriginIframe(
        "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        defaultCrossOriginIframeInputHTML
    );
}

TEST(SiteIsolation, FirstRectForCharacterRangeInCrossOriginIframeWithScrolledMainFrame)
{
    checkFirstRectForCharacterRangeInCrossOriginIframe(
        "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        defaultCrossOriginIframeInputHTML,
        ^(TestWKWebView *webView, WKFrameInfo *) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 400;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

TEST(SiteIsolation, FirstRectForCharacterRangeInScrolledCrossOriginIframe)
{
    checkFirstRectForCharacterRangeInCrossOriginIframe(
        "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        tallCrossOriginIframeInputHTML,
        ^(TestWKWebView *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

TEST(SiteIsolation, FirstRectForCharacterRangeInScrolledCrossOriginIframeWithScrolledMainFrame)
{
    checkFirstRectForCharacterRangeInCrossOriginIframe(
        "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        tallCrossOriginIframeInputHTML,
        ^(TestWKWebView *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 400;
            }));
            [webView waitForNextPresentationUpdate];

            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

static void checkValidationMessageAnchorInCrossOriginIframe(const String& mainframeHTML, const String& subframeHTML, void (^prepareBeforeSubmit)(TestWKWebView *, WKFrameInfo *) = nil)
{
    HTTPServer server({
        { "/control"_s, { "<body style='margin: 0'><input id='input' style='position: absolute; left: 120px; top: 130px;' required></body>"_s } },
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    auto validationBubbleAnchorRect = [webView] -> NSRect {
        NSDictionary *contents = [webView _contentsOfUserInterfaceItem:@"validationBubble"][@"validationBubble"];
        NSDictionary *anchorRect = contents[@"anchorRect"];
        if (!anchorRect)
            return NSZeroRect;
        return NSMakeRect([anchorRect[@"x"] doubleValue], [anchorRect[@"y"] doubleValue], [anchorRect[@"width"] doubleValue], [anchorRect[@"height"] doubleValue]);
    };

    // The bare <input required> in /control has no enclosing <form>, so reportValidity() is called
    // directly on the input; in /subframe (and the equivalent main-frame markup below) the input is
    // wrapped in a <form> so the same call can be made from either the form or the input.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/control"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"input.reportValidity()"];
    [webView waitForNextPresentationUpdate];
    NSRect controlRect = validationBubbleAnchorRect();
    EXPECT_FALSE(NSIsEmptyRect(controlRect));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];

    if (prepareBeforeSubmit)
        prepareBeforeSubmit(webView.get(), childFrameInfo.get());

    // form.reportValidity() (like Element::focus()) is a no-op for cross-origin non-main-frame
    // iframes without a user gesture; retry until the validation bubble lands (bounded, so a
    // regression fails the assertion below rather than hanging). If the message were mis-routed
    // to the main frame's process, this would spin until the timeout with an empty rect.
    NSRect rect = NSZeroRect;
    EXPECT_TRUE(Util::waitFor([&] {
        [webView objectByEvaluatingJavaScriptWithUserGesture:@"form.reportValidity()" inFrame:childFrameInfo.get()];
        [webView waitForNextPresentationUpdate];
        rect = validationBubbleAnchorRect();
        return !NSIsEmptyRect(rect);
    }, 500));

    EXPECT_NEAR(rect.origin.x, controlRect.origin.x, 2);
    EXPECT_NEAR(rect.origin.y, controlRect.origin.y, 2);
}

static ASCIILiteral defaultCrossOriginIframeRequiredInputHTML = "<body style='margin: 0'><form id='form'><input id='input' style='position: absolute; left: 20px; top: 30px;' required></form></body>"_s;
static ASCIILiteral tallCrossOriginIframeRequiredInputHTML = "<body style='margin: 0; min-height: 1000px'><form id='form'><input id='input' style='position: absolute; left: 20px; top: 530px;' required></form></body>"_s;

TEST(SiteIsolation, ValidationMessageAnchorInCrossOriginIframe)
{
    checkValidationMessageAnchorInCrossOriginIframe(
        "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        defaultCrossOriginIframeRequiredInputHTML
    );
}

TEST(SiteIsolation, ValidationMessageAnchorInCrossOriginIframeWithScrolledMainFrame)
{
    checkValidationMessageAnchorInCrossOriginIframe(
        "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        defaultCrossOriginIframeRequiredInputHTML,
        ^(TestWKWebView *webView, WKFrameInfo *) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 400;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

TEST(SiteIsolation, ValidationMessageAnchorInScrolledCrossOriginIframe)
{
    checkValidationMessageAnchorInCrossOriginIframe(
        "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        tallCrossOriginIframeRequiredInputHTML,
        ^(TestWKWebView *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

TEST(SiteIsolation, ValidationMessageAnchorInScrolledCrossOriginIframeWithScrolledMainFrame)
{
    checkValidationMessageAnchorInCrossOriginIframe(
        "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        tallCrossOriginIframeRequiredInputHTML,
        ^(TestWKWebView *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 400;
            }));
            [webView waitForNextPresentationUpdate];

            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

static bool didReturnCorrectionPanelGrammarMarker = false;
static bool didShowCorrectionPanelIndicator = false;
static NSRect capturedCorrectionPanelAnchorRect = NSZeroRect;

static NSArray<NSTextCheckingResult *> *swizzledCheckStringForCorrectionPanelAnchor(id, SEL, NSString *stringToCheck, NSRange, NSTextCheckingTypes types, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    NSRange phraseRange = [stringToCheck rangeOfString:@"go in then"];
    if (phraseRange.location == NSNotFound)
        return @[ ];

    RetainPtr results = adoptNS([[NSMutableArray alloc] init]);
    if (types & NSTextCheckingTypeSpelling)
        [results addObject:[NSTextCheckingResult spellCheckingResultWithRange:phraseRange]];
    if (types & NSTextCheckingTypeGrammar) {
        NSRange grammarRange = NSMakeRange(phraseRange.location + 3, phraseRange.length - 3);
        NSDictionary *detail = @{
            NSGrammarRange: [NSValue valueWithRange:NSMakeRange(0, grammarRange.length)],
            NSGrammarCorrections: @[ @"in the" ],
        };
        [results addObject:[NSTextCheckingResult grammarCheckingResultWithRange:grammarRange details:@[ detail ]]];
        didReturnCorrectionPanelGrammarMarker = true;
    }
    return results.autorelease();
}

using CorrectionPanelIndicatorCompletionHandler = void (^)(NSString *);

static void swizzledShowCorrectionIndicatorCapturingAnchorRect(id, SEL, NSCorrectionIndicatorType, NSString *, NSArray<NSString *> *, NSRect anchorRect, NSView *, CorrectionPanelIndicatorCompletionHandler completionHandler)
{
    capturedCorrectionPanelAnchorRect = anchorRect;
    didShowCorrectionPanelIndicator = true;
    if (completionHandler)
        completionHandler(nil);
}

static NSString * const correctionPanelTestText = @"Let's go in then store\n";

static NSRect triggerCorrectionPanelAndCaptureAnchorRect(TestWKWebView<NSTextInputClient> *webView, WKFrameInfo *frame)
{
    didReturnCorrectionPanelGrammarMarker = false;
    didShowCorrectionPanelIndicator = false;
    capturedCorrectionPanelAnchorRect = NSZeroRect;

    auto evaluate = [webView, frame](NSString *script) {
        if (frame)
            [webView objectByEvaluatingJavaScript:script inFrame:frame];
        else
            [webView objectByEvaluatingJavaScript:script];
    };

    evaluate(@"getSelection().setPosition(document.body)");
    [webView insertText:correctionPanelTestText replacementRange:NSMakeRange(0, 0)];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(Util::runFor(&didReturnCorrectionPanelGrammarMarker, 2_s));

    // Establish an old selection in a different word so respondToChangedSelection fires below.
    evaluate(@"(() => { const r = document.createRange(); r.setStart(document.body.firstChild, 19); r.collapse(true); const s = getSelection(); s.removeAllRanges(); s.addRange(r); })()");
    [webView waitForNextPresentationUpdate];

    // End-of-word for the caret in "then" is offset 16, matching both markers' endOffsets.
    evaluate(@"(() => { const r = document.createRange(); r.setStart(document.body.firstChild, 14); r.collapse(true); const s = getSelection(); s.removeAllRanges(); s.addRange(r); })()");

    Util::runFor(&didShowCorrectionPanelIndicator, 3_s);
    return capturedCorrectionPanelAnchorRect;
}

static void checkCorrectionPanelAnchorInCrossOriginIframe(const String& mainframeHTML, const String& subframeHTML, void (^prepareBeforeTyping)(TestWKWebView<NSTextInputClient> *, WKFrameInfo *) = nil)
{
    HTTPServer server({
        { "/control"_s, { "<body contenteditable style='margin: 100px 0 0 100px; font-family: monospace; font-size: 24px; width: 500px;'></body>"_s } },
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringForCorrectionPanelAnchor)
    };
    InstanceMethodSwizzler showIndicatorSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(showCorrectionIndicatorOfType:primaryString:alternativeStrings:forStringInRect:view:completionHandler:),
        reinterpret_cast<IMP>(swizzledShowCorrectionIndicatorCapturingAnchorRect)
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/control"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView _setGrammarCheckingEnabledForTesting:YES];
    NSRect controlRect = triggerCorrectionPanelAndCaptureAnchorRect(webView.get(), nil);
    EXPECT_TRUE(didShowCorrectionPanelIndicator);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView _setGrammarCheckingEnabledForTesting:YES];
    RetainPtr childFrameInfo = [webView firstChildFrame];

    // focus() is a no-op for cross-origin non-main-frame iframes without a user gesture; retry
    // until the child frame reports itself as focused.
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrameInfo _isFocused])
        childFrameInfo = [webView firstChildFrame];

    if (prepareBeforeTyping)
        prepareBeforeTyping(webView.get(), childFrameInfo.get());

    NSRect rect = triggerCorrectionPanelAndCaptureAnchorRect(webView.get(), childFrameInfo.get());

    // If the anchor rect were left in the iframe's local root-view coordinates instead of being
    // converted to main-frame view coordinates, this would not match the control's on-screen position.
    EXPECT_TRUE(didShowCorrectionPanelIndicator);
    EXPECT_NEAR(rect.origin.x, controlRect.origin.x, 2);
    EXPECT_NEAR(rect.origin.y, controlRect.origin.y, 2);
}

static ASCIILiteral defaultCrossOriginIframeEditableBodyHTML = "<body contenteditable style='margin: 0; font-family: monospace; font-size: 24px;'></body>"_s;
static ASCIILiteral tallCrossOriginIframeEditableBodyHTML = "<body contenteditable style='margin: 0; padding-top: 500px; min-height: 1000px; font-family: monospace; font-size: 24px;'></body>"_s;

TEST(SiteIsolation, CorrectionPanelAnchorInCrossOriginIframe)
{
    checkCorrectionPanelAnchorInCrossOriginIframe(
        "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 500px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        defaultCrossOriginIframeEditableBodyHTML
    );
}

TEST(SiteIsolation, CorrectionPanelAnchorInCrossOriginIframeWithScrolledMainFrame)
{
    checkCorrectionPanelAnchorInCrossOriginIframe(
        "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='display: block; margin-left: 100px; margin-top: 500px; width: 500px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        defaultCrossOriginIframeEditableBodyHTML,
        ^(TestWKWebView<NSTextInputClient> *webView, WKFrameInfo *) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 400;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

TEST(SiteIsolation, CorrectionPanelAnchorInScrolledCrossOriginIframe)
{
    checkCorrectionPanelAnchorInCrossOriginIframe(
        "<body style='margin: 0'><iframe id='iframe' style='margin: 100px; width: 500px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        tallCrossOriginIframeEditableBodyHTML,
        ^(TestWKWebView<NSTextInputClient> *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

TEST(SiteIsolation, CorrectionPanelAnchorInScrolledCrossOriginIframeWithScrolledMainFrame)
{
    checkCorrectionPanelAnchorInCrossOriginIframe(
        "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='display: block; margin-left: 100px; margin-top: 500px; width: 500px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s,
        tallCrossOriginIframeEditableBodyHTML,
        ^(TestWKWebView<NSTextInputClient> *webView, WKFrameInfo *childFrameInfo) {
            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 400)"];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 400;
            }));
            [webView waitForNextPresentationUpdate];

            [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 500)" inFrame:childFrameInfo];
            EXPECT_TRUE(Util::waitFor([&] {
                return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrameInfo] intValue] == 500;
            }));
            [webView waitForNextPresentationUpdate];
        }
    );
}

static bool shouldReportBadGrammar = false;
static RetainPtr<NSString> stringForGrammarReview;
static BlockPtr<void(NSInteger, NSArray<NSTextCheckingResult *> *)> grammarReviewCompletionHandler;

static NSArray<NSTextCheckingResult *> *badGrammarResults(NSString *string)
{
    NSRange range = [string rangeOfString:@"in then"];
    if (range.location == NSNotFound)
        return @[ ];
    NSDictionary *detail = @{
        NSGrammarRange: [NSValue valueWithRange:NSMakeRange(0, range.length)],
        NSGrammarCorrections: @[ @"in the" ],
    };
    return @[ [NSTextCheckingResult grammarCheckingResultWithRange:range details:@[ detail ]] ];
}

static NSArray<NSTextCheckingResult *> *swizzledCheckStringForGrammarReview(id, SEL, NSString *stringToCheck, NSRange, NSTextCheckingTypes types, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    if (!shouldReportBadGrammar || !(types & NSTextCheckingTypeGrammar))
        return @[ ];
    return badGrammarResults(stringToCheck);
}

using GrammarReviewCompletionHandler = void (^)(NSInteger, NSArray<NSTextCheckingResult *> *);

static NSInteger swizzledRequestGrammarReview(id, SEL, NSString *stringToCheck, NSRange, NSString *, NSDictionary *, GrammarReviewCompletionHandler completionHandler)
{
    if ([stringToCheck containsString:@"in then"]) {
        stringForGrammarReview = stringToCheck;
        grammarReviewCompletionHandler = makeBlockPtr(completionHandler);
    }
    return 0;
}

TEST(SiteIsolation, ExtendedProofreadingGrammarMarkerInCrossOriginIframe)
{
    shouldReportBadGrammar = false;
    stringForGrammarReview = nil;
    grammarReviewCompletionHandler = nullptr;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringForGrammarReview)
    };
    InstanceMethodSwizzler requestGrammarCheckingSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(requestGrammarCheckingOfString:range:language:options:completionHandler:),
        reinterpret_cast<IMP>(swizzledRequestGrammarReview)
    };

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWebsiteDataStore:[server.httpsProxyConfiguration() websiteDataStore]];
    setFeatureEnabled(configuration.get(), @"ExtendedProofreadingEnabled", true);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView _setGrammarCheckingEnabledForTesting:YES];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:childFrame.get()];

    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body)" inFrame:childFrame.get()];
    [(id<NSTextInputClient>)webView.get() insertText:@"Let's go in then store\n" replacementRange:NSMakeRange(NSNotFound, 0)];
    ASSERT_TRUE(Util::waitFor([] {
        return !!grammarReviewCompletionHandler;
    }));

    shouldReportBadGrammar = true;
    grammarReviewCompletionHandler(0, badGrammarResults(stringForGrammarReview.get()));
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"internals.markerCountForNode(document.body.firstChild, 'grammar')" inFrame:childFrame.get()] intValue] > 0;
    }));

    shouldReportBadGrammar = false;
    stringForGrammarReview = nil;
    grammarReviewCompletionHandler = nullptr;
}

TEST(SiteIsolation, OpenedWindowFocusDelegates)
{
    auto openerHTML = "<script>"
        "    let w = window.open('https://domain2.com/opened');"
        "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);

    __block bool calledFocusWebView = false;
    [opened.uiDelegate setFocusWebView:^(WKWebView *viewToFocus) {
        calledFocusWebView = true;
    }];

    __block bool calledUnfocusWebView = false;
    [opened.uiDelegate setUnfocusWebView:^(WKWebView *viewToFocus) {
        calledUnfocusWebView = true;
    }];

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];
    Util::run(&calledFocusWebView);

    [opener.webView.get() evaluateJavaScript:@"w.blur()" completionHandler:nil];
    Util::run(&calledUnfocusWebView);
}

TEST(SiteIsolation, PopunderPreventedByConsumedAction)
{
    auto openerHTML = "<script>"
        "window.name = 'opener';"
        "addEventListener('mouseup', () => {"
        "    window.open('https://domain3.com/popup');"
        "    w.focus();"
        "});"
        "let w = window.open('https://domain2.com/opened');"
        "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } },
        { "/popup"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block WebViewAndDelegates opener;
    __block WebViewAndDelegates opened;
    __block RetainPtr<TestWKWebView> popupWebView;
    __block RetainPtr<TestNavigationDelegate> popupNavigationDelegate;
    __block int windowOpenCount = 0;
    opener.navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [opener.navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    opener.webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    opener.webView.get().navigationDelegate = opener.navigationDelegate.get();
    opener.uiDelegate = adoptNS([TestUIDelegate new]);
    opener.uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *navigationAction, WKWindowFeatures *) {
        enableSiteIsolation(configuration);
        if (!windowOpenCount++) {
            // First window.open: the pre-opened cross-origin window.
            opened.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
            opened.navigationDelegate = adoptNS([TestNavigationDelegate new]);
            [opened.navigationDelegate allowAnyTLSCertificate];
            opened.uiDelegate = adoptNS([TestUIDelegate new]);
            opened.webView.get().navigationDelegate = opened.navigationDelegate.get();
            opened.webView.get().UIDelegate = opened.uiDelegate.get();
            return opened.webView.get();
        }
        // Second window.open (the popup): consume the gesture.
        [navigationAction._userInitiatedAction consume];
        popupNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [popupNavigationDelegate allowAnyTLSCertificate];
        popupWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        [popupWebView setNavigationDelegate:popupNavigationDelegate.get()];
        return popupWebView.get();
    };
    [opener.webView setUIDelegate:opener.uiDelegate.get()];
    opener.webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [opener.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!opened.webView)
        Util::spinRunLoop();
    [opened.navigationDelegate waitForDidFinishNavigation];

    __block bool focusCalled = false;
    [opened.uiDelegate setFocusWebView:^(WKWebView *) {
        focusCalled = true;
    }];

    [opener.webView mouseDownAtPoint:CGPointMake(50, 50) simulatePressure:NO];
    [opener.webView mouseUpAtPoint:CGPointMake(50, 50)];
    [opener.webView waitForPendingMouseEvents];
    while (!popupWebView)
        Util::spinRunLoop();

    // The popup was created, but w.focus() on the cross-origin window should NOT
    // have called the focus delegate because the user gesture was already consumed
    // during popup creation. This exercises the FocusRemoteFrame IPC path.
    EXPECT_FALSE(focusCalled);
}

static CGRect findIndicatorRectInIsolatedIframe(int mainFrameScrollY, CGFloat topObscuredInset = 0, bool setInsetsAfterLoad = false)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0; height: 2000px'><iframe style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><body style='margin: 0'><p style='margin: 50px'>Hello world</p></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    if (topObscuredInset) {
        [webView _setAutomaticallyAdjustsContentInsets:NO];
        if (!setInsetsAfterLoad)
            [webView setObscuredContentInsets:NSEdgeInsetsMake(topObscuredInset, 0, 0, 0)];
    }

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    if (topObscuredInset && setInsetsAfterLoad) {
        [webView setObscuredContentInsets:NSEdgeInsetsMake(topObscuredInset, 0, 0, 0)];
        [webView waitForNextPresentationUpdate];
    }

    if (mainFrameScrollY) {
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(0, %d)", mainFrameScrollY]];
        while ([[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] != mainFrameScrollY)
            Util::spinRunLoop();
        [webView waitForNextPresentationUpdate];
    }

    [webView _findString:@"Hello world" options:_WKFindOptionsCaseInsensitive | _WKFindOptionsWrapAround | _WKFindOptionsShowFindIndicator | _WKFindOptionsShowOverlay maxCount:1];

    CGRect rect = CGRectNull;
    while (CGRectIsNull(rect)) {
        Util::spinRunLoop();
        rect = [webView _textIndicatorBoundingRectForTesting];
    }
    return rect;
}

TEST(SiteIsolation, FindStringIndicatorPositionInIsolatedIframe)
{
    auto rect = findIndicatorRectInIsolatedIframe(0);

    EXPECT_NEAR(CGRectGetMinX(rect), 150, 5);
    EXPECT_NEAR(CGRectGetMinY(rect), 550, 5);
    EXPECT_GE(CGRectGetMinY(rect), 500);
    EXPECT_LE(CGRectGetMaxY(rect), 800);
}

TEST(SiteIsolation, FindStringIndicatorPositionWithScrolledMainFrame)
{
    auto rect = findIndicatorRectInIsolatedIframe(400);

    EXPECT_NEAR(CGRectGetMinX(rect), 150, 5);
    EXPECT_NEAR(CGRectGetMinY(rect), 150, 5);
}

TEST(SiteIsolation, FindStringIndicatorPositionWithObscuredContentInsets)
{
    auto rect = findIndicatorRectInIsolatedIframe(0, 100);

    EXPECT_NEAR(CGRectGetMinX(rect), 150, 5);
    EXPECT_NEAR(CGRectGetMinY(rect), 650, 5);
}

TEST(SiteIsolation, FindStringIndicatorPositionWithObscuredContentInsetsChangedAfterLoad)
{
    auto rect = findIndicatorRectInIsolatedIframe(0, 100, true);

    EXPECT_NEAR(CGRectGetMinX(rect), 150, 5);
    EXPECT_NEAR(CGRectGetMinY(rect), 650, 5);
}

TEST(SiteIsolation, ProcessDisplayNames)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "<script></script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    __block bool done { false };
    [webView.get().configuration.websiteDataStore removeDataOfTypes:WKWebsiteDataStore.allWebsiteDataTypes modifiedSince:NSDate.distantPast completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t mainFramePID { 0 };
    pid_t iframePID { 0 };
    auto trees = frameTrees(webView.get());
    EXPECT_EQ([trees count], 2u);
    for (_WKFrameTreeNode *tree in trees.get()) {
        if (tree.info._isLocalFrame)
            mainFramePID = tree.info._processIdentifier;
        else if (tree.childFrames.count)
            iframePID = tree.childFrames[0].info._processIdentifier;
    }
    EXPECT_NE(mainFramePID, iframePID);
    EXPECT_NE(mainFramePID, 0);
    EXPECT_NE(iframePID, 0);

    done = false;
    WKProcessPool *pool = webView.get().configuration.processPool;
    [pool _getActivePagesOriginsInWebProcessForTesting:mainFramePID completionHandler:^(NSArray<NSString *> *result) {
        EXPECT_EQ(result.count, 1u);
        EXPECT_WK_STREQ(result[0], "https://example.com");
        done = true;
    }];
    Util::run(&done);

    done = false;
    [pool _getActivePagesOriginsInWebProcessForTesting:iframePID completionHandler:^(NSArray<NSString *> *result) {
        EXPECT_EQ(result.count, 1u);
        EXPECT_WK_STREQ(result[0], "https://apple.com");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, SelectAll)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe' src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "test"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView selectAll:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();
}

TEST(SiteIsolation, TopContentInsetAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/source"_s, { "<script> location.href = 'https://webkit.org/destination'; </script>"_s } },
        { "/destination"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _setTopContentInset:10];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(-10, [[webView objectByEvaluatingJavaScript:@"window.innerHeight"] intValue]);
}

#if defined(NDEBUG)

TEST(SiteIsolation, UnresponsiveProcessKeydown)
{
    HTTPServer server({
        { "/parent"_s, { "<!DOCTYPE html><body onload='iframe1.focus()'><div contenteditable style='width: 100px; height: 100px; border: solid 1px blue;'></div><iframe id='iframe1' src='https://webkit.org/unresponsive-page'></iframe></body>"_s } },
        { "/unresponsive-page"_s, { "<!DOCTYPE html><html><body onload='window.addEventListener(`keydown`, () => { while (true) { } });'>unresponsive</body></html>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateWithUnresponsiveCallback new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/parent"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView mainFrame].info._processIdentifier, [webView firstChildFrame]._processIdentifier);

    [webView objectByEvaluatingJavaScript:@"iframe1.focus()" inFrame:[webView mainFrame].info];
    [webView typeCharacter:' '];

    Util::runFor(4_s);

    [webView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];

    Util::runFor(1_s);

    EXPECT_TRUE(navigationDelegate.get().didBecomeUnresponsive);
    EXPECT_FALSE(navigationDelegate.get().didBecomeResponsive);
}

TEST(SiteIsolation, ResponsiveProcessAfterMousedown)
{
    HTTPServer server({
        { "/parent"_s, { "<!DOCTYPE html><body><iframe src='https://webkit.org/unresponsive-page'></iframe></body>"_s } },
        { "/unresponsive-page"_s, { "<!DOCTYPE html><html><body>unresponsive</body></html>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateWithUnresponsiveCallback new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/parent"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView mainFrame].info._processIdentifier, [webView firstChildFrame]._processIdentifier);

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    [webView sendClicksAtPoint:eventLocationInWindow numberOfClicks:1];
    Util::runFor(4_s);

    EXPECT_FALSE(navigationDelegate.get().didBecomeUnresponsive);
}

TEST(SiteIsolation, UnresponsiveProcessMousedown)
{
    HTTPServer server({
        { "/parent"_s, { "<!DOCTYPE html><head><style>iframe { width: 100px; height: 100px; }</style></head><body><iframe src='https://webkit.org/unresponsive-page'></iframe><br><iframe id='iframe1' src='https://w3.org/eventually-responsive-page'></iframe></body>"_s } },
        { "/unresponsive-page"_s, { "<!DOCTYPE html><html><body onload='window.addEventListener(`mousedown`, () => { while (true) { } });'>unresponsive</body></html>"_s } },
        { "/eventually-responsive-page"_s, { "<!DOCTYPE html><html><body>eventually responsive<script>addEventListener(`keydown`, () => { const start = performance.now(); while (start + 3500 > performance.now()) { }; document.body.textContent = `responsive now`; });</script></body></html>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateWithUnresponsiveCallback new]);

    // The two iframes must be in different processes for this test to distinguish a hung process
    // from one that recovered, so keep w3.org out of the shared process.
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([navigationAction.request.URL.host isEqualToString:@"w3.org"])
            preferences._prefersIsolatedProcess = YES;
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/parent"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView mainFrame].info._processIdentifier, [webView firstChildFrame]._processIdentifier);
    EXPECT_NE([webView firstChildFrame]._processIdentifier, [webView secondChildFrame]._processIdentifier);

    [webView objectByEvaluatingJavaScript:@"iframe1.focus()" inFrame:[webView mainFrame].info];
    [webView typeCharacter:' '];

    [webView sendClicksAtPoint:[webView convertPoint:CGPointMake(50, 50) toView:nil] numberOfClicks:1];
    Util::runFor(4_s);

    EXPECT_TRUE(navigationDelegate.get().didBecomeUnresponsive);
    EXPECT_FALSE(navigationDelegate.get().didBecomeResponsive);
}

TEST(SiteIsolation, UnresponsiveProcessDies)
{
    HTTPServer server({
        { "/parent"_s, { "<!DOCTYPE html><body><iframe src='https://webkit.org/unresponsive-page'></iframe></body>"_s } },
        { "/unresponsive-page"_s, { "<!DOCTYPE html><html><body onload='window.addEventListener(`mousedown`, () => { while (true) { } });'>unresponsive</body></html>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateWithUnresponsiveCallback new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/parent"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t childFramePID = [webView firstChildFrame]._processIdentifier;
    EXPECT_NE([webView mainFrame].info._processIdentifier, childFramePID);

    [webView sendClicksAtPoint:[webView convertPoint:CGPointMake(50, 50) toView:nil] numberOfClicks:1];
    Util::runFor(3500_ms);

    EXPECT_TRUE(navigationDelegate.get().didBecomeUnresponsive);
    kill(childFramePID, 9);

    Util::runFor(100_ms);
    EXPECT_TRUE(navigationDelegate.get().didBecomeResponsive);
}

static bool waitForMainFrameMouseDown(TestWKWebView *webView, unsigned maxAttempts)
{
    for (unsigned i = 0; i < maxAttempts; ++i) {
        if ([[webView objectByEvaluatingJavaScript:@"window.mouseDownCount"] intValue] > 0)
            return true;
        Util::runFor(100_ms);
    }
    return false;
}

static constexpr auto mainFrameWithUnresponsiveSubframeHTML = "<!DOCTYPE html><head><style>iframe { width: 100px; height: 100px; }</style></head>"
    "<body style='height: 500px'><iframe src='https://webkit.org/unresponsive-page'></iframe>"
    "<script>window.mouseDownCount = 0; addEventListener('mousedown', () => { window.mouseDownCount++; });</script></body>"_s;

TEST(SiteIsolation, MouseEventsAfterHoveringOverUnresponsiveSubframe)
{
    HTTPServer server({
        { "/parent"_s, { mainFrameWithUnresponsiveSubframeHTML } },
        { "/unresponsive-page"_s, { "<!DOCTYPE html><html><body onload='window.addEventListener(`mousemove`, () => { while (true) { } });'>unresponsive</body></html>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateWithUnresponsiveCallback new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/parent"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView mainFrame].info._processIdentifier, [webView firstChildFrame]._processIdentifier);

    CGPoint insideSubframe = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    [webView mouseEnterAtPoint:insideSubframe];
    [webView mouseMoveToPoint:insideSubframe withFlags:0];

    // The subframe process never replies to the mouse move, but that shouldn't block mouse events for the main frame forever.
    CGPoint outsideSubframe = [webView convertPoint:CGPointMake(400, 400) toView:nil];
    [webView mouseMoveToPoint:outsideSubframe withFlags:0];
    [webView mouseDownAtPoint:outsideSubframe simulatePressure:NO];
    [webView mouseUpAtPoint:outsideSubframe];

    EXPECT_TRUE(waitForMainFrameMouseDown(webView.get(), 100));
}

TEST(SiteIsolation, MouseEventsAfterUnresponsiveSubframeProcessIsTerminated)
{
    HTTPServer server({
        { "/parent"_s, { mainFrameWithUnresponsiveSubframeHTML } },
        { "/unresponsive-page"_s, { "<!DOCTYPE html><html><body onload='window.addEventListener(`mousedown`, () => { while (true) { } });'>unresponsive</body></html>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateWithUnresponsiveCallback new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/parent"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t childFramePID = [webView firstChildFrame]._processIdentifier;
    EXPECT_NE([webView mainFrame].info._processIdentifier, childFramePID);

    [webView sendClicksAtPoint:[webView convertPoint:CGPointMake(50, 50) toView:nil] numberOfClicks:1];
    Util::runFor(500_ms);
    kill(childFramePID, 9);

    // Mouse events should resume as soon as the process goes away, well before the subframe mouse event timeout.
    CGPoint outsideSubframe = [webView convertPoint:CGPointMake(400, 400) toView:nil];
    [webView mouseMoveToPoint:outsideSubframe withFlags:0];
    [webView mouseDownAtPoint:outsideSubframe simulatePressure:NO];
    [webView mouseUpAtPoint:outsideSubframe];

    EXPECT_TRUE(waitForMainFrameMouseDown(webView.get(), 20));
}

#endif

TEST(SiteIsolation, SharedProcessAfterClick)
{
    HTTPServer server({
        { "/warmup"_s, { "<iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterClickDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterClickTestITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    [defaultFileManager removeItemAtPath:itpRoot.path error:nil];
    // Its recorded import would make a second run of this binary skip the import entirely.
    [defaultFileManager removeItemAtPath:dataStoreRoot.path error:nil];

    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, dataStoreRoot, itpRoot);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/warmup"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://w3.org"_s } }
        },
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [webView waitForPendingMouseEvents];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, SharedProcessAfterKeyDown)
{
    HTTPServer server({
        { "/webkit"_s, { "<iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterKeyDownDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterKeyDownTestITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    [defaultFileManager removeItemAtPath:itpRoot.path error:nil];
    // Its recorded import would make a second run of this binary skip the import entirely.
    [defaultFileManager removeItemAtPath:dataStoreRoot.path error:nil];

    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, dataStoreRoot, itpRoot);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Both subframe sites are new to the user, so they share one process.
    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://apple.com"_s }, { "https://w3.org"_s } }
        },
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView typeCharacter:'n'];
    [webView waitForNextPresentationUpdate];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // The user has now used apple.com as a page, so it gets a process of its own rather than joining
    // w3.org, which it shared one with before.
    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://apple.com"_s }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://w3.org"_s } }
        },
    });
}

TEST(SiteIsolation, SharedProcessAfterUserInteractionInSharedProcesss)
{
    HTTPServer server({
        { "/warmup"_s, { "<iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/payload"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<TestWKWebView> opendWebView;
    RetainPtr uiDelegate = adoptNS([[TestUIDelegate new] init]);
    auto *sharedNavigationDelegate = navigationDelegate.get();
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opendWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 200) configuration:configuration]);
        opendWebView.get().navigationDelegate = sharedNavigationDelegate;
        return opendWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/warmup"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://w3.org"_s } }
        },
    });

    [webView evaluateJavaScript:@"w = window.open('https://w3.org/w3c', 'newWindow')" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ([webView mainFrame].childFrames.firstObject.info._processIdentifier, [opendWebView mainFrame].info._processIdentifier);

    [opendWebView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [opendWebView waitForPendingMouseEvents];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/payload"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s }, { "https://w3.org"_s } }
        },
    });
}


TEST(SiteIsolation, SharedProcessWebProcessCacheSharedProcessForSiteWithUserInteraction)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/w3c"_s, { "w3c"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://apple.com"_s,
        },
    });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);

    [webView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [webView waitForPendingMouseEvents];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame } },
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s } },
        },
    });
    auto mainFrameProcessC = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1C = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2C = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_EQ(mainFrameProcessC, mainFrameProcess);
    // The cached shared process hosted apple.com, which is now an isolated site, so it is
    // discarded entirely rather than reused for webkit.org.
    EXPECT_NE(childFrameProcess1C, childFrameProcess1);
    EXPECT_NE(childFrameProcess2C, childFrameProcess2);
}

TEST(SiteIsolation, SharedProcessWithUserInteractionOverride)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, nil, nil, @"apple.com");
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame } }
        }
    });
}

TEST(SiteIsolation, StorageAccessUnderOpenerWithRemoteFrameOpener)
{
    // This test verifies that when Site Isolation is enabled and a popup
    // triggers user interaction, a cross-origin iframe (in a different
    // WebProcess) can access its unpartitioned cookies.

    auto iframeHTML = "<script>"
        "window.addEventListener('message', function(e) {"
        "    e.source.postMessage(document.cookie, '*');"
        "});"
        "</script>"_s;

    auto popupHTML = "popup content"_s;

    auto mainHTML = "<iframe src='https://webkit.org/iframe'></iframe>"
        "<script>"
        "window.addEventListener('message', function(e) {"
        "    document.title = e.data;"
        "});"
        "</script>"_s;

    HTTPServer server({
        { "/main"_s, { mainHTML } },
        { "/iframe"_s, { iframeHTML } },
        { "/popup"_s, { { { "Set-Cookie"_s, "auth=loggedin; path=/; SameSite=None; Secure"_s } }, popupHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"StorageAccessUnderOpenerDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"StorageAccessUnderOpenerITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    [defaultFileManager removeItemAtPath:dataStoreRoot.path error:nil];
    [defaultFileManager removeItemAtPath:itpRoot.path error:nil];
    [defaultFileManager createDirectoryAtURL:dataStoreRoot withIntermediateDirectories:YES attributes:nil error:nil];
    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];

    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    // Mark webkit.org as prevalent so its third-party cookies are blocked by ITP.
    auto database = makeUniqueRef<WebCore::SQLiteDatabase>();
    EXPECT_TRUE(database->open(itpDatabaseFile.path));
    EXPECT_TRUE(database->executeCommand("UPDATE ObservedDomains SET isPrevalent = 1 WHERE registrableDomain = 'webkit.org'"_s));
    database->close();

    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithDirectory:dataStoreRoot]);
    dataStoreConfiguration.get()._resourceLoadStatisticsDirectory = itpRoot;
    [dataStoreConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];

    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [dataStore _setResourceLoadStatisticsDebugMode:YES];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    enableSiteIsolation(configuration.get());

    __block RetainPtr<TestWKWebView> popupWebView;

    RetainPtr openerNavDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavDelegate allowAnyTLSCertificate];

    RetainPtr popupNavDelegate = adoptNS([TestNavigationDelegate new]);
    [popupNavDelegate allowAnyTLSCertificate];

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *config, WKNavigationAction *action, WKWindowFeatures *features) {
        enableSiteIsolation(config);
        popupWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:config]);
        popupWebView.get().navigationDelegate = popupNavDelegate.get();
        return popupWebView.get();
    };

    RetainPtr openerWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    openerWebView.get().navigationDelegate = openerNavDelegate.get();
    openerWebView.get().UIDelegate = uiDelegate.get();
    openerWebView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [openerNavDelegate waitForDidFinishNavigation];

    // Wait for the cross-origin iframe to load in its own process.
    while (![[openerWebView firstChildFrame].securityOrigin.host isEqualToString:@"webkit.org"])
        Util::spinRunLoop();

    // Verify that webkit.org cookies are blocked as third-party under example.com.
    NSString *cookieBefore = [openerWebView stringByEvaluatingJavaScript:@"document.cookie" inFrame:[openerWebView firstChildFrame]];
    EXPECT_WK_STREQ(cookieBefore, "");

    // Open a popup to webkit.org which sets a cookie in first-party context.
    [openerWebView evaluateJavaScript:@"window.open('https://webkit.org/popup')" completionHandler:nil];
    [popupNavDelegate waitForDidFinishNavigation];

    // The popup is in a different process from the opener (Site Isolation).
    // The opener is a RemoteFrame from the popup's perspective.
    EXPECT_NE([openerWebView _webProcessIdentifier], [popupWebView _webProcessIdentifier]);

    NSString *cookieInPopup = [popupWebView stringByEvaluatingJavaScript:@"document.cookie"];
    EXPECT_WK_STREQ(cookieInPopup, "auth=loggedin");

    // Simulate user interaction in the popup.
    [popupWebView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [popupWebView waitForPendingMouseEvents];

    // Poll until the iframe can read its unpartitioned cookie.
    NSString *cookieAfter = @"";
    while (true) {
        cookieAfter = [openerWebView stringByEvaluatingJavaScript:@"document.cookie" inFrame:[openerWebView firstChildFrame]];
        if ([cookieAfter length] > 0)
            break;
        Util::runFor(0.1_s);
    }
    EXPECT_WK_STREQ(cookieAfter, "auth=loggedin");
}

#if ENABLE(DRAG_SUPPORT)

TEST(SiteIsolation, DragSourceEndedAtCoordinateTransformation)
{
    static constexpr ASCIILiteral mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' style='position: absolute; top: 200px; left: 200px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
    "    const draggable = document.getElementById('draggable');"
    "    draggable.addEventListener('dragstart', (event) => {"
    "        parent.postMessage('dragstart:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "    draggable.addEventListener('dragend', (event) => {"
    "        parent.postMessage('dragend:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(300, 300) to:CGPointMake(350, 350)];

    RetainPtr<NSArray<NSString *>> events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_GT([events count], 0U);

    bool foundDragStart = false;
    bool foundDragEnd = false;
    NSString *dragEndEvent = nil;

    for (NSString *event in events.get()) {
        if ([event hasPrefix:@"dragstart:"]) {
            foundDragStart = true;
        } else if ([event hasPrefix:@"dragend:"]) {
            foundDragEnd = true;
            dragEndEvent = event;
        }
    }

    EXPECT_TRUE(foundDragStart) << "Should have received dragstart event in remote frame";
    EXPECT_TRUE(foundDragEnd) << "Should have received dragend event in remote frame";

    if (dragEndEvent) {
        RetainPtr coords = [dragEndEvent substringFromIndex:[@"dragend:" length]];
        RetainPtr components = [coords componentsSeparatedByString:@","];
        if ([components count] == 2) {
            int x = [components[0] intValue];
            int y = [components[1] intValue];
            EXPECT_TRUE(x >= 144 && x <= 154) << "Expected dragend x coordinate around 148, got " << x;
            EXPECT_TRUE(y >= 144 && y <= 154) << "Expected dragend y coordinate around 148, got " << y;
        }
    }
}

TEST(SiteIsolation, DragSourceEndedAtCoordinateTransformationNested)
{
    static constexpr ASCIILiteral mainframeHTML =
    "<iframe width='500' height='500' style='position: absolute; top: 50px; left: 50px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='500' height='500' style='position: absolute; top: 50px; left: 50px; border: 2px solid blue;' src='https://domain3.com/nestedSubframe'></iframe>"_s;

    static constexpr ASCIILiteral nestedSubframeHTML = "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: yellow; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
    "    const draggable = document.getElementById('draggable');"
    "    draggable.addEventListener('dragstart', (event) => {"
    "        parent.postMessage('dragstart:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "    draggable.addEventListener('dragend', (event) => {"
    "        parent.postMessage('dragend:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } },
        { "/nestedSubframe"_s, { nestedSubframeHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 800, 800) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(204, 204) to:CGPointMake(300, 300)];

    RetainPtr<NSArray<NSString *>> events = [webView objectByEvaluatingJavaScript:@"window.events" inFrame:[webView firstChildFrame]];
    EXPECT_GT([events count], 0U);

    bool foundDragStart = false;
    bool foundDragEnd = false;
    NSString *dragEndEvent = nil;

    for (NSString *event in events.get()) {
        if ([event hasPrefix:@"dragstart:"]) {
            foundDragStart = true;
        } else if ([event hasPrefix:@"dragend:"]) {
            foundDragEnd = true;
            dragEndEvent = event;
        }
    }

    EXPECT_TRUE(foundDragStart) << "Should have received dragstart event in remote frame";
    EXPECT_TRUE(foundDragEnd) << "Should have received dragend event in remote frame";

    if (dragEndEvent) {
        RetainPtr coords = [dragEndEvent substringFromIndex:[@"dragend:" length]];
        RetainPtr components = [coords componentsSeparatedByString:@","];
        if ([components count] == 2) {
            int x = [components[0] intValue];
            int y = [components[1] intValue];
            EXPECT_TRUE(x >= 190 && x <= 200) << "Expected dragend x coordinate around 196, got " << x;
            EXPECT_TRUE(y >= 190 && y <= 200) << "Expected dragend y coordinate around 196, got " << y;
        }
    }
}

TEST(SiteIsolation, DragSourceEndedOutsideRemoteFrame)
{
    static constexpr ASCIILiteral mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' style='position: absolute; top: 200px; left: 200px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
    "    const draggable = document.getElementById('draggable');"
    "    draggable.addEventListener('dragstart', (event) => {"
    "        parent.postMessage('dragstart', '*');"
    "    });"
    "    draggable.addEventListener('dragend', (event) => {"
    "        parent.postMessage('dragend:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // Drop in the main frame, outside the iframe, twice. The main frame's process handles the end of the
    // drag, so the iframe's process has to be told separately to dispatch dragend and reset its drag state.
    RetainPtr<NSArray<NSString *>> events;
    for (unsigned i = 0; i < 2; ++i) {
        [simulator runFrom:CGPointMake(300, 300) to:CGPointMake(100, 100)];
        bool receivedDragEnd = Util::waitFor([&] {
            events = [webView objectByEvaluatingJavaScript:@"window.events"];
            return [events count] >= 2 * (i + 1);
        });
        if (!receivedDragEnd)
            break;
    }

    ASSERT_EQ(4U, [events count]);
    for (unsigned i = 0; i < 2; ++i) {
        EXPECT_WK_STREQ("dragstart", [events objectAtIndex:2 * i]);
        NSString *dragEndEvent = [events objectAtIndex:2 * i + 1];
        EXPECT_TRUE([dragEndEvent hasPrefix:@"dragend:"]) << [dragEndEvent UTF8String];
        RetainPtr components = [[dragEndEvent substringFromIndex:[@"dragend:" length]] componentsSeparatedByString:@","];
        if ([components count] == 2) {
            // (100, 100) in the main frame is (-102, -102) in the iframe, which is offset by 200px plus its 2px border.
            int x = [components.get()[0] intValue];
            int y = [components.get()[1] intValue];
            EXPECT_TRUE(x >= -107 && x <= -97) << "Expected dragend x coordinate around -102, got " << x;
            EXPECT_TRUE(y >= -107 && y <= -97) << "Expected dragend y coordinate around -102, got " << y;
        }
    }
}

TEST(SiteIsolation, DragImageLocation)
{
    static constexpr ASCIILiteral mainframeHTML = "<iframe width='300' height='300' style='position: absolute; top: 200px; left: 200px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
    "    const draggable = document.getElementById('draggable');"
    "    draggable.addEventListener('dragstart', (event) => {"
    "        e.dataTransfer.setData('text/plain', this.textContent);"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(300, 300) to:CGPointMake(350, 350)];

    EXPECT_EQ([simulator initialDragImageLocationInView], NSMakePoint(252, 252));
}

static NSPoint selectionDragImageLocationInSubframe(bool siteIsolationEnabled, unsigned mainFrameScrollY, unsigned subframeScrollY)
{
    // The iframe and the editable text are offset by the scroll amounts so that they appear at the same place in the view after scrolling.
    auto mainframeHTML = makeString("<body style='margin: 0; height: 3000px;'><iframe width='300' height='300' style='position: absolute; top: "_s, 200 + mainFrameScrollY, "px; left: 200px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe></body>"_s);
    auto subframeHTML = makeString("<body style='margin: 0; height: 3000px;'>"
        "<div id='editor' contenteditable style='position: absolute; top: "_s, 50 + subframeScrollY, "px; left: 50px; font-size: 24px; line-height: 30px;'>Hello world</div>"
        "</body>"_s);

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    if (siteIsolationEnabled)
        enableSiteIsolation(configuration.get());
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"scrollTo(0, %u); true", mainFrameScrollY]];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"scrollTo(0, %u); getSelection().selectAllChildren(document.getElementById('editor')); true", subframeScrollY] inFrame:[webView firstChildFrame]];
    [webView waitForNextPresentationUpdate];

    // The iframe's content box starts at (202, 202), so "Hello world" starts at (252, 252) in the view.
    [simulator runFrom:CGPointMake(280, 267) to:CGPointMake(380, 367)];
    return [simulator initialDragImageLocationInView];
}

static void testSelectionDragImageLocation(unsigned mainFrameScrollY, unsigned subframeScrollY)
{
    auto expected = selectionDragImageLocationInSubframe(false, mainFrameScrollY, subframeScrollY);
    auto actual = selectionDragImageLocationInSubframe(true, mainFrameScrollY, subframeScrollY);
    EXPECT_GT(expected.x, 250);
    EXPECT_GT(expected.y, 250);
    EXPECT_EQ(expected, actual);
}

TEST(SiteIsolation, SelectionDragImageLocation)
{
    testSelectionDragImageLocation(0, 0);
}

TEST(SiteIsolation, SelectionDragImageLocationInScrolledMainFrame)
{
    testSelectionDragImageLocation(500, 0);
}

TEST(SiteIsolation, SelectionDragImageLocationInScrolledSubframe)
{
    testSelectionDragImageLocation(0, 500);
}

TEST(SiteIsolation, SelectionDragImageLocationInScrolledMainFrameAndSubframe)
{
    testSelectionDragImageLocation(500, 500);
}

TEST(SiteIsolation, MouseClickAfterIncompleteDragging)
{
    static constexpr ASCIILiteral mainframeHTML = "<body><iframe id='testFrame' width='300' height='300' style='position: absolute; top: 100px; left: 100px;' src='https://domain2.com/subframe'></iframe></body>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 20px;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue;'></div>"
    "<script>"
    "    let clickCount = 0;"
    "    document.addEventListener('click', () => {"
    "        clickCount++;"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // To simulate drag without finishing via mouse up, don't use simulator's runTo function.
    [webView mouseDownAtPoint:NSMakePoint(170, 430) simulatePressure:NO];
    [webView setEventTimestampOffset:0.25];
    [webView mouseDragToPoint:NSMakePoint(200, 450)];
    [webView waitForPendingMouseEvents];

    // Wait for drag states to update and settle.
    [webView waitForNextPresentationUpdate];

    [webView sendClickAtPoint:NSMakePoint(250, 350)];
    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];

    [webView waitForNextPresentationUpdate];
    RetainPtr clickCount = [webView objectByEvaluatingJavaScript:@"clickCount" inFrame:[webView firstChildFrame]];

    EXPECT_EQ([clickCount intValue], 1);
}

TEST(SiteIsolation, DragOverStateInfo)
{
    static constexpr ASCIILiteral mainframeHTML = "<body>"
    "<div draggable='true' id='item1'>Item 1</div>"
    "<iframe id='testFrame' width='300' height='300' src='https://domain2.com/subframe'></iframe>"
    "<script>"
    "   window.dragStarted = false;"
    "   let dragElement = document.getElementById('item1');"
    "   dragElement.addEventListener('dragstart', function(e) {"
    "       window.dragStarted = true;"
    "       e.dataTransfer.setData('text/plain', this.textContent);"
    "   });"
    "</script>"
    "</body>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 20px;'>"
    "<div id='iframe-dropzone' style='width: 100px; height: 100px; background-color: blue;'></div>"
    "<script>"
    "   let dragOverData = '';"
    "   const dropzone = document.getElementById('iframe-dropzone');"
    "   dropzone.addEventListener('dragenter', function(e) {"
    "       e.preventDefault();"
    "   });"
    "   dropzone.addEventListener('dragover', function(e) {"
    "       e.preventDefault();"
    "       const data = e.dataTransfer.getData('text/plain');"
    "       dragOverData = data;"
    "   });"
    "   dropzone.addEventListener('drop', function(e) {"
    "       e.preventDefault();"
    "   });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [simulator runFrom:CGPointMake(300, 17) to:CGPointMake(78, 96)];

    [webView waitForNextPresentationUpdate];

    bool dragStarted = [webView objectByEvaluatingJavaScript:@"window.dragStarted"];

    __block NSString *dragOverData;
    __block bool done = false;

    [webView evaluateJavaScript:@"dragOverData" inFrame:[webView firstChildFrame] completionHandler:^(id resultValue, NSError *error) {
        EXPECT_NULL(error);
        done = true;
        dragOverData = resultValue;
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ(dragOverData, "");
    EXPECT_TRUE(dragStarted);
}

TEST(SiteIsolation, DragOverDataTransferTypesInCrossOriginSubframe)
{
    static constexpr ASCIILiteral mainframeHTML = "<body style='margin: 0'>"
    "<iframe width='400' height='400' style='border: none;' src='https://domain2.com/subframe'></iframe>"
    "</body>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; width: 100%; height: 100vh;'>"
    "<script>"
    "    window.dragOverTypes = [];"
    "    window.didDrop = false;"
    "    document.addEventListener('dragover', (e) => {"
    "        window.dragOverTypes = Array.from(e.dataTransfer.types);"
    "        if (window.dragOverTypes.includes('Files'))"
    "            e.preventDefault();"
    "    });"
    "    document.addEventListener('drop', (e) => {"
    "        e.preventDefault();"
    "        window.didDrop = true;"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [simulator writeFiles:@[ [NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"] ]];
    [simulator runFrom:CGPointMake(200, 200) to:CGPointMake(200, 200)];

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"dragOverTypes.includes('Files')" inFrame:childFrame.get()] boolValue]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"didDrop" inFrame:childFrame.get()] boolValue]);
}

TEST(SiteIsolation, DropExternalFileInCrossOriginSubframe)
{
    static constexpr ASCIILiteral mainframeHTML = "<body style='margin: 0'>"
    "<iframe width='400' height='400' style='border: none;' src='https://domain2.com/subframe'></iframe>"
    "</body>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; width: 100%; height: 100vh;'>"
    "<script>"
    "    window.dropFileName = '';"
    "    document.addEventListener('dragover', (e) => e.preventDefault());"
    "    document.addEventListener('drop', (e) => {"
    "        e.preventDefault();"
    "        window.dropFileName = e.dataTransfer.files[0].name;"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr fileURL = [NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"];
    [simulator writeFiles:@[ fileURL.get() ]];
    [simulator runFrom:CGPointMake(200, 200) to:CGPointMake(200, 200)];

    __block RetainPtr<NSString> dropFileName;
    __block bool done = false;
    [webView evaluateJavaScript:@"window.dropFileName" inFrame:[webView firstChildFrame] completionHandler:^(id result, NSError *error) {
        dropFileName = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ(dropFileName.get(), "apple.gif");
}

#endif // ENABLE(DRAG_SUPPORT)

NSPoint testColorPickerPopoverLocation(const String& mainPageSource, const String& iframeSource)
{
    HTTPServer server({
        { "/mainframe"_s, { mainPageSource } },
        { "/iframe"_s, { iframeSource } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool done = false;
    __block RetainPtr<NSView> popoverPositioningView;

    InstanceMethodSwizzler swizzler {
        NSPopover.class,
        @selector(showRelativeToRect:ofView:preferredEdge:),
        imp_implementationWithBlock(^(id, NSRect, NSView *positioningView, NSRectEdge) {
            popoverPositioningView = positioningView;
            done = true;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");
    [webView waitForNextPresentationUpdate];

    [webView sendClickAtPoint:NSMakePoint(200, 400)];

    Util::run(&done);

    return [popoverPositioningView convertRect:[popoverPositioningView bounds] toView:webView.get()].origin;
}

TEST(SiteIsolation, ColorInputPickerLocationInCrossSiteIframe)
{
    auto mainPageSource =
        "<iframe id=iframe style='margin: 100px; width: 400px; height: 300px;' src='https://webkit.org/iframe' onload='load()'></iframe>"_s
        "<script>function load() { alert('loaded'); }</script>"_s;

    auto iframeSource =
        "<!DOCTYPE html>"_s
        "<input style='margin: 50px; appearance: none; width: 50px; height: 50px;' type='color'>"_s;

    auto pickerLocation = testColorPickerPopoverLocation(mainPageSource, iframeSource);
    EXPECT_EQ(pickerLocation, NSMakePoint(168, 168));
}

TEST(SiteIsolation, ColorInputPickerLocationInScrolledCrossSiteIframe)
{
    auto mainPageSource =
        "<iframe id=iframe style='margin: 100px; width: 400px; height: 300px;' src='https://webkit.org/iframe' onload='load()'></iframe>"_s
        "<script>function load() { alert('loaded'); }</script>"_s;

    auto iframeSource =
        "<!DOCTYPE html>"_s
        "<div style='height: 1000px'></div>"_s
        "<input style='margin: 50px; appearance: none; width: 50px; height: 50px;' type='color'>"_s
        "<div style='height: 1000px'></div>"_s
        "<script>onload = () => window.scroll(0, 1000);</script>"_s;

    auto pickerLocation = testColorPickerPopoverLocation(mainPageSource, iframeSource);
    EXPECT_EQ(pickerLocation, NSMakePoint(168, 168));
}

TEST(SiteIsolation, ColorInputPickerLocationInDelayLoadedCrossSiteIframe)
{
    auto mainPageSource =
        "<div style='height: 1000px'></div>"_s
        "<iframe id=iframe style='margin: 100px; width: 400px; height: 300px;'></iframe>"_s
        "<div style='height: 1000px'></div>"_s
        "<script>"_s
        "  window.scroll(0, 1000);"_s
        "  requestAnimationFrame(() => {"_s
        "    requestAnimationFrame(() => {"_s
        "      requestAnimationFrame(() => {"_s
        "        iframe.onload = () => alert('loaded');"_s
        "        iframe.src = 'https://webkit.org/iframe';"_s
        "      });"_s
        "    });"_s
        "  });"_s
        "</script>"_s;

    auto iframeSource =
        "<!DOCTYPE html>"_s
        "<input style='margin: 50px; appearance: none; width: 50px; height: 50px;' type='color'>"_s;

    auto pickerLocation = testColorPickerPopoverLocation(mainPageSource, iframeSource);
    EXPECT_EQ(pickerLocation, NSMakePoint(168, 168));
}

static CGRect dataListSuggestionsElementRectAfterClicking(HTTPServer& server, NSPoint clickLocation)
{
    __block CGRect dataListSuggestionsElementRect = CGRectNull;
    __block bool didRequestDataListSuggestionsDropdownRect = false;

    InstanceMethodSwizzler dropdownRectSwizzler {
        NSClassFromString(@"WKDataListSuggestionsController"),
        NSSelectorFromString(@"dropdownRectForElementRect:"),
        imp_implementationWithBlock(^NSRect(id, const WebCore::IntRect& elementRect) {
            dataListSuggestionsElementRect = CGRectMake(elementRect.x(), elementRect.y(), elementRect.width(), elementRect.height());
            didRequestDataListSuggestionsDropdownRect = true;
            return NSZeroRect;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");
    [webView waitForNextPresentationUpdate];

    [webView sendClickAtPoint:clickLocation];
    Util::run(&didRequestDataListSuggestionsDropdownRect);

    return dataListSuggestionsElementRect;
}

TEST(SiteIsolation, DataListSuggestionsElementRectInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'>"
            "<iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/iframe'></iframe>"
            "</body>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html>"
            "<body style='margin: 0' onload='alert(\"loaded\")'>"
            "<input list='fruits' style='display: block; margin: 50px; width: 100px; height: 50px; border: none; padding: 0;'>"
            "<datalist id='fruits'>"
            "<option>Apple</option>"
            "<option>Orange</option>"
            "</datalist>"
            "</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto elementRect = dataListSuggestionsElementRectAfterClicking(server, NSMakePoint(200, 425));
    EXPECT_EQ(elementRect, CGRectMake(150, 150, 100, 50));
}

TEST(SiteIsolation, DataListSuggestionsElementRectInNestedCrossOriginIframes)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'>"
            "<iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/middle'></iframe>"
            "</body>"_s } },
        { "/middle"_s, { "<!DOCTYPE html>"
            "<body style='margin: 0'>"
            "<iframe style='display: block; margin: 50px; width: 200px; height: 150px; border: none;' src='https://domain3.com/inner'></iframe>"
            "</body>"_s } },
        { "/inner"_s, { "<!DOCTYPE html>"
            "<body style='margin: 0' onload='alert(\"loaded\")'>"
            "<input list='fruits' style='display: block; margin: 25px; width: 100px; height: 50px; border: none; padding: 0;'>"
            "<datalist id='fruits'>"
            "<option>Apple</option>"
            "<option>Orange</option>"
            "</datalist>"
            "</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // Two nested frame offsets: 100 + 50 + 25.
    auto elementRect = dataListSuggestionsElementRectAfterClicking(server, NSMakePoint(225, 400));
    EXPECT_EQ(elementRect, CGRectMake(175, 175, 100, 50));
}

TEST(SiteIsolation, SelectElementPopupAfterFocusChangesDuringTracking)
{
    auto mainframeHTML = "<body style='margin:0'>"
        "<select id='sel' style='width:200px;height:30px;'>"
        "<option value='a'>Alpha</option>"
        "<option value='b'>Bravo</option>"
        "<option value='c'>Charlie</option>"
        "</select>"
        "<iframe id='iframe' style='display:block;width:100%;height:500px;border:none;' src='https://domain2.com/subframe'></iframe>"
        "</body>"_s;
    auto subframeHTML = "<body style='margin:0'><script>alert('iframe loaded');</script></body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"SelectShowPickerEnabled", true);
    auto [webViewBinding, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    RetainPtr webView = webViewBinding;

    // Replace AppKit's modal popUpMenu: tracking with a synchronous poke at the
    // backing NSPopUpButtonCell. WebPopupMenuProxyMac::showPopupMenu reads
    // [m_popup indexOfSelectedItem] after this returns to compute the
    // value-change IPC payload. The cell is normally the NSMenu's delegate;
    // fall back to a menu item's target in case AppKit's wiring changes.
    RetainPtr menuProto = adoptNS([NSMenu new]);
    InstanceMethodSwizzler popUpSwizzler {
        [[menuProto _menuImpl] class],
        NSSelectorFromString(@"popUpMenu:atLocation:width:forView:withSelectedItem:withFont:withFlags:withOptions:"),
        imp_implementationWithBlock(^(id, NSMenu *menu, NSPoint, CGFloat, NSView *, NSInteger, NSFont *, NSUInteger, NSDictionary *) {
            id delegate = [menu delegate];
            NSPopUpButtonCell *popupCell = [delegate isKindOfClass:[NSPopUpButtonCell class]] ? (NSPopUpButtonCell *)delegate : nil;
            if (!popupCell) {
                for (NSMenuItem *item in [menu itemArray]) {
                    if ([item.target isKindOfClass:[NSPopUpButtonCell class]]) {
                        popupCell = (NSPopUpButtonCell *)item.target;
                        break;
                    }
                }
            }
            [popupCell selectItemAtIndex:2];
        })
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    EXPECT_WK_STREQ("iframe loaded", [webView _test_waitForAlert]);

    // Park focus in the remote iframe so that focusedOrMainFrame() resolves to
    // the iframe's process when valueChangedForPopupMenu fires — that's the
    // bug condition under the old code.
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while ([webView mainFrame].info._isFocused || ![webView firstChildFrame]._isFocused)
        Util::spinRunLoop();

    // showPicker() opens the popup without focusing the <select>, so focus
    // remains in the iframe through to the IPC.
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('sel').showPicker()"];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.getElementById('sel').value"] isEqualToString:@"c"];
    }));
}

// In every test below, the <select> ends up at (150, 150) with size 100x30 in main frame view coordinates.
static NSRect selectPopupMenuRectInCrossSiteIframe(ASCIILiteral mainframeHTML, ASCIILiteral subframeHTML, int mainFrameScrollY = 0, int subframeScrollY = 0)
{
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webViewBinding, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    RetainPtr webView = webViewBinding;

    // WebPopupMenuProxyMac anchors the menu to a temporary subview of the web view whose frame is the rect
    // it received from the web process. Record that rect instead of running AppKit's modal menu tracking.
    __block bool done = false;
    __block NSRect popupRect = NSZeroRect;
    RetainPtr menuProto = adoptNS([NSMenu new]);
    InstanceMethodSwizzler popUpSwizzler {
        [[menuProto _menuImpl] class],
        NSSelectorFromString(@"popUpMenu:atLocation:width:forView:withSelectedItem:withFont:withFlags:withOptions:"),
        imp_implementationWithBlock(^(id, NSMenu *, NSPoint, CGFloat, NSView *view, NSInteger, NSFont *, NSUInteger, NSDictionary *) {
            popupRect = view.frame;
            done = true;
        })
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    if (mainFrameScrollY)
        scrollFrameAndWait(webView.get(), nil, mainFrameScrollY);
    if (subframeScrollY)
        scrollFrameAndWait(webView.get(), [webView firstChildFrame], subframeScrollY);
    [webView waitForNextPresentationUpdate];

    // Click the center of the <select>. Window coordinates have a bottom-left origin.
    [webView sendClickAtPoint:NSMakePoint(200, 600 - 165)];
    Util::run(&done);

    return popupRect;
}

static constexpr ASCIILiteral selectPopupMenuMainframeHTML =
    "<body style='margin: 0'>"
    "<iframe style='display: block; margin: 100px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe>"
    "</body>"_s;

static constexpr ASCIILiteral selectPopupMenuTallMainframeHTML =
    "<body style='margin: 0; height: 2000px'>"
    "<iframe style='display: block; margin-left: 100px; margin-top: 500px; width: 400px; height: 300px; border: none;' src='https://domain2.com/subframe'></iframe>"
    "</body>"_s;

static constexpr ASCIILiteral selectPopupMenuSubframeHTML =
    "<body style='margin: 0'>"
    "<select style='appearance: none; position: absolute; left: 50px; top: 50px; width: 100px; height: 30px; margin: 0; padding: 0; border: none; box-sizing: border-box;'>"
    "<option>Alpha</option>"
    "<option>Bravo</option>"
    "</select>"
    "</body>"_s;

static constexpr ASCIILiteral selectPopupMenuTallSubframeHTML =
    "<body style='margin: 0; height: 2000px'>"
    "<select style='appearance: none; position: absolute; left: 50px; top: 550px; width: 100px; height: 30px; margin: 0; padding: 0; border: none; box-sizing: border-box;'>"
    "<option>Alpha</option>"
    "<option>Bravo</option>"
    "</select>"
    "</body>"_s;

TEST(SiteIsolation, SelectPopupMenuLocationInCrossSiteIframe)
{
    auto rect = selectPopupMenuRectInCrossSiteIframe(selectPopupMenuMainframeHTML, selectPopupMenuSubframeHTML);
    EXPECT_EQ(rect, NSMakeRect(150, 150, 100, 30));
}

TEST(SiteIsolation, SelectPopupMenuLocationInScrolledCrossSiteIframe)
{
    auto rect = selectPopupMenuRectInCrossSiteIframe(selectPopupMenuMainframeHTML, selectPopupMenuTallSubframeHTML, 0, 500);
    EXPECT_EQ(rect, NSMakeRect(150, 150, 100, 30));
}

TEST(SiteIsolation, SelectPopupMenuLocationInCrossSiteIframeWithScrolledMainFrame)
{
    auto rect = selectPopupMenuRectInCrossSiteIframe(selectPopupMenuTallMainframeHTML, selectPopupMenuSubframeHTML, 400, 0);
    EXPECT_EQ(rect, NSMakeRect(150, 150, 100, 30));
}

TEST(SiteIsolation, SelectPopupMenuLocationInScrolledCrossSiteIframeWithScrolledMainFrame)
{
    auto rect = selectPopupMenuRectInCrossSiteIframe(selectPopupMenuTallMainframeHTML, selectPopupMenuTallSubframeHTML, 400, 500);
    EXPECT_EQ(rect, NSMakeRect(150, 150, 100, 30));
}

const ASCIILiteral contextMenuLocationTestMainPage =
    "<body style='margin: 0'>"_s
    "  <iframe style='margin: 100px; width: 300px; height: 300px;' src='https://webkit.org/iframe'></iframe>"_s
    "</body>"_s;

const ASCIILiteral contextMenuLocationTestScrolledMainPage =
    "<body style='margin: 0'>"_s
    "  <div style='height: 1000px'></div>"_s
    "  <iframe style='margin: 100px; width: 300px; height: 300px;' src='https://webkit.org/iframe'></iframe>"_s
    "  <div style='height: 1000px'></div>"_s
    "</body>"_s;

const ASCIILiteral contextMenuLocationTestIframePage =
    "<p style='font-size: 100px'>Iframe</p>"_s
    "<script>onload = alert('loaded');</script>"_s;

const ASCIILiteral contextMenuLocationTestScrolledIframePage =
    "<div style='height: 1000px'></div>"_s
    "<p style='font-size: 100px'>Iframe</p>"_s
    "<div style='height: 1000px'></div>"_s
    "<script>onload = alert('loaded');</script>"_s;

static void testContextMenuLocationInCrossSiteIframe(ASCIILiteral mainframeHTML, ASCIILiteral iframeHTML, int mainFrameScrollY = 0, int iframeScrollY = 0)
{
    // Invariant: subframe is at (100, 100) of the main frame, with size (300, 300)
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webViewBinding, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    RetainPtr webView = webViewBinding;

    __block bool done = false;
    __block NSPoint contextMenuActualLocation = NSZeroPoint;
    InstanceMethodSwizzler popUpSwizzler {
        NSMenu.class,
        NSSelectorFromString(@"_popUpContextMenu:withEvent:forView:"),
        imp_implementationWithBlock(^(id, NSMenu *, NSEvent *event, NSView *) {
            contextMenuActualLocation = [event locationInWindow];
            done = true;
        })
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    EXPECT_WK_STREQ("loaded", [webView _test_waitForAlert]);

    if (mainFrameScrollY)
        scrollFrameAndWait(webView, nil, mainFrameScrollY);
    if (iframeScrollY)
        scrollFrameAndWait(webView, [webView firstChildFrame], iframeScrollY);
    [webView waitForNextPresentationUpdate];

    // Right click on point (200, 200) in the main frame's view (Y flipped for window coordinate)
    // This should be the "Iframe" text inside the iframe.
    auto contextMenuExpectedLocation = NSMakePoint(200, 600 - 200);
    [webView rightClickAtPoint:contextMenuExpectedLocation];
    Util::run(&done);

    // Context menu should be shown where the right click is.
    EXPECT_EQ(contextMenuActualLocation, contextMenuExpectedLocation);
}

TEST(SiteIsolation, ContextMenuLocationInCrossSiteIframe)
{
    testContextMenuLocationInCrossSiteIframe(contextMenuLocationTestMainPage, contextMenuLocationTestIframePage);
}

TEST(SiteIsolation, ContextMenuLocationInScrolledCrossSiteIframe)
{
    testContextMenuLocationInCrossSiteIframe(contextMenuLocationTestMainPage, contextMenuLocationTestScrolledIframePage, 0, 1000);
}

TEST(SiteIsolation, ContextMenuLocationInCrossSiteIframeWithScrolledMainPage)
{
    testContextMenuLocationInCrossSiteIframe(contextMenuLocationTestScrolledMainPage, contextMenuLocationTestIframePage, 1000, 0);
}

TEST(SiteIsolation, ContextMenuLocationInScrolledCrossSiteIframeWithScrolledMainPage)
{
    testContextMenuLocationInCrossSiteIframe(contextMenuLocationTestScrolledMainPage, contextMenuLocationTestScrolledIframePage, 1000, 1000);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

static constexpr NSPoint airPlayPickerPointerInMainFrameView { 200, 340 };

static constexpr ASCIILiteral airPlayPickerMainframeHTML =
    "<body style='margin: 0'>"
    "<script>internals.setMockMediaPlaybackTargetPickerEnabled(true);</script>"
    "<div style='height: 200px'></div>"
    "<iframe style='display: block; margin-left: 120px; width: 320px; height: 240px; border: none;' src='https://domain2.com/subframe'></iframe>"
    "</body>"_s;

static constexpr ASCIILiteral airPlayPickerTallMainframeHTML =
    "<body style='margin: 0; height: 2000px'>"
    "<script>internals.setMockMediaPlaybackTargetPickerEnabled(true);</script>"
    "<div style='height: 600px'></div>"
    "<iframe style='display: block; margin-left: 120px; width: 320px; height: 240px; border: none;' src='https://domain2.com/subframe'></iframe>"
    "</body>"_s;

static constexpr ASCIILiteral airPlayPickerSubframeHTML =
    "<body style='margin: 0; width: 1000px; height: 1000px'>"
    "<video id='video' muted playsinline preload='auto' src='/video-with-audio.mp4'"
    " style='position: absolute; left: 60px; top: 300px; width: 200px; height: 150px'></video>"
    "<script>"
    "window.airPlayIsAvailable = false;"
    "internals.settings.setAllowsAirPlayForMediaPlayback(true);"
    "document.getElementById('video').addEventListener('webkitplaybacktargetavailabilitychanged', (event) => {"
    "    if (event.availability === 'available')"
    "        window.airPlayIsAvailable = true;"
    "}, true);"
    "</script>"
    "</body>"_s;

static NSPoint airPlayPickerAnchorInCrossSiteIframe(ASCIILiteral mainframeHTML, int mainFrameScrollY = 0, int subframeScrollX = 0, int subframeScrollY = 0)
{
    RetainPtr videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainframeHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, airPlayPickerSubframeHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto [webViewBinding, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    RetainPtr webView = webViewBinding;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.getElementById('video').readyState >= HTMLMediaElement.HAVE_METADATA" inFrame:childFrame.get()] boolValue];
    }));

    [webView objectByEvaluatingJavaScript:@"internals.setMockMediaPlaybackTargetPickerState('', 'DeviceAvailable')"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.airPlayIsAvailable" inFrame:childFrame.get()] boolValue];
    }));

    if (mainFrameScrollY)
        scrollFrameAndWait(webView.get(), nil, 0, mainFrameScrollY);
    if (subframeScrollX || subframeScrollY)
        scrollFrameAndWait(webView.get(), childFrame.get(), subframeScrollX, subframeScrollY);

    NSPoint pointerInWindow = [webView convertPoint:airPlayPickerPointerInMainFrameView toView:nil];
    [webView mouseEnterAtPoint:pointerInWindow];
    [webView mouseMoveToPoint:pointerInWindow withFlags:0];
    [webView waitForPendingMouseEvents];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('video').webkitShowPlaybackTargetPicker()" inFrame:childFrame.get()];

    NSPoint anchorInScreen = NSZeroPoint;
    EXPECT_TRUE(Util::waitFor([&] {
        RetainPtr anchor = [webView objectByCallingAsyncFunction:@"const rect = await internals.mockMediaPlaybackTargetPickerRect(); return { x: rect.x, y: rect.y };" withArguments:nil];
        anchorInScreen = NSMakePoint([[anchor objectForKey:@"x"] doubleValue], [[anchor objectForKey:@"y"] doubleValue]);
        return !NSEqualPoints(anchorInScreen, NSZeroPoint);
    }));

    NSRect anchorInWindow = [[webView window] convertRectFromScreen:NSMakeRect(anchorInScreen.x, anchorInScreen.y, 0, 0)];
    return [webView convertPoint:anchorInWindow.origin fromView:nil];
}

static void expectAnchoredAtPointer(NSPoint anchor)
{
    EXPECT_NEAR(anchor.x, airPlayPickerPointerInMainFrameView.x, 1);
    EXPECT_NEAR(anchor.y, airPlayPickerPointerInMainFrameView.y, 1);
}

TEST(SiteIsolation, AirPlayPickerLocationInCrossSiteIframe)
{
    expectAnchoredAtPointer(airPlayPickerAnchorInCrossSiteIframe(airPlayPickerMainframeHTML));
}

TEST(SiteIsolation, AirPlayPickerLocationInScrolledCrossSiteIframe)
{
    expectAnchoredAtPointer(airPlayPickerAnchorInCrossSiteIframe(airPlayPickerMainframeHTML, 0, 60, 300));
}

TEST(SiteIsolation, AirPlayPickerLocationInCrossSiteIframeWithScrolledMainFrame)
{
    expectAnchoredAtPointer(airPlayPickerAnchorInCrossSiteIframe(airPlayPickerTallMainframeHTML, 400));
}

TEST(SiteIsolation, AirPlayPickerLocationInScrolledCrossSiteIframeWithScrolledMainFrame)
{
    expectAnchoredAtPointer(airPlayPickerAnchorInCrossSiteIframe(airPlayPickerTallMainframeHTML, 400, 60, 300));
}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

TEST(SiteIsolation, AccessibilityTokenAfterPageNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/webkit"]]];
    [opened.navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [opened.webView hasRemoteAccessibilityChild];
    }));
}

TEST(SiteIsolation, CrossOriginIframeWithHorizontalOverflowWillHandleHorizontalScrollEvents)
{
    auto mainHTML = "<body style='margin:0'><iframe id='frame' src='https://webkit.org/iframe' style='width:300px;height:300px;border:none'></iframe></body>"_s;
    auto iframeHTML = "<body style='margin:0;width:2000px;overflow-x:scroll'><script>onload=()=>{alert('loaded')}</script></body>"_s;

    HTTPServer server({
        { "/main"_s, { mainHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");

    // Trigger a layout in the iframe's process so that
    // recomputeShortCircuitHorizontalWheelEventsState runs.
    [webView evaluateJavaScript:@"document.body.offsetHeight" inFrame:[webView firstChildFrame] completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return WKPageWillHandleHorizontalScrollEvents([webView _pageForTesting]);
    }));
}

TEST(SiteIsolation, CrossOriginIframeWithoutHorizontalOverflowCanShortCircuitHorizontalScrollEvents)
{
    auto mainHTML = "<body style='margin:0'><iframe id='frame' src='https://webkit.org/iframe' style='width:300px;height:300px;border:none'></iframe></body>"_s;
    auto iframeHTML = "<body style='margin:0'><script>onload=()=>{alert('loaded')}</script></body>"_s;

    HTTPServer server({
        { "/main"_s, { mainHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");

    // Trigger a layout in the iframe's process so that
    // recomputeShortCircuitHorizontalWheelEventsState runs.
    [webView evaluateJavaScript:@"document.body.offsetHeight" inFrame:[webView firstChildFrame] completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return !WKPageWillHandleHorizontalScrollEvents([webView _pageForTesting]);
    }));
}

// Hosted subtree nodes are removed without the frame-scoped pruning in commitTreeStateInternal(),
// stranding active entries that then fail a MESSAGE_CHECK. See rdar://175191840.
TEST(SiteIsolation, RemoveIframeWithActiveScrollProxyNodes)
{
    auto mainHTML = "<body style='margin:0'><iframe id='frame' src='https://webkit.org/iframe' style='width:300px;height:200px;border:none'></iframe></body>"_s;

    // Overflow on html and body keeps body a composited scroller; the clipped
    // composited banners are what give it overflow scroll proxy nodes.
    auto iframeHTML = "<style>"
        "  html { margin: 0; height: 100%; overflow: hidden auto; }"
        "  body { margin: 0; height: 100%; overflow: hidden auto; position: relative; }"
        "  #content { height: 3000px; }"
        "  .clipper { position: relative; overflow: hidden; width: 200px; height: 100px; border-radius: 8px; }"
        "  .banner { position: absolute; top: 10px; width: 150px; height: 50px;"
        "            background-color: green; will-change: transform; }"
        "</style>"
        "<div id='content'>"
        "  <div class='clipper'><div class='banner'></div></div>"
        "  <div class='clipper'><div class='banner'></div></div>"
        "</div>"
        "<script>onload=()=>{ document.body.scrollTop = 400; alert('loaded') }</script>"_s;

    HTTPServer server({
        { "/main"_s, { mainHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView _scrollingTreeIncludingNodeIDsAsText] containsString:@"(overflow scroll proxy nodes"]);

    [webView objectByEvaluatingJavaScript:@"document.getElementById('frame').remove();1"];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView _scrollingTreeIncludingNodeIDsAsText] containsString:@"(overflow scroll proxy nodes"]);

    // The UI process surviving to round-trip this is the real assertion.
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"'alive'"], "alive");
}

TEST(SiteIsolation, ContextMenuPasteInCrossOriginFrame)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div id='editor' contenteditable style='width: 200px; height: 100px'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    [NSPasteboard.generalPasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
    [NSPasteboard.generalPasteboard setString:@"hello" forType:NSPasteboardTypeString];

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 400));
    [webView loadURL:[NSURL URLWithString:@"https://example.com/example"]];
    [delegate waitForDidFinishNavigation];

    [webView sendClickAtPoint:NSMakePoint(50, 350)];
    [webView rightClick:NSMakePoint(50, 350) andSelectItemMatching:^BOOL(NSMenuItem *item) {
        return [item.title isEqualToString:@"Paste"];
    }];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.getElementById('editor').textContent" inFrame:[webView firstChildFrame]] isEqualToString:@"hello"];
    }));
}

TEST(SiteIsolation, ContextMenuKeyInCrossOriginFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<script>addEventListener('contextmenu', event => { event.preventDefault(); window.receivedContextMenu = true; })</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    [webView objectByEvaluatingJavaScript:@"addEventListener('contextmenu', event => { event.preventDefault(); window.receivedContextMenu = true; }); true"];

    [webView showContextMenuForSelection:nil];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"!!window.receivedContextMenu" inFrame:childFrame.get()] boolValue];
    }));
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.receivedContextMenu"] boolValue]);
}

TEST(SiteIsolation, ContextMenuCopyInUnfocusedCrossOriginFrame)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe><input id='input'>"_s } },
        { "/iframe"_s, { "<body style='margin: 0; font-size: 100px'>hello<script>addEventListener('mousedown', event => event.preventDefault())</script></body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 400));
    [webView loadURL:[NSURL URLWithString:@"https://example.com/example"]];
    [delegate waitForDidFinishNavigation];
    [webView objectByEvaluatingJavaScript:@"document.getElementById('input').focus(); true"];

    [NSPasteboard.generalPasteboard clearContents];
    [webView rightClick:NSMakePoint(50, 350) andSelectItemMatching:^BOOL(NSMenuItem *item) {
        return [item.title isEqualToString:@"Copy"];
    }];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString] isEqualToString:@"hello"];
    }));
    EXPECT_WK_STREQ("input", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);
}

// The process variant SPI reads process paths and entitlements TestWebKitAPI cannot see on iOS.

TEST(SiteIsolation, LockdownModeInheritedByCrossSiteIframeProcess)
{
    HTTPServer server(mainAndSubframeResponses(), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences.lockdownModeEnabled = YES;
    });

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    EXPECT_WK_STREQ([webView _webContentProcessVariantForFrame:nil], @"lockdown");
    EXPECT_WK_STREQ([webView _webContentProcessVariantForFrame:childFrame.get()._handle], @"lockdown");
}

TEST(SiteIsolation, EnhancedSecurityInheritedByCrossSiteIframeProcess)
{
    HTTPServer server(mainAndSubframeResponses(), HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = mainFrameOnlyPolicyViewAndDelegate(server, ^(WKWebpagePreferences *preferences) {
        preferences.securityRestrictionMode = WKSecurityRestrictionModeMaximizeCompatibility;
    });

    RetainPtr childFrame = loadAndWaitForCrossSiteChildFrame(webView.get(), navigationDelegate.get(), @"https://a.com/mainframe", @"b.com");

    EXPECT_WK_STREQ([webView _webContentProcessVariantForFrame:nil], @"security");
    EXPECT_WK_STREQ([webView _webContentProcessVariantForFrame:childFrame.get()._handle], @"security");
}

TEST(SiteIsolation, UndoAndRedoEditInCrossOriginIframeFromPlatformUndoManager)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = [webView firstChildFrame];
    insertTextInFrame(webView.get(), childFrame.get(), @"document.body", @"hello");
    EXPECT_WK_STREQ("hello", [webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()]);

    RetainPtr undoManager = [webView undoManager];
    EXPECT_TRUE(Util::waitFor([&] {
        return !![undoManager canUndo];
    }));

    [undoManager undo];
    EXPECT_TRUE(waitForTextContentInFrame(webView.get(), childFrame.get(), @"document.body", @""));

    EXPECT_TRUE(Util::waitFor([&] {
        return !![undoManager canRedo];
    }));

    [undoManager redo];
    EXPECT_TRUE(waitForTextContentInFrame(webView.get(), childFrame.get(), @"document.body", @"hello"));
}

TEST(SiteIsolation, UndoAfterCrossOriginIframeProcessCrashesDoesNotOfferRedo)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    insertTextInFrame(webView.get(), [webView firstChildFrame], @"document.body", @"hello");

    RetainPtr undoManager = [webView undoManager];
    EXPECT_TRUE(Util::waitFor([&] {
        return !![undoManager canUndo];
    }));

    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    kill(iframePID, SIGKILL);
    while (processStillRunning(iframePID))
        Util::spinRunLoop();

    // The step lives only in the process that just died, so the undo cannot happen. The command must not
    // move to the redo stack and enable Redo for an operation that would silently do nothing.
    [undoManager undo];
    EXPECT_FALSE([undoManager canRedo]);
}

TEST(SiteIsolation, CrossSiteIframeProcessesDoNotReportMainFrameScroll)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0; height: 5000px'>"
            "<iframe style='width: 300px; height: 200px; border: none' src='https://domain2.com/subframe'></iframe>"
            "<iframe style='width: 300px; height: 200px; border: none' src='https://domain3.com/subframe'></iframe>"
            "</body>"_s } },
        { "/subframe"_s, { "<body style='background-color: green'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegateWithoutSharedProcess(server, CGRectMake(0, 0, 800, 600));
    RetainPtr scrollCounter = adoptNS([SiteIsolationPageScrollCounter new]);
    webView.get().UIDelegate = scrollCounter.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr<NSArray<_WKFrameTreeNode *>> childFrames = [webView mainFrame].childFrames;
    EXPECT_EQ([childFrames count], 2u);

    auto pageScrollsForMainFrameScrollTo = [&](int y) {
        [scrollCounter setPageScrollCount:0];
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(0, %d)", y]];
        EXPECT_TRUE(Util::waitFor([&] {
            return [scrollCounter pageScrollCount] > 0;
        }));

        // Wait for the next presentation update since the main frame broadcasts its scroll position
        // to remote frame processes as part of the rendering update. Then wait for each remote
        // frame process to do some request/response IPC to make sure it's processed that update.
        [webView waitForNextPresentationUpdate];
        for (_WKFrameTreeNode *childFrame in childFrames.get())
            [webView objectByEvaluatingJavaScript:@"0" inFrame:childFrame.info];

        return [scrollCounter pageScrollCount];
    };

    EXPECT_EQ(pageScrollsForMainFrameScrollTo(100), 1u);
    EXPECT_EQ(pageScrollsForMainFrameScrollTo(300), 1u);
}

TEST(SiteIsolation, ValidateMenuItemInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // The item starts out disabled and can only become enabled when the web process that validates the
    // command replies. The main frame has no selection, so its process would report Copy as disabled.
    RetainPtr menu = adoptNS([NSMenu new]);
    [menu setAutoenablesItems:NO];
    RetainPtr item = adoptNS([NSMenuItem new]);
    [item setTarget:webView.get()];
    [item setAction:@selector(copy:)];
    [item setEnabled:NO];
    [menu addItem:item.get()];

    [webView validateUserInterfaceItem:item.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [item isEnabled];
    }));
}

TEST(SiteIsolation, ChangeFontAttributesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    RetainPtr converter = adoptNS([SiteIsolationUnderlineAttributeConverter new]);
    [webView changeAttributes:converter.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.queryCommandState('underline')" inFrame:childFrame.get()] boolValue];
    }));
}

TEST(SiteIsolation, TypingAttributesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable style='font-size: 37px'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 3)", _WKSelectionAttributeIsCaret);

    __block bool done = false;
    __block RetainPtr<NSDictionary> typingAttributes;
    [static_cast<id<NSTextInputClient_Async_Staging_44648564>>(webView.get()) typingAttributesWithCompletionHandler:^(NSDictionary<NSString *, id> *attributes) {
        typingAttributes = attributes;
        done = true;
    }];
    Util::run(&done);

    NSFont *font = [typingAttributes objectForKey:NSFontAttributeName];
    EXPECT_NOT_NULL(font);
    EXPECT_EQ(37, font.pointSize);
}

TEST(SiteIsolation, AttributedSubstringInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);

    __block bool done = false;
    __block RetainPtr<NSString> substring;
    [static_cast<id<NSTextInputClient_Async>>(webView.get()) attributedSubstringForProposedRange:NSMakeRange(0, 8) completionHandler:^(NSAttributedString *string, NSRange) {
        substring = string.string;
        done = true;
    }];
    Util::run(&done);

    EXPECT_WK_STREQ("subframe", substring.get());
}

TEST(SiteIsolation, SelectionGeometryInCrossOriginIframeUsesMainFrameCoordinates)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='position: absolute; left: 100px; top: 150px; width: 400px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body contenteditable style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);

    // The iframe is at (100, 150) in the main frame, so rects left relative to the iframe would be near the origin.
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);
    NSRect caretRect = NSZeroRect;
    EXPECT_TRUE(Util::waitFor([&] {
        caretRect = [webView _caretRectForTesting];
        return !NSIsEmptyRect(caretRect);
    }));
    EXPECT_NEAR(NSMinX(caretRect), 100, 2);
    EXPECT_NEAR(NSMinY(caretRect), 150, 2);

    auto selectTextAndWaitForSelectionBounds = [&] {
        [webView objectByEvaluatingJavaScript:@"getSelection().removeAllRanges()" inFrame:childFrame.get()];
        EXPECT_TRUE(Util::waitFor([&] {
            return ![webView _selectionRectsForTesting].count;
        }));
        [webView objectByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)" inFrame:childFrame.get()];
        NSRect bounds = NSZeroRect;
        EXPECT_TRUE(Util::waitFor([&] {
            bounds = NSZeroRect;
            RetainPtr<NSArray<NSValue *>> rects = [webView _selectionRectsForTesting];
            for (NSValue *rect in rects.get())
                bounds = NSUnionRect(bounds, rect.rectValue);
            return !NSIsEmptyRect(bounds);
        }));
        return bounds;
    };

    auto selectionBounds = selectTextAndWaitForSelectionBounds();
    EXPECT_NEAR(NSMinX(selectionBounds), 100, 2);
    EXPECT_NEAR(NSMinY(selectionBounds), 150, 2);

    [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 100)"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 100;
    }));
    [webView waitForNextPresentationUpdate];

    selectionBounds = selectTextAndWaitForSelectionBounds();
    EXPECT_NEAR(NSMinX(selectionBounds), 100, 2);
    EXPECT_NEAR(NSMinY(selectionBounds), 50, 2);
}

TEST(SiteIsolation, ChangeSpellingInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>teh</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // changeSpelling: reads the replacement from the sender's selected cell, like the spelling panel's guess list.
    [webView changeSpelling:[NSTextField labelWithString:@"the"]];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"the"];
    }));
}

static NSArray<NSTextCheckingResult *> *swizzledCheckStringReportingMisspelledWord(id, SEL, NSString *stringToCheck, NSRange, NSTextCheckingTypes types, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    if (!(types & NSTextCheckingTypeSpelling))
        return @[ ];

    NSRange misspelledRange = [stringToCheck rangeOfString:@"teh"];
    if (misspelledRange.location == NSNotFound)
        return @[ ];

    return @[ [NSTextCheckingResult spellCheckingResultWithRange:misspelledRange] ];
}

TEST(SiteIsolation, CheckSpellingInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>hello teh world</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringReportingMisspelledWord)
    };

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);

    // Check Spelling selects the next misspelled word after the selection.
    [webView checkSpelling:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"teh"];
    }));
}

TEST(SiteIsolation, CenterSelectionInVisibleAreaInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable style='margin: 0'><div style='height: 2000px'></div><span id='target'>target</span></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(target.firstChild, 0)", _WKSelectionAttributeIsCaret);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrame.get()] intValue]);

    // Only the iframe's own scroll position is checked; revealing the selection in ancestor frames that
    // live in other processes is a separate concern.
    [webView centerSelectionInVisibleArea:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrame.get()] intValue] > 0;
    }));
}

TEST(SiteIsolation, WriteSelectionToPasteboardInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // Services ask for the selection with a synchronous request per type. Plain text and web archive data
    // come from different messages, so check both. The main frame has no selection, so its process would
    // return nothing for either.
    RetainPtr stringPasteboard = [NSPasteboard pasteboardWithUniqueName];
    [webView writeSelectionToPasteboard:stringPasteboard.get() types:@[ WebCore::legacyStringPasteboardTypeSingleton() ]];
    EXPECT_WK_STREQ("subframe text", [stringPasteboard stringForType:WebCore::legacyStringPasteboardTypeSingleton()]);

    RetainPtr dataPasteboard = [NSPasteboard pasteboardWithUniqueName];
    [webView writeSelectionToPasteboard:dataPasteboard.get() types:@[ UTTypeWebArchive.identifier ]];
    EXPECT_GT([dataPasteboard dataForType:UTTypeWebArchive.identifier].length, 0U);

    [stringPasteboard releaseGlobally];
    [dataPasteboard releaseGlobally];
}

TEST(SiteIsolation, ReadSelectionFromPasteboardInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>original text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    RetainPtr pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard clearContents];
    [pasteboard setString:@"pasted text" forType:NSPasteboardTypeString];

    // This fails if the request goes to the main frame's process, which has no selection. It also fails if
    // the iframe's process isn't granted access to the pasteboard, in which case it reads nothing.
    EXPECT_TRUE([webView readSelectionFromPasteboard:pasteboard.get()]);
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"pasted text"];
    }));

    [pasteboard releaseGlobally];
}

#if ENABLE(ATTACHMENT_ELEMENT)

TEST(SiteIsolation, AttachmentIconInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><attachment title='main.txt' type='text/plain'></attachment><iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body><attachment title='subframe.txt' type='text/plain'></attachment></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // Use the internals-enabled plug-in to reach the attachment's user agent shadow tree, and point the data
    // store at the test HTTPS proxy, since _test_configurationWithTestPlugInClassName: doesn't set one up.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    [configuration _setAttachmentElementEnabled:YES];

    // A wide-layout attachment shows its icon in an <img> in its shadow tree, so a delivered icon is observable.
    // This has to be set on the configuration; WKWebView overwrites the preference from it at initialization.
    [configuration _setAttachmentWideLayoutEnabled:YES];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSString *iconIsLoadedScript = @"(() => {"
        "    const icon = internals.shadowRoot(document.querySelector('attachment'))?.getElementById('attachment-icon');"
        "    return !!icon && icon.src.startsWith('blob:');"
        "})()";

    // The main frame's attachment checks that icons are delivered at all, so a failure below is specific to the iframe.
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:iconIsLoadedScript] boolValue];
    }));

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:iconIsLoadedScript inFrame:childFrame.get()] boolValue];
    }));
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

TEST(SiteIsolation, DeviceScaleFactorChangeUpdatesCrossOriginIframeCompositingScale)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body><div style='will-change: transform; width: 100px; height: 100px; background: green'></div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrame = [webView firstChildFrame];

    NSString *layerTreeScript = @"internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_VISIBLE_RECTS)";

    [webView _setOverrideDeviceScaleFactor:3];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:layerTreeScript inFrame:childFrame.get()] containsString:@"(contentsScale 3.00)"];
    }));

    [webView _setOverrideDeviceScaleFactor:1];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:layerTreeScript inFrame:childFrame.get()] containsString:@"(contentsScale 1.00)"];
    }));
}

// Replies to a text checking request must go to the web process that made it. Only that process has the
// pending request; any other process drops the reply.

static unsigned synchronousTextCheckCount;
static RetainPtr<NSString> pendingExtendedCheckString;
static BlockPtr<void(NSInteger, NSArray<NSTextCheckingResult *> *)> pendingExtendedCheckCompletion;

static NSArray<NSTextCheckingResult *> *swizzledCheckStringCountingChecks(id, SEL, NSString *, NSRange, NSTextCheckingTypes, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    ++synchronousTextCheckCount;
    return @[ ];
}

static NSInteger swizzledRequestGrammarCheckingDeferringCompletion(id, SEL, NSString *stringToCheck, NSRange, NSString *, NSDictionary *, void (^completionHandler)(NSInteger, NSArray<NSTextCheckingResult *> *))
{
    pendingExtendedCheckString = stringToCheck;
    pendingExtendedCheckCompletion = makeBlockPtr(completionHandler);
    return 0;
}

// Types into the editable body of the frame (the main frame if nil), then replies to the extended proofreading
// request that follows with a grammar result the synchronous check didn't report. The web process that made the
// request responds by checking the paragraph again, so this returns whether another synchronous check arrives.
// Setup problems are reported as separate failures, so a bare false means the reply never reached the requester.
static bool extendedProofreadingReplyTriggersRecheck(TestWKWebView *webView, WKFrameInfo *frame)
{
    synchronousTextCheckCount = 0;
    pendingExtendedCheckString = nil;
    pendingExtendedCheckCompletion = nullptr;

    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body)" inFrame:frame];
    [(id<NSTextInputClient>)webView insertText:@"Let's go in then store\n" replacementRange:NSMakeRange(NSNotFound, 0)];
    if (!Util::waitFor([] { return !!pendingExtendedCheckCompletion; })) {
        ADD_FAILURE() << "No extended proofreading request was made";
        return false;
    }

    // Let every check caused by the insertion finish, so that any check after the reply is the re-check.
    [webView waitForNextPresentationUpdate];
    auto checkCountBeforeReply = synchronousTextCheckCount;

    NSRange phraseRange = [pendingExtendedCheckString rangeOfString:@"go in then"];
    if (phraseRange.location == NSNotFound) {
        ADD_FAILURE() << "The extended proofreading request didn't include the typed text: " << [pendingExtendedCheckString UTF8String];
        return false;
    }
    NSDictionary *detail = @{
        NSGrammarRange: [NSValue valueWithRange:NSMakeRange(0, phraseRange.length)],
        NSGrammarCorrections: @[ @"go in the" ],
    };
    auto completion = std::exchange(pendingExtendedCheckCompletion, nullptr);
    completion(0, @[ [NSTextCheckingResult grammarCheckingResultWithRange:phraseRange details:@[ detail ]] ]);

    return Util::waitFor([&] {
        return synchronousTextCheckCount > checkCountBeforeReply;
    });
}

TEST(SiteIsolation, ExtendedProofreadingReplyReachesCrossOriginIframe)
{
    HTTPServer server({
        { "/control"_s, { "<body contenteditable></body>"_s } },
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringCountingChecks)
    };
    InstanceMethodSwizzler requestGrammarCheckingSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(requestGrammarCheckingOfString:range:language:options:completionHandler:),
        reinterpret_cast<IMP>(swizzledRequestGrammarCheckingDeferringCompletion)
    };

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"ExtendedProofreadingEnabled", true);

    // Check the whole mechanism in a main frame first, so that a failure below can only be the routing of the reply.
    {
        auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/control"]]];
        [navigationDelegate waitForDidFinishNavigation];
        [webView _setContinuousSpellCheckingEnabledForTesting:YES];
        [webView _setGrammarCheckingEnabledForTesting:YES];
        [webView objectByEvaluatingJavaScript:@"document.body.focus()"];
        EXPECT_TRUE(extendedProofreadingReplyTriggersRecheck(webView.get(), nil));
    }

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView _setGrammarCheckingEnabledForTesting:YES];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:childFrame.get()];
    EXPECT_TRUE(extendedProofreadingReplyTriggersRecheck(webView.get(), childFrame.get()));

    pendingExtendedCheckCompletion = nullptr;
    pendingExtendedCheckString = nil;
}

// Point-based selection. The UI process hands these messages a point in web-view coordinates. The iOS
// selection gestures hit-test it and re-dispatch into the cross-origin iframe under it, so they work whether
// or not that iframe is focused; `CharacterIndexForPointAsync` resolves it in the focused frame's process.

TEST(SiteIsolation, CharacterIndexForPointInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);

    // characterIndexForPoint: takes a screen point, so undo what WebViewImpl will redo. WKWebView is
    // flipped on macOS, so its coordinates match the CSS pixels the helper reports.
    CGPoint pointInView = pointAtCharacterInIframe(webView.get(), childFrame.get(), 6);
    NSPoint pointInWindow = [webView convertPoint:NSPointFromCGPoint(pointInView) toView:nil];
    NSPoint point = [webView window] ? [[webView window] convertPointToScreen:pointInWindow] : pointInWindow;

    __block NSUInteger index = NSNotFound;
    __block bool done = false;
    [static_cast<id<NSTextInputClient_Async>>(webView.get()) characterIndexForPoint:point completionHandler:^(NSUInteger result) {
        index = result;
        done = true;
    }];
    Util::run(&done);

    // "w" is at offset 6 in the iframe's "hello world".
    EXPECT_EQ(6U, index);
}

#if HAVE(REDESIGNED_TEXT_CURSOR)

TEST(SiteIsolation, DictationCaretStateInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 8)", _WKSelectionAttributeIsCaret);

    auto isCaretBlinkingSuspendedInIframe = [&] {
        return [[webView objectByEvaluatingJavaScript:@"internals.isCaretBlinkingSuspended()" inFrame:childFrame.get()] boolValue];
    };
    ASSERT_FALSE(isCaretBlinkingSuspendedInIframe());

    [[NSNotificationCenter defaultCenter] postNotificationName:@"_NSTextInputContextDictationDidPauseNotification" object:nil];
    EXPECT_TRUE(Util::waitFor(isCaretBlinkingSuspendedInIframe));

    // Changing the caret animator type replaces the animator, and a new animator's blinking isn't suspended.
    [[NSNotificationCenter defaultCenter] postNotificationName:@"_NSTextInputContextDictationDidStartNotification" object:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return !isCaretBlinkingSuspendedInIframe();
    }));
}

#endif // HAVE(REDESIGNED_TEXT_CURSOR)

// The immediate-action (force-click) hit test starts in the main frame's process, which hands it to the process of a
// cross-origin iframe under the point. That process must answer, whether or not anything in it is focused, and the
// main frame's process must not act on the hit test it handed off. Once it answers, Look Up must be offered.

static constexpr auto mainFrameWithCrossOriginIframeAtTopLeft = "<body style='margin: 0'><iframe id='iframe' style='position: absolute; left: 0; top: 0; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;

static std::pair<RetainPtr<WKWebViewForTestingImmediateActions>, RetainPtr<TestNavigationDelegate>> immediateActionWebViewWithCrossOriginIframe(const HTTPServer& server)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[WKWebViewForTestingImmediateActions alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

// Focuses the iframe, so that its process answers the immediate-action hit test even without the fix for
// ImmediateActionInUnfocusedCrossOriginIframe.
static RetainPtr<WKFrameInfo> focusCrossOriginIframe(TestWKWebView *webView)
{
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    RetainPtr childFrame = [webView firstChildFrame];
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }
    return childFrame;
}

TEST(SiteIsolation, ImmediateActionInUnfocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_FALSE([childFrame _isFocused]);

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);
    EXPECT_TRUE([[hitTestResult frameInfo]._handle isEqual:[childFrame _handle]]);
}

TEST(SiteIsolation, ImmediateActionInCrossOriginIframeDoesNotDispatchForceWillBeginInParent)
{
    static constexpr auto countForceWillBegin = "<script>window.forceWillBeginCount = 0; addEventListener('webkitmouseforcewillbegin', () => window.forceWillBeginCount++, true);</script>"_s;
    HTTPServer server({
        { "/mainframe"_s, { makeString(countForceWillBegin, mainFrameWithCrossOriginIframeAtTopLeft) } },
        { "/iframe"_s, { makeString(countForceWillBegin, "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s) } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);

    RetainPtr childFrame = focusCrossOriginIframe(webView.get());

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);

    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"window.forceWillBeginCount" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"window.forceWillBeginCount"] intValue]);
}

TEST(SiteIsolation, ImmediateActionOffersLookUpInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    focusCrossOriginIframe(webView.get());

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);
    EXPECT_NOT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionLookupText);
}

// Geometry in an immediate-action hit test result must be in the main frame's coordinates, like it is without site
// isolation, so the UI process can anchor link previews and highlights to it.
TEST(SiteIsolation, ImmediateActionElementBoundingBoxInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='position: absolute; left: 100px; top: 100px; width: 300px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body style='margin: 0'><div id='text' style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    RetainPtr childFrame = [webView firstChildFrame];

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(116, 116)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);

    // The hit is on the text node, so the box is the text's.
    RetainPtr textRect = [webView objectByEvaluatingJavaScript:@"(() => { const range = document.createRange(); range.selectNodeContents(document.getElementById('text').firstChild); const rect = range.getBoundingClientRect(); return [rect.x, rect.y, rect.width, rect.height]; })()" inFrame:childFrame.get()];
    CGRect boundingBox = [hitTestResult elementBoundingBox];
    EXPECT_NEAR(CGRectGetMinX(boundingBox), 100 + [[textRect objectAtIndex:0] doubleValue], 1);
    EXPECT_NEAR(CGRectGetMinY(boundingBox), 100 + [[textRect objectAtIndex:1] doubleValue], 1);
    EXPECT_NEAR(CGRectGetWidth(boundingBox), [[textRect objectAtIndex:2] doubleValue], 1);
    EXPECT_NEAR(CGRectGetHeight(boundingBox), [[textRect objectAtIndex:3] doubleValue], 1);
}

// The animation can begin before the hit test's reply arrives, in which case the UI process waits for it. When the
// hit is in a cross-origin iframe, the reply that matters comes from the iframe's process, not the main frame's.
TEST(SiteIsolation, ImmediateActionAnimationBeginsBeforeCrossOriginIframeAnswers)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);

    auto [hitTestResult, actionType] = [webView simulateImmediateActionBeginningAnimationImmediately:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);
    EXPECT_NOT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionLookupText);
}

// Pressing or releasing a modifier key while the mouse is still re-runs the hover hit test. Over a cross-origin
// iframe, it must report what's under the mouse in the iframe, not the main frame's <iframe> element.
TEST(SiteIsolation, ModifierKeyChangeOverLinkInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><a href='https://webkit.org/destination' style='display: block; width: 400px; height: 300px;'>link label</a></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto linkLocation = NSMakePoint(200, 150);
    LocalEventMonitorSwizzler localMonitorSwizzler;
    // The flags-changed monitor takes the mouse location from the window, which a test can't move.
    InstanceMethodSwizzler mouseLocationSwizzler {
        NSWindow.class,
        @selector(mouseLocationOutsideOfEventStream),
        imp_implementationWithBlock(^{
            return linkLocation;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 300));
    struct {
        RetainPtr<_WKHitTestResult> hitTestResult;
        NSEventModifierFlags flags { 0 };
    } lastHover;
    auto* lastHoverPointer = &lastHover;
    RetainPtr uiDelegate = adoptNS([SiteIsolationMouseMoveOverElementDelegate new]);
    [uiDelegate setMouseDidMoveOverElement:^(_WKHitTestResult *hitTestResult, NSEventModifierFlags flags) {
        lastHoverPointer->hitTestResult = hitTestResult;
        lastHoverPointer->flags = flags;
    }];
    [webView setUIDelegate:uiDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView _createFlagsChangedEventMonitorForTesting];

    [webView mouseMoveToPoint:linkLocation withFlags:0];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[lastHover.hitTestResult absoluteLinkURL].absoluteString isEqualToString:@"https://webkit.org/destination"];
    }));

    lastHover.hitTestResult = nil;
    localMonitorSwizzler.sendEventToMonitor([NSEvent mouseEventWithType:NSEventTypeMouseMoved location:linkLocation modifierFlags:NSEventModifierFlagCommand timestamp:0 windowNumber:[[webView hostWindow] windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0]);
    EXPECT_TRUE(Util::waitFor([&] {
        return lastHover.hitTestResult && (lastHover.flags & NSEventModifierFlagCommand);
    }));
    EXPECT_WK_STREQ("https://webkit.org/destination", [lastHover.hitTestResult absoluteLinkURL].absoluteString);
}

// Clicking a background window only activates it, unless the click lands on something it can act on right away, like
// a selection it can start dragging. The focused frame's process answers that, and it has to hand the question on when
// the click is over a frame in another process.

struct InactiveWebViewWithSelectionInCrossOriginIframe {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<NSWindow> window;
    NSPoint selectionCenterInWindow;
};

static InactiveWebViewWithSelectionInCrossOriginIframe inactiveWebViewWithSelectionInFocusedCrossOriginIframe(const HTTPServer& server)
{
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 300));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr childFrame = focusCrossOriginIframe(webView.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.getElementById('text'))", _WKSelectionAttributeIsRange);

    // The iframe is at the top left of the view, so its coordinates are the view's. The window's are flipped.
    RetainPtr center = [webView objectByEvaluatingJavaScript:@"(() => { const rect = getSelection().getRangeAt(0).getBoundingClientRect(); return [rect.x + rect.width / 2, rect.y + rect.height / 2]; })()" inFrame:childFrame.get()];
    auto selectionCenterInWindow = NSMakePoint([[center objectAtIndex:0] doubleValue], NSHeight([webView frame]) - [[center objectAtIndex:1] doubleValue]);

    // These questions are only asked when the window isn't key, and TestWKWebView's own window always is.
    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([window isKeyWindow]);

    return { WTF::move(webView), WTF::move(navigationDelegate), WTF::move(window), selectionCenterInWindow };
}

static RetainPtr<NSEvent> firstMouseDownEvent(NSWindow *window, NSPoint location)
{
    return [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:location modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil eventNumber:1 clickCount:1 pressure:1];
}

static constexpr auto largeTextAtTopLeft = "<body style='margin: 0'><div id='text' style='font-size: 100px;'>Foobar</div></body>"_s;

TEST(SiteIsolation, AcceptsFirstMouseOverSelectionInFocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { largeTextAtTopLeft } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, window, selectionCenter] = inactiveWebViewWithSelectionInFocusedCrossOriginIframe(server);
    EXPECT_TRUE([webView acceptsFirstMouse:firstMouseDownEvent(window.get(), selectionCenter).get()]);
}

TEST(SiteIsolation, ShouldDelayWindowOrderingOverSelectionInFocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { largeTextAtTopLeft } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, window, selectionCenter] = inactiveWebViewWithSelectionInFocusedCrossOriginIframe(server);
    EXPECT_TRUE([webView shouldDelayWindowOrderingForEvent:firstMouseDownEvent(window.get(), selectionCenter).get()]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
