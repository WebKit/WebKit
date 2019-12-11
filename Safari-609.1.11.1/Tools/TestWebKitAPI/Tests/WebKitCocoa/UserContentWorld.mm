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

TEST(UserContentWorld, NormalWorld)
{
    RetainPtr<WKUserScript> basicUserScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
    EXPECT_EQ([basicUserScript _userContentWorld], [_WKUserContentWorld normalWorld]);
    EXPECT_NULL([basicUserScript _userContentWorld].name);
}

TEST(UserContentWorld, NormalWorldUserScript)
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

TEST(UserContentWorld, IsolatedWorld)
{
    RetainPtr<_WKUserContentWorld> isolatedWorld = [_WKUserContentWorld worldWithName:@"TestWorld"];
    EXPECT_WK_STREQ([isolatedWorld name], @"TestWorld");

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:isolatedWorld.get()]);
    EXPECT_EQ([userScript _userContentWorld], isolatedWorld.get());
}

TEST(UserContentWorld, IsolatedWorldUserScript)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<_WKUserContentWorld> isolatedWorld = [_WKUserContentWorld worldWithName:@"TestWorld"];
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.setFromUserScript = true;" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:isolatedWorld.get()]);
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

@interface UserContentWorldRemoteObject : NSObject <UserContentWorldProtocol>
@end

@implementation UserContentWorldRemoteObject

- (void)didObserveNormalWorld
{
    didObserveNormalWorld = true;
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
    
    RetainPtr<_WKUserContentWorld> isolatedWorld = [_WKUserContentWorld worldWithName:@"TestWorld"];
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.setFromUserScript = true;" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:isolatedWorld.get()]);
    [[configuration userContentController] addUserScript:userScript.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<UserContentWorldRemoteObject> object = adoptNS([[UserContentWorldRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(UserContentWorldProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];

    TestWebKitAPI::Util::run(&didObserveNormalWorld);
    TestWebKitAPI::Util::run(&didObserveWorldWithName);
}
