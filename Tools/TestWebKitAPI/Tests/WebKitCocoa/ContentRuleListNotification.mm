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
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import <WebKit/WKContentRuleListPrivate.h>
#import <WebKit/WKContentRuleListStore.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKUserContentController.h>
#import <WebKit/WKWebView.h>
#import <WebKit/_WKContentRuleListAction.h>
#import <wtf/RetainPtr.h>
#import <wtf/SHA1.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>
#import <wtf/text/WTFString.h>

static bool receivedNotification;
static bool receivedAlert;

struct Notification {
    String identifier;
    String url;
    bool blockedLoad { false };
    bool blockedCookies { false };
    bool madeHTTPS { false };
    Vector<String> notifications;
    
    bool operator==(const Notification& other) const
    {
        return identifier == other.identifier
            && url == other.url
            && blockedLoad == other.blockedLoad
            && blockedCookies == other.blockedCookies
            && madeHTTPS == other.madeHTTPS
            && notifications == other.notifications;
    }
};

static Vector<Notification> notificationList;
static RetainPtr<NSURL> notificationURL;
static RetainPtr<NSString> notificationIdentifier;

@interface ContentRuleListNotificationDelegate : NSObject <WKNavigationDelegatePrivate, WKURLSchemeHandler, WKUIDelegate>
@end

@implementation ContentRuleListNotificationDelegate

- (void)_webView:(WKWebView *)webView URL:(NSURL *)url contentRuleListIdentifiers:(NSArray<NSString *> *)identifiers notifications:(NSArray<NSString *> *)notifications
{
    notificationURL = url;
    EXPECT_EQ(identifiers.count, 1u);
    notificationIdentifier = [identifiers objectAtIndex:0];
    EXPECT_EQ(notifications.count, 1u);
    EXPECT_STREQ([notifications objectAtIndex:0].UTF8String, "testnotification");
    receivedNotification = true;
}

- (void)_webView:(WKWebView *)webView contentRuleListWithIdentifier:(NSString *)identifier performedAction:(_WKContentRuleListAction *)action forURL:(NSURL *)url
{
    notificationList.append({ identifier, url.absoluteString, !!action.blockedLoad, !!action.blockedCookies, !!action.madeHTTPS, makeVector<String>(action.notifications) });
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    [urlSchemeTask didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]).get()];
    [urlSchemeTask didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    receivedAlert = true;
    completionHandler();
}

@end

static NSString *notificationSource = @"[{\"action\":{\"type\":\"notify\",\"notification\":\"testnotification\"},\"trigger\":{\"url-filter\":\"match\"}}]";

static RetainPtr<WKContentRuleList> makeContentRuleList(NSString *source, NSString *identifier = @"testidentifier")
{
    __block bool doneCompiling = false;
    __block RetainPtr<WKContentRuleList> contentRuleList;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:identifier encodedContentRuleList:source completionHandler:^(WKContentRuleList *list, NSError *error) {
        EXPECT_TRUE(list);
        contentRuleList = list;
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
    return contentRuleList;
}

TEST(ContentRuleList, NotificationMainResource)
{
    auto delegate = adoptNS([[ContentRuleListNotificationDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addContentRuleList:makeContentRuleList(notificationSource).get()];
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"apitest"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"apitest:///match"]]];
    TestWebKitAPI::Util::run(&receivedNotification);
    EXPECT_STREQ([notificationURL absoluteString].UTF8String, "apitest:///match");
    EXPECT_STREQ([notificationIdentifier UTF8String], "testidentifier");
}

TEST(ContentRuleList, NotificationSubresource)
{
    auto delegate = adoptNS([[ContentRuleListNotificationDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addContentRuleList:makeContentRuleList(notificationSource).get()];
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"apitest"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>fetch('match').then(function(response){alert('fetch complete')})</script>" baseURL:[NSURL URLWithString:@"apitest:///"]];
    TestWebKitAPI::Util::run(&receivedAlert);
    EXPECT_TRUE(receivedNotification);
    EXPECT_STREQ([notificationURL absoluteString].UTF8String, "apitest:///match");
    EXPECT_STREQ([notificationIdentifier UTF8String], "testidentifier");
}

TEST(ContentRuleList, PerformedActionForURL)
{
    NSString *firstList = @"[{\"action\":{\"type\":\"notify\",\"notification\":\"testnotification\"},\"trigger\":{\"url-filter\":\"notify\"}}]";
    NSString *secondList = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"block\"}}]";
    auto delegate = adoptNS([[ContentRuleListNotificationDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addContentRuleList:makeContentRuleList(firstList, @"firstList").get()];
    [[configuration userContentController] addContentRuleList:makeContentRuleList(secondList, @"secondList").get()];
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"apitest"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>fetch('notify').then(function(){fetch('block').then().catch(function(){alert('test complete')})})</script>" baseURL:[NSURL URLWithString:@"apitest:///"]];
    TestWebKitAPI::Util::run(&receivedAlert);
    while (notificationList.size() < 2)
        TestWebKitAPI::Util::spinRunLoop();

    Vector<Notification> expectedNotifications {
        { "firstList", "apitest:///notify", false, false, false, { "testnotification" } },
        { "secondList", "apitest:///block", true, false, false, { } }
    };
    EXPECT_TRUE(expectedNotifications == notificationList);
}

static String webSocketAcceptValue(const Vector<char>& request)
{
    constexpr auto* keyHeaderField = "Sec-WebSocket-Key: ";
    const char* keyBegin = strnstr(request.data(), keyHeaderField, request.size()) + strlen(keyHeaderField);
    EXPECT_NOT_NULL(keyBegin);
    const char* keyEnd = strnstr(keyBegin, "\r\n", request.size() + (keyBegin - request.data()));
    EXPECT_NOT_NULL(keyEnd);

    constexpr auto* webSocketKeyGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    SHA1 sha1;
    sha1.addBytes(reinterpret_cast<const uint8_t*>(keyBegin), keyEnd - keyBegin);
    sha1.addBytes(reinterpret_cast<const uint8_t*>(webSocketKeyGUID), strlen(webSocketKeyGUID));
    SHA1::Digest hash;
    sha1.computeHash(hash);
    return base64Encode(hash.data(), SHA1::hashSize);
}

TEST(ContentRuleList, ResourceTypes)
{
    using namespace TestWebKitAPI;
    HTTPServer webSocketServer([](Connection connection) {
        connection.receiveHTTPRequest([=](Vector<char>&& request) {
            connection.send(HTTPResponse(101, {
                { "Upgrade", "websocket" },
                { "Connection", "Upgrade" },
                { "Sec-WebSocket-Accept", webSocketAcceptValue(request) }
            }).serialize(HTTPResponse::IncludeContentLength::No));
        });
    });
    auto serverPort = webSocketServer.port();

    auto handler = [[TestURLSchemeHandler new] autorelease];
    handler.startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        auto respond = [task] (const char* html) {
            NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:strlen(html) textEncodingName:nil] autorelease];
            [task didReceiveResponse:response];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        };
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/checkWebSocket.html"])
            return respond([NSString stringWithFormat:@"<script>var ws = new WebSocket('ws://localhost:%d/test');ws.onopen=()=>{alert('onopen')};ws.onerror=()=>{alert('onerror')}</script>", serverPort].UTF8String);
        if ([path isEqualToString:@"/checkFetch.html"])
            return respond("<script>fetch('test:///fetchContent').then(()=>{alert('fetched')}).catch(()=>{alert('did not fetch')})</script>");
        if ([path isEqualToString:@"/fetchContent"])
            return respond("hello");
        if ([path isEqualToString:@"/checkXHR.html"])
            return respond("<script>var xhr = new XMLHttpRequest();xhr.open('GET', 'test:///fetchContent');xhr.onreadystatechange=()=>{if(xhr.readyState==4){setTimeout(()=>{alert('xhr finished')}, 0)}};xhr.onerror=()=>{alert('xhr error')};xhr.send()</script>");

        ASSERT_NOT_REACHED();
    };
    auto configuration = [[WKWebViewConfiguration new] autorelease];
    [configuration setURLSchemeHandler:handler forURLScheme:@"test"];
    configuration.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto webView = [[[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration] autorelease];

    auto listWithResourceType = [] (const char* type) {
        return makeContentRuleList([NSString stringWithFormat:@"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*test\",\"resource-type\":[\"%s\"]}}]", type]);
    };

    WKUserContentController *userContentController = webView.configuration.userContentController;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///checkWebSocket.html"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "onopen");
    [userContentController addContentRuleList:listWithResourceType("websocket").get()];
    [webView reload];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "onerror");

    [userContentController removeAllContentRuleLists];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///checkFetch.html"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");
    [userContentController addContentRuleList:listWithResourceType("fetch").get()];
    [webView reload];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "did not fetch");
    
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///checkXHR.html"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "xhr error");
    EXPECT_WK_STREQ([webView _test_waitForAlert], "xhr finished");
    [userContentController removeAllContentRuleLists];
    [webView reload];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "xhr finished");
    
    HTTPServer beaconServer({
        { "/", { "<script>navigator.sendBeacon('/testBeaconTarget', 'hello');fetch('/testFetchTarget').then(()=>{alert('fetch done')})</script>" } },
        { "/testBeaconTarget", { "hi" } },
        { "/testFetchTarget", { "hi" } },
    });
    [webView loadRequest:beaconServer.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "fetch done");
    EXPECT_EQ(beaconServer.totalRequests(), 3u);
    [userContentController addContentRuleList:listWithResourceType("other").get()];
    [webView reload];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "fetch done");
    EXPECT_EQ(beaconServer.totalRequests(), 5u);
}

TEST(ContentRuleList, SupportsRegex)
{
    NSArray<NSString *> *allowed = @[
        @".*",
        @"a.*b"
    ];
    for (NSString *regex in allowed)
        EXPECT_TRUE([WKContentRuleList _supportsRegularExpression:regex]);
    
    NSArray<NSString *> *disallowed = @[
        @"Ä",
        @"\\d\\D\\w\\s\\v\\h\\i\\c",
        @"",
        @"(?<A>a)\\k<A>",
        @"a^",
        @"\\b",
        @"[\\d]",
        @"(?!)",
        @"this|that",
        @"$$",
        @"a{0,2}b"
    ];
    for (NSString *regex in disallowed)
        EXPECT_FALSE([WKContentRuleList _supportsRegularExpression:regex]);
}
