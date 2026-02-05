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
#import <WebKit/WKFoundation.h>

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestScriptMessageHandler.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/MakeString.h>

TEST(WKWebView, EvaluateJavaScriptBlockCrash)
{
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
        [webView loadRequest:request];

        [webView evaluateJavaScript:@"" completionHandler:^(id result, NSError *error) {
            // NOTE: By referencing the request here, we convert the block into a stack block rather than a global block and thus allow the copying of the block
            // in evaluateJavaScript to be meaningful.
            (void)request;
            
            EXPECT_NULL(result);
            EXPECT_NOT_NULL(error);

            isDone = true;
        }];

        // Force the WKWebView to be destroyed to allow evaluateJavaScript's completion handler to be called with an error.
        webView = nullptr;
    }

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, EvaluateJavaScriptErrorCases)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    NSArray<NSString *> *unsupportedTypes = @[
        @"document.body",
        @"[ document.body ]",
        @"{ a : document.body }",
    ];
    for (NSString *unsupported in unsupportedTypes) {
        isDone = false;
        [webView evaluateJavaScript:unsupported completionHandler:^(id result, NSError *error) {
            EXPECT_NULL(result);
            EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
            EXPECT_EQ(WKErrorJavaScriptResultTypeIsUnsupported, [error code]);
            isDone = true;
        }];
        TestWebKitAPI::Util::run(&isDone);
    }

    auto handler = adoptNS([TestScriptMessageHandler new]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    NSString *handlerName = @"testHandler";
    [[webView configuration].userContentController addScriptMessageHandler:handler.get() name:handlerName];
    NSString *postMessages = @""
        "window.webkit.messageHandlers.testHandler.postMessage(document.body);"
        "window.webkit.messageHandlers.testHandler.postMessage('abc');"
        "window.webkit.messageHandlers.testHandler.postMessage(null);"
        "window.webkit.messageHandlers.testHandler.postMessage(undefined);"
        "window.webkit.messageHandlers.testHandler.postMessage(new DOMException(null, null));"
        "window.webkit.messageHandlers.testHandler.postMessage(new DOMMatrix());"
    "";
    [webView evaluateJavaScript:postMessages completionHandler:nil];
    RetainPtr firstMessage = [handler waitForMessage];
    EXPECT_EQ(firstMessage.get().name, handlerName);
    EXPECT_WK_STREQ(firstMessage.get().name, handlerName);
    EXPECT_WK_STREQ(firstMessage.get().body, "abc");
    EXPECT_EQ(firstMessage.get().body, firstMessage.get().body);
    EXPECT_EQ([handler waitForMessage].body, NSNull.null);
    EXPECT_NULL([handler waitForMessage].body);
    EXPECT_NULL([handler waitForMessage].body);
    EXPECT_NULL([handler waitForMessage].body);

    [webView evaluateJavaScript:@"document.body.insertBefore(document, document)" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
        EXPECT_EQ(WKErrorJavaScriptExceptionOccurred, [error code]);
        EXPECT_NOT_NULL([error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey]);
        EXPECT_GT([[error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey] length], (unsigned long)0);
        EXPECT_EQ(1, [[error.userInfo objectForKey:_WKJavaScriptExceptionLineNumberErrorKey] intValue]);
        EXPECT_EQ(27, [[error.userInfo objectForKey:_WKJavaScriptExceptionColumnNumberErrorKey] intValue]);

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    [webView evaluateJavaScript:@"\n\nthrow 'something bad'" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
        EXPECT_EQ(WKErrorJavaScriptExceptionOccurred, [error code]);
        EXPECT_WK_STREQ(@"something bad", [error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey]);
        EXPECT_EQ(3, [[error.userInfo objectForKey:_WKJavaScriptExceptionLineNumberErrorKey] intValue]);
        EXPECT_EQ(22, [[error.userInfo objectForKey:_WKJavaScriptExceptionColumnNumberErrorKey] intValue]);
        EXPECT_NOT_NULL([error.userInfo objectForKey:_WKJavaScriptExceptionSourceURLErrorKey]);

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, WKContentWorld)
{
    EXPECT_NULL(WKContentWorld.pageWorld.name);
    EXPECT_NULL(WKContentWorld.defaultClientWorld.name);
    EXPECT_FALSE(WKContentWorld.pageWorld == WKContentWorld.defaultClientWorld);

    WKContentWorld *namedWorld = [WKContentWorld worldWithName:@"Name"];
    EXPECT_TRUE([namedWorld.name isEqualToString:@"Name"]);
    EXPECT_EQ(namedWorld, [WKContentWorld worldWithName:@"Name"]);
}

@interface DummyMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation DummyMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
}
@end

TEST(WKWebView, EvaluateJavaScriptInWorlds)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<html><body><iframe sandbox=""></frame></html>"];

    // Set a variable in the main world via "normal" evaluateJavaScript
    __block bool isDone = false;
    __block size_t testsPassed = 0;
    [webView evaluateJavaScript:@"var foo = 'bar'" completionHandler:^(id result, NSError *error) {
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Verify that value is visible when evaluating in the pageWorld
    [webView evaluateJavaScript:@"foo" inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bar"]);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Verify that value is not visible when evaluating in the defaultClientWorld
    [webView evaluateJavaScript:@"foo" inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Verify that value is visible when calling a function in the pageWorld
    [webView callAsyncJavaScript:@"return foo" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bar"]);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Verify that value is not visible when calling a function in the defaultClientWorld
    [webView callAsyncJavaScript:@"return foo" arguments:nil inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Add a scriptMessageHandler in a named world.
    RetainPtr<WKContentWorld> namedWorld = [WKContentWorld worldWithName:@"NamedWorld"];
    auto handler = adoptNS([[DummyMessageHandler alloc] init]);
    [webView.get().configuration.userContentController _addScriptMessageHandler:handler.get() name:@"testHandlerName" userContentWorld:namedWorld.get()._userContentWorld];

    // Set a variable value in that named world.
    [webView evaluateJavaScript:@"var bar = 'baz'" inFrame:nil inContentWorld:namedWorld.get() completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Set a global variable value in that named world via a function call.
    [webView callAsyncJavaScript:@"window.baz = 'bat'" arguments:nil inFrame:nil inContentWorld:namedWorld.get() completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NULL(error);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Remove the dummy message handler
    [webView.get().configuration.userContentController _removeScriptMessageHandlerForName:@"testHandlerName" userContentWorld:namedWorld.get()._userContentWorld];

    // Verify the variables we set are there in that named world.
    [webView evaluateJavaScript:@"bar" inFrame:nil inContentWorld:namedWorld.get() completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"baz"]);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView evaluateJavaScript:@"window.baz" inFrame:nil inContentWorld:namedWorld.get() completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bat"]);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Verify they aren't there in the defaultClientWorld.
    [webView evaluateJavaScript:@"bar" inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView evaluateJavaScript:@"window.baz" inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    // Verify they aren't there in the pageWorld.
    [webView evaluateJavaScript:@"bar" inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView evaluateJavaScript:@"window.baz" inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        isDone = true;
        testsPassed++;
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 1U);
        [webView evaluateJavaScript:@"'PASS'" inFrame:mainFrame.childFrames[0].info inContentWorld:namedWorld.get() completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_TRUE([result isEqualToString:@"PASS"]);
            if (!error)
                testsPassed++;
            isDone = true;
        }];
    }];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_EQ(testsPassed, 13u);
}

TEST(WKWebView, EvaluateJavaScriptInWorldsWithGlobalObjectAvailable)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ContentWorldPlugIn"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:@"<html></html>"];

    __block bool done = false;
    [webView evaluateJavaScript:@"window.worldName" inFrame:nil inContentWorld:[WKContentWorld worldWithName:@"testName"] completionHandler:^(id result, NSError *error) {
        EXPECT_WK_STREQ(result, "testName");
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebView, EvaluateJavaScriptInWorldsWithGlobalObjectAvailableInCrossOriginIframe)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ContentWorldPlugIn"];
    auto handler = adoptNS([TestURLSchemeHandler new]);
    __block bool childFrameLoaded = false;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *responseString = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"frame://host1/"])
            responseString = @"<iframe src='frame://host2/' onload='fetch(\"loadedChildFrame\")'></iframe>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host2/"])
            responseString = @"frame content";
        else if ([task.request.URL.path isEqualToString:@"/loadedChildFrame"]) {
            responseString = @"fetched content";
            childFrameLoaded = true;
        }

        ASSERT(responseString);
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:responseString.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[responseString dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"frame"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"frame://host1/"]]];

    TestWebKitAPI::Util::run(&childFrameLoaded);
    
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        [webView _evaluateJavaScript:@"window.worldName" inFrame:mainFrame.childFrames[0].info inContentWorld:[WKContentWorld worldWithName:@"testName"] completionHandler:^(id result, NSError *error) {
            EXPECT_WK_STREQ(result, "testName");
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, GetFrameInfo_detachedFrame)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"<iframe id='testFrame' src='about:blank'></iframe>"];

    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 1U);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    auto pid = [webView _webProcessIdentifier];

    [webView evaluateJavaScript:@"document.getElementById('testFrame').remove();" completionHandler:nil];

    __block bool hasChildFrame = true;
    do {
        done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            hasChildFrame = mainFrame.childFrames.count > 0;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    } while (hasChildFrame);

    EXPECT_EQ(pid, [webView _webProcessIdentifier]);
}

TEST(WebKit, EvaluateJavaScriptInAttachments)
{
    // Attachments displayed inline are in sandboxed documents.
    // Evaluating JavaScript in such a document should fail and result in an error.

    using namespace TestWebKitAPI;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [](Connection connection) -> ConnectionTask {
        co_await connection.awaitableReceiveHTTPRequest();
        co_await connection.awaitableSend("HTTP/1.1 200 OK\r\n"
            "Content-Length: 12\r\n"
            "Content-Disposition: attachment; filename=fromHeader.txt;\r\n\r\n"
            "Hello world!"_s);
    });
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    __block bool done = false;
    [webView evaluateJavaScript:@"Hello" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        EXPECT_TRUE([[error description] containsString:@"Cannot execute JavaScript in this document"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

// FIXME: Re-enable this test for iOS once webkit.org/b/207874 is resolved
#if !(PLATFORM(IOS) || PLATFORM(VISION))
TEST(WebKit, AllowsContentJavaScript)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<script>var foo = 'bar'</script>"];

    __block bool done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bar"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    RetainPtr<WKWebpagePreferences> preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    EXPECT_TRUE(preferences.get().allowsContentJavaScript);
    preferences.get().allowsContentJavaScript = NO;
    [webView synchronouslyLoadHTMLString:@"<script>var foo = 'bar'</script>" preferences:preferences.get()];

    done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE([[error description] containsString:@"Can't find variable: foo"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    TestWebKitAPI::HTTPServer server({
        { "/script"_s, { "var foo = 'bar'"_s } }
    });
    preferences.get().allowsContentJavaScript = YES;
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<script src='http://127.0.0.1:%d/script'></script>", server.port()] preferences:preferences.get()];

    done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"bar"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    preferences.get().allowsContentJavaScript = NO;
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<script src='http://127.0.0.1:%d/script'></script>", server.port()] preferences:preferences.get()];

    done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE([[error description] containsString:@"Can't find variable: foo"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    preferences.get().allowsContentJavaScript = YES;
    [webView synchronouslyLoadHTMLString:@"<iframe src='javascript:window.foo = 1'></iframe>" preferences:preferences.get()];

    done = false;
    [webView evaluateJavaScript:@"window.frames[0].foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
        EXPECT_TRUE([result isEqualToNumber:@1]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    preferences.get().allowsContentJavaScript = NO;
    [webView synchronouslyLoadHTMLString:@"<iframe src='javascript:window.foo = 1'></iframe>" preferences:preferences.get()];

    done = false;
    [webView evaluateJavaScript:@"window.frames[0].foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}
#endif

TEST(WebKit, AllowsContentJavaScriptFromDefaultPreferences)
{
    RetainPtr<WKWebpagePreferences> preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setAllowsContentJavaScript:NO];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setDefaultWebpagePreferences:preferences.get()];

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:@"<script>var foo = 'bar'</script>"];

    __block bool done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE([[error description] containsString:@"Can't find variable: foo"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, AllowsContentJavaScriptAffectsNoscriptElements)
{
    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setAllowsContentJavaScript:NO];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setDefaultWebpagePreferences:preferences.get()];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:@"<noscript>this text should be inserted into the DOM</noscript>"];
    EXPECT_WK_STREQ([webView contentsAsString], "this text should be inserted into the DOM");
}

TEST(WebKit, SPIJavascriptMarkupVsAPIContentJavaScript)
{
    // There's not a dynamically configuration setting for javascript markup,
    // but it can be configured at WKWebView creation time.
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Verify that the following JS does not execute.
    [webView synchronouslyLoadHTMLString:@"<script>var foo = 'bar'</script>"];

    __block bool done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE([[error description] containsString:@"Can't find variable: foo"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Try to explicitly enable script markup using WKWebpagePreferences, but verify it should still fail because
    // of the above configuration setting.
    RetainPtr<WKWebpagePreferences> preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    EXPECT_TRUE(preferences.get().allowsContentJavaScript);
    [webView synchronouslyLoadHTMLString:@"<script>var foo = 'bar'</script>" preferences:preferences.get()];

    done = false;
    [webView evaluateJavaScript:@"foo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE([[error description] containsString:@"Can't find variable: foo"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

}

static RetainPtr<NSMutableSet<WKFrameInfo *>> allFrames;

@interface FramesMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation FramesMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_TRUE(message.world == WKContentWorld.defaultClientWorld);
    [allFrames addObject:message.frameInfo];
}
@end

static const char* framesMainResource = R"FRAMESRESOURCE(
Hello world<br>
<iframe id='theFrame' src='otherprotocol://test/index.html'></iframe>
)FRAMESRESOURCE";

static NSString *userScriptSource = @"window.webkit.messageHandlers.framesTester.postMessage('hi');";

// This test loads a document under one protocol.
// That document embeds an iframe under a different protocol
// (ensuring they cannot access each other under the cross-origin rules of the web security model)
// It uses a WKUserScript to collect all of the frames on the page, and then uses new forms of evaluateJavaScript
// and callAsyncJavaScript to confirm that it can execute JS directly in each of those frames.
TEST(EvaluateJavaScript, JavaScriptInFramesFromPostMessage)
{
    allFrames = adoptNS([[NSMutableSet<WKFrameInfo *> alloc] init]);
    auto messageHandler = adoptNS([[FramesMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO inContentWorld:WKContentWorld.defaultClientWorld]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addUserScript:userScript.get()];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() contentWorld:WKContentWorld.defaultClientWorld name:@"framesTester"];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"framestest://test/index.html"]]];

    EXPECT_EQ([allFrames count], 2u);

    static size_t finishedFrames = 0;
    static bool isDone = false;

    for (WKFrameInfo *frame in allFrames.get()) {
        bool isMainFrame = frame.isMainFrame;
        [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == [allFrames count] * 2)
                isDone = true;
        }];


        [webView evaluateJavaScript:@"location.href;" inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == [allFrames count] * 2)
                isDone = true;
        }];
    }

    TestWebKitAPI::Util::run(&isDone);
}

// This test loads a document under one protocol.
// That document embeds an iframe under a different protocol
// (ensuring they cannot access each other under the cross-origin rules of the web security model)
// It collects all the frames seen during navigation delegate callbacks, and then uses new forms of evaluateJavaScript
// and callAsyncJavaScript to confirm that it can execute JS directly in each of those frames.
TEST(EvaluateJavaScript, JavaScriptInFramesFromNavigationDelegate)
{
    allFrames = adoptNS([[NSMutableSet<WKFrameInfo *> alloc] init]);

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            NSData *data = [@"FooBarBaz" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    [navigationDelegate setDecidePolicyForNavigationAction:[&] (WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (action.targetFrame)
            [allFrames addObject:action.targetFrame];

        decisionHandler(WKNavigationActionPolicyAllow);
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"framestest://test/index.html"]]];

    TestWebKitAPI::Util::run(&didFinishNavigation);

    EXPECT_EQ([allFrames count], 2u);

    static size_t finishedFrames = 0;
    static bool isDone = false;

    for (WKFrameInfo *frame in allFrames.get()) {
        bool isMainFrame = frame.isMainFrame;
        [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == [allFrames count] * 2)
                isDone = true;
        }];


        [webView evaluateJavaScript:@"location.href;" inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == [allFrames count] * 2)
                isDone = true;
        }];
    }

    TestWebKitAPI::Util::run(&isDone);
}

// This test verifies that evaluating JavaScript in a frame that has since gone missing
// due to removal from the DOM results in an appropriate error
TEST(EvaluateJavaScript, JavaScriptInMissingFrameError)
{
    allFrames = adoptNS([[NSMutableSet<WKFrameInfo *> alloc] init]);

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            NSData *data = [@"FooBarBaz" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    RetainPtr<WKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    [navigationDelegate setDecidePolicyForNavigationAction:[&] (WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (action.targetFrame)
            [allFrames addObject:action.targetFrame];

        decisionHandler(WKNavigationActionPolicyAllow);
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"framestest://test/index.html"]]];

    TestWebKitAPI::Util::run(&didFinishNavigation);

    EXPECT_EQ([allFrames count], 2u);

    static bool isDone = false;
    [webView evaluateJavaScript:@"var frame = document.getElementById('theFrame'); frame.parentNode.removeChild(frame);" inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[] (id result, NSError *error) {
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    for (WKFrameInfo *frame in allFrames.get()) {
        if (frame.isMainFrame)
            continue;

        [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[] (id result, NSError *error) {
            EXPECT_FALSE(error == nil);
            EXPECT_TRUE(error.domain == WKErrorDomain);
            EXPECT_TRUE(error.code == WKErrorJavaScriptInvalidFrameTarget);
            isDone = true;
        }];
    }

    TestWebKitAPI::Util::run(&isDone);
}

// This test verifies that evaluating JavaScript in a frame from the previous main navigation results in an error
TEST(EvaluateJavaScript, JavaScriptInMissingFrameAfterNavigationError)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    allFrames = adoptNS([[NSMutableSet<WKFrameInfo *> alloc] init]);

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            NSData *data = [@"FooBarBaz" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"framestest://index2.html"]) {
            NSData *data = [@"Hi" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    RetainPtr<WKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    [navigationDelegate setDecidePolicyForNavigationAction:[&] (WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (action.targetFrame)
            [allFrames addObject:action.targetFrame];

        decisionHandler(WKNavigationActionPolicyAllow);
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"framestest://test/index.html"]]];

    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;

    EXPECT_EQ([allFrames count], 2u);

    RetainPtr<WKFrameInfo> iframe;
    for (WKFrameInfo *frame in allFrames.get()) {
        if (frame.isMainFrame)
            continue;
        iframe = frame;
        break;
    }

    EXPECT_NOT_NULL(iframe);

    // Get rid of the frame by navigating
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"framestest://index2.html"]]];
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;

    static bool isDone = false;

    [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:iframe.get() inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[] (id result, NSError *error) {
        EXPECT_FALSE(error == nil);
        EXPECT_TRUE(error.domain == WKErrorDomain);
        EXPECT_TRUE(error.code == WKErrorJavaScriptInvalidFrameTarget);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    isDone = false;
}

TEST(EvaluateJavaScript, WindowPersistency)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadTestPageNamed:@"simple"];
    [webView stringByEvaluatingJavaScript:@""];

    TestWebKitAPI::Util::run(&didFinishNavigation);

    __block bool done = false;
    [webView evaluateJavaScript:@"window.caches ? 'PASS': 'FAIL'" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(error == nil);
        EXPECT_WK_STREQ(@"PASS", (NSString *)result);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(EvaluateJavaScript, ReturnTypes)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    __block bool didEvaluateJavaScript = false;
    NSString *jsTopLevelReplacedByDict = @"(function(){return /hello/})()";
    // Behaves the same as if sending "(function(){ return {} })()" because of JSValue's containerValueToObject filtering
    [webView evaluateJavaScript:jsTopLevelReplacedByDict completionHandler:^(id value, NSError *error) {
        NSDictionary *dict = (NSDictionary *)value;
        EXPECT_TRUE([dict isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([dict count], 0u);
    }];
    NSString *jsWithTopLevelDict = @"(function(){return { } })()";
    [webView evaluateJavaScript:jsWithTopLevelDict completionHandler:^(id value, NSError *error) {
        NSDictionary *dict = (NSDictionary *)value;
        EXPECT_TRUE([dict isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([dict count], 0u);
    }];
    NSString *jsWithBlob = @""
    "(function(){return {"
    "    blob: new Blob(['Hello']),\n"
    "}})()";
    [webView evaluateJavaScript:jsWithBlob completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(error);
        NSDictionary *dict = (NSDictionary *)value;
        EXPECT_EQ([dict objectForKey:@"blob"], [NSNull null]);
    }];

    NSString *containersWithNullAndUndefined = @"(function(){return [{'a':null, 'b':undefined}, [null, undefined, 0]]})()";
    [webView evaluateJavaScript:containersWithNullAndUndefined completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(error);
        NSArray *expected = @[@{ @"a":NSNull.null }, @[NSNull.null, NSNull.null, @0]];
        EXPECT_TRUE([value isEqual:expected]);
    }];

    NSString *jsWithNestedObjects = @""
    "(function(){"
    "    let aBool = true;\n"
    "    let someObject = {};\n"
    "    const theKey = 'key';\n"
    "    someObject[theKey] = null;\n"
    "return {"
    "    obj1: someObject,\n"
    "    obj2: 41,\n"
    "    obj3: someObject,\n"
    "    obj4: aBool,\n"
    "    obj5: aBool,\n"
    "    [theKey]: false,\n"
    "    theValueToo: theKey,\n"
    "}})()";
    [webView evaluateJavaScript:jsWithNestedObjects completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(error);
        NSDictionary *dict = (NSDictionary *)value;
        EXPECT_TRUE([dict isKindOfClass:[NSDictionary class]]);
        id obj1 = [dict objectForKey:@"obj1"];
        id obj2 = [dict objectForKey:@"obj2"];
        id obj3 = [dict objectForKey:@"obj3"];
        id obj4 = [dict objectForKey:@"obj4"];
        id obj5 = [dict objectForKey:@"obj5"];
        EXPECT_TRUE(obj1 == obj3);
        EXPECT_FALSE(obj1 == obj2);
        EXPECT_TRUE([obj2 isKindOfClass:[NSNumber class]]);
        EXPECT_TRUE([obj1 isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([obj1 objectForKey:@"key"], [NSNull null]);
        EXPECT_TRUE([obj4 isKindOfClass:[NSNumber class]]);
        EXPECT_EQ(obj4, @YES);
        EXPECT_TRUE(obj4 == obj5);
        // The key string objects can be shared too
        id theValueToo = [dict objectForKey:@"theValueToo"];
        NSEnumerator *keyEnumerator = [dict keyEnumerator];
        id key;
        while ((key = [keyEnumerator nextObject])) {
            NSNumber *v = [dict objectForKey:key];
            if ([v isEqual:@NO])
                EXPECT_EQ(key, theValueToo);
        }
    }];
    NSString *jsWithRegexType = @""
    "(function(){return {"
    "    text:\"Hello\",\n"
    "    number: 41,\n"
    "    undef: undefined,\n"
    "    bool: true,\n"
    "    null: null,\n"
    "    array: [ new Date(8.64e15)],\n"
    "    regex: /hello/,\n"
    "    blob: new Blob(['Hello']),\n"
    "buffer: new ArrayBuffer(8),\n"
    "int32View: new Int32Array(this.buffer),\n"
    "objMap: new Map([\n"
    "    ['key1', 'value1'],\n"
    "    ['key2', 'value2']\n"
    "]),\n"
    "aSet: new Set([1, 2, 3]),\n"
    "zero: 0,\n"
    "one: 1,\n"
    "boolFalse: false,\n"
    "aDouble: 3.14,\n"
    "file: new File(['content'], 'file.txt', { type: 'text/plain' }),\n"
    "fileList: new DataTransfer().files,\n"
    "imageData: new ImageData(100, 100),\n"
    "emptyString: '',\n"
    "arrayBuffer: new ArrayBuffer(8),\n"
    "arrayBufferView: new Uint8Array(this.arrayBuffer),\n"
    "bigInt: BigInt(9007199254740991),\n"
    "Error: new Error('An error occurred'),\n"
    "}})()";

    [webView evaluateJavaScript:jsWithRegexType completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(error);
        NSDictionary *dict = (NSDictionary *)value;
        EXPECT_TRUE([dict isKindOfClass:[NSDictionary class]]);
        NSString *text = [dict objectForKey:@"text"];
        EXPECT_TRUE([text isKindOfClass:[NSString class]]);
        if ([text isKindOfClass:[NSString class]])
            EXPECT_TRUE([text isEqual:@"Hello"]);
        NSNumber* number = [dict objectForKey:@"number"];
        EXPECT_TRUE([number isKindOfClass:[NSNumber class]]);
        if ([number isKindOfClass:[NSNumber class]])
            EXPECT_TRUE([number isEqual:@41]);
        EXPECT_EQ([dict objectForKey:@"undef"], nil);
        EXPECT_TRUE([[dict objectForKey:@"bool"] isKindOfClass:[NSNumber class]]);
        if ([[dict objectForKey:@"bool"] isKindOfClass:[NSNumber class]]) {
            BOOL b = [[dict objectForKey:@"bool"] boolValue];
            EXPECT_TRUE(b);
        }

        EXPECT_TRUE([[dict objectForKey:@"boolFalse"] isKindOfClass:[NSNumber class]]);
        if ([[dict objectForKey:@"boolFalse"] isKindOfClass:[NSNumber class]]) {
            BOOL b = [[dict objectForKey:@"boolFalse"] boolValue];
            EXPECT_FALSE(b);
        }
        EXPECT_EQ([dict objectForKey:@"null"], [NSNull null]);
        NSArray* arr = [dict objectForKey:@"array"];
        EXPECT_TRUE([arr isKindOfClass:[NSArray class]]);
        EXPECT_NE(arr, nil);
        EXPECT_EQ([arr count], 1u);
        if ([arr count] == 1u)
            EXPECT_TRUE([arr.firstObject isKindOfClass:[NSDate class]]);
        NSDictionary* regex = [dict objectForKey:@"regex"];
        EXPECT_TRUE([regex isKindOfClass:[NSDictionary class]]); // Converted to empty dictionary
        EXPECT_EQ([regex count], 0u);

        EXPECT_EQ([dict objectForKey:@"blob"], [NSNull null]); // Converted to null
        EXPECT_TRUE([[dict objectForKey:@"aDouble"] isKindOfClass:[NSNumber class]]);
        EXPECT_TRUE([[dict objectForKey:@"zero"] isKindOfClass:[NSNumber class]]);
        EXPECT_EQ([dict objectForKey:@"imageData"], [NSNull null]); // Converted to null
        EXPECT_EQ([dict objectForKey:@"file"], [NSNull null]); // Converted to null
        NSDictionary* arrayBufferView = [dict objectForKey:@"arrayBufferView"];
        EXPECT_TRUE([arrayBufferView isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([arrayBufferView count], 0u);

        EXPECT_EQ([dict objectForKey:@"int32view"], nil);

        NSDictionary* objMap = [dict objectForKey:@"objMap"];
        EXPECT_TRUE([objMap isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([objMap count], 0u);

        NSDictionary* buffer = [dict objectForKey:@"buffer"];
        EXPECT_TRUE([buffer isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([buffer count], 0u);
        NSDictionary* arrayBuffer = [dict objectForKey:@"arrayBuffer"];
        EXPECT_TRUE([arrayBuffer isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([arrayBuffer count], 0u);

        EXPECT_EQ([dict objectForKey:@"error"], nil);

        EXPECT_TRUE([[dict objectForKey:@"one"] isKindOfClass:[NSNumber class]]);
        NSDictionary* aSet = [dict objectForKey:@"aSet"];
        EXPECT_TRUE([aSet isKindOfClass:[NSDictionary class]]);
        EXPECT_EQ([aSet count], 0u);

        EXPECT_EQ([dict objectForKey:@"fileList"], [NSNull null]); // Converted to null
        NSString *emptyString = [dict objectForKey:@"emptyString"];
        EXPECT_TRUE([emptyString isKindOfClass:[NSString class]]);
        if ([emptyString isKindOfClass:[NSString class]])
            EXPECT_TRUE([emptyString isEqual:@""]);
    }];
    [webView evaluateJavaScript:@"(function(){return null)()" completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(value); // top level object must be a dictionary
    }];
    [webView evaluateJavaScript:@"(function(){return \"hello\")()" completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(value); // top level object must be a dictionary
    }];

    constexpr NSUInteger depth { 100000 };
    NSString *deeplyNestedArray = [[@"" stringByPaddingToLength:depth withString: @"{" startingAtIndex:0] stringByAppendingString:[@"" stringByPaddingToLength:depth withString: @"{" startingAtIndex:0]];
    [webView evaluateJavaScript:deeplyNestedArray completionHandler:^(id value, NSError *error) {
        EXPECT_WK_STREQ(error.domain, WKErrorDomain);
        EXPECT_EQ(error.code, WKErrorJavaScriptExceptionOccurred);
    }];

    [webView evaluateJavaScript:@"(function(){ var array = []; array.push(array); return array })()" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        NSArray *array = (NSArray *)value;
        EXPECT_TRUE([array isKindOfClass:NSArray.class]);
        EXPECT_EQ(array.count, 1u);
        EXPECT_TRUE(array[0] == array);
    }];

    [webView evaluateJavaScript:@"(function(){return [\"Array\"])()" completionHandler:^(id value, NSError *error) {
        EXPECT_FALSE(value); // top level object must be a dictionary
        didEvaluateJavaScript = true;
    }];

    TestWebKitAPI::Util::run(&didEvaluateJavaScript);
}

TEST(EvaluateJavaScript, ExceptionAccessingProperty)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);
    id result = [webView objectByCallingAsyncFunction:@"return { get foo() { throw new Error(); }, get bar() { return 123; } }" withArguments:nil];
    EXPECT_TRUE([result isEqual:@{ @"bar": @123 }]);
}

// Tests that evaluating @"" means the same as evaluating nil string. Also, no crashes.
TEST(EvaluateJavaScript, EmptyStrings)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    bool didRunCase1 = false;
    {
        [webView evaluateJavaScript:@"" completionHandler:[&](id value, NSError *error) {
            EXPECT_FALSE(value);
            EXPECT_NULL(error);
            didRunCase1 = true;
        }];
        EXPECT_FALSE(didRunCase1); // The evaluation is async.
        TestWebKitAPI::Util::run(&didRunCase1);
    }
    {
        bool didRunCase2 = false;
        NSString *nilScript = nil;
        [webView evaluateJavaScript:nilScript completionHandler:[&](id value, NSError *error) {
            EXPECT_FALSE(value);
            EXPECT_NULL(error);
            didRunCase2 = true;
        }];
        EXPECT_FALSE(didRunCase2); // The evaluation is async.
        TestWebKitAPI::Util::run(&didRunCase2);
    }
}

// Tests that evaluating long strings work.
TEST(EvaluateJavaScript, LongStrings)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    {
        bool didRunCase1 = false;
        RetainPtr<NSString> evalResult;
        Vector<Latin1Character> longLatin1Data(1024*1024, ' ');
        auto s1 = makeString("'a'"_s, String { longLatin1Data }, "+ 'b'"_s);
        RetainPtr ns1 = s1.createNSString();
        [webView evaluateJavaScript:ns1.get() completionHandler:[&](id value, NSError *error) {
            EXPECT_NULL(error);
            didRunCase1 = true;
            evalResult = [NSString stringWithFormat:@"%@", value];

        }];
        EXPECT_FALSE(didRunCase1); // The evaluation is async.
        TestWebKitAPI::Util::run(&didRunCase1);
        EXPECT_WK_STREQ(evalResult.get(), "ab");
    }
    {
        bool didRunCase2 = false;
        RetainPtr<NSString> evalResult;
        Vector<char16_t> longUnicodeData(1024*1200, u' ');
        auto s2 = makeString(u"'z'"_str, String { longUnicodeData }, u"+ 'u'"_str);
        RetainPtr ns2 = s2.createNSString();
        [webView evaluateJavaScript:ns2.get() completionHandler:[&](id value, NSError *error) {
            EXPECT_NULL(error);
            didRunCase2 = true;
            evalResult = [NSString stringWithFormat:@"%@", value];
        }];
        EXPECT_FALSE(didRunCase2); // The evaluation is async.
        TestWebKitAPI::Util::run(&didRunCase2);
        EXPECT_WK_STREQ(evalResult.get(), "zu");
    }
}

@interface TestScriptMessageHandlerWithReply : NSObject <WKScriptMessageHandlerWithReply>
@end

@implementation TestScriptMessageHandlerWithReply

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message replyHandler:(void(^)(id, NSString *))replyHandler
{
    replyHandler([NSString stringWithFormat:@"UI process received: %@", message.body], nil);
}

@end

TEST(WKWebView, LegacySynchronousMessages)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration _setAllowPostingLegacySynchronousMessages:YES];
    RetainPtr handler = adoptNS([TestScriptMessageHandlerWithReply new]);
    [[configuration userContentController] addScriptMessageHandlerWithReply:handler.get() contentWorld:WKContentWorld.pageWorld name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView loadHTMLString:@"<script>alert(window.webkit.messageHandlers.testHandler.postLegacySynchronousMessage('hello!'))</script>" baseURL:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "UI process received: hello!");
}

// Serialization of JavaScript objects using the old SerializedScriptValue code path
// had a supported object depth of 40,000.
// The new JSExtractor code path - when working recursively - blew out the stack well before 40,000.
// This test validates that its iterative refactoring works at 40,000 deep.
static constexpr auto deepObjectJS = R"SERIALIZEJS(
window.deepObject = {
    foo: "bar"
};

var currentObject = window.deepObject;

for (var i = 0; i < 40000; ++i) {
    currentObject.baz = {
        foo: "bar"
    }
    currentObject = currentObject.baz
}

return 1;
)SERIALIZEJS"_s;

// Verify that an array with some serializable values, but some unserializable,
// comes out as complete as possible in the UI process.
static constexpr auto unserializableArrayJS = R"SERIALIZEJS(
window.arrayObject = [1, 2, 3, 4, 5, 6];
window.arrayObject[0] = window;

return 1;
)SERIALIZEJS"_s;

// Verify that an object with some serializable values, and some unserializable,
// comes out as complete as possible in the UI process.
static constexpr auto unserializableObjectJS = R"SERIALIZEJS(
window.objectObject = {
    foo: "bar",
    bar: 17,
    baz: window
};

return 1;
)SERIALIZEJS"_s;

TEST(EvaluateJavaScript, Serialization)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);

    id result = [webView objectByCallingAsyncFunction:[NSString stringWithUTF8String:deepObjectJS] withArguments:nil];
    EXPECT_TRUE([result isEqual:@1]);
    result = [webView objectByCallingAsyncFunction:[NSString stringWithUTF8String:unserializableArrayJS] withArguments:nil];
    EXPECT_TRUE([result isEqual:@1]);
    result = [webView objectByCallingAsyncFunction:[NSString stringWithUTF8String:unserializableObjectJS] withArguments:nil];
    EXPECT_TRUE([result isEqual:@1]);

    // The full deepObject is 40,001 nesting levels deep, which should not be able to serialize.
    NSError *error = nil;
    result = [webView objectByCallingAsyncFunction:@"return window.deepObject" withArguments:nil error:&error];
    EXPECT_TRUE(!!error);

    // Returning an object 40,000 nesting levels deep should succeed.
    error = nil;
    result = [webView objectByCallingAsyncFunction:@"return window.deepObject.baz" withArguments:nil error:&error];
    EXPECT_NULL(error);

    size_t depth = 0;
    NSDictionary *nextDictionary = (NSDictionary *)result;
    while (nextDictionary) {
        nextDictionary = (NSDictionary *)nextDictionary[@"baz"];
        ++depth;
    }
    EXPECT_EQ(depth, 40000u);

    // One of the array members is not serializable, so it should be missing from the result.
    result = [webView objectByCallingAsyncFunction:@"return window.arrayObject" withArguments:nil error:&error];
    EXPECT_NOT_NULL(error);
    EXPECT_NULL(result);

    // One of the object values is not serializable, so it should be missing from the result.
    result = [webView objectByCallingAsyncFunction:@"return window.objectObject" withArguments:nil error:&error];
    EXPECT_NOT_NULL(error);
    EXPECT_NULL(result);
}

