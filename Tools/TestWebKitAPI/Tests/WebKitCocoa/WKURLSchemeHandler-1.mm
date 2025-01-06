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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFrameHandle.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/WTFString.h>

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

TEST(URLSchemeHandler, BasicWithHTTPS)
{
    using namespace TestWebKitAPI;

    done = false;

    HTTPServer httpsServer({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, String::fromUTF8(mainBytes) } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];

    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO] mimeType:@"text/html"]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site1.example/"]];
    [webView loadRequest:request];
    Util::run(&done);

    EXPECT_EQ([handler.get().startedURLs count], 1u);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:image"]]);
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

static NSString *handledSchemes[] = {
    @"about",
    @"applewebdata",
    @"blob",
    @"data",
    @"file",
    @"ftp",
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

static NSString *notHandledSchemes[] = {
    @"gopher",
    @"my-custom-scheme",
};

TEST(URLSchemeHandler, BuiltinSchemes)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:nil mimeType:nil]);

    for (NSString *scheme : handledSchemes) {
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
    for (NSString *scheme : notHandledSchemes) {
        EXPECT_FALSE([WKWebView handlesURLScheme:scheme]);

        bool exceptionRaised = false;
        @try {
            [configuration setURLSchemeHandler:handler.get() forURLScheme:scheme];
        } @catch (NSException *exception) {
            exceptionRaised = true;
        }
        EXPECT_FALSE(exceptionRaised);
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

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
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
    APIRedirect,
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
                [(id<WKURLSchemeTaskPrivate>)task _didPerformRedirection:adoptNS([[NSURLResponse alloc] init]).get() newRequest:adoptNS([[NSURLRequest alloc] init]).get()];
                break;
            case Command::APIRedirect:
                [(id<WKURLSchemeTaskPrivate>)task _willPerformRedirection:adoptNS([[NSURLResponse alloc] init]).get() newRequest:adoptNS([[NSURLRequest alloc] init]).get() completionHandler:^(NSURLRequest*) { }];
                break;
            case Command::Response:
                [task didReceiveResponse:adoptNS([[NSURLResponse alloc] init]).get()];
                break;
            case Command::Data:
                [task didReceiveData:adoptNS([[NSData alloc] init]).get()];
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

enum class ShouldRaiseException : bool { No, Yes };

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
    checkCallSequence({Command::APIRedirect, Command::Redirect}, ShouldRaiseException::Yes);
    checkCallSequence({Command::APIRedirect, Command::Response}, ShouldRaiseException::Yes);
    checkCallSequence({Command::APIRedirect, Command::Data}, ShouldRaiseException::Yes);
    checkCallSequence({Command::APIRedirect, Command::Finish}, ShouldRaiseException::Yes);
    checkCallSequence({Command::APIRedirect, Command::Error}, ShouldRaiseException::No);
}

struct SchemeResourceInfo {
    RetainPtr<NSString> mimeType;
    const char* data;
    bool shouldRespond;
};

static bool startedXHR;
static bool receivedStop;

@interface SyncScheme : NSObject <WKURLSchemeHandler> {
@public
    HashMap<String, SchemeResourceInfo> resources;
}
@end

@implementation SyncScheme

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    auto entry = resources.find([task.request.URL absoluteString]);
    if (entry == resources.end()) {
        NSLog(@"Did not find resource entry for URL %@", task.request.URL);
        return;
    }

    if (entry->key == "syncxhr://host/test.dat"_s)
        startedXHR = true;

    if (!entry->value.shouldRespond)
        return;
    
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:entry->value.mimeType.get() expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];

    [task didReceiveData:[NSData dataWithBytesNoCopy:(void*)entry->value.data length:strlen(entry->value.data) freeWhenDone:NO]];
    [task didFinish];

    if (entry->key == "syncxhr://host/test.dat"_s)
        startedXHR = false;
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    EXPECT_TRUE([[task.request.URL absoluteString] isEqualToString:@"syncxhr://host/test.dat"]);
    receivedStop = true;
}

@end

static bool receivedMessage;

@interface SyncMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SyncMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message body])
        [receivedMessages addObject:[message body]];
    else
        [receivedMessages addObject:@""];

    receivedMessage = true;
}
@end

static const char syncMainBytes[] = R"SYNCRESOURCE(
<script>

var req = new XMLHttpRequest();
req.open("GET", "test.dat", false);
try
{
    req.send(null);
    window.webkit.messageHandlers.sync.postMessage(req.responseText);
}
catch (e)
{
    window.webkit.messageHandlers.sync.postMessage("Failed sync XHR load");
}

</script>
)SYNCRESOURCE";

static const char syncXHRBytes[] = "My XHR text!";

TEST(URLSchemeHandler, SyncXHR)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto handler = adoptNS([[SyncScheme alloc] init]);
        [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"syncxhr"];

        handler.get()->resources.set("syncxhr://host/main.html"_s, SchemeResourceInfo { @"text/html", syncMainBytes, true });
        handler.get()->resources.set("syncxhr://host/test.dat"_s, SchemeResourceInfo { @"text/plain", syncXHRBytes, true });

        auto messageHandler = adoptNS([[SyncMessageHandler alloc] init]);
        [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sync"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"syncxhr://host/main.html"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&receivedMessage);
        receivedMessage = false;

        EXPECT_EQ((unsigned)receivedMessages.get().count, (unsigned)1);
        EXPECT_TRUE([receivedMessages.get()[0] isEqualToString:@"My XHR text!"]);

        // Now try again, but hang the WebProcess in the reply to the XHR by telling the scheme handler to never
        // respond to it.
        handler.get()->resources.find("syncxhr://host/test.dat"_s)->value.shouldRespond = false;
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&startedXHR);
        receivedMessage = false;

        [webView _close];
    }
    
    TestWebKitAPI::Util::run(&receivedStop);
}

@interface SyncErrorScheme : NSObject <WKURLSchemeHandler, WKUIDelegate>
@end

@implementation SyncErrorScheme

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([task.request.URL.absoluteString isEqualToString:@"syncerror:///main.html"]) {
        static const char* bytes = "<script>var xhr=new XMLHttpRequest();xhr.open('GET','subresource',false);try{xhr.send(null);alert('no error')}catch(e){alert(e)}</script>";
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:strlen(bytes) textEncodingName:nil]).get()];
        [task didReceiveData:[NSData dataWithBytes:bytes length:strlen(bytes)]];
        [task didFinish];
    } else {
        EXPECT_STREQ(task.request.URL.absoluteString.UTF8String, "syncerror:///subresource");
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] init]).get()];
        [task didFailWithError:[NSError errorWithDomain:@"TestErrorDomain" code:123 userInfo:nil]];
    }
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ(message.UTF8String, "NetworkError:  A network error occurred.");
    completionHandler();
    done = true;
}

@end

TEST(URLSchemeHandler, SyncXHRError)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[SyncErrorScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"syncerror"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView setUIDelegate:handler.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"syncerror:///main.html"]]];
    TestWebKitAPI::Util::run(&done);
}

static constexpr auto xhrPostDocument = R"XHRPOSTRESOURCE(<html><head><script>
window.onload = function()
{
    {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/arraybuffer');
        
        var chars = [];
        var str = "Hi there";
        for (var i = 0; i < str.length; ++i)
            chars.push(str.charCodeAt(i));

        xhr.send(new Uint8Array(chars));
    }
    {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/string');
        xhr.send('foo=bar');
    }
    {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/string-upload');
        var upload = xhr.upload;
        xhr.send('foo=bar2');
    }
    {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/document');
        xhr.send(window.document);
    }
    {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/formdata');
        
        var formData = new FormData();
        formData.append("foo", "baz");
        xhr.send(formData);
    }
    {
//        // FIXME: XHR posting of Blobs is currently unsupported
//        // https://bugs.webkit.org/show_bug.cgi?id=197237
//        var xhr = new XMLHttpRequest();
//        xhr.open('POST', '/blob');
//        var blob = new Blob(["Hello world!"], {type: "text/plain"});
//        xhr.send(blob);
    }
};
</script></head>
<body>
Hello world!
</body></html>)XHRPOSTRESOURCE"_s;


TEST(URLSchemeHandler, XHRPost)
{
    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"xhrpost"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    static bool done;
    static uint8_t seenTasks;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.absoluteString isEqualToString:@"xhrpost://example/string"]) {
            static bool reached;
            EXPECT_FALSE(reached);
            reached = true;
            EXPECT_EQ(task.request.HTTPBody.length, 7u);
            EXPECT_STREQ(static_cast<const char*>(task.request.HTTPBody.bytes), "foo=bar");
        } else if ([task.request.URL.absoluteString isEqualToString:@"xhrpost://example/string-upload"]) {
            static bool reached;
            EXPECT_FALSE(reached);
            reached = true;
            auto stream = task.request.HTTPBodyStream;
            EXPECT_TRUE(!!stream);
            [stream open];
            EXPECT_TRUE(stream.hasBytesAvailable);
            std::array<uint8_t, 9> buffer = { };
            auto length = [stream read:buffer.data() maxLength:buffer.size()];
            EXPECT_EQ(length, 8);
            EXPECT_STREQ(reinterpret_cast<const char*>(buffer.data()), "foo=bar2");
            EXPECT_FALSE(stream.hasBytesAvailable);
            [stream close];
        } else if ([task.request.URL.absoluteString isEqualToString:@"xhrpost://example/arraybuffer"]) {
            static bool reached;
            EXPECT_FALSE(reached);
            reached = true;
            EXPECT_EQ(task.request.HTTPBody.length, 8u);
            EXPECT_STREQ(static_cast<const char*>(task.request.HTTPBody.bytes), "Hi there");
        } else if ([task.request.URL.absoluteString isEqualToString:@"xhrpost://example/document"]) {
            static bool reached;
            EXPECT_FALSE(reached);
            reached = true;
            EXPECT_EQ(task.request.HTTPBody.length, strlen(xhrPostDocument));
            EXPECT_STREQ(static_cast<const char*>(task.request.HTTPBody.bytes), xhrPostDocument);
        } else if ([task.request.URL.absoluteString isEqualToString:@"xhrpost://example/formdata"]) {
            static bool reached;
            EXPECT_FALSE(reached);
            reached = true;
            // The length of this is variable
            auto *formDataString = [NSString stringWithUTF8String:static_cast<const char*>(task.request.HTTPBody.bytes)];
            EXPECT_TRUE([formDataString containsString:@"Content-Disposition: form-data; name=\"foo\""]);
            EXPECT_TRUE([formDataString containsString:@"baz"]);
            EXPECT_TRUE([formDataString containsString:@"WebKitFormBoundary"]);
        } else if ([task.request.URL.absoluteString isEqualToString:@"xhrpost://example/blob"]) {
            static bool reached;
            EXPECT_FALSE(reached);
            reached = true;
            
            // FIXME: XHR posting of Blobs is currently unsupported
            // https://bugs.webkit.org/show_bug.cgi?id=197237

            FAIL();
        } else {
            // We only expect one of the 5 URLs up above.
            FAIL();
        }

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didFinish];
        
        if (++seenTasks == 5)
            done = true;
    }];
    
    [webView loadHTMLString:[NSString stringWithUTF8String:xhrPostDocument] baseURL:[NSURL URLWithString:@"xhrpost://example/xhrtest"]];
    TestWebKitAPI::Util::run(&done);
}

TEST(URLSchemeHandler, Threads)
{
    static bool done;
    static NeverDestroyed<RetainPtr<id<WKURLSchemeTask>>> theTask;
    static RefPtr<Thread> theThread;

    @autoreleasepool {
        auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setURLSchemeHandler:handler.get() forURLScheme:@"threads"];
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
            theTask.get() = retainPtr(task);
            theThread = Thread::create("A"_s, [task] {
                auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
                [task didReceiveResponse:response.get()];
                [task didFinish];
                done = true;
            });
        }];

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"threads://main.html"]]];

        TestWebKitAPI::Util::run(&done);

        handler = nil;
        configuration = nil;
        webView = nil;
        theThread = nullptr;
    }

    Thread::create("B"_s, [] {
        theTask.get() = nil;
    })->waitForCompletion();
}

TEST(URLSchemeHandler, CORS)
{
    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    __block bool done = false;
    __block bool includeCORSHeaderFieldInResponse = false;
    __block bool corssuccess = false;
    __block bool corsfailure = false;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [@"<script>fetch('cors://host2/corsresource').then(function(){fetch('/corssuccess')}).catch(function(){fetch('/corsfailure')})</script>" dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/corsresource"]) {
            if (includeCORSHeaderFieldInResponse) {
                [task didReceiveResponse:adoptNS([[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:200 HTTPVersion:nil headerFields:@{
                    @"Access-Control-Allow-Origin": @"*",
                    @"Content-Length": @"2",
                    @"Content-Type":@"text/html"
                }]).get()];
            } else
                [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
            [task didReceiveData:[@"HI" dataUsingEncoding:NSUTF8StringEncoding]];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/corssuccess"]) {
            corssuccess = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/corsfailure"]) {
            corsfailure = true;
            done = true;
        }
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(corsfailure);
    EXPECT_FALSE(corssuccess);

    corsfailure = false;
    corssuccess = false;
    done = false;

    includeCORSHeaderFieldInResponse = true;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(URLSchemeHandler, DisableCORS)
{
    TestWebKitAPI::HTTPServer server({
        { "/subresource"_s, { {{ "Content-Type"_s, "application/json"_s }, { "headerName"_s, "headerValue"_s }}, "{\"testKey\":\"testValue\"}"_s } }
    });

    bool corssuccess = false;
    bool corsfailure = false;
    bool done = false;

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    NSString *testJS = [NSString stringWithFormat:
        @"fetch('http://127.0.0.1:%d/subresource').then(async (r) => {"
            "if (r.headers.get('headerName') != 'headerValue')"
                "return fetch('/corsfailure');"
            "const object = await r.json();"
            "if (object.testKey != 'testValue')"
                "return fetch('/corsfailure');"
            "fetch('/corssuccess');"
        "}).catch(function(){fetch('/corsfailure')})"
        , server.port()];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script>%@</script>", testJS] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/corssuccess"]) {
            corssuccess = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/corsfailure"]) {
            corsfailure = true;
            done = true;
        } else
            ASSERT_NOT_REACHED();
    }];
    
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);
    
    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration.get()._corsDisablingPatterns = @[@"*://*/*"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);

    corssuccess = false;
    corsfailure = false;
    done = false;
}

TEST(URLSchemeHandler, DisableCORSCredentials)
{
    TestWebKitAPI::HTTPServer server({
        { "/subresource"_s, { {{ "Access-Control-Allow-Origin"_s, "*"_s }}, "subresourcecontent"_s } }
    });

    bool corssuccess = false;
    bool corsfailure = false;
    bool done = false;

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script>fetch('http://127.0.0.1:%d/subresource', {credentials:'include'}).then(function(){fetch('/corssuccess')}).catch(function(){fetch('/corsfailure')})</script>", server.port()] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/corssuccess"]) {
            corssuccess = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/corsfailure"]) {
            corsfailure = true;
            done = true;
        } else
            ASSERT_NOT_REACHED();
    }];
    
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);
    
    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration.get()._crossOriginAccessControlCheckEnabled = NO;
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(URLSchemeHandler, DisableCORSScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "fetch('loadSuccess')"_s } }
    });

    bool loadSuccess = false;
    bool loadFail = false;
    bool done = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script type='text/javascript' crossorigin='anonymous' onerror='fetch(\"loadFail\")' src='http://127.0.0.1:%d/'></script>", server.port()] dataUsingEncoding:NSUTF8StringEncoding];
            
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/loadSuccess"]) {
            loadSuccess = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/loadFail"]) {
            loadFail = true;
            done = true;
        } else
            ASSERT_NOT_REACHED();
    }];

    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(loadSuccess);
    EXPECT_TRUE(loadFail);
    
    loadSuccess = false;
    loadFail = false;
    done = false;

    configuration.get()._corsDisablingPatterns = @[@"*://*/*"];
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_TRUE(loadSuccess);
    EXPECT_FALSE(loadFail);
}

TEST(URLSchemeHandler, DisableCORSCanvas)
{
    bool corssuccess = false;
    bool corsfailure = false;
    bool done = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        NSData *response = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            mimeType = @"text/html";
            response = [@"<canvas id='canvas'></canvas><img src='cors://host2/image.png' onload='imageloaded()' id='img'></img><script>"
                "function imageloaded() {"
                    "let canvas = document.getElementById('canvas');"
                    "let context = canvas.getContext('2d');"
                    "let img = document.getElementById('img');"
                    "context.drawImage(img, 0, 0);"
                    "try {"
                        "let dataURL = canvas.toDataURL('image/png', 1);"
                        "fetch('corssuccess');"
                    "} catch(err) {"
                        "fetch('corsfailure');"
                    "}"
                "}"
            "</script>" dataUsingEncoding:NSUTF8StringEncoding];
        } else if ([task.request.URL.path isEqualToString:@"/corssuccess"]) {
            corssuccess = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/corsfailure"]) {
            corsfailure = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/image.png"]) {
            mimeType = @"image/png";
            response = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"400x400-green" withExtension:@"png"]];
        } else
            ASSERT_NOT_REACHED();

        if (response) {
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:response.length textEncodingName:nil]).get()];
            [task didReceiveData:response];
            [task didFinish];
        }
    }];

    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);

    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration.get()._corsDisablingPatterns = @[@"cors://*/*"];
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(URLSchemeHandler, DisableCORSAndCORP)
{
    TestWebKitAPI::HTTPServer server({
        { "/subresource"_s, { {{ "Content-Type"_s, "application/json"_s }, { "Cross-Origin-Resource-Policy"_s, "same-origin"_s }}, "{\"testKey\":\"testValue\"}"_s } }
    });

    bool corssuccess = false;
    bool corsfailure = false;
    bool done = false;

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    NSString *testJS = [NSString stringWithFormat:
        @"fetch('http://127.0.0.1:%d/subresource').then(async (r) => {"
            "const object = await r.json();"
            "if (object.testKey != 'testValue')"
                "return fetch('/corsfailure');"
            "fetch('/corssuccess');"
        "}).catch(function(){fetch('/corsfailure')})"
        , server.port()];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script>%@</script>", testJS] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]).get()];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/corssuccess"]) {
            corssuccess = true;
            done = true;
        } else if ([task.request.URL.path isEqualToString:@"/corsfailure"]) {
            corsfailure = true;
            done = true;
        } else
            ASSERT_NOT_REACHED();
    }];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];

    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);

    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration.get()._corsDisablingPatterns = @[ @"*://*/*" ];
    webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];

    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(WebKit, OriginHeaderWithCORSDisablingPatternsInUnrelatedWebView)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get()._corsDisablingPatterns = @[ @"*://*/*" ];
    auto addPatterns = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [addPatterns synchronouslyLoadHTMLString:@"start network process and add patterns"];

    using namespace TestWebKitAPI;
    bool done { false };
    HTTPServer server([&] (Connection connection) {
        connection.receiveHTTPRequest([&, connection](Vector<char>&& requestBytes) {
            auto path = HTTPServer::parsePath(requestBytes);
            if (path == "/"_s) {
                auto html = "<head><link rel='modulepreload' href='https://webkit.org/module'></head>"_s;
                connection.send(HTTPResponse(html).serialize());
            } else if (path == "/module"_s) {
                EXPECT_TRUE(strnstr(requestBytes.data(), "Origin: https://example.com\r\n", requestBytes.size()));
                done = true;
            }
        });
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:server.httpsProxyConfiguration()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    TestWebKitAPI::Util::run(&done);
}

TEST(URLSchemeHandler, LoadsFromNetwork)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { {{ "Access-Control-Allow-Origin"_s, "*"_s }}, "test content"_s } }
    });

    HTTPServer webSocketServer([](Connection connection) {
        connection.webSocketHandshake();
    });

    std::optional<bool> loadSuccess;
    std::optional<bool> webSocketSuccess;
    bool done = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main.html"]) {
            respond(task, [NSString stringWithFormat:@"<script>"
                "function checkWebSockets() {"
                    "var ws = new WebSocket('ws://127.0.0.1:%d');"
                    "ws.onerror = function() { fetch('/webSocketFail') };"
                    "ws.onopen = function() { fetch('/webSocketSuccess') };"
                "}"
                "fetch('http://localhost:%d/').then(()=>{"
                    "fetch('/loadSuccess').then(()=>{ checkWebSockets() })"
                "}).catch(()=>{"
                    "fetch('/loadFail').then(()=>{ checkWebSockets() })"
                "})"
                "</script>", webSocketServer.port(), server.port()].UTF8String);
        } else if ([path isEqualToString:@"/loadSuccess"]) {
            respond(task, "hi");
            loadSuccess = true;
        } else if ([path isEqualToString:@"/loadFail"]) {
            respond(task, "hi");
            loadSuccess = false;
        } else if ([path isEqualToString:@"/webSocketSuccess"]) {
            webSocketSuccess = true;
            done = true;
        } else if ([path isEqualToString:@"/webSocketFail"]) {
            webSocketSuccess = false;
            done = true;
        } else
            ASSERT_NOT_REACHED();
    }];
    
    auto runTest = [&] {
        loadSuccess = std::nullopt;
        webSocketSuccess = std::nullopt;
        done = false;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    };

    runTest();
    EXPECT_TRUE(*loadSuccess);
    EXPECT_TRUE(*webSocketSuccess);
    EXPECT_EQ(server.totalRequests(), 1u);

    configuration.get()._loadsFromNetwork = NO;
    runTest();
    EXPECT_FALSE(*loadSuccess);
    EXPECT_FALSE(*webSocketSuccess);
    EXPECT_EQ(server.totalRequests(), 1u);
    
    configuration.get()._allowedNetworkHosts = [NSSet set];
    runTest();
    EXPECT_FALSE(*loadSuccess);
    EXPECT_FALSE(*webSocketSuccess);
    EXPECT_EQ(server.totalRequests(), 1u);

    configuration.get()._allowedNetworkHosts = nil;
    runTest();
    EXPECT_TRUE(*loadSuccess);
    EXPECT_TRUE(*webSocketSuccess);
    EXPECT_EQ(server.totalRequests(), 2u);

    configuration.get()._allowedNetworkHosts = [NSSet setWithObject:@"localhost"];
    runTest();
    EXPECT_TRUE(*loadSuccess);
    EXPECT_FALSE(*webSocketSuccess);
    EXPECT_EQ(server.totalRequests(), 3u);
}

TEST(URLSchemeHandler, AllowedNetworkHostsRedirect)
{
    TestWebKitAPI::HTTPServer serverLocalhost({
        { "/redirectTarget"_s, { {{ "Access-Control-Allow-Origin"_s, "*"_s }}, "test content"_s } }
    });
    TestWebKitAPI::HTTPServer server127001({
        { "/"_s, { 301, {
            { "Access-Control-Allow-Origin"_s, "*"_s },
            { "Location"_s, makeString("http://localhost:"_s, serverLocalhost.port(), "/redirectTarget"_s) }
        }}},
    });

    std::optional<bool> loadSuccess;
    bool done = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main.html"]) {
            respond(task, [NSString stringWithFormat:@"<script>"
                "fetch('http://127.0.0.1:%d/').then(()=>{"
                    "fetch('/loadSuccess')"
                "}).catch(()=>{"
                    "fetch('/loadFail')"
                "})"
                "</script>", server127001.port()].UTF8String);
        } else if ([path isEqualToString:@"/loadSuccess"]) {
            loadSuccess = true;
            done = true;
        } else if ([path isEqualToString:@"/loadFail"]) {
            loadSuccess = false;
            done = true;
        }
    }];

    auto runTest = [&] {
        loadSuccess = std::nullopt;
        done = false;
        configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    };

    runTest();
    EXPECT_TRUE(*loadSuccess);
    EXPECT_EQ(serverLocalhost.totalRequests(), 1u);
    EXPECT_EQ(server127001.totalRequests(), 1u);
    
    configuration.get()._allowedNetworkHosts = [NSSet set];
    runTest();
    EXPECT_FALSE(*loadSuccess);
    EXPECT_EQ(serverLocalhost.totalRequests(), 1u);
    EXPECT_EQ(server127001.totalRequests(), 1u);

    configuration.get()._allowedNetworkHosts = [NSSet setWithObject:@"127.0.0.1"];
    runTest();
    EXPECT_FALSE(*loadSuccess);
    EXPECT_EQ(serverLocalhost.totalRequests(), 1u);
    EXPECT_EQ(server127001.totalRequests(), 2u);
}

static void serverLoop(const TestWebKitAPI::Connection& connection, bool& loadedImage, bool& loadedIFrame)
{
    using namespace TestWebKitAPI;
    connection.receiveHTTPRequest([&, connection] (Vector<char>&& request) {
        auto path = HTTPServer::parsePath(request);
        auto sendReply = [&, connection] (const HTTPResponse& response) {
            connection.send(response.serialize(), [&, connection] {
                serverLoop(connection, loadedImage, loadedIFrame);
            });
        };
        if (path == "/main.html"_s)
            sendReply({ { { "Content-Type"_s, "text/html"_s } }, "<img src='/imgsrc'></img><iframe src='/iframesrc'></iframe>"_s });
        else if (path == "/imgsrc"_s) {
            loadedImage = true;
            sendReply({ "image content"_s });
        } else if (path == "/iframesrc"_s) {
            loadedIFrame = true;
            sendReply({ "iframe content"_s });
        } else
            ASSERT_NOT_REACHED();
    });
}

TEST(WKWebViewConfiguration, LoadsSubresources)
{
    bool loadedImage = false;
    bool loadedIFrame = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    TestWebKitAPI::HTTPServer server([&] (const TestWebKitAPI::Connection& connection) {
        serverLoop(connection, loadedIFrame, loadedImage);
    });

    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadRequest:server.request("/main.html"_s)];
        TestWebKitAPI::Util::run(&loadedImage);
        TestWebKitAPI::Util::run(&loadedIFrame);
    }
    
    loadedImage = false;
    loadedIFrame = false;

    configuration.get()._loadsSubresources = NO;
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        auto delegate = adoptNS([TestNavigationDelegate new]);
        webView.get().navigationDelegate = delegate.get();
        [webView loadRequest:server.request("/main.html"_s)];
        [delegate waitForDidFinishNavigation];
        TestWebKitAPI::Util::spinRunLoop(100);
        EXPECT_FALSE(loadedIFrame);
        EXPECT_FALSE(loadedImage);
    }
}

@interface FrameSchemeHandler : NSObject <WKURLSchemeHandler>
- (void)waitForAllRequests;
- (void)setExpectedWebView:(WKWebView *)webView;
@end

@implementation FrameSchemeHandler {
    size_t _requestCount;
    WeakObjCPtr<WKWebView> _webView;
}

- (void)waitForAllRequests
{
    while (_requestCount < 3)
        TestWebKitAPI::Util::spinRunLoop();
}

- (void)setExpectedWebView:(WKWebView *)webView
{
    _webView = webView;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    auto check = [&] (id<WKURLSchemeTask> task, const char* url, bool mainFrame, const char* request, const char* originProtocol, const char* originHost, NSInteger originPort) {
        EXPECT_WK_STREQ(task.request.URL.absoluteString, url);
        WKFrameInfo *info = ((id<WKURLSchemeTaskPrivate>)task)._frame;
        EXPECT_EQ(info.isMainFrame, mainFrame);
        EXPECT_WK_STREQ(info.request.URL.absoluteString, request);
        EXPECT_WK_STREQ(info.securityOrigin.protocol, originProtocol);
        EXPECT_WK_STREQ(info.securityOrigin.host, originHost);
        EXPECT_EQ(info.securityOrigin.port, originPort);
        EXPECT_EQ(info.webView, _webView.get().get());
    };

    auto respond = [] (id<WKURLSchemeTask> task, const char* bytes) {
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:strlen(bytes) textEncodingName:nil]).get()];
        [task didReceiveData:[NSData dataWithBytes:bytes length:strlen(bytes)]];
        [task didFinish];
    };

    switch (++_requestCount) {
    case 1:
        check(task, "frame://host1/main", true, "", "", "", 0);
        respond(task, "<iframe src='//host2:1234/iframe'></iframe>");
        return;
    case 2:
        check(task, "frame://host2:1234/iframe", false, "", "frame", "host1", 0);
        respond(task, "<script>fetch('subresource')</script>");
        return;
    case 3:
        check(task, "frame://host2:1234/subresource", false, "frame://host2:1234/iframe", "frame", "host2", 1234);
        respond(task, "done!");
        return;
    }
    ASSERT_NOT_REACHED();
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    ASSERT_NOT_REACHED();
}

@end

TEST(URLSchemeHandler, Frame)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto handler = adoptNS([FrameSchemeHandler new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"frame"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [handler setExpectedWebView:webView.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"frame://host1/main"]]];
    [handler waitForAllRequests];
}

TEST(URLSchemeHandler, Frames)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto handler = adoptNS([TestURLSchemeHandler new]);
    __block size_t grandchildFramesLoaded = 0;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *responseString = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"frame://host1/"])
            responseString = @"<iframe src='frame://host2/'></iframe>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host2/"])
            responseString = @"<iframe src='frame://host3/' onload='fetch(\"loadedGrandchildFrame\")'></iframe><iframe src='frame://host4/' onload='fetch(\"loadedGrandchildFrame\")'></iframe>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host3/"])
            responseString = @"frame content";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host4/"])
            responseString = @"frame content";
        else if ([task.request.URL.path isEqualToString:@"/loadedGrandchildFrame"]) {
            responseString = @"fetched content";
            ++grandchildFramesLoaded;
        }

        ASSERT(responseString);
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:responseString.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[responseString dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"frame"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"frame://host1/"]]];

    while (grandchildFramesLoaded < 2)
        TestWebKitAPI::Util::spinRunLoop();
    
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "host1");
        EXPECT_WK_STREQ(mainFrame.info.request.URL.host, "host1");
        EXPECT_EQ(mainFrame.childFrames.count, 1u);
        EXPECT_TRUE(mainFrame.info.isMainFrame);

        _WKFrameTreeNode *child = mainFrame.childFrames[0];
        EXPECT_WK_STREQ(child.info.request.URL.host, "host2");
        EXPECT_WK_STREQ(child.info.securityOrigin.host, "host2");
        EXPECT_EQ(child.childFrames.count, 2u);
        EXPECT_FALSE(child.info.isMainFrame);

        _WKFrameTreeNode *grandchild1 = child.childFrames[0];
        EXPECT_WK_STREQ(grandchild1.info.request.URL.host, "host3");
        EXPECT_WK_STREQ(grandchild1.info.securityOrigin.host, "host3");
        EXPECT_EQ(grandchild1.childFrames.count, 0u);
        EXPECT_FALSE(grandchild1.info.isMainFrame);

        _WKFrameTreeNode *grandchild2 = child.childFrames[1];
        EXPECT_WK_STREQ(grandchild2.info.request.URL.host, "host4");
        EXPECT_WK_STREQ(grandchild2.info.securityOrigin.host, "host4");
        EXPECT_EQ(grandchild2.childFrames.count, 0u);
        EXPECT_FALSE(grandchild2.info.isMainFrame);

        EXPECT_NE(mainFrame.info._handle.frameID, child.info._handle.frameID);
        EXPECT_NE(mainFrame.info._handle.frameID, grandchild1.info._handle.frameID);
        EXPECT_NE(mainFrame.info._handle.frameID, grandchild2.info._handle.frameID);
        EXPECT_NE(child.info._handle.frameID, grandchild1.info._handle.frameID);
        EXPECT_NE(child.info._handle.frameID, grandchild2.info._handle.frameID);
        EXPECT_NE(grandchild1.info._handle.frameID, grandchild2.info._handle.frameID);

        EXPECT_NULL(mainFrame.info._parentFrameHandle);
        EXPECT_EQ(mainFrame.info._handle.frameID, child.info._parentFrameHandle.frameID);
        EXPECT_EQ(child.info._handle.frameID, grandchild1.info._parentFrameHandle.frameID);
        EXPECT_EQ(child.info._handle.frameID, grandchild2.info._parentFrameHandle.frameID);

        [webView _callAsyncJavaScript:@"window.customProperty = 'customValue'" arguments:nil inFrame:grandchild1.info inContentWorld:[WKContentWorld defaultClientWorld] completionHandler:^(id, NSError *error) {
            [webView _evaluateJavaScript:@"window.location.href + window.customProperty" inFrame:grandchild1.info inContentWorld:[WKContentWorld defaultClientWorld] completionHandler:^(id result, NSError *error) {
                EXPECT_WK_STREQ(result, "frame://host3/customValue");
                [webView _frameInfoFromHandle:grandchild1.info._handle completionHandler:^(WKFrameInfo *fetchedInfo) {
                    EXPECT_WK_STREQ(fetchedInfo.request.URL.host, "host3");
                    done = true;
                }];
            }];
        }];
    }];
    TestWebKitAPI::Util::run(&done);
    
    done = false;
    auto emptyWebView = adoptNS([WKWebView new]);
    [emptyWebView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_NOT_NULL(mainFrame.info._handle);
#if PLATFORM(MAC)
        EXPECT_EQ(mainFrame.info._handle.frameID, 0u);
#endif
        [emptyWebView _evaluateJavaScript:@"window.location.href" inFrame:mainFrame.info inContentWorld:[WKContentWorld defaultClientWorld] completionHandler:^(id result, NSError *error) {
            EXPECT_WK_STREQ(result, "about:blank");
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(URLSchemeHandler, Origin)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *responseString = @"<script>alert("
            "new URL('registered://host:123/path').origin + ', ' + "
            "new URL('notregistered://host:123/path').origin"
        ")</script>";
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:responseString.length textEncodingName:nil]).get()];
        [task didReceiveData:[responseString dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    auto delegate = adoptNS([TestUIDelegate new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"registered"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"registered:///"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "registered://host:123, null");
}

@interface URLSchemeHandlerMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation URLSchemeHandlerMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    lastScriptMessage = message;
    receivedScriptMessage = true;
}
@end

TEST(URLSchemeHandler, isSecureContext)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([TestURLSchemeHandler new]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *responseString = @"<script>window.webkit.messageHandlers.testHandler.postMessage(window.isSecureContext ? 'secure': 'not secure');</script>";
        [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:responseString.length textEncodingName:nil]).get()];
        [task didReceiveData:[responseString dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];

    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"testing"];

    auto messageHandler = adoptNS([[URLSchemeHandlerMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    receivedScriptMessage = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing://localhost/test.html"]]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"secure", [lastScriptMessage body]);

    receivedScriptMessage = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing://main/test.html"]]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"secure", [lastScriptMessage body]);
}

TEST(URLSchemeHandler, APIRedirect)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([TestURLSchemeHandler new]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto redirectResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:nil expectedContentLength:0 textEncodingName:nil]);
        auto redirectRequest = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"redirectone://bar.com/anothertest.html"]]);

        [(id<WKURLSchemeTaskPrivate>)task _willPerformRedirection:redirectResponse.get() newRequest:redirectRequest.get() completionHandler:^(NSURLRequest *proposedRequest) {
            NSString *html = @"<script>window.webkit.messageHandlers.testHandler.postMessage('Document URL: ' + document.URL);</script>";
            auto finalResponse = adoptNS([[NSURLResponse alloc] initWithURL:proposedRequest.URL MIMEType:@"text/html" expectedContentLength:html.length textEncodingName:nil]);

            [task didReceiveResponse:finalResponse.get()];
            [task didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
            [task didFinish];
        }];
    }];

    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"redirectone"];

    auto messageHandler = adoptNS([[URLSchemeHandlerMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    receivedScriptMessage = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"redirectone://foo.com/test.html"]]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Document URL: redirectone://bar.com/anothertest.html", [lastScriptMessage body]);
}

TEST(URLSchemeHandler, Ranges)
{
    RetainPtr<NSData> videoData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"mp4"]];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"ranges"];
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    __block bool foundRangeRequest = false;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSString *html = @"<video autoplay onplaying=\"alert('playing')\"><source src='/video.m4v' type='video/mp4'></video>";
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:html.length textEncodingName:nil] autorelease]];
            [task didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
            [task didFinish];
            return;
        }

        NSString *requestRange = [task.request.allHTTPHeaderFields objectForKey:@"Range"];
        EXPECT_TRUE(requestRange);

        String requestRangeString(requestRange);
        String rangeBytes = "bytes="_s;
        auto begin = requestRangeString.find(rangeBytes, 0);
        ASSERT(begin != notFound);
        auto dash = requestRangeString.find('-', begin);
        ASSERT(dash != notFound);
        auto end = requestRangeString.length();

        auto rangeBeginString = requestRangeString.substring(begin + rangeBytes.length(), dash - begin - rangeBytes.length());
        auto rangeEndString = requestRangeString.substring(dash + 1, end - dash - 1);
        auto rangeBegin = parseInteger<uint64_t>(rangeBeginString).value_or(0);
        auto rangeEnd = rangeEndString.isEmpty() ? [videoData length] - 1 : parseInteger<uint64_t>(rangeEndString).value_or(0);
        auto contentLength = rangeEnd - rangeBegin + 1;

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@"https://webkit.org/"] statusCode:206 HTTPVersion:@"HTTP/1.1" headerFields:@{
            @"Content-Range" : [NSString stringWithFormat:@"bytes %llu-%llu/%lu", rangeBegin, rangeEnd, (unsigned long)[videoData length]],
            @"Content-Length" : [NSString stringWithFormat:@"%llu", contentLength]
        }]);

        [task didReceiveResponse:response.get()];
        [task didReceiveData:[videoData subdataWithRange:NSMakeRange(rangeBegin, contentLength)]];
        [task didFinish];
        foundRangeRequest = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"ranges:///main.html"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "playing");
    EXPECT_TRUE(foundRangeRequest);
}

TEST(URLSchemeHandler, HandleURLRewrittenByPlugIn)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"SchemeChangingPlugIn"];
    configuration._allowedNetworkHosts = [NSSet set];
    auto handler = adoptNS([TestURLSchemeHandler new]);
    __block bool done = false;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        EXPECT_WK_STREQ(task.request.URL.absoluteString, "test+rewritten+scheme://webkit.org/testpath");
        done = true;
    }];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test+rewritten+scheme"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/testpath"]]];
    TestWebKitAPI::Util::run(&done);
}

TEST(URLSchemeHandler, ModulePreload)
{
    __block bool done = false;
    auto handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"])
            return respond(task, "<link rel=modulepreload href='test://webkit.org/module.js'/><script>import('test://webkit.org/module.js')</script>");
        EXPECT_WK_STREQ(task.request.URL.path, "/module.js");
        EXPECT_WK_STREQ(task.request.allHTTPHeaderFields[@"Origin"], "test://webkit.org");
        done = true;
    }];
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://webkit.org/main.html"]]];
    TestWebKitAPI::Util::run(&done);
}

static void runRedirectToHandledSchemeTest(unsigned redirectionCode)
{
    TestWebKitAPI::HTTPServer server({
        { "/redirect.html"_s, { redirectionCode, {{ "Location"_s, "testing://destination.html"_s }}, "redirecting..."_s } }
    });

    __block bool schemeHandledCalled = false;
    RetainPtr schemeHandler = adoptNS([TestURLSchemeHandler new]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        schemeHandledCalled = true;
    }];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"testing"];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request("/redirect.html"_s)];

    TestWebKitAPI::Util::run(&schemeHandledCalled);
}

TEST(URLSchemeHandler, Redirect301FromHTTPToHandledScheme)
{
    runRedirectToHandledSchemeTest(301);
}

TEST(URLSchemeHandler, Redirect302FromHTTPToHandledScheme)
{
    runRedirectToHandledSchemeTest(302);
}
