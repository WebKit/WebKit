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
#import <WebKit/WKContentRuleListStore.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKUserContentController.h>
#import <WebKit/WKWebView.h>
#import <WebKit/_WKContentRuleListAction.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
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

static Vector<String> toVector(NSArray<NSString *> *array)
{
    Vector<String> vector;
    vector.reserveInitialCapacity(array.count);
    for (NSString *string in array)
        vector.append(string);
    return vector;
}

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
    notificationList.append({ identifier, url.absoluteString, !!action.blockedLoad, !!action.blockedCookies, !!action.madeHTTPS, toVector(action.notifications) });
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    [urlSchemeTask didReceiveResponse:[[[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease]];
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

TEST(WebKit, ContentRuleListNotificationMainResource)
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

TEST(WebKit, ContentRuleListNotificationSubresource)
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

TEST(WebKit, PerformedActionForURL)
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
