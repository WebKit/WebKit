/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>

@interface DeallocWebViewTestGeolocationProvider : NSObject <WebGeolocationProvider>
@property (nonatomic, readonly) NSUInteger registerCount;
@property (nonatomic, readonly) NSUInteger unregisterCount;
@end

@implementation DeallocWebViewTestGeolocationProvider

- (void)registerWebView:(WebView *)webView
{
    ++_registerCount;
}

- (void)unregisterWebView:(WebView *)webView
{
    ++_unregisterCount;
}

- (WebGeolocationPosition *)lastPosition
{
    return nil;
}

@end

@interface DeallocWebViewTestNotificationProvider : NSObject <WebNotificationProvider>
@property (nonatomic, readonly) NSUInteger showCount;
@property (nonatomic, readonly) NSUInteger destroyCount;
@end

@implementation DeallocWebViewTestNotificationProvider

- (void)registerWebView:(WebView *)webView
{
}

- (void)unregisterWebView:(WebView *)webView
{
}

- (void)showNotification:(WebNotification *)notification fromWebView:(WebView *)webView
{
    ++_showCount;
}

- (void)cancelNotification:(WebNotification *)notification
{
}

- (void)notificationDestroyed:(WebNotification *)notification
{
    ++_destroyCount;
}

- (void)clearNotifications:(NSArray *)notificationIDs
{
}

- (WebNotificationPermission)policyForOrigin:(WebSecurityOrigin *)origin
{
    return WebNotificationPermissionAllowed;
}

- (void)webView:(WebView *)webView didShowNotification:(NSString *)notificationID
{
}

- (void)webView:(WebView *)webView didClickNotification:(NSString *)notificationID
{
}

- (void)webView:(WebView *)webView didCloseNotifications:(NSArray *)notificationIDs
{
}

@end

@interface DeallocWebViewTestDelegate : NSObject <WebFrameLoadDelegate, WebUIDelegate>
@property (nonatomic, readonly) BOOL didFinishLoad;
@end

@implementation DeallocWebViewTestDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    _didFinishLoad = YES;
}

- (void)webView:(WebView *)webView decidePolicyForGeolocationRequestFromOrigin:(WebSecurityOrigin *)origin frame:(WebFrame *)frame listener:(id<WebAllowDenyPolicyListener>)listener
{
    [listener allow];
}

@end

static bool webViewDeallocated;

@interface DeallocWebViewTestWebView : WebView
@end

@implementation DeallocWebViewTestWebView

- (void)dealloc
{
    [super dealloc];
    webViewDeallocated = true;
}

@end

namespace TestWebKitAPI {

static RetainPtr<WebView> createWebView(DeallocWebViewTestDelegate *delegate)
{
    webViewDeallocated = false;
    RetainPtr<WebView> webView = adoptNS([[DeallocWebViewTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    [webView setFrameLoadDelegate:delegate];
    [webView setUIDelegate:delegate];
    [webView _setVisibilityState:WebPageVisibilityStateVisible isInitialState:YES];
    return webView;
}

static void loadHTMLString(WebView *webView, DeallocWebViewTestDelegate *delegate, NSString *html)
{
    [[webView mainFrame] loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    Util::waitForConditionWithLogging([&] {
        return delegate.didFinishLoad;
    }, 5, @"Timed out waiting for the load to finish.");
}

TEST(WebKitLegacy, GeolocationProviderUnregistersWebViewOnDealloc)
{
    RetainPtr provider = adoptNS([[DeallocWebViewTestGeolocationProvider alloc] init]);
    RetainPtr delegate = adoptNS([[DeallocWebViewTestDelegate alloc] init]);
    BOOL wasIconLoadingEnabled = [WebView _isIconLoadingEnabled];
    [WebView _setIconLoadingEnabled:NO];
    auto restoreIconLoading = makeScopeExit([&] {
        [WebView _setIconLoadingEnabled:wasIconLoadingEnabled];
    });

    @autoreleasepool {
        RetainPtr webView = createWebView(delegate.get());
        [webView _setGeolocationProvider:provider.get()];
        loadHTMLString(webView.get(), delegate.get(), @"<script>navigator.geolocation.watchPosition(() => { });</script>");
        Util::waitForConditionWithLogging([&] {
            return [provider registerCount] == 1;
        }, 5, @"Timed out waiting for the WebView to register with the geolocation provider.");
        EXPECT_EQ([provider unregisterCount], 0u);
    }

    EXPECT_TRUE(Util::runFor(&webViewDeallocated, 5_s));
    EXPECT_EQ([provider registerCount], 1u);
    EXPECT_EQ([provider unregisterCount], 1u);
}

TEST(WebKitLegacy, GeolocationProviderNotUnregisteredOnDeallocWhenUnused)
{
    RetainPtr provider = adoptNS([[DeallocWebViewTestGeolocationProvider alloc] init]);
    RetainPtr delegate = adoptNS([[DeallocWebViewTestDelegate alloc] init]);
    BOOL wasIconLoadingEnabled = [WebView _isIconLoadingEnabled];
    [WebView _setIconLoadingEnabled:NO];
    auto restoreIconLoading = makeScopeExit([&] {
        [WebView _setIconLoadingEnabled:wasIconLoadingEnabled];
    });

    @autoreleasepool {
        RetainPtr webView = createWebView(delegate.get());
        [webView _setGeolocationProvider:provider.get()];
        loadHTMLString(webView.get(), delegate.get(), @"<body></body>");
    }

    EXPECT_TRUE(Util::runFor(&webViewDeallocated, 5_s));
    EXPECT_EQ([provider registerCount], 0u);
    EXPECT_EQ([provider unregisterCount], 0u);
}

TEST(WebKitLegacy, NotificationProviderNotifiedOfDestroyedNotificationsOnDealloc)
{
    RetainPtr provider = adoptNS([[DeallocWebViewTestNotificationProvider alloc] init]);
    RetainPtr delegate = adoptNS([[DeallocWebViewTestDelegate alloc] init]);
    BOOL wasIconLoadingEnabled = [WebView _isIconLoadingEnabled];
    [WebView _setIconLoadingEnabled:NO];
    auto restoreIconLoading = makeScopeExit([&] {
        [WebView _setIconLoadingEnabled:wasIconLoadingEnabled];
    });

    @autoreleasepool {
        RetainPtr webView = createWebView(delegate.get());
        RetainPtr preferences = adoptNS([[WebPreferences alloc] initWithIdentifier:@"NotificationProviderNotifiedOfDestroyedNotificationsOnDealloc"]);
        [preferences setNotificationsEnabled:YES];
        [webView setPreferences:preferences.get()];
        [webView _setNotificationProvider:provider.get()];
        loadHTMLString(webView.get(), delegate.get(), @"<script>window.notification = new Notification('title');</script>");
        Util::waitForConditionWithLogging([&] {
            return [provider showCount] == 1;
        }, 5, @"Timed out waiting for the notification to show.");
        EXPECT_EQ([provider destroyCount], 0u);
    }

    EXPECT_TRUE(Util::runFor(&webViewDeallocated, 5_s));
    EXPECT_EQ([provider destroyCount], 1u);
}

} // namespace TestWebKitAPI
