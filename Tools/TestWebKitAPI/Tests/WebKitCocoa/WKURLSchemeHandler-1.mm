/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if WK_API_ENABLED

static bool done;

@interface SchemeHandler : NSObject <WKURLSchemeHandler>
@property (readonly) NSMutableArray<NSURL *> *startedURLs;
@property (readonly) NSMutableArray<NSURL *> *stoppedURLs;
@property (assign) BOOL shouldFinish;
- (instancetype)initWithData:(NSData *)data mimeType:(NSString *)inMIMEType;
@end

@implementation SchemeHandler {
    RetainPtr<NSData> resourceData;
    RetainPtr<NSString> mimeType;
}

- (instancetype)initWithData:(NSData *)data mimeType:(NSString *)inMIMEType
{
    self = [super init];
    if (!self)
        return nil;

    resourceData = data;
    mimeType = inMIMEType;
    _startedURLs = [[NSMutableArray alloc] init];
    _stoppedURLs = [[NSMutableArray alloc] init];
    _shouldFinish = YES;

    return self;
}

- (void)dealloc
{
    [_startedURLs release];
    [_stoppedURLs release];
    [super dealloc];
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    [_startedURLs addObject:task.request.URL];

    // Always fail the image load.
    if ([task.request.URL.absoluteString isEqualToString:@"testing:image"]) {
        [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
        done = true;
        return;
    }

    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType.get() expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:resourceData.get()];
    if (_shouldFinish)
        [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    [_stoppedURLs addObject:task.request.URL];

    done = true;
}

@end

@interface URLSchemeHandlerAsyncNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation URLSchemeHandlerAsyncNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(WKNavigationActionPolicyAllow);
    });

}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(WKNavigationResponsePolicyAllow);
    });
}
@end


static const char mainBytes[] =
"<html>" \
"<img src='testing:image'>" \
"</html>";

TEST(URLSchemeHandler, Basic)
{
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO] mimeType:@"text/html"]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ([handler.get().startedURLs count], 2u);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:1] isEqual:[NSURL URLWithString:@"testing:image"]]);
    EXPECT_EQ([handler.get().stoppedURLs count], 0u);
}

TEST(URLSchemeHandler, BasicWithAsyncPolicyDelegate)
{
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO] mimeType:@"text/html"]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[URLSchemeHandlerAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ([handler.get().startedURLs count], 2u);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:1] isEqual:[NSURL URLWithString:@"testing:image"]]);
    EXPECT_EQ([handler.get().stoppedURLs count], 0u);
}

TEST(URLSchemeHandler, NoMIMEType)
{
    // Since there's no MIMEType, and no NavigationDelegate to tell WebKit to do the load anyways, WebKit will ignore (silently fail) the load.
    // This test makes sure that is communicated back to the URLSchemeHandler.

    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO] mimeType:nil]);
    handler.get().shouldFinish = NO;
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ([handler.get().startedURLs count], 1u);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
    EXPECT_EQ([handler.get().stoppedURLs count], 1u);
    EXPECT_TRUE([[handler.get().stoppedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
}

static NSString *schemes[] = {
    @"about",
    @"applewebdata",
    @"blob",
    @"data",
    @"file",
    @"ftp",
    @"gopher",
    @"http",
    @"https",
    @"javascript",
    @"webkit-fake-url",
    @"ws",
    @"wss",
#if PLATFORM(MAC)
    @"safari-extension",
#endif
#if ENABLE(CONTENT_FILTERING)
    @"x-apple-content-filter",
#endif
#if USE(QUICK_LOOK)
    @"x-apple-ql-id",
#endif
};

TEST(URLSchemeHandler, BuiltinSchemes)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:nil mimeType:nil]);

    for (NSString *scheme : schemes) {
        EXPECT_TRUE([WKWebView handlesURLScheme:scheme]);

        bool exceptionRaised = false;
        @try {
            [configuration setURLSchemeHandler:handler.get() forURLScheme:scheme];
        } @catch (NSException *exception) {
            EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
            exceptionRaised = true;
        }
        EXPECT_TRUE(exceptionRaised);
    }
}

static bool receivedRedirect;
static bool responsePolicyDecided;

@interface RedirectSchemeHandler : NSObject <WKURLSchemeHandler, WKNavigationDelegate, WKScriptMessageHandler>
@end

@implementation RedirectSchemeHandler { }

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    ASSERT_STREQ(task.request.URL.absoluteString.UTF8String, "testing:///initial");
    auto redirectResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:nil expectedContentLength:0 textEncodingName:nil]);
    auto request = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"testing:///redirected"]]);
    [(id<WKURLSchemeTaskPrivate>)task _didPerformRedirection:redirectResponse.get() newRequest:request.get()];
    ASSERT_FALSE(receivedRedirect);
    ASSERT_STREQ(task.request.URL.absoluteString.UTF8String, "testing:///redirected");
    NSString *html = @"<script>window.webkit.messageHandlers.testHandler.postMessage('Document URL: ' + document.URL);</script>";
    auto finalResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:html.length textEncodingName:nil]);
    [task didReceiveResponse:finalResponse.get()];
    [task didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    ASSERT_TRUE(false);
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(null_unspecified WKNavigation *)navigation
{
    ASSERT_FALSE(receivedRedirect);
    receivedRedirect = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    ASSERT_TRUE(receivedRedirect);
    ASSERT_STREQ(navigationResponse.response.URL.absoluteString.UTF8String, "testing:///redirected");
    ASSERT_FALSE(responsePolicyDecided);
    responsePolicyDecided = true;
    decisionHandler(WKNavigationResponsePolicyAllow);
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(@"Document URL: testing:///redirected", [message body]);
    done = true;
}
@end

TEST(URLSchemeHandler, Redirection)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[RedirectSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:handler.get()];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///initial"]];
    [webView loadRequest:request];
    
    TestWebKitAPI::Util::run(&done);
    
    EXPECT_TRUE(responsePolicyDecided);
    EXPECT_STREQ(webView.get().URL.absoluteString.UTF8String, "testing:///redirected");
}

enum class Command {
    Redirect,
    Response,
    Data,
    Finish,
    Error,
};

@interface TaskSchemeHandler : NSObject <WKURLSchemeHandler>
- (instancetype)initWithCommands:(Vector<Command>&&)commandVector expectedException:(bool)expected;
@end

@implementation TaskSchemeHandler {
    Vector<Command> commands;
    bool expectedException;
}

- (instancetype)initWithCommands:(Vector<Command>&&)commandVector expectedException:(bool)expected
{
    self = [super init];
    if (!self)
        return nil;
    
    self->commands = WTFMove(commandVector);
    self->expectedException = expected;
    
    return self;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    bool caughtException = false;
    @try {
        for (auto command : commands) {
            switch (command) {
            case Command::Redirect:
                [(id<WKURLSchemeTaskPrivate>)task _didPerformRedirection:[[[NSURLResponse alloc] init] autorelease] newRequest:[[[NSURLRequest alloc] init] autorelease]];
                break;
            case Command::Response:
                [task didReceiveResponse:[[[NSURLResponse alloc] init] autorelease]];
                break;
            case Command::Data:
                [task didReceiveData:[[[NSData alloc] init] autorelease]];
                break;
            case Command::Finish:
                [task didFinish];
                break;
            case Command::Error:
                [task didFailWithError:[NSError errorWithDomain:@"WebKit" code:1 userInfo:nil]];
                break;
            }
        }
    }
    @catch(NSException *exception)
    {
        caughtException = true;
    }
    ASSERT_EQ(caughtException, expectedException);
    done = true;
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}
@end

enum class ShouldRaiseException { No, Yes };

static void checkCallSequence(Vector<Command>&& commands, ShouldRaiseException shouldRaiseException)
{
    done = false;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[TaskSchemeHandler alloc] initWithCommands:WTFMove(commands) expectedException:shouldRaiseException == ShouldRaiseException::Yes]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///initial"]]];
    TestWebKitAPI::Util::run(&done);
}

TEST(URLSchemeHandler, Exceptions)
{
    checkCallSequence({Command::Response, Command::Data, Command::Finish}, ShouldRaiseException::No);
    checkCallSequence({Command::Response, Command::Redirect}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Redirect, Command::Response}, ShouldRaiseException::No);
    checkCallSequence({Command::Data, Command::Finish}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Error}, ShouldRaiseException::No);
    checkCallSequence({Command::Error, Command::Error}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Error, Command::Data}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Response, Command::Finish, Command::Data}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Response, Command::Finish, Command::Redirect}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Response, Command::Finish, Command::Response}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Response, Command::Finish, Command::Finish}, ShouldRaiseException::Yes);
    checkCallSequence({Command::Response, Command::Finish, Command::Error}, ShouldRaiseException::Yes);
}

#endif
