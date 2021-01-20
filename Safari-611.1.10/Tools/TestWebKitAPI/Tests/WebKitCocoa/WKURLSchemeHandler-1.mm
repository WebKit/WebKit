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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFrameHandle.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

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

    if (entry->key == "syncxhr://host/test.dat")
        startedXHR = true;

    if (!entry->value.shouldRespond)
        return;
    
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:entry->value.mimeType.get() expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];

    [task didReceiveData:[NSData dataWithBytesNoCopy:(void*)entry->value.data length:strlen(entry->value.data) freeWhenDone:NO]];
    [task didFinish];

    if (entry->key == "syncxhr://host/test.dat")
        startedXHR = false;
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    EXPECT_TRUE([[task.request.URL absoluteString] isEqualToString:@"syncxhr://host/test.dat"]);
    receivedStop = true;
}

@end

static RetainPtr<NSMutableArray> receivedMessages = adoptNS([@[] mutableCopy]);
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

static const char* syncMainBytes = R"SYNCRESOURCE(
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

static const char* syncXHRBytes = "My XHR text!";

TEST(URLSchemeHandler, SyncXHR)
{
    auto *pool = [[NSAutoreleasePool alloc] init];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[SyncScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"syncxhr"];
    
    handler.get()->resources.set("syncxhr://host/main.html", SchemeResourceInfo { @"text/html", syncMainBytes, true });
    handler.get()->resources.set("syncxhr://host/test.dat", SchemeResourceInfo { @"text/plain", syncXHRBytes, true });

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
    handler.get()->resources.find("syncxhr://host/test.dat")->value.shouldRespond = false;
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&startedXHR);
    receivedMessage = false;

    webView = nil;
    [pool drain];
    
    TestWebKitAPI::Util::run(&receivedStop);
}

@interface SyncErrorScheme : NSObject <WKURLSchemeHandler, WKUIDelegate>
@end

@implementation SyncErrorScheme

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([task.request.URL.absoluteString isEqualToString:@"syncerror:///main.html"]) {
        static const char* bytes = "<script>var xhr=new XMLHttpRequest();xhr.open('GET','subresource',false);try{xhr.send(null);alert('no error')}catch(e){alert(e)}</script>";
        [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:strlen(bytes) textEncodingName:nil] autorelease]];
        [task didReceiveData:[NSData dataWithBytes:bytes length:strlen(bytes)]];
        [task didFinish];
    } else {
        EXPECT_STREQ(task.request.URL.absoluteString.UTF8String, "syncerror:///subresource");
        [task didReceiveResponse:[[[NSURLResponse alloc] init] autorelease]];
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

static const char* xhrPostDocument = R"XHRPOSTRESOURCE(<html><head><script>
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
</body></html>)XHRPOSTRESOURCE";


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
            uint8_t buffer[9];
            memset(buffer, 0, 9);
            auto length = [stream read:buffer maxLength:9];
            EXPECT_EQ(length, 8);
            EXPECT_STREQ(reinterpret_cast<const char*>(buffer), "foo=bar2");
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
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"threads"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    static bool done;
    static id<WKURLSchemeTask> theTask;
    static RefPtr<Thread> theThread;
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        theTask = task;
        [task retain];
        theThread = Thread::create("A", [task] {
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
    [pool drain];

    Thread::create("B", [] {
        [theTask release];
        theTask = nil;
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
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
            [task didReceiveData:data];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/corsresource"]) {
            if (includeCORSHeaderFieldInResponse) {
                [task didReceiveResponse:[[[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:200 HTTPVersion:nil headerFields:@{
                    @"Access-Control-Allow-Origin": @"*",
                    @"Content-Length": @"2",
                    @"Content-Type":@"text/html"
                }] autorelease]];
            } else
                [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
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
        { "/subresource", { {{ "Content-Type", "application/json" }, { "headerName", "headerValue" }}, "{\"testKey\":\"testValue\"}" } }
    });

    bool corssuccess = false;
    bool corsfailure = false;
    bool done = false;

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
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
            NSData *data = [[NSString stringWithFormat:
                @"<script>%@</script>", testJS] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
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
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);
    
    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration._corsDisablingPatterns = @[@"*://*/*"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);

    corssuccess = false;
    corsfailure = false;
    done = false;

    webView.get()._corsDisablingPatterns = @[];
    [webView evaluateJavaScript:testJS completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);

    corssuccess = false;
    corsfailure = false;
    done = false;

    webView.get()._corsDisablingPatterns = @[@"*://*/*"];
    [webView evaluateJavaScript:testJS completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(URLSchemeHandler, DisableCORSCredentials)
{
    TestWebKitAPI::HTTPServer server({
        { "/subresource", { {{ "Access-Control-Allow-Origin", "*" }}, "subresourcecontent" } }
    });

    bool corssuccess = false;
    bool corsfailure = false;
    bool done = false;

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script>fetch('http://127.0.0.1:%d/subresource', {credentials:'include'}).then(function(){fetch('/corssuccess')}).catch(function(){fetch('/corsfailure')})</script>", server.port()] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
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
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);
    
    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration._crossOriginAccessControlCheckEnabled = NO;
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(URLSchemeHandler, DisableCORSScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { "fetch('loadSuccess')" } }
    });

    bool loadSuccess = false;
    bool loadFail = false;
    bool done = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"cors"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script type='text/javascript' crossorigin='anonymous' onerror='fetch(\"loadFail\")' src='http://127.0.0.1:%d/'></script>", server.port()] dataUsingEncoding:NSUTF8StringEncoding];
            
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
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
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(loadSuccess);
    EXPECT_TRUE(loadFail);
    
    loadSuccess = false;
    loadFail = false;
    done = false;

    configuration._corsDisablingPatterns = @[@"*://*/*"];
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
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

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
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
            response = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
        } else
            ASSERT_NOT_REACHED();

        if (response) {
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:response.length textEncodingName:nil] autorelease]];
            [task didReceiveData:response];
            [task didFinish];
        }
    }];

    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(corssuccess);
    EXPECT_TRUE(corsfailure);

    corssuccess = false;
    corsfailure = false;
    done = false;

    configuration._corsDisablingPatterns = @[@"*://*/*"];
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"cors://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_TRUE(corssuccess);
    EXPECT_FALSE(corsfailure);
}

TEST(URLSchemeHandler, LoadsFromNetwork)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { {{ "Access-Control-Allow-Origin", "*" }}, "test content" } }
    });

    bool loadSuccess = false;
    bool loadFail = false;
    bool done = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSData *data = [[NSString stringWithFormat:@"<script>"
                "fetch('http://127.0.0.1:%d/').then(()=>{"
                    "fetch('/loadSuccess')"
                "}).catch(()=>{"
                    "var ws = new WebSocket('ws://127.0.0.1:%d');"
                    "ws.onerror = function() { fetch('/loadFail') };"
                "})"
                "</script>", server.port(), server.port()] dataUsingEncoding:NSUTF8StringEncoding];
            [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
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
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_TRUE(loadSuccess);
    EXPECT_FALSE(loadFail);
    EXPECT_EQ(server.totalRequests(), 1u);
    
    loadSuccess = false;
    loadFail = false;
    done = false;

    configuration._loadsFromNetwork = NO;
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host1/main.html"]]];
        TestWebKitAPI::Util::run(&done);
    }
    EXPECT_FALSE(loadSuccess);
    EXPECT_TRUE(loadFail);
    EXPECT_EQ(server.totalRequests(), 1u);
}

TEST(URLSchemeHandler, LoadsSubresources)
{
    bool loadedImage = false;
    bool loadedIFrame = false;

    auto handler = adoptNS([TestURLSchemeHandler new]);

    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];

    [handler setStartURLSchemeTaskHandler:[&](WKWebView *, id<WKURLSchemeTask> task) {
        NSString *response = nil;
        if ([task.request.URL.path isEqualToString:@"/main.html"])
            response = @"<img src='/imgsrc'></img><iframe src='/iframesrc'></iframe>";
        else if ([task.request.URL.path isEqualToString:@"/imgsrc"]) {
            response = @"image content";
            loadedImage = true;
        } else if ([task.request.URL.path isEqualToString:@"/iframesrc"]) {
            response = @"iframe content";
            loadedIFrame = true;
        } else
            ASSERT_NOT_REACHED();
        [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:response.length textEncodingName:nil] autorelease]];
        [task didReceiveData:[response dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host1/main.html"]]];
        TestWebKitAPI::Util::run(&loadedImage);
        TestWebKitAPI::Util::run(&loadedIFrame);
    }
    
    loadedImage = false;
    loadedIFrame = false;

    configuration._loadsSubresources = NO;
    {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
        auto delegate = adoptNS([TestNavigationDelegate new]);
        webView.get().navigationDelegate = delegate.get();
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test://host1/main.html"]]];
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
        [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:strlen(bytes) textEncodingName:nil] autorelease]];
        [task didReceiveData:[NSData dataWithBytes:bytes length:strlen(bytes)]];
        [task didFinish];
    };

    switch (++_requestCount) {
    case 1:
        check(task, "frame://host1/main", true, "", "", "", 0);
        respond(task, "<iframe src='//host2:123/iframe'></iframe>");
        return;
    case 2:
        check(task, "frame://host2:123/iframe", false, "", "frame", "host1", 0);
        respond(task, "<script>fetch('subresource')</script>");
        return;
    case 3:
        check(task, "frame://host2:123/subresource", false, "frame://host2:123/iframe", "frame", "host2", 123);
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
        EXPECT_WK_STREQ(mainFrame.securityOrigin.host, "host1");
        EXPECT_WK_STREQ(mainFrame.request.URL.host, "host1");
        EXPECT_EQ(mainFrame.childFrames.count, 1u);
        EXPECT_TRUE(mainFrame.isMainFrame);

        _WKFrameTreeNode *child = mainFrame.childFrames[0];
        EXPECT_WK_STREQ(child.request.URL.host, "host2");
        EXPECT_WK_STREQ(child.securityOrigin.host, "host2");
        EXPECT_EQ(child.childFrames.count, 2u);
        EXPECT_FALSE(child.isMainFrame);

        _WKFrameTreeNode *grandchild1 = child.childFrames[0];
        EXPECT_WK_STREQ(grandchild1.request.URL.host, "host3");
        EXPECT_WK_STREQ(grandchild1.securityOrigin.host, "host3");
        EXPECT_EQ(grandchild1.childFrames.count, 0u);
        EXPECT_FALSE(grandchild1.isMainFrame);

        _WKFrameTreeNode *grandchild2 = child.childFrames[1];
        EXPECT_WK_STREQ(grandchild2.request.URL.host, "host4");
        EXPECT_WK_STREQ(grandchild2.securityOrigin.host, "host4");
        EXPECT_EQ(grandchild2.childFrames.count, 0u);
        EXPECT_FALSE(grandchild2.isMainFrame);

        EXPECT_NE(mainFrame._handle.frameID, child._handle.frameID);
        EXPECT_NE(mainFrame._handle.frameID, grandchild1._handle.frameID);
        EXPECT_NE(mainFrame._handle.frameID, grandchild2._handle.frameID);
        EXPECT_NE(child._handle.frameID, grandchild1._handle.frameID);
        EXPECT_NE(child._handle.frameID, grandchild2._handle.frameID);
        EXPECT_NE(grandchild1._handle.frameID, grandchild2._handle.frameID);

        EXPECT_NULL(mainFrame._parentFrameHandle);
        EXPECT_EQ(mainFrame._handle.frameID, child._parentFrameHandle.frameID);
        EXPECT_EQ(child._handle.frameID, grandchild1._parentFrameHandle.frameID);
        EXPECT_EQ(child._handle.frameID, grandchild2._parentFrameHandle.frameID);

        [webView _callAsyncJavaScript:@"window.customProperty = 'customValue'" arguments:nil inFrame:grandchild1 inContentWorld:[WKContentWorld defaultClientWorld] completionHandler:^(id, NSError *error) {
            [webView _evaluateJavaScript:@"window.location.href + window.customProperty" inFrame:grandchild1 inContentWorld:[WKContentWorld defaultClientWorld] completionHandler:^(id result, NSError *error) {
                EXPECT_WK_STREQ(result, "frame://host3/customValue");
                done = true;
            }];
        }];
    }];
    TestWebKitAPI::Util::run(&done);
    
    done = false;
    auto emptyWebView = adoptNS([WKWebView new]);
    [emptyWebView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_NOT_NULL(mainFrame._handle);
#if PLATFORM(MAC)
        EXPECT_EQ(mainFrame._handle.frameID, 0u);
#endif
        [emptyWebView _evaluateJavaScript:@"window.location.href" inFrame:mainFrame inContentWorld:[WKContentWorld defaultClientWorld] completionHandler:^(id result, NSError *error) {
            EXPECT_WK_STREQ(result, "about:blank");
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}
