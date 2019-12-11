/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#import "ParserYieldTokenTests.h"

#import "PlatformUtilities.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/BlockPtr.h>
#import <wtf/Seconds.h>

@interface ParserYieldTokenTestWebView : TestWKWebView <ParserYieldTokenTestRunner>
@property (nonatomic, readonly) BOOL finishedDocumentLoad;
@property (nonatomic, readonly) BOOL finishedLoad;
@end

@implementation ParserYieldTokenTestWebView {
    RetainPtr<id <ParserYieldTokenTestBundle>> _bundle;
    RetainPtr<TestURLSchemeHandler> _schemeHandler;
}

+ (RetainPtr<ParserYieldTokenTestWebView>)webView
{
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ParserYieldTokenPlugIn"];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"custom"];
    auto webView = adoptNS([[ParserYieldTokenTestWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:configuration]);
    [[webView _remoteObjectRegistry] registerExportedObject:webView.get() interface:[_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ParserYieldTokenTestRunner)]];
    webView->_bundle = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:[_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ParserYieldTokenTestBundle)]];
    webView->_schemeHandler = WTFMove(schemeHandler);
    return webView;
}

- (id <ParserYieldTokenTestBundle>)bundle
{
    return _bundle.get();
}

- (TestURLSchemeHandler *)schemeHandler
{
    return _schemeHandler.get();
}

- (void)didFinishDocumentLoad
{
    _finishedDocumentLoad = YES;
}

- (void)didFinishLoad
{
    _finishedLoad = YES;
}

@end

static void waitForDelay(Seconds delay)
{
    __block bool done = false;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay.nanoseconds()), dispatch_get_main_queue(), ^{
        done = true;
    });
    TestWebKitAPI::Util::run(&done);
}

TEST(ParserYieldTokenTests, PreventDocumentLoadByTakingParserYieldToken)
{
    auto webView = [ParserYieldTokenTestWebView webView];
    [[webView bundle] takeDocumentParserTokenAfterCommittingLoad];
    [webView loadTestPageNamed:@"simple"];

    // Give the web view a chance to load the page. In the case where the token fails to yield document parsing, it's very likely that
    // the document will finish loading, and we'll fail the test expectations below. In the unlikely event where page load may take
    // longer than 1 second, this test should still pass.
    waitForDelay(1_s);
    EXPECT_FALSE([webView finishedDocumentLoad]);
    EXPECT_FALSE([webView finishedLoad]);

    // Now, allow document parsing to continue, and wait for the page to finish loading.
    [[webView bundle] releaseDocumentParserToken];

    while (![webView finishedLoad] || ![webView finishedDocumentLoad])
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
}

TEST(ParserYieldTokenTests, TakeMultipleParserYieldTokens)
{
    auto webView = [ParserYieldTokenTestWebView webView];
    [[webView bundle] takeDocumentParserTokenAfterCommittingLoad];
    [webView loadTestPageNamed:@"simple"];

    waitForDelay(0.5_s);
    EXPECT_FALSE([webView finishedDocumentLoad]);
    EXPECT_FALSE([webView finishedLoad]);

    [[webView bundle] takeDocumentParserTokenAfterCommittingLoad];
    [[webView bundle] releaseDocumentParserToken];

    waitForDelay(0.5_s);
    EXPECT_FALSE([webView finishedDocumentLoad]);
    EXPECT_FALSE([webView finishedLoad]);

    [[webView bundle] releaseDocumentParserToken];

    while (![webView finishedLoad] || ![webView finishedDocumentLoad])
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
}

TEST(ParserYieldTokenTests, DeferredScriptExecutesBeforeDocumentLoadWhenTakingParserYieldToken)
{
    auto webView = [ParserYieldTokenTestWebView webView];

    [[webView bundle] takeDocumentParserTokenAfterCommittingLoad];
    [webView loadTestPageNamed:@"text-with-deferred-script"];

    waitForDelay(0.5_s);
    EXPECT_FALSE([webView finishedDocumentLoad]);
    EXPECT_FALSE([webView finishedLoad]);

    [[webView bundle] releaseDocumentParserToken];

    while (![webView finishedLoad] || ![webView finishedDocumentLoad])
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];

    // Verify that synchronous script is executed before deferred script and deferred script is run before document load.
    NSArray<NSString *> *eventMessages = [webView objectByEvaluatingJavaScript:@"window.eventMessages"];
    EXPECT_EQ(eventMessages.count, 8U);
    EXPECT_WK_STREQ("Running sync", eventMessages[0]);
    EXPECT_WK_STREQ("Loaded script: sync", eventMessages[1]);
    EXPECT_WK_STREQ("Running defer-before", eventMessages[2]);
    EXPECT_WK_STREQ("Loaded script: defer-before", eventMessages[3]);
    EXPECT_WK_STREQ("Running defer-after", eventMessages[4]);
    EXPECT_WK_STREQ("Loaded script: defer-after", eventMessages[5]);
    EXPECT_WK_STREQ("Document finished loading", eventMessages[6]);
    EXPECT_WK_STREQ("Finished loading", eventMessages[7]);
}

TEST(ParserYieldTokenTests, AsyncScriptRunsWhenFetched)
{
    auto webView = [ParserYieldTokenTestWebView webView];
    [webView schemeHandler].startURLSchemeTaskHandler = [] (WKWebView *, id <WKURLSchemeTask> task) {
        auto script = retainPtr(@"window.eventMessages.push('Running async script.');");
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/javascript" expectedContentLength:[script length] textEncodingName:nil]);
        dispatch_async(dispatch_get_main_queue(), [task = retainPtr(task), response = WTFMove(response), script = WTFMove(script)] {
            [task didReceiveResponse:response.get()];
            [task didReceiveData:[script dataUsingEncoding:NSUTF8StringEncoding]];
            [task didFinish];
        });
    };

    [[webView bundle] takeDocumentParserTokenAfterCommittingLoad];

    NSURL *pageURL = [[NSBundle mainBundle] URLForResource:@"text-with-async-script" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadHTMLString:[NSString stringWithContentsOfURL:pageURL encoding:NSUTF8StringEncoding error:nil] baseURL:[NSURL URLWithString:@"custom://"]];

    waitForDelay(0.5_s);
    EXPECT_FALSE([webView finishedDocumentLoad]);
    EXPECT_FALSE([webView finishedLoad]);

    [[webView bundle] releaseDocumentParserToken];

    while (![webView finishedLoad] || ![webView finishedDocumentLoad])
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];

    NSArray<NSString *> *eventMessages = [webView objectByEvaluatingJavaScript:@"window.eventMessages"];
    EXPECT_EQ(eventMessages.count, 4U);
    EXPECT_WK_STREQ("Before requesting async script.", eventMessages[0]);
    EXPECT_WK_STREQ("After requesting async script.", eventMessages[1]);
    EXPECT_WK_STREQ("Running async script.", eventMessages[2]);
    EXPECT_WK_STREQ("Finished requesting async script.", eventMessages[3]);
}
