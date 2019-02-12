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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserScript.h>
#import <WebKit/WKUserScriptPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserContentWorld.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if WK_API_ENABLED

static bool isDoneWithNavigation;

@interface SimpleNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation SimpleNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDoneWithNavigation = true;
}

@end

static bool receivedScriptMessage;
static Vector<RetainPtr<WKScriptMessage>> scriptMessages;

@interface ScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation ScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

@end

TEST(WKUserContentController, ScriptMessageHandlerBasicPost)
{
    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.webkit.messageHandlers.testHandler.postMessage('Hello')" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"Hello", (NSString *)[scriptMessages[0] body]);
}

TEST(WKUserContentController, ScriptMessageHandlerBasicPostIsolatedWorld)
{
    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"window.webkit.messageHandlers.testHandler.postMessage('Hello')" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] _addScriptMessageHandler:handler.get() name:@"testHandler" userContentWorld:world.get()];
    [[configuration userContentController] addUserScript:userScript.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<SimpleNavigationDelegate> delegate = adoptNS([[SimpleNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    isDoneWithNavigation = false;
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"Hello", (NSString *)[scriptMessages[0] body]);

    if (!isDoneWithNavigation)
        TestWebKitAPI::Util::run(&isDoneWithNavigation);

    __block bool isDoneEvaluatingScript = false;
    __block NSString *resultValue = @"";
    [webView evaluateJavaScript:
        @"var result;"
         "if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.testHandler) {"
         "    result = { 'result': 'FAIL' };"
         "} else {"
         "    result = { 'result': 'PASS' };"
         "} " 
         "result;"
         completionHandler:^(id value, NSError *error) {
            resultValue = [((NSDictionary *)value)[@"result"] copy];
            isDoneEvaluatingScript = true;
        }];

    TestWebKitAPI::Util::run(&isDoneEvaluatingScript);

    EXPECT_WK_STREQ(@"PASS", resultValue);
}

TEST(WKUserContentController, ScriptMessageHandlerBasicRemove)
{
    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKUserContentController> userContentController = [configuration userContentController];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToRemove"];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToPost"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

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

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessages[0] body]);

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

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessages[0] body]);
}

TEST(WKUserContentController, ScriptMessageHandlerCallRemovedHandler)
{
    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKUserContentController> userContentController = [configuration userContentController];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToRemove"];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToPost"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"var handlerToRemove = window.webkit.messageHandlers.handlerToRemove;" completionHandler:nil];

    [userContentController removeScriptMessageHandlerForName:@"handlerToRemove"];

    // Test that we throw an exception if you try to use a message handler that has been removed.
    [webView evaluateJavaScript:
        @"try {"
         "    handlerToRemove.postMessage('FAIL');"
         "} catch (e) {"
         "    window.webkit.messageHandlers.handlerToPost.postMessage('PASS');"
         "}" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessages[0] body]);
}

static RetainPtr<WKWebView> webViewForScriptMessageHandlerMultipleHandlerRemovalTest(WKWebViewConfiguration *configuration)
{
    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configurationCopy = adoptNS([configuration copy]);
    [configurationCopy setUserContentController:[[[WKUserContentController alloc] init] autorelease]];
    [[configurationCopy userContentController] addScriptMessageHandler:handler.get() name:@"handlerToRemove"];
    [[configurationCopy userContentController] addScriptMessageHandler:handler.get() name:@"handlerToPost"];
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationCopy.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    return webView;
}

TEST(WKUserContentController, ScriptMessageHandlerMultipleHandlerRemoval)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<_WKProcessPoolConfiguration> processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setMaximumProcessCount:1];
    [configuration setProcessPool:[[[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()] autorelease]];

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

    EXPECT_WK_STREQ(@"PASS", (NSString *)[scriptMessages[0] body]);
}

#if !PLATFORM(IOS_FAMILY) // FIXME: hangs in the iOS simulator
TEST(WKUserContentController, ScriptMessageHandlerWithNavigation)
{
    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.webkit.messageHandlers.testHandler.postMessage('First Message')" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_WK_STREQ(@"First Message", (NSString *)[scriptMessages[0] body]);
    
    receivedScriptMessage = false;

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.webkit.messageHandlers.testHandler.postMessage('Second Message')" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_WK_STREQ(@"Second Message", (NSString *)[scriptMessages[1] body]);
}
#endif

TEST(WKUserContentController, ScriptMessageHandlerReplaceWithSameName)
{
    RetainPtr<ScriptMessageHandler> handler = adoptNS([[ScriptMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKUserContentController> userContentController = [configuration userContentController];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToReplace"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Test that handlerToReplace was succesfully added.
    [webView evaluateJavaScript:@"window.webkit.messageHandlers.handlerToReplace.postMessage('PASS1');" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS1", (NSString *)[scriptMessages[0] body]);

    [userContentController removeScriptMessageHandlerForName:@"handlerToReplace"];
    [userContentController addScriptMessageHandler:handler.get() name:@"handlerToReplace"];

    // Test that handlerToReplace still works.
    [webView evaluateJavaScript:@"window.webkit.messageHandlers.handlerToReplace.postMessage('PASS2');" completionHandler:nil];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;

    EXPECT_WK_STREQ(@"PASS2", (NSString *)[scriptMessages[1] body]);
}

static NSString *styleSheetSource = @"body { background-color: green !important; }";
static NSString *backgroundColorScript = @"window.getComputedStyle(document.body, null).getPropertyValue('background-color')";
static NSString *frameBackgroundColorScript = @"window.getComputedStyle(document.getElementsByTagName('iframe')[0].contentDocument.body, null).getPropertyValue('background-color')";
static const char* greenInRGB = "rgb(0, 128, 0)";
static const char* redInRGB = "rgb(255, 0, 0)";
static const char* whiteInRGB = "rgba(0, 0, 0, 0)";

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

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    expectScriptEvaluatesToColor(webView.get(), backgroundColorScript, greenInRGB);
}

TEST(WKUserContentController, NonCanonicalizedURL)
{
    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];
    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] baseURL:[[NSURL alloc] initWithString:@"http://CamelCase/"] userContentWorld:world.get()]);
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

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    
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

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld2 = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    
    [userContentController _addUserStyleSheet:styleSheet.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][1]);
    EXPECT_EQ(styleSheetAssociatedWithWorld2.get(), [userContentController _userStyleSheets][2]);

    [userContentController _removeAllUserStyleSheetsAssociatedWithUserContentWorld:world.get()];

    EXPECT_EQ(1u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
}

TEST(WKUserContentController, UserStyleSheetRemoveAllByNormalWorld)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<_WKUserStyleSheet> styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    RetainPtr<_WKUserStyleSheet> styleSheetAssociatedWithWorld2 = adoptNS([[_WKUserStyleSheet alloc] initWithSource:styleSheetSource forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    
    [userContentController _addUserStyleSheet:styleSheet.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld.get()];
    [userContentController _addUserStyleSheet:styleSheetAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheet.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][1]);
    EXPECT_EQ(styleSheetAssociatedWithWorld2.get(), [userContentController _userStyleSheets][2]);

    [userContentController _removeAllUserStyleSheetsAssociatedWithUserContentWorld:[_WKUserContentWorld normalWorld]];

    EXPECT_EQ(2u, [userContentController _userStyleSheets].count);
    EXPECT_EQ(styleSheetAssociatedWithWorld.get(), [userContentController _userStyleSheets][0]);
    EXPECT_EQ(styleSheetAssociatedWithWorld2.get(), [userContentController _userStyleSheets][1]);
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

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    
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

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld2 = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    
    [userContentController addUserScript:userScript.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][1]);
    EXPECT_EQ(userScriptAssociatedWithWorld2.get(), [userContentController userScripts][2]);

    [userContentController _removeAllUserScriptsAssociatedWithUserContentWorld:world.get()];

    EXPECT_EQ(1u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
}

TEST(WKUserContentController, UserScriptRemoveAllByNormalWorld)
{
    RetainPtr<WKUserContentController> userContentController = adoptNS([[WKUserContentController alloc] init]);

    RetainPtr<_WKUserContentWorld> world = [_WKUserContentWorld worldWithName:@"TestWorld"];

    RetainPtr<WKUserScript> userScript = adoptNS([[WKUserScript alloc] initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    RetainPtr<WKUserScript> userScriptAssociatedWithWorld2 = adoptNS([[WKUserScript alloc] _initWithSource:@"" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO legacyWhitelist:@[] legacyBlacklist:@[] userContentWorld:world.get()]);
    
    [userContentController addUserScript:userScript.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld.get()];
    [userContentController addUserScript:userScriptAssociatedWithWorld2.get()];
    
    EXPECT_EQ(3u, [userContentController userScripts].count);
    EXPECT_EQ(userScript.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][1]);
    EXPECT_EQ(userScriptAssociatedWithWorld2.get(), [userContentController userScripts][2]);

    [userContentController _removeAllUserScriptsAssociatedWithUserContentWorld:[_WKUserContentWorld normalWorld]];

    EXPECT_EQ(2u, [userContentController userScripts].count);
    EXPECT_EQ(userScriptAssociatedWithWorld.get(), [userContentController userScripts][0]);
    EXPECT_EQ(userScriptAssociatedWithWorld2.get(), [userContentController userScripts][1]);
}

static void waitForMessages(size_t expectedCount)
{
    while (scriptMessages.size() < expectedCount) {
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
    }
}

static void compareMessages(Vector<const char*>&& expectedMessages)
{
    EXPECT_EQ(expectedMessages.size(), scriptMessages.size());
    for (size_t i = 0; i < expectedMessages.size(); ++i)
        EXPECT_STREQ([[scriptMessages[i] body] UTF8String], expectedMessages[i]);
}

TEST(WKUserContentController, InjectUserScriptImmediately)
{
    auto handler = adoptNS([[ScriptMessageHandler alloc] init]);
    auto startAllFrames = adoptNS([[WKUserScript alloc] initWithSource:@"window.webkit.messageHandlers.testHandler.postMessage('start all')" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    auto endMainFrameOnly = adoptNS([[WKUserScript alloc] initWithSource:@"window.webkit.messageHandlers.testHandler.postMessage('end main')" injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[SimpleNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple-iframe" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

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

#endif
