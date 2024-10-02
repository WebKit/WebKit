/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContentWorld.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKScriptMessage.h>
#import <WebKit/WKScriptMessageHandlerWithReply.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserScript.h>
#import <WebKit/WKUserScriptPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserContentWorld.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>

static bool isDoneWithNavigation;

@interface SimpleNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation SimpleNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDoneWithNavigation = true;
}

@end

static Vector<RetainPtr<WKScriptMessage>> scriptMessagesVector;

@interface ScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation ScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessagesVector.append(message);
}

@end

TEST(WKUserContentController, ScriptMessageHandlerBasicPost)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.webkit.messageHandlers.testHandler.postMessage('Hello')" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"Hello", (NSString *)[scriptMessagesVector[0] body]);
}

TEST(WKUserContentController, ScriptMessageHandlerBasicPostIsolatedWorld)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.webkit.messageHandlers.testHandler.postMessage('Hello')" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:world.get() deferRunningUntilNotification:NO]);

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] _addScriptMessageHandler:handler.get() name:@"testHandler" contentWorld:world.get()];
    [[configuration userContentController] addUserScript:userScript.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<SimpleNavigationDelegate> delegate = adoptNS([[SimpleNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];

    isDoneWithNavigation = false;
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"Hello", (NSString *)[scriptMessagesVector[0] body]);

    if (!isDoneWithNavigation)
        TestWebKitAPI::Util::run(&isDoneWithNavigation);

    __block bool isDoneEvaluatingScript = false;
    __block RetainPtr<NSString> resultValue = @"";
    [webView evaluateJavaScript:
        @"var result;"
         "if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.testHandler) {"
         "    result = { 'result': 'FAIL' };"
         "} else {"
         "    result = { 'result': 'PASS' };"
         "} " 
         "result;"
         completionHandler:^(id value, NSError *error) {
            resultValue = ((NSDictionary *)value)[@"result"];
            isDoneEvaluatingScript = true;
        }];

    TestWebKitAPI::Util::run(&isDoneEvaluatingScript);

    EXPECT_WK_STREQ(@"PASS", resultValue.get());
}

TEST(WKUserContentController, ScriptMessageHandlerBasicRemove)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKUserContentController> userContentController = [configuration userContentController];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToRemove"];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToPost"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Test that handlerToRemove was succesfully added.
    [webView evaluateJavaScript:
        @"if (window.webkit.messageHandlers.handlerToRemove) {"
         "    window.webkit.messageHandlers.handlerToPost.postMessage('PASS');"
         "} else {"
         "    window.webkit.messageHandlers.handlerToPost.postMessage('FAIL');"
         "}" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessagesVector[0] body]);

    [userContentController removeScriptMessageHandlerForName:@"handlerToRemove"];

    // Test that handlerToRemove has been removed.
    [webView evaluateJavaScript:
        @"if (window.webkit.messageHandlers.handlerToRemove) {"
         "    window.webkit.messageHandlers.handlerToPost.postMessage('FAIL');"
         "} else {"
         "    window.webkit.messageHandlers.handlerToPost.postMessage('PASS');"
         "}" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessagesVector[0] body]);
}

TEST(WKUserContentController, ScriptMessageHandlerCallRemovedHandler)
{
    receivedScriptMessage = false;

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKUserContentController> userContentController = [configuration userContentController];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToRemove"];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToPost"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"var handlerToRemove = window.webkit.messageHandlers.handlerToRemove;" completionHandler:nil];

    [userContentController removeScriptMessageHandlerForName:@"handlerToRemove"];

    __block bool done = false;
    // Test that we throw an exception if you try to use a message handler that has been removed.
    [webView callAsyncJavaScript:@"return handlerToRemove.postMessage('FAIL')" arguments:nil inFrame:nil inContentWorld:[WKContentWorld pageWorld] completionHandler:^ (id value, NSError * error) {
        EXPECT_NULL(value);
        EXPECT_NOT_NULL(error);
        EXPECT_TRUE([[error description] containsString:@"InvalidAccessError"]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    receivedScriptMessage = false;
}

static RetainPtr<WKWebView> webViewForScriptMessageHandlerMultipleHandlerRemovalTest(WKWebViewConfiguration *configuration)
{
    receivedScriptMessage = false;

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configurationCopy = adoptNS([configuration copy]);
    [configurationCopy setUserContentController:adoptNS([[WKUserContentController alloc] init]).get()];
    [[configurationCopy userContentController] addScriptMessageHandler:handler.get() name:@"handlerToRemove"];
    [[configurationCopy userContentController] addScriptMessageHandler:handler.get() name:@"handlerToPost"];
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationCopy.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    return webView;
}

TEST(WKUserContentController, ScriptMessageHandlerMultipleHandlerRemoval)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<_WKProcessPoolConfiguration> processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [configuration setProcessPool:adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]).get()];

    RetainPtr<WKWebView> webView = webViewForScriptMessageHandlerMultipleHandlerRemovalTest(configuration.get());
    RetainPtr<WKWebView> webView2 = webViewForScriptMessageHandlerMultipleHandlerRemovalTest(configuration.get());

    [[[webView configuration] userContentController] removeScriptMessageHandlerForName:@"handlerToRemove"];
    [[[webView2 configuration] userContentController] removeScriptMessageHandlerForName:@"handlerToRemove"];

    [webView evaluateJavaScript:
     @"try {"
     "    handlerToRemove.postMessage('FAIL');"
     "} catch (e) {"
     "    window.webkit.messageHandlers.handlerToPost.postMessage('PASS');"
     "}" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessagesVector[0] body]);
}

#if !PLATFORM(IOS_FAMILY) // FIXME: hangs in the iOS simulator
TEST(WKUserContentController, ScriptMessageHandlerWithNavigation)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.webkit.messageHandlers.testHandler.postMessage('First Message')" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_WK_STREQ(@"First Message", (NSString *)[scriptMessagesVector[0] body]);
    
    receivedScriptMessage = false;

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.webkit.messageHandlers.testHandler.postMessage('Second Message')" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_WK_STREQ(@"Second Message", (NSString *)[scriptMessagesVector[1] body]);
}
#endif

TEST(WKUserContentController, ScriptMessageHandlerReplaceWithSameName)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKUserContentController> userContentController = [configuration userContentController];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToReplace"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Test that handlerToReplace was succesfully added.
    [webView evaluateJavaScript:@"window.webkit.messageHandlers.handlerToReplace.postMessage('PASS1');" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS1", (NSString *)[scriptMessagesVector[0] body]);

    [userContentController removeScriptMessageHandlerForName:@"handlerToReplace"];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToReplace"];

    // Test that handlerToReplace still works.
    [webView evaluateJavaScript:@"window.webkit.messageHandlers.handlerToReplace.postMessage('PASS2');" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS2", (NSString *)[scriptMessagesVector[1] body]);
}

static NSString *styleSheetSource = @"body { background-color: green !important; }";
static NSString *backgroundColorScript = @"window.getComputedStyle(document.body, null).getPropertyValue('background-color')";
static NSString *frameBackgroundColorScript = @"window.getComputedStyle(document.getElementsByTagName('iframe')[0].contentDocument.body, null).getPropertyValue('background-color')";
static NSString *styleSheetSourceForSpecificityLevelTests = @"#body { background-color: green; }";
static const char* greenInRGB = "rgb(0, 128, 0)";
static const char* redInRGB = "rgb(255, 0, 0)";
static const char* whiteInRGB = "rgba(0, 0, 0, 0)";
static const char* blueInRGB = "rgb(0, 0, 255)";

static void expectScriptEvaluatesToColor(WKWebView *webView, NSString *script, const char* color)
{
    static bool didCheckBackgroundColor;

    [webView evaluateJavaScript:script completionHandler:^ (id value, NSError * error) {
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(color, value);
        didCheckBackgroundColor = true;
    }];

    TestWebKitAPI::Util::run(&didCheckBackgroundColor);
    didCheckBackgroundColor = false;
}

TEST(WKUserContentController, AddUserStyleSheetBeforeCreatingView)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleUserLevel contentWorld:nil]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);
}

TEST(WKUserContentController, NonCanonicalizedURL)
{
    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:adoptNS([[NSURL alloc] initWithString:@"http://CamelCase/"]).get() level:_WKUserStyleUserLevel contentWorld:world.get()]);
}

TEST(WKUserContentController, AddUserStyleSheetAfterCreatingView)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:YES]);
    [[webView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlyMainFrame)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'><iframe></iframe></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // The main frame should be affected.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    // The subframe shouldn't be affected.
    expectScriptEvaluatesToColor(webView.get(), frameBackgroundColorScript, whiteInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingAllFrames)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'><iframe></iframe></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // The main frame should be affected.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    // The subframe should also be affected.
    expectScriptEvaluatesToColor(webView.get(), frameBackgroundColorScript, greenInRGB);
}

TEST(WKUserContentController, UserStyleSheetRemove)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    [userContentController _addUserStyleSheet:styleSheet.get()];
    
    EXPECT_EQ(1u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);

    [userContentController _removeUserStyleSheet:styleSheet.get()];

    EXPECT_EQ(0u, [userContentController _userStyleSheets].count);
}

TEST(WKUserContentController, UserStyleSheetRemoveAll)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:nil level:_WKUserStyleUserLevel contentWorld:world.get()]);
    
    [userContentController _addUserStyleSheet:styleSheet.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld.get()];
    
    EXPECT_EQ(2u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][1]);

    [userContentController _removeAllUserStyleSheets];

    EXPECT_EQ(0u, [userContentController _userStyleSheets].count);
}

TEST(WKUserContentController, UserStyleSheetRemoveAllByWorld)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:nil level:_WKUserStyleUserLevel contentWorld:world.get()]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld2 = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:nil level:_WKUserStyleUserLevel contentWorld:world.get()]);
    
    [userContentController _addUserStyleSheet:styleSheet.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][1]);
    EXPECT_EQ(styleSheetAssociatedWithWorld2.get(), [userContentController _userStyleSheets][2]);

    [userContentController _removeAllUserStyleSheetsAssociatedWithContentWorld:world.get()];

    EXPECT_EQ(1u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
}

TEST(WKUserContentController, UserStyleSheetRemoveAllByNormalWorld)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:nil level:_WKUserStyleUserLevel contentWorld:world.get()]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld2 = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:nil forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:nil level:_WKUserStyleUserLevel contentWorld:world.get()]);
    
    [userContentController _addUserStyleSheet:styleSheet.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][1]);
    EXPECT_EQ(styleSheetAssociatedWithWorld2.get(), [userContentController _userStyleSheets][2]);

    [userContentController _removeAllUserStyleSheetsAssociatedWithContentWorld:[WKContentWorld pageWorld]];

    EXPECT_EQ(2u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld2.get(), [userContentController _userStyleSheets][1]);
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlySpecificWebView)
{
    RetainPtr<WKWebView> targetWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<WKWebView> otherWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [targetWebView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [targetWebView _test_waitForDidFinishNavigation];

    [otherWebView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [otherWebView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(targetWebView.get(), backgroundColorScript, redInRGB);
    expectScriptEvaluatesToColor(otherWebView.get(), backgroundColorScript, redInRGB);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    auto styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:targetWebView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[targetWebView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(targetWebView.get(), backgroundColorScript, greenInRGB);
    expectScriptEvaluatesToColor(otherWebView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingSpecificWebViewInjectionImmediatelyAfterCreation)
{
    RetainPtr<WKWebView> targetWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [targetWebView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:targetWebView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[targetWebView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    [targetWebView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(targetWebView.get(), backgroundColorScript, greenInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingSpecificWebViewInjectionAndRemovalImmediatelyAfterCreation)
{
    RetainPtr<WKWebView> targetWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [targetWebView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:targetWebView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[targetWebView configuration].userContentController _addUserStyleSheet:styleSheet.get()];
    [[targetWebView configuration].userContentController _removeUserStyleSheet:styleSheet.get()];

    [targetWebView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(targetWebView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlySpecificWebViewSharedConfiguration)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<WKWebView> targetWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> otherWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [targetWebView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [targetWebView _test_waitForDidFinishNavigation];

    [otherWebView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [otherWebView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(targetWebView.get(), backgroundColorScript, redInRGB);
    expectScriptEvaluatesToColor(otherWebView.get(), backgroundColorScript, redInRGB);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:targetWebView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[targetWebView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(targetWebView.get(), backgroundColorScript, greenInRGB);
    expectScriptEvaluatesToColor(otherWebView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlySpecificWebViewRemoval)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:webView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[webView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    [[webView configuration].userContentController _removeUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlySpecificWebViewRemovalAfterMultipleAdditions)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:webView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[webView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    [[webView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    [[webView configuration].userContentController _removeUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlySpecificWebViewRemovalAfterNavigation)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:webView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[webView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    [webView loadHTMLString:@"<body style='background-color: blue;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, blueInRGB);
}

static RetainPtr<_WKProcessPoolConfiguration> psonProcessPoolConfiguration()
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
    return processPoolConfiguration;
}

TEST(WKUserContentController, UserStyleSheetAffectingOnlySpecificWebViewRemovalAfterNavigationPSON)
{
    auto processPoolConfiguration = psonProcessPoolConfiguration();
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forWKWebView:webView.get() forMainFrameOnly:YES includeMatchPatternStrings:nil excludeMatchPatternStrings:nil baseURL:nil level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[webView configuration].userContentController _addUserStyleSheet:styleSheet.get()];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);

    [webView loadHTMLString:@"<body style='background-color: blue;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, blueInRGB);
}

TEST(WKUserContentController, UserStyleSheetSpecificityLevelNotSpecified)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSourceForSpecificityLevelTests forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"<style>body { background-color: red }</style><body id='body'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // By default, the injected style sheet will not be able to override due to the "user" level being used.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetSpecificityLevelUser)
{
    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSourceForSpecificityLevelTests forWKWebView:nil forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:[[NSURL alloc] initWithString:@"http://example.com/"] level:_WKUserStyleUserLevel contentWorld:world.get()]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"<style>body { background-color: red }</style><body id='body'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // The injected style sheet will not be able to override due to the "user" level being used.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, redInRGB);
}

TEST(WKUserContentController, UserStyleSheetSpecificityLevelAuthor)
{
    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSourceForSpecificityLevelTests forWKWebView:nil forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] baseURL:[[NSURL alloc] initWithString:@"http://example.com/"] level:_WKUserStyleAuthorLevel contentWorld:world.get()]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"<style>body { background-color: red }</style><body id='body'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // The injected style sheet *will* be able to override due to the "author" level being used.
    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);
}

TEST(WKUserContentController, UserScriptRemove)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    [userContentController addUserScript:userScript.get()];
    
    EXPECT_EQ(1u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);

    [userContentController _removeUserScript:userScript.get()];

    EXPECT_EQ(0u, [userContentController userScripts].count);
}

TEST(WKUserContentController, UserScriptRemoveAll)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    auto userScriptAssociatedWithWorld = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:world.get() deferRunningUntilNotification:NO]);

    [userContentController addUserScript:userScript.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld.get()];
    
    EXPECT_EQ(2u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][1]);

    [userContentController removeAllUserScripts];

    EXPECT_EQ(0u, [userContentController userScripts].count);
}

TEST(WKUserContentController, UserScriptRemoveAllByWorld)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:world.get() deferRunningUntilNotification:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld2 = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:world.get() deferRunningUntilNotification:NO]);
    
    [userContentController addUserScript:userScript.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][1]);
    EXPECT_EQ(userScriptAssociatedWithWorld2.get(), [userContentController userScripts][2]);

    [userContentController _removeAllUserScriptsAssociatedWithContentWorld:world.get()];

    EXPECT_EQ(1u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
}

TEST(WKUserContentController, UserScriptRemoveAllByNormalWorld)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:world.get() deferRunningUntilNotification:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld2 = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:world.get() deferRunningUntilNotification:NO]);
    
    [userContentController addUserScript:userScript.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][1]);
    EXPECT_EQ(userScriptAssociatedWithWorld2.get(), [userContentController userScripts][2]);

    [userContentController _removeAllUserScriptsAssociatedWithContentWorld:[WKContentWorld pageWorld]];

    EXPECT_EQ(2u, [userContentController userScripts].count);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld2.get(), [userContentController userScripts][1]);
}

static void waitForMessages(size_t expectedCount)
{
    while (scriptMessagesVector.size() < expectedCount) {
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
    }
}

static void compareMessages(Vector<const char*>&& expectedMessages)
{
    EXPECT_EQ(expectedMessages.size(), scriptMessagesVector.size());
    for (size_t i = 0; i < expectedMessages.size(); ++i)
        EXPECT_STREQ([[scriptMessagesVector[i] body] UTF8String], expectedMessages[i]);
}

TEST(WKUserContentController, InjectUserScriptImmediately)
{
    scriptMessagesVector.clear();
    receivedScriptMessage = false;

    auto handler = adoptNS([[ScriptMessageHandler alloc] init]);
    auto startAllFrames = adoptNS([[WKUserScript alloc] initWithSource:@"window.webkit.messageHandlers.testHandler.postMessage('start all')" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    auto endMainFrameOnly = adoptNS([[WKUserScript alloc] initWithSource:@"window.webkit.messageHandlers.testHandler.postMessage('end main')" injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[SimpleNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple-iframe" withExtension:@"html"]];

    isDoneWithNavigation = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDoneWithNavigation);

    receivedScriptMessage = false;
    [[configuration userContentController] _addUserScriptImmediately:startAllFrames.get()];
    // simple-iframe.html has a main frame and one iframe.
    waitForMessages(2);
    [[configuration userContentController] _addUserScriptImmediately:endMainFrameOnly.get()];
    waitForMessages(3);
    [webView reload];
    waitForMessages(6);
    compareMessages({"start all", "start all", "end main", "start all", "end main", "start all"});
}

TEST(WKUserContentController, UserScriptNotification)
{
    auto waitsForNotification = adoptNS([[WKUserScript alloc] _initWithSource:@"alert('waited for notification')" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:[NSURL URLWithString:@"test:///script"] contentWorld:[WKContentWorld defaultClientWorld] deferRunningUntilNotification:YES]);
    auto documentEnd = adoptNS([[WKUserScript alloc] initWithSource:@"alert('document parsing ended')" injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);

    auto webView1 = adoptNS([TestWKWebView new]);
    EXPECT_TRUE([webView1 _deferrableUserScriptsNeedNotification]);
    [[webView1 configuration].userContentController addUserScript:waitsForNotification.get()];
    [[webView1 configuration].userContentController addUserScript:documentEnd.get()];
    auto delegate = adoptNS([TestUIDelegate new]);
    [webView1 setUIDelegate:delegate.get()];
    [webView1 loadTestPageNamed:@"simple"];
    EXPECT_WK_STREQ([delegate waitForAlert], "document parsing ended");
    EXPECT_TRUE([webView1 _deferrableUserScriptsNeedNotification]);
    [webView1 _notifyUserScripts];
    EXPECT_FALSE([webView1 _deferrableUserScriptsNeedNotification]);
    EXPECT_WK_STREQ([delegate waitForAlert], "waited for notification");

    [webView1 _killWebContentProcessAndResetState];
    [webView1 reload];

    EXPECT_WK_STREQ([delegate waitForAlert], "document parsing ended");
    EXPECT_TRUE([webView1 _deferrableUserScriptsNeedNotification]);
    [webView1 _notifyUserScripts];
    EXPECT_FALSE([webView1 _deferrableUserScriptsNeedNotification]);
    EXPECT_WK_STREQ([delegate waitForAlert], "waited for notification");

    [webView1 reload];

    EXPECT_FALSE([webView1 _deferrableUserScriptsNeedNotification]);
    EXPECT_WK_STREQ([delegate waitForAlert], "waited for notification");
    EXPECT_WK_STREQ([delegate waitForAlert], "document parsing ended");

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _deferrableUserScriptsShouldWaitUntilNotification]);
    configuration.get()._deferrableUserScriptsShouldWaitUntilNotification = NO;
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    EXPECT_FALSE([webView2 _deferrableUserScriptsNeedNotification]);
    [[webView2 configuration].userContentController addUserScript:waitsForNotification.get()];
    [[webView2 configuration].userContentController addUserScript:documentEnd.get()];
    [webView2 setUIDelegate:delegate.get()];
    [webView2 loadTestPageNamed:@"simple"];
    EXPECT_WK_STREQ([delegate waitForAlert], "waited for notification");
    EXPECT_WK_STREQ([delegate waitForAlert], "document parsing ended");

    auto webView3 = adoptNS([TestWKWebView new]);
    EXPECT_TRUE([webView3 _deferrableUserScriptsNeedNotification]);
    [[webView3 configuration].userContentController addUserScript:waitsForNotification.get()];
    [[webView3 configuration].userContentController addUserScript:documentEnd.get()];
    [webView3 setUIDelegate:delegate.get()];
    [webView3 loadTestPageNamed:@"simple"];
    [webView3 _notifyUserScripts];
    EXPECT_FALSE([webView3 _deferrableUserScriptsNeedNotification]);
    EXPECT_WK_STREQ([delegate waitForAlert], "waited for notification");
    EXPECT_WK_STREQ([delegate waitForAlert], "document parsing ended");

    auto webView4 = adoptNS([TestWKWebView new]);
    EXPECT_TRUE([webView4 _deferrableUserScriptsNeedNotification]);
    [[webView4 configuration].userContentController addUserScript:waitsForNotification.get()];
    [[webView4 configuration].userContentController addUserScript:documentEnd.get()];
    [webView4 setUIDelegate:delegate.get()];
    [webView4 loadTestPageNamed:@"simple-iframe"];
    [webView4 _notifyUserScripts];

    // If this is broken, two alerts would appear back-to-back with the same text due to the frame.
    EXPECT_WK_STREQ([delegate waitForAlert], "waited for notification");
    EXPECT_WK_STREQ([delegate waitForAlert], "document parsing ended");
}

TEST(WKUserContentController, AddUserScriptInWorldWithGlobalObjectAvailableInIframe)
{
    RetainPtr<WKContentWorld> testWorld = [WKContentWorld worldWithName:@"testWorld"];
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ContentWorldPlugIn"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

    auto delegate = adoptNS([[SimpleNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple-iframe" withExtension:@"html"]];

    isDoneWithNavigation = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDoneWithNavigation);

    auto contentScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.worldName" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:nil contentWorld:testWorld.get() deferRunningUntilNotification:NO]);
    [[webView configuration].userContentController _addUserScriptImmediately:contentScript.get()];

    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        [webView _evaluateJavaScript:@"window.worldName" inFrame:mainFrame.childFrames[0].info inContentWorld:testWorld.get() completionHandler:^(id result, NSError *error) {
            EXPECT_WK_STREQ(result, "testWorld");
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

@interface AsyncScriptMessageHandler : NSObject <WKScriptMessageHandlerWithReply>
@end

@implementation AsyncScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message replyHandler:(void (^)(id, NSString *errorMessage))replyHandler
{
    if ([message.name isEqualToString:@"otherWorldHandler"])
        EXPECT_TRUE(message.world != nil);
    if ([message.body isKindOfClass:[NSString class]]) {
        if ([message.body isEqualToString:@"Fulfill"]) {
            replyHandler(@"Fulfilled!", nil);
            return;
        }
        if ([message.body isEqualToString:@"Reject"]) {
            replyHandler(nil, @"Rejected!");
            return;
        }
        if ([message.body isEqualToString:@"Undefined"]) {
            replyHandler(nil, nil);
            bool caught = false;
            @try {
                replyHandler(nil, nil);
            } @catch (NSException *exception) {
                caught = true;
            }
            EXPECT_TRUE(caught);
            return;
        }
        if ([message.body isEqualToString:@"Do nothing"]) {
            // Drop the reply handler without responding to see what happens
            return;
        }
        if ([message.body isEqualToString:@"Invalid reply"]) {
            replyHandler(adoptNS([[NSData alloc] init]).get(), nil);
            return;
        }
    }

    // All other inputs should just be round tripped back to the message handler
    replyHandler(message.body, nil);
}

@end

TEST(WKUserContentController, MessageHandlerAPI)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[AsyncScriptMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:WKContentWorld.pageWorld name:@"testHandler1"];

    bool hadException = false;
    @try {
        [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:WKContentWorld.pageWorld name:@"testHandler1"];
    } @catch (NSException *exception) {
        hadException = true;
    }

    EXPECT_TRUE(hadException);
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:WKContentWorld.defaultClientWorld name:@"testHandler2"];

    auto *world = [WKContentWorld worldWithName:@"otherWorld"];
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:world name:@"testHandler3"];
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:world name:@"testHandler4"];
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:world name:@"testHandler5"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    bool done = false;
    NSString *functionBody = @"var p = window.webkit.messageHandlers[handler].postMessage(arg); await p; return p;";

    // pageWorld is where testhandler1 lives
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler1", @"arg" : @1 } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Trying to find testHandler1 in the defaultClientWorld should fail
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler1", @"arg" : @1 } inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // defaultClientWorld is where testhandler2 lives
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler2", @"arg" : @1 } inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // But if we remvoe it, it should no longer live there, and using it should cause an error.
    [[configuration userContentController] removeScriptMessageHandlerForName:@"testHandler2" contentWorld:WKContentWorld.defaultClientWorld];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler2", @"arg" : @1 } inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Verify handlers 3, 4, and 5 are all in the custom world.
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler3", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
    }];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler4", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
    }];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler5", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Remove 3 from the wrong world, verify it is still there in the custom world.
    [[configuration userContentController] removeScriptMessageHandlerForName:@"testHandler3" contentWorld:WKContentWorld.defaultClientWorld];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler3", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Remove 3 from the correct world, verify it is gone, but 4 and 5 are still there.
    [[configuration userContentController] removeScriptMessageHandlerForName:@"testHandler3" contentWorld:world];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler3", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
    }];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler4", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
    }];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler5", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isEqualToNumber:@1]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Remove "all" in the custom world, verify 4 and 5 are now gone.
    [[configuration userContentController] removeAllScriptMessageHandlersFromContentWorld:world];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler4", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
    }];
    [webView callAsyncJavaScript:functionBody arguments:@{ @"handler" : @"testHandler5", @"arg" : @1 } inFrame:nil inContentWorld:world completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKUserContentController, AsyncScriptMessageHandlerBasicPost)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[AsyncScriptMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:WKContentWorld.pageWorld name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    bool done = false;
    NSString *functionBody = @"var p = window.webkit.messageHandlers.testHandler.postMessage('Fulfill'); await p; return p;";
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"Fulfilled!"]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    done = false;
    functionBody = @"var p = window.webkit.messageHandlers.testHandler.postMessage('Reject'); await p; return p;";
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE(!!error);
        EXPECT_TRUE([[error description] containsString:@"Rejected!"]);
        
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    done = false;
    functionBody = @"var p = window.webkit.messageHandlers.testHandler.postMessage('Undefined'); var result = await p; return result == undefined ? 'Yes' : 'No'";
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"Yes"]);

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    done = false;
    functionBody = @"var p = window.webkit.messageHandlers.testHandler.postMessage('Do nothing'); await p; return p;";
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE(!!error);
        EXPECT_TRUE([[error description] containsString:@"did not respond to this postMessage"]);

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    done = false;
    functionBody = @"var p = window.webkit.messageHandlers.testHandler.postMessage('Invalid reply'); await p; return p;";
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE(!!error);
        EXPECT_TRUE([[error description] containsString:@"unable to be serialized"]);

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(WKUserContentController, WorldLifetime)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[AsyncScriptMessageHandler alloc] init]);

    RetainPtr<WKContentWorld> world = [WKContentWorld worldWithName:@"otherWorld"];
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:world.get() name:@"testHandler"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Set a variable in the world.
    bool done = false;
    [webView evaluateJavaScript:@"var foo = 'bar'" inFrame:nil inContentWorld:world.get() completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Have the message handler bounce back that value.
    NSString *functionBody = @"var p = window.webkit.messageHandlers.testHandler.postMessage(foo); await p; return p;";
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:world.get() completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bar"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Remove the message handler, which used to cause the world to be destroyed in the web process.
    // But by evaluating JS make sure the value is still there.
    [[configuration userContentController] removeAllScriptMessageHandlersFromContentWorld:world.get()];
    [webView evaluateJavaScript:@"foo" inFrame:nil inContentWorld:world.get() completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bar"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKUserContentController, RemoveAllScriptMessageHandlers)
{
    WeakObjCPtr<ScriptMessageHandler> weakHandler;
    auto handler = adoptNS([ScriptMessageHandler new]);
    weakHandler = handler.get();
    auto controller = adoptNS([WKUserContentController new]);
    [controller addScriptMessageHandler:handler.get() name:@"testname"];
    handler = nullptr;
    EXPECT_NOT_NULL(weakHandler.get());
    [controller removeAllScriptMessageHandlers];
    EXPECT_NULL(weakHandler.get());
}
