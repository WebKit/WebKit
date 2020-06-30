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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/RetainPtr.h>

static bool isDone;

TEST(WKWebView, EvaluateJavaScriptBlockCrash)
{
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.body" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
        EXPECT_EQ(WKErrorJavaScriptResultTypeIsUnsupported, [error code]);

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

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
    [webView synchronouslyLoadHTMLString:@"<html></html>"];

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
    id handler = [[[DummyMessageHandler alloc] init] autorelease];
    [webView.get().configuration.userContentController _addScriptMessageHandler:handler name:@"testHandlerName" userContentWorld:namedWorld.get()._userContentWorld];

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
    
    EXPECT_EQ(testsPassed, 12u);
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
        [webView _evaluateJavaScript:@"window.worldName" inFrame:mainFrame.childFrames[0] inContentWorld:[WKContentWorld worldWithName:@"testName"] completionHandler:^(id result, NSError *error) {
            EXPECT_WK_STREQ(result, "testName");
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, EvaluateJavaScriptInAttachments)
{
    // Attachments displayed inline are in sandboxed documents.
    // Evaluating JavaScript in such a document should fail and result in an error.

    using namespace TestWebKitAPI;
    TCPServer server([](int socket) {
        NSString *response = @"HTTP/1.1 200 OK\r\n"
            "Content-Length: 12\r\n"
            "Content-Disposition: attachment; filename=fromHeader.txt;\r\n\r\n"
            "Hello world!";

        TCPServer::read(socket);
        TCPServer::write(socket, response.UTF8String, response.length);
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
#if HAVE(NETWORK_FRAMEWORK) && !PLATFORM(IOS)
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
        { "/script", { "var foo = 'bar'" } }
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

TEST(WebKit, SPIJavascriptMarkupVsAPIContentJavaScript)
{
    // There's not a dynamically configuration setting for javascript markup,
    // but it can be configured at WKWebView creation time.
    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    configuration._allowsJavaScriptMarkup = NO;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

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

static NSMutableSet<WKFrameInfo *> *allFrames;
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
    allFrames = [[NSMutableSet<WKFrameInfo *> alloc] init];
    auto messageHandler = adoptNS([[FramesMessageHandler alloc] init]);
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:userScriptSource injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO inContentWorld:WKContentWorld.defaultClientWorld]);

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration.userContentController addUserScript:userScript.get()];
    [configuration.userContentController addScriptMessageHandler:messageHandler.get() contentWorld:WKContentWorld.defaultClientWorld name:@"framesTester"];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"framestest://test/index.html"]]];

    EXPECT_EQ(allFrames.count, 2u);

    static size_t finishedFrames = 0;
    static bool isDone = false;

    for (WKFrameInfo *frame in allFrames) {
        bool isMainFrame = frame.isMainFrame;
        [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == allFrames.count * 2)
                isDone = true;
        }];


        [webView evaluateJavaScript:@"location.href;" inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == allFrames.count * 2)
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
    allFrames = [[NSMutableSet<WKFrameInfo *> alloc] init];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            NSData *data = [@"FooBarBaz" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    RetainPtr<WKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

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

    EXPECT_EQ(allFrames.count, 2u);

    static size_t finishedFrames = 0;
    static bool isDone = false;

    for (WKFrameInfo *frame in allFrames) {
        bool isMainFrame = frame.isMainFrame;
        [webView callAsyncJavaScript:@"return location.href;" arguments:nil inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == allFrames.count * 2)
                isDone = true;
        }];


        [webView evaluateJavaScript:@"location.href;" inFrame:frame inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[isMainFrame] (id result, NSError *error) {
            EXPECT_NULL(error);
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);

            if (isMainFrame)
                EXPECT_TRUE([result isEqualToString:@"framestest://test/index.html"]);
            else
                EXPECT_TRUE([result isEqualToString:@"otherprotocol://test/index.html"]);

            if (++finishedFrames == allFrames.count * 2)
                isDone = true;
        }];
    }

    TestWebKitAPI::Util::run(&isDone);
}

// This test verifies that evaluating JavaScript in a frame that has since gone missing
// due to removal from the DOM results in an appropriate error
TEST(EvaluateJavaScript, JavaScriptInMissingFrameError)
{
    allFrames = [[NSMutableSet<WKFrameInfo *> alloc] init];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            NSData *data = [@"FooBarBaz" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    RetainPtr<WKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

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

    EXPECT_EQ(allFrames.count, 2u);

    static bool isDone = false;
    [webView evaluateJavaScript:@"var frame = document.getElementById('theFrame'); frame.parentNode.removeChild(frame);" inFrame:nil inContentWorld:WKContentWorld.defaultClientWorld completionHandler:[] (id result, NSError *error) {
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    for (WKFrameInfo *frame in allFrames) {
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
    allFrames = [[NSMutableSet<WKFrameInfo *> alloc] init];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"framestest://test/index.html"]) {
            NSData *data = [[NSString stringWithFormat:@"%s", framesMainResource] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"otherprotocol://test/index.html"]) {
            NSData *data = [@"FooBarBaz" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.absoluteString isEqualToString:@"framestest://index2.html"]) {
            NSData *data = [@"Hi" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else
            ASSERT_NOT_REACHED();
    }];

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"framestest"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"otherprotocol"];

    RetainPtr<WKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

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

    EXPECT_EQ(allFrames.count, 2u);

    RetainPtr<WKFrameInfo> iframe;
    for (WKFrameInfo *frame in allFrames) {
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
