/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTask.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKInspectorConfiguration.h>
#import <WebKit/_WKInspectorDebuggableInfo.h>
#import <WebKit/_WKInspectorDelegate.h>
#import <WebKit/_WKInspectorPrivateForTesting.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

#if PLATFORM(MAC)

@class InspectorDelegate;
@class SimpleURLSchemeHandler;

static bool inspectorFrontendLoadedCalled = false;
static bool willCloseLocalInspectorCalled = false;
static bool browserDomainEnabledForInspectorCalled = false;
static bool browserDomainDisabledForInspectorCalled = false;
static bool shouldCallInspectorCloseReentrantly = false;
static bool openURLExternallyCalled = false;
static bool configurationForLocalInspectorCalled = false;
static bool startURLSchemeTaskCalled = false;

static RetainPtr<SimpleURLSchemeHandler> sharedSimpleURLSchemeHandler;
static RetainPtr<id <_WKInspectorDelegate>> sharedInspectorDelegate;
static RetainPtr<NSURL> urlToOpen;

static void resetInspectorGlobalState()
{
    didAttachLocalInspectorCalled = false;
    inspectorFrontendLoadedCalled = false;
    willCloseLocalInspectorCalled = false;
    browserDomainEnabledForInspectorCalled = false;
    browserDomainDisabledForInspectorCalled = false;
    shouldCallInspectorCloseReentrantly = false;
    openURLExternallyCalled = false;
    configurationForLocalInspectorCalled = false;
    startURLSchemeTaskCalled = false;

    sharedSimpleURLSchemeHandler.clear();
    sharedInspectorDelegate.clear();
    urlToOpen.clear();
}

@interface SimpleURLSchemeHandler : NSObject <WKURLSchemeHandler>
@property (nonatomic, weak) NSURL *expectedURL;
@end

@implementation SimpleURLSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if (_expectedURL)
        EXPECT_STREQ(_expectedURL.absoluteString.UTF8String, task.request.URL.absoluteString.UTF8String);

    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:[NSData dataWithBytes:"1" length:1]];
    [task didFinish];

    startURLSchemeTaskCalled = true;
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

@interface InspectorDelegate : NSObject <_WKInspectorDelegate>
@end

@implementation InspectorDelegate

- (void)inspector:(_WKInspector *)inspector openURLExternally:(NSURL *)url
{
    EXPECT_STREQ(url.absoluteString.UTF8String, urlToOpen.get().absoluteString.UTF8String);
    openURLExternallyCalled = true;
}

- (void)inspectorFrontendLoaded:(_WKInspector *)inspector
{
    inspectorFrontendLoadedCalled = true;
}

@end

@interface UIDelegateForTesting : NSObject <WKUIDelegate>
@end

@implementation UIDelegateForTesting

- (_WKInspectorConfiguration *)_webView:(WKWebView *)webView configurationForLocalInspector:(_WKInspector *)inspector
{
    configurationForLocalInspectorCalled = true;

    sharedInspectorDelegate = [InspectorDelegate new];
    [inspector setDelegate:sharedInspectorDelegate.get()];

    sharedSimpleURLSchemeHandler = adoptNS([[SimpleURLSchemeHandler alloc] init]);
    auto inspectorConfiguration = adoptNS([[_WKInspectorConfiguration alloc] init]);
    [inspectorConfiguration setURLSchemeHandler:sharedSimpleURLSchemeHandler.get() forURLScheme:@"testing"];
    return inspectorConfiguration.autorelease();
}

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    EXPECT_EQ(webView._inspector, inspector);

    didAttachLocalInspectorCalled = true;
}

- (void)_webView:(WKWebView *)webView willCloseLocalInspector:(_WKInspector *)inspector
{
    EXPECT_EQ(webView._inspector, inspector);
    willCloseLocalInspectorCalled = true;

    if (shouldCallInspectorCloseReentrantly)
        [inspector close];
}

- (void)_webViewDidEnableInspectorBrowserDomain:(WKWebView *)webView
{
    browserDomainEnabledForInspectorCalled = true;
}

- (void)_webViewDidDisableInspectorBrowserDomain:(WKWebView *)webView
{
    browserDomainDisabledForInspectorCalled = true;
}

@end

TEST(WKInspectorDelegate, InspectorLifecycleCallbacks)
{
    resetInspectorGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTesting new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    EXPECT_FALSE(webView.get()._isBeingInspected);

    [[webView _inspector] show];

    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);
    TestWebKitAPI::Util::run(&browserDomainEnabledForInspectorCalled);

    [[webView _inspector] close];
    TestWebKitAPI::Util::run(&browserDomainDisabledForInspectorCalled);
    TestWebKitAPI::Util::run(&willCloseLocalInspectorCalled);
}

TEST(WKInspectorDelegate, InspectorCloseCalledReentrantly)
{
    resetInspectorGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTesting new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    EXPECT_FALSE(webView.get()._isBeingInspected);

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    {
        SetForScope closeReentrantlyFromDelegate(shouldCallInspectorCloseReentrantly, true);
        [[webView _inspector] close];
        TestWebKitAPI::Util::run(&willCloseLocalInspectorCalled);
    }
}

TEST(WKInspectorDelegate, ShowURLExternally)
{
    resetInspectorGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTesting new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);
    TestWebKitAPI::Util::run(&inspectorFrontendLoadedCalled);

    urlToOpen = [NSURL URLWithString:@"https://www.webkit.org/"];

    // Check the case where the load is intercepted by the navigation delegate.
    [[webView _inspector] _openURLExternallyForTesting:urlToOpen.get() useFrontendAPI:NO];
    TestWebKitAPI::Util::run(&openURLExternallyCalled);

    // Check the case where the frontend calls InspectorFrontendHost.openURLExternally().
    [[webView _inspector] _openURLExternallyForTesting:urlToOpen.get() useFrontendAPI:YES];
    TestWebKitAPI::Util::run(&openURLExternallyCalled);
}

TEST(WKInspectorDelegate, InspectorConfiguration)
{
    resetInspectorGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTesting new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    EXPECT_FALSE(webView.get()._isBeingInspected);

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&configurationForLocalInspectorCalled);
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);
    TestWebKitAPI::Util::run(&inspectorFrontendLoadedCalled);

    urlToOpen = [NSURL URLWithString:@"testing:main1"];
    sharedSimpleURLSchemeHandler.get().expectedURL = urlToOpen.get();
    [[webView _inspector] _fetchURLForTesting:urlToOpen.get()];
    TestWebKitAPI::Util::run(&startURLSchemeTaskCalled);

    [[webView _inspector] close];
    TestWebKitAPI::Util::run(&willCloseLocalInspectorCalled);
}

#endif // PLATFORM(MAC)
