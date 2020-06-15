/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "UserContentWorldProtocol.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserScriptPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <WebKit/_WKUserContentWorld.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

TEST(ContentWorld, NormalWorld)
{
    RetainPtr<WKUserScript> basicUserScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
    EXPECT_NE([basicUserScript _userContentWorld], [_WKUserContentWorld normalWorld]);
    EXPECT_WK_STREQ([basicUserScript _userContentWorld].name, [_WKUserContentWorld normalWorld].name);
    EXPECT_NULL([basicUserScript _userContentWorld].name);
}

TEST(ContentWorld, NormalWorldUserScript)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"window.setFromUserScript = true;" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
    [[configuration userContentController] addUserScript:userScript.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool isDone = false;
    [webView evaluateJavaScript:@"window.setFromUserScript ? 'variable accessible' : 'variable inaccessible';" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(result, @"variable accessible");

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(ContentWorld, IsolatedWorld)
{
    RetainPtr<WKContentWorld> isolatedWorld = [WKContentWorld worldWithName:@"TestWorld"];
    EXPECT_WK_STREQ([isolatedWorld name], @"TestWorld");

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:isolatedWorld.get() deferRunningUntilNotification:NO]);
    EXPECT_EQ([userScript _contentWorld], isolatedWorld.get());
    EXPECT_WK_STREQ([userScript _userContentWorld].name, [isolatedWorld name]);
}

TEST(ContentWorld, IsolatedWorldUserScript)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<WKContentWorld> isolatedWorld = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.setFromUserScript = true;" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:isolatedWorld.get() deferRunningUntilNotification:NO]);
    [[configuration userContentController] addUserScript:userScript.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool isDone = false;
    [webView evaluateJavaScript:@"window.setFromUserScript ? 'variable accessible' : 'variable inaccessible';" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(result, @"variable inaccessible");

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

static bool didObserveNormalWorld;
static bool didObserveWorldWithName;
static bool didObserveMainFrame;
static bool didObserveSubframe;

@interface UserContentWorldRemoteObject : NSObject <UserContentWorldProtocol>
@end

@implementation UserContentWorldRemoteObject

- (void)didObserveNormalWorld
{
    didObserveNormalWorld = true;
}

- (void)didObserveMainFrame
{
    didObserveMainFrame = true;
}

- (void)didObserveSubframe
{
    didObserveSubframe = true;
}

- (void)didObserveWorldWithName:(NSString *)name
{
    EXPECT_WK_STREQ(@"TestWorld", name);
    didObserveWorldWithName = true;
}

@end

TEST(UserContentWorld, IsolatedWorldPlugIn)
{
    NSString * const testPlugInClassName = @"UserContentWorldPlugIn";

    RetainPtr<WKWebViewConfiguration> configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);
    
    RetainPtr<WKContentWorld> isolatedWorld = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.setFromUserScript = true;" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:isolatedWorld.get() deferRunningUntilNotification:NO]);
    [[configuration userContentController] addUserScript:userScript.get()];
    RetainPtr<TestURLSchemeHandler> schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"testscheme"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<UserContentWorldRemoteObject> object = adoptNS([[UserContentWorldRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(UserContentWorldProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    schemeHandler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *body;
        if ([task.request.URL.path isEqualToString:@"/mainresource"])
            body = @"<body style='background-color: red;'><iframe src='/iframesrc'></iframe></body>";
        else {
            EXPECT_WK_STREQ(task.request.URL.path, "/iframesrc");
            body = @"iframe content";
        }
        [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:body.length textEncodingName:nil] autorelease]];
        [task didReceiveData:[body dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme:///mainresource"]]];

    TestWebKitAPI::Util::run(&didObserveNormalWorld);
    TestWebKitAPI::Util::run(&didObserveWorldWithName);
    TestWebKitAPI::Util::run(&didObserveMainFrame);
    TestWebKitAPI::Util::run(&didObserveSubframe);
}
