/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TCPServer.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFrameHandle.h>
#import <WebKit/_WKResourceLoadDelegate.h>
#import <WebKit/_WKResourceLoadInfo.h>
#import <wtf/RetainPtr.h>

@interface TestResourceLoadDelegate : NSObject <_WKResourceLoadDelegate>

@property (nonatomic, copy) void (^didSendRequest)(WKWebView *, _WKResourceLoadInfo *, NSURLRequest *);
@property (nonatomic, copy) void (^didPerformHTTPRedirection)(WKWebView *, _WKResourceLoadInfo *, NSURLResponse *, NSURLRequest *);
@property (nonatomic, copy) void (^didReceiveChallenge)(WKWebView *, _WKResourceLoadInfo *, NSURLAuthenticationChallenge *);
@property (nonatomic, copy) void (^didReceiveResponse)(WKWebView *, _WKResourceLoadInfo *, NSURLResponse *);
@property (nonatomic, copy) void (^didCompleteWithError)(WKWebView *, _WKResourceLoadInfo *, NSError *, NSURLResponse *);

@end

@implementation TestResourceLoadDelegate

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didSendRequest:(NSURLRequest *)request
{
    if (_didSendRequest)
        _didSendRequest(webView, resourceLoad, request);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didPerformHTTPRedirection:(NSURLResponse *)response newRequest:(NSURLRequest *)request
{
    if (_didPerformHTTPRedirection)
        _didPerformHTTPRedirection(webView, resourceLoad, response, request);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (_didReceiveChallenge)
        _didReceiveChallenge(webView, resourceLoad, challenge);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didReceiveResponse:(NSURLResponse *)response
{
    if (_didReceiveResponse)
        _didReceiveResponse(webView, resourceLoad, response);
}

- (void)webView:(WKWebView *)webView resourceLoad:(_WKResourceLoadInfo *)resourceLoad didCompleteWithError:(NSError *)error response:(NSURLResponse *)response
{
    if (_didCompleteWithError)
        _didCompleteWithError(webView, resourceLoad, error, response);
}

@end

TEST(ResourceLoadDelegate, Basic)
{
    auto webView = adoptNS([WKWebView new]);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    __block bool done = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    __block RetainPtr<NSURLRequest> requestFromDelegate;
    auto resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    [webView _setResourceLoadDelegate:resourceLoadDelegate.get()];
    [resourceLoadDelegate setDidSendRequest:^(WKWebView *, _WKResourceLoadInfo *, NSURLRequest *request) {
        requestFromDelegate = request;
    }];

    RetainPtr<NSURLRequest> requestLoaded = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:requestLoaded.get()];
    TestWebKitAPI::Util::run(&done);
    
    EXPECT_WK_STREQ(requestLoaded.get().URL.absoluteString, requestFromDelegate.get().URL.absoluteString);
}

TEST(ResourceLoadDelegate, BeaconAndSyncXHR)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { "hello" } },
        { "/xhrTarget", { {{ "Content-Type", "text/html" }},  "hi" } },
        { "/beaconTarget", { "hi" } },
    });

    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadRequest:server.request()];

    __block RetainPtr<NSURLRequest> requestFromDelegate;
    __block bool receivedCallback = false;
    auto resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    [webView _setResourceLoadDelegate:resourceLoadDelegate.get()];
    [resourceLoadDelegate setDidSendRequest:^(WKWebView *, _WKResourceLoadInfo *info, NSURLRequest *request) {
        requestFromDelegate = request;
        receivedCallback = true;
        EXPECT_TRUE(!!info.frame);
        EXPECT_FALSE(!!info.parentFrame);
    }];
    
    __block bool receivedAlert = false;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    [uiDelegate setRunJavaScriptAlertPanelWithMessage:^(WKWebView *, NSString *, WKFrameInfo *, void (^completionHandler)(void)) {
        receivedAlert = true;
        completionHandler();
    }];

    [webView evaluateJavaScript:@"navigator.sendBeacon('/beaconTarget')" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedCallback);
    EXPECT_WK_STREQ("/beaconTarget", requestFromDelegate.get().URL.path);

    receivedCallback = false;
    [webView evaluateJavaScript:
        @"var request = new XMLHttpRequest();"
        "var asynchronous = false;"
        "request.open('GET', 'xhrTarget', asynchronous);"
        "request.send();"
        "alert('done');" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedCallback);
    EXPECT_WK_STREQ("/xhrTarget", requestFromDelegate.get().URL.path);
    TestWebKitAPI::Util::run(&receivedAlert);
}

TEST(ResourceLoadDelegate, Redirect)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { 301, {{ "Location", "/redirectTarget" }} } },
        { "/redirectTarget", { "hi" } },
    });

    __block bool done = false;
    auto resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    [resourceLoadDelegate setDidPerformHTTPRedirection:^(WKWebView *, _WKResourceLoadInfo *loadInfo, NSURLResponse *response, NSURLRequest *request) {
        EXPECT_WK_STREQ(response.URL.path, "/");
        EXPECT_WK_STREQ(request.URL.path, "/redirectTarget");
        EXPECT_WK_STREQ(loadInfo.originalURL.path, "/");
        EXPECT_WK_STREQ(loadInfo.originalHTTPMethod, "GET");
    }];
    [resourceLoadDelegate setDidCompleteWithError:^(WKWebView *, _WKResourceLoadInfo *loadInfo, NSError *, NSURLResponse *) {
        EXPECT_WK_STREQ(loadInfo.originalURL.path, "/");
        EXPECT_WK_STREQ(loadInfo.originalHTTPMethod, "GET");
        done = true;
    }];

    auto webView = adoptNS([WKWebView new]);
    [webView _setResourceLoadDelegate:resourceLoadDelegate.get()];
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
}

TEST(ResourceLoadDelegate, ResourceType)
{
    const char* testJS = R"TESTJS(
    function loadMoreThingsAfterFetchAndXHR() {
        navigator.sendBeacon('beaconTarget');

        var img = document.createElement('img');
        img.src = 'imageSource';
        document.body.appendChild(img);

        var style = document.createElement('link');
        style.rel = 'stylesheet';
        style.type = 'text/css';
        style.href = 'styleSource';
        document.head.appendChild(style);
    }
    fetch('fetchTarget', { body: 'a=b&c=d', method: 'post'}).then(()=>{
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function () {
            if (this.readyState == 4)
                loadMoreThingsAfterFetchAndXHR();
        };
        xhr.open('GET', 'xhrTarget');
        xhr.send();
    })
    )TESTJS";
    TestWebKitAPI::HTTPServer server({
        { "/", { "<script src='scriptSrc'></script><div>text needing a font</div>" } },
        { "/scriptSrc", { {{ "Content-Type", "application/javascript" }}, testJS } },
        { "/fetchTarget", { "hi" } },
        { "/xhrTarget", { {{ "Content-Type", "application/octet-stream" }}, "hi" } },
        { "/beaconTarget", { "hi" } },
        { "/imageSource", { "not really an image" } },
        { "/styleSource", { "@font-face { font-family: TestFontFamily; src: url(fontSource) } div { font-family: TestFontFamily }" } },
        { "/fontSource", { "not really a font" } },
    });

    __block Vector<RetainPtr<_WKResourceLoadInfo>> loadInfos;

    __block size_t requestCount = 0;
    auto delegate = adoptNS([TestResourceLoadDelegate new]);
    [delegate setDidSendRequest:^(WKWebView *webView, _WKResourceLoadInfo *loadInfo, NSURLRequest *request) {
        loadInfos.append(loadInfo);
        requestCount++;
    }];

    auto webView = adoptNS([WKWebView new]);
    [webView _setResourceLoadDelegate:delegate.get()];
    [webView loadRequest:server.request()];

    Vector<_WKResourceLoadInfoResourceType> expectedTypes {
        _WKResourceLoadInfoResourceTypeDocument,
        _WKResourceLoadInfoResourceTypeScript,
        _WKResourceLoadInfoResourceTypeFetch,
        _WKResourceLoadInfoResourceTypeXMLHTTPRequest,
        _WKResourceLoadInfoResourceTypeBeacon,
        _WKResourceLoadInfoResourceTypeImage,
        _WKResourceLoadInfoResourceTypeStylesheet,
        _WKResourceLoadInfoResourceTypeFont,
    };

    while (requestCount < expectedTypes.size())
        TestWebKitAPI::Util::spinRunLoop();
    for (size_t i = 0; i < expectedTypes.size(); ++i)
        EXPECT_EQ(loadInfos[i].get().resourceType, expectedTypes[i]);
}

TEST(ResourceLoadDelegate, LoadInfo)
{
    TestWebKitAPI::HTTPServer server({
        { "/", { "<iframe src='iframeSrc'></iframe>" } },
        { "/iframeSrc", { "<script>fetch('fetchTarget', { body: 'a=b&c=d', method: 'post'})</script>" } },
        { "/fetchTarget", { "hi" } },
    });

    enum class Callback {
        DidSendRequest,
        DidReceiveResponse,
        DidCompleteWithError,
    };

    __block Vector<Callback> callbacks;
    __block Vector<RetainPtr<WKWebView>> webViews;
    __block Vector<RetainPtr<_WKResourceLoadInfo>> loadInfos;
    __block Vector<RetainPtr<id>> otherParameters;

    __block size_t resourceCompletionCount = 0;
    auto delegate = adoptNS([TestResourceLoadDelegate new]);
    [delegate setDidSendRequest:^(WKWebView *webView, _WKResourceLoadInfo *loadInfo, NSURLRequest *request) {
        callbacks.append(Callback::DidSendRequest);
        webViews.append(webView);
        loadInfos.append(loadInfo);
        otherParameters.append(request);
    }];
    [delegate setDidReceiveResponse:^(WKWebView *webView, _WKResourceLoadInfo *loadInfo, NSURLResponse *response) {
        callbacks.append(Callback::DidReceiveResponse);
        webViews.append(webView);
        loadInfos.append(loadInfo);
        otherParameters.append(response);
    }];
    [delegate setDidCompleteWithError:^(WKWebView *webView, _WKResourceLoadInfo *loadInfo, NSError *error, NSURLResponse *response) {
        callbacks.append(Callback::DidCompleteWithError);
        webViews.append(webView);
        loadInfos.append(loadInfo);
        otherParameters.append(error);
        otherParameters.append(response);
        resourceCompletionCount++;
    }];

    auto webView = adoptNS([WKWebView new]);
    [webView _setResourceLoadDelegate:delegate.get()];
    [webView loadRequest:server.request()];
    while (resourceCompletionCount < 3)
        TestWebKitAPI::Util::spinRunLoop();

    Vector<Callback> expectedCallbacks {
        Callback::DidSendRequest,
        Callback::DidReceiveResponse,
        Callback::DidCompleteWithError,
        Callback::DidSendRequest,
        Callback::DidReceiveResponse,
        Callback::DidCompleteWithError,
        Callback::DidSendRequest,
        Callback::DidReceiveResponse,
        Callback::DidCompleteWithError
    };
    EXPECT_EQ(callbacks, expectedCallbacks);

    EXPECT_EQ(webViews.size(), 9ull);
    for (auto& view : webViews)
        EXPECT_EQ(webView.get(), view.get());

    EXPECT_EQ(loadInfos.size(), 9ull);
    EXPECT_EQ(loadInfos[0].get().resourceLoadID, loadInfos[1].get().resourceLoadID);
    EXPECT_EQ(loadInfos[0].get().resourceLoadID, loadInfos[2].get().resourceLoadID);
    EXPECT_NE(loadInfos[0].get().resourceLoadID, loadInfos[3].get().resourceLoadID);
    EXPECT_EQ(loadInfos[3].get().resourceLoadID, loadInfos[4].get().resourceLoadID);
    EXPECT_EQ(loadInfos[3].get().resourceLoadID, loadInfos[5].get().resourceLoadID);
    EXPECT_NE(loadInfos[3].get().resourceLoadID, loadInfos[6].get().resourceLoadID);
    EXPECT_EQ(loadInfos[6].get().resourceLoadID, loadInfos[7].get().resourceLoadID);
    EXPECT_EQ(loadInfos[6].get().resourceLoadID, loadInfos[8].get().resourceLoadID);
    EXPECT_NE(loadInfos[6].get().resourceLoadID, loadInfos[0].get().resourceLoadID);
    auto checkFrames = ^(size_t index, _WKFrameHandle *expectedFrame, _WKFrameHandle *expectedParent, _WKResourceLoadInfoResourceType expectedType) {
        _WKResourceLoadInfo *info = loadInfos[index].get();
        EXPECT_EQ(!!info.frame, !!expectedFrame);
        EXPECT_EQ(!!info.parentFrame, !!expectedParent);
        EXPECT_EQ(info.frame.frameID, expectedFrame.frameID);
        EXPECT_EQ(info.parentFrame.frameID, expectedParent.frameID);
        EXPECT_EQ(info.resourceType, expectedType);
    };
    _WKFrameHandle *main = loadInfos[0].get().frame;
    _WKFrameHandle *sub = loadInfos[8].get().frame;
    EXPECT_TRUE(!!main);
    EXPECT_TRUE(!!sub);
    EXPECT_TRUE(main.frameID != sub.frameID);
    checkFrames(0, main, nil, _WKResourceLoadInfoResourceTypeDocument);
    checkFrames(1, main, nil, _WKResourceLoadInfoResourceTypeDocument);
    checkFrames(2, main, nil, _WKResourceLoadInfoResourceTypeDocument);
    checkFrames(3, sub, main, _WKResourceLoadInfoResourceTypeDocument);
    checkFrames(4, sub, main, _WKResourceLoadInfoResourceTypeDocument);
    checkFrames(5, sub, main, _WKResourceLoadInfoResourceTypeDocument);
    checkFrames(6, sub, main, _WKResourceLoadInfoResourceTypeFetch);
    checkFrames(7, sub, main, _WKResourceLoadInfoResourceTypeFetch);
    checkFrames(8, sub, main, _WKResourceLoadInfoResourceTypeFetch);

    EXPECT_EQ(otherParameters.size(), 12ull);
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[0] class]), "NSMutableURLRequest");
    EXPECT_WK_STREQ([otherParameters[0] URL].path, "/");
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[1] class]), "NSHTTPURLResponse");
    EXPECT_WK_STREQ([otherParameters[1] URL].path, "/");
    EXPECT_EQ(otherParameters[2], nil);
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[3] class]), "NSHTTPURLResponse");
    EXPECT_WK_STREQ([otherParameters[3] URL].path, "/");

    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[4] class]), "NSMutableURLRequest");
    EXPECT_WK_STREQ([otherParameters[4] URL].path, "/iframeSrc");
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[5] class]), "NSHTTPURLResponse");
    EXPECT_WK_STREQ([otherParameters[5] URL].path, "/iframeSrc");
    EXPECT_EQ(otherParameters[6], nil);
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[7] class]), "NSHTTPURLResponse");
    EXPECT_WK_STREQ([otherParameters[7] URL].path, "/iframeSrc");

    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[8] class]), "NSMutableURLRequest");
    EXPECT_WK_STREQ([otherParameters[8] URL].path, "/fetchTarget");
    EXPECT_WK_STREQ([[[NSString alloc] initWithData:[otherParameters[8] HTTPBody] encoding:NSUTF8StringEncoding] autorelease], "a=b&c=d");
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[9] class]), "NSHTTPURLResponse");
    EXPECT_WK_STREQ([otherParameters[9] URL].path, "/fetchTarget");
    EXPECT_EQ(otherParameters[10], nil);
    EXPECT_WK_STREQ(NSStringFromClass([otherParameters[11] class]), "NSHTTPURLResponse");
    EXPECT_WK_STREQ([otherParameters[11] URL].path, "/fetchTarget");

    _WKResourceLoadInfo *original = loadInfos[0].get();
    NSError *error = nil;
    NSData *archiveData = [NSKeyedArchiver archivedDataWithRootObject:original requiringSecureCoding:YES error:&error];
    EXPECT_EQ(archiveData.length, 607ull);
    EXPECT_FALSE(error);
    _WKResourceLoadInfo *deserialized = [NSKeyedUnarchiver unarchivedObjectOfClass:[_WKResourceLoadInfo class] fromData:archiveData error:&error];
    EXPECT_FALSE(error);
    EXPECT_TRUE(deserialized.resourceLoadID == original.resourceLoadID);
    EXPECT_TRUE(deserialized.frame.frameID == original.frame.frameID);
    EXPECT_TRUE(deserialized.parentFrame.frameID == original.parentFrame.frameID);
    EXPECT_WK_STREQ(deserialized.originalURL.absoluteString, original.originalURL.absoluteString);
    EXPECT_WK_STREQ(deserialized.originalHTTPMethod, original.originalHTTPMethod);
    EXPECT_EQ(deserialized.eventTimestamp.timeIntervalSince1970, original.eventTimestamp.timeIntervalSince1970);
}

// FIXME: Add a test for loadedFromCache.

TEST(ResourceLoadDelegate, Challenge)
{
    using namespace TestWebKitAPI;
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodHTTPBasic);
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialWithUser:@"testuser" password:@"testpassword" persistence:NSURLCredentialPersistenceNone]);
    }];

    __block bool receivedErrorNotification = false;
    __block bool receivedChallengeNotificiation = false;
    auto resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    [resourceLoadDelegate setDidReceiveChallenge:^(WKWebView *, _WKResourceLoadInfo *, NSURLAuthenticationChallenge *challenge) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodHTTPBasic);
        receivedChallengeNotificiation = true;
    }];
    [resourceLoadDelegate setDidCompleteWithError:^(WKWebView *, _WKResourceLoadInfo *, NSError *error, NSURLResponse *) {
        EXPECT_FALSE(error);
        receivedErrorNotification = true;
    }];

    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView _setResourceLoadDelegate:resourceLoadDelegate.get()];
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&receivedErrorNotification);
    EXPECT_TRUE(receivedChallengeNotificiation);
}
