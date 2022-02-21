/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#import "Test.h"

#if HAVE(APP_SSO)

#import "ClassMethodSwizzler.h"
#import "HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/cocoa/AppSSOSoftLink.h>
#import <pal/spi/cocoa/AuthKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/StringPrintStream.h>
#import <wtf/URL.h>
#import <wtf/text/StringConcatenateNumbers.h>
#import <wtf/text/WTFString.h>

static bool navigationCompleted = false;
static bool policyForAppSSOPerformed = false;
static bool authorizationPerformed = false;
static bool authorizationCancelled = false;
static bool uiShowed = false;
static bool newWindowCreated = false;
static bool haveHttpBody = false;
static bool navigationPolicyDecided = false;
static bool allMessagesReceived = false;
static String finalURL;
static SOAuthorization* gAuthorization;
static id<SOAuthorizationDelegate> gDelegate;
#if PLATFORM(MAC)
static BlockPtr<void(NSNotification *)> gNotificationCallback;
#endif
static RetainPtr<WKWebView> gNewWindow;

static const char* openerTemplate =
"<html>"
"<button onclick='clickMe()' style='width:400px;height:400px'>window.open</button>"
"<script>"
"function clickMe()"
"{"
"    window.addEventListener('message', receiveMessage, false);"
"    newWindow = window.open('%s');"
"    %s"
"    if (!newWindow)"
"        return;"
"    const pollTimer = window.setInterval(function() {"
"        if (newWindow.closed) {"
"            window.clearInterval(pollTimer);"
"            window.webkit.messageHandlers.testHandler.postMessage('WindowClosed.');"
"        }"
"    }, 200);"
"}"
"function receiveMessage(event)"
"{"
"    if (event.origin == 'http://www.example.com') {"
"        window.webkit.messageHandlers.testHandler.postMessage(event.data);"
"        %s"
"    }"
"}"
"</script>"
"</html>";

static const char* newWindowResponseTemplate =
"<html>"
"<script>"
"window.opener.postMessage('Hello.', '*');"
"%s"
"</script>"
"</html>";

static const char* parentTemplate =
"<html>"
"<meta name='referrer' content='origin' />"
"<iframe src='%s'></iframe>"
"<script>"
"function receiveMessage(event)"
"{"
"    window.webkit.messageHandlers.testHandler.postMessage(event.origin);"
"    window.webkit.messageHandlers.testHandler.postMessage(event.data);"
"}"
"window.addEventListener('message', receiveMessage, false);"
"</script>"
"</html>";

static const char* iframeTemplate =
"<html>"
"<script>"
"parent.postMessage('Hello.', '*');"
"%s"
"</script>"
"</html>";

static const char* samlResponse =
"<html>"
"<script>"
"window.webkit.messageHandlers.testHandler.postMessage('SAML');"
"</script>"
"</html>";

@interface TestSOAuthorizationBasicDelegate : NSObject <WKNavigationDelegate>
@end

@implementation TestSOAuthorizationBasicDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    EXPECT_FALSE(navigationCompleted);
    navigationCompleted = true;
    finalURL = navigation._request.URL.absoluteString;
}

@end

@interface TestSOAuthorizationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@property bool isDefaultPolicy;
@property bool shouldOpenExternalSchemes;
@property bool allowSOAuthorizationLoad;
@property bool isAsyncExecution;
- (instancetype)init;
@end

@implementation TestSOAuthorizationDelegate

- (instancetype)init
{
    if (self = [super init]) {
        self.isDefaultPolicy = true;
        self.shouldOpenExternalSchemes = false;
        self.allowSOAuthorizationLoad = true;
        self.isAsyncExecution = false;
    }
    return self;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    EXPECT_FALSE(navigationCompleted);
    navigationCompleted = true;
    finalURL = navigation._request.URL.absoluteString;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    navigationPolicyDecided = true;
    EXPECT_EQ(navigationAction._shouldOpenExternalSchemes, self.shouldOpenExternalSchemes);
    if (self.isDefaultPolicy) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }
    decisionHandler(_WKNavigationActionPolicyAllowWithoutTryingAppLink);
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    EXPECT_FALSE(newWindowCreated);
    newWindowCreated = true;
    gNewWindow = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [gNewWindow setNavigationDelegate:self];
    return gNewWindow.get();
}

- (void)_webView:(WKWebView *)webView decidePolicyForSOAuthorizationLoadWithCurrentPolicy:(_WKSOAuthorizationLoadPolicy)policy forExtension:(NSString *)extension completionHandler:(void (^)(_WKSOAuthorizationLoadPolicy policy))completionHandler
{
    EXPECT_EQ(policy, _WKSOAuthorizationLoadPolicyAllow);
    EXPECT_TRUE([extension isEqual:@"Test"]);
    policyForAppSSOPerformed = true;
    if (!self.isAsyncExecution) {
        if (self.allowSOAuthorizationLoad)
            completionHandler(policy);
        else
            completionHandler(_WKSOAuthorizationLoadPolicyIgnore);
        return;
    }

    auto allowSOAuthorizationLoad = self.allowSOAuthorizationLoad;
    dispatch_async(dispatch_get_main_queue(), ^() {
        if (allowSOAuthorizationLoad)
            completionHandler(_WKSOAuthorizationLoadPolicyAllow);
        else
            completionHandler(_WKSOAuthorizationLoadPolicyIgnore);
    });
}

#if PLATFORM(IOS)
- (UIViewController *)_presentingViewControllerForWebView:(WKWebView *)webView
{
    return nil;
}
#endif

@end

#if PLATFORM(MAC)
@interface TestSOAuthorizationViewController : NSViewController
#elif PLATFORM(IOS)
@interface TestSOAuthorizationViewController : UIViewController
#endif
@end

@implementation TestSOAuthorizationViewController

- (void)viewDidAppear
{
    EXPECT_FALSE(uiShowed);
    uiShowed = true;
}

- (void)viewDidDisappear
{
    EXPECT_TRUE(uiShowed);
    uiShowed = false;
}

@end

@interface TestSOAuthorizationScriptMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestSOAuthorizationScriptMessageHandler {
    RetainPtr<NSMutableArray> _messages;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (!_messages)
        _messages = adoptNS([[NSMutableArray alloc] init]);
    [_messages addObject:message.body];

    if ([message.body isEqual:@""]) {
        allMessagesReceived = true;
        EXPECT_EQ([_messages count], 5u);
        EXPECT_WK_STREQ("SOAuthorizationDidStart", [_messages objectAtIndex:1]);
        EXPECT_WK_STREQ("SOAuthorizationDidCancel", [_messages objectAtIndex:3]);
        EXPECT_WK_STREQ("", [_messages objectAtIndex:4]);
    }

    if ([message.body isEqual:@"Hello."]) {
        allMessagesReceived = true;
        EXPECT_EQ([_messages count], 4u);
        EXPECT_WK_STREQ("SOAuthorizationDidStart", [_messages objectAtIndex:1]);
        EXPECT_WK_STREQ("Hello.", [_messages objectAtIndex:3]);
    }
}

@end

static bool overrideCanPerformAuthorizationWithURL(id, SEL, NSURL *url, NSInteger)
{
    if (![url.lastPathComponent isEqual:@"simple.html"] && ![url.host isEqual:@"www.example.com"] && ![url.lastPathComponent isEqual:@"GetSessionCookie.html"])
        return false;
    return true;
}

static void overrideSetDelegate(id self, SEL, id<SOAuthorizationDelegate> delegate)
{
    gAuthorization = self;
    gDelegate = delegate;
}

static void overrideBeginAuthorizationWithURL(id, SEL, NSURL *url, NSDictionary *headers, NSData *body)
{
    EXPECT_TRUE(headers != nil);
    EXPECT_TRUE((body == nil) ^ haveHttpBody);
    EXPECT_FALSE(authorizationPerformed);
    authorizationPerformed = true;
}

static void overrideCancelAuthorization(id, SEL)
{
    EXPECT_FALSE(authorizationCancelled);
    authorizationCancelled = true;
}

using GetAuthorizationHintsCallback = void (^)(SOAuthorizationHints *authorizationHints, NSError *error);
static void overrideGetAuthorizationHintsWithURL(id, SEL, NSURL *url, NSInteger responseCode, GetAuthorizationHintsCallback completion)
{
    EXPECT_EQ(responseCode, 0l);
    auto soAuthorizationHintsCore = adoptNS([PAL::allocSOAuthorizationHintsCoreInstance() initWithLocalizedExtensionBundleDisplayName:@"Test"]);
    auto soAuthorizationHints = adoptNS([PAL::allocSOAuthorizationHintsInstance() initWithAuthorizationHintsCore:soAuthorizationHintsCore.get()]);
    completion(soAuthorizationHints.get(), nullptr);
}

#if PLATFORM(MAC)
using NotificationCallback = void (^)(NSNotification *note);
static id overrideAddObserverForName(id, SEL, NSNotificationName name, id, NSOperationQueue *, NotificationCallback block)
{
    if ([name isEqual:NSWindowWillCloseNotification])
        gNotificationCallback = makeBlockPtr(block);
    return @YES;
}
#endif

static bool overrideIsURLFromAppleOwnedDomain(id, SEL, NSURL *)
{
    return true;
}

static void resetState()
{
    navigationCompleted = false;
    policyForAppSSOPerformed = false;
    authorizationPerformed = false;
    authorizationCancelled = false;
    uiShowed = false;
    newWindowCreated = false;
    haveHttpBody = false;
    navigationPolicyDecided = false;
    allMessagesReceived = false;
    finalURL = emptyString();
    gAuthorization = nullptr;
    gDelegate = nullptr;
#if PLATFORM(MAC)
    gNotificationCallback = nullptr;
#endif
    gNewWindow = nullptr;
}

enum class OpenExternalSchemesPolicy : bool {
    Allow,
    Disallow
};

static void configureSOAuthorizationWebView(TestWKWebView *webView, TestSOAuthorizationDelegate *delegate, OpenExternalSchemesPolicy policy = OpenExternalSchemesPolicy::Disallow)
{
    [webView setNavigationDelegate:delegate];
    [webView setUIDelegate:delegate];
    delegate.shouldOpenExternalSchemes = policy == OpenExternalSchemesPolicy::Allow;
}

static String generateHtml(const char* templateHtml, const String& substitute, const String& optionalSubstitute1 = emptyString(), const String& optionalSubstitute2 = emptyString())
{
    StringPrintStream stream;
    stream.printf(templateHtml, substitute.utf8().data(), optionalSubstitute1.utf8().data(), optionalSubstitute2.utf8().data());
    return stream.toString();
}

static void checkAuthorizationOptions(bool userActionInitiated, String initiatorOrigin, int initiatingAction)
{
    EXPECT_TRUE(gAuthorization);
    EXPECT_EQ(((NSNumber *)[gAuthorization authorizationOptions][SOAuthorizationOptionUserActionInitiated]).boolValue, userActionInitiated);
    EXPECT_WK_STREQ([gAuthorization authorizationOptions][SOAuthorizationOptionInitiatorOrigin], initiatorOrigin);
    EXPECT_EQ(((NSNumber *)[gAuthorization authorizationOptions][SOAuthorizationOptionInitiatingAction]).intValue, initiatingAction);
}

namespace TestWebKitAPI {

TEST(SOAuthorizationRedirect, NoInterceptions)
{
    resetState();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, DisableSSO)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = configuration.get().preferences;
    EXPECT_TRUE(preferences._isExtensibleSSOEnabled);

    preferences._extensibleSSOEnabled = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionError)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_TRUE(policyForAppSSOPerformed);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionDoNotHandle)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidNotHandle:gAuthorization];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionCancel)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidCancel:gAuthorization];
    // FIXME: Find a delegate method that can detect load cancels.
    Util::sleep(0.5);
    EXPECT_WK_STREQ("", webView.get()._committedURL.absoluteString);
}

TEST(SOAuthorizationRedirect, InterceptionCompleteWithoutData)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidComplete:gAuthorization];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionUnexpectedCompletion)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorization:gAuthorization didCompleteWithHTTPAuthorizationHeaders:adoptNS([[NSDictionary alloc] init]).get()];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

// { Default delegate method, No App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed1)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationBasicDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_FALSE(policyForAppSSOPerformed); // The delegate isn't registered, so this won't be set.
#if PLATFORM(MAC)
    EXPECT_TRUE(gAuthorization.enableEmbeddedAuthorizationViewController);
#elif PLATFORM(IOS)
    EXPECT_FALSE(gAuthorization.enableEmbeddedAuthorizationViewController);
#endif

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

// { Default delegate method, With App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed2)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationBasicDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    // Force App Links with a request.URL that has a different host than the current one (empty host) and ShouldOpenExternalURLsPolicy::ShouldAllow.
    auto testURL = URL(URL(), "https://www.example.com");
#if PLATFORM(MAC)
    [webView _loadRequest:[NSURLRequest requestWithURL:testURL] shouldOpenExternalURLs:YES];
#elif PLATFORM(IOS)
    // Here we force RedirectSOAuthorizationSession to go to the with user gestures route.
    [webView evaluateJavaScript: [NSString stringWithFormat:@"location = '%@'", (id)testURL.string()] completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
#if PLATFORM(MAC)
    checkAuthorizationOptions(false, "", 0);
#elif PLATFORM(IOS)
    checkAuthorizationOptions(true, "null", 0);
#endif
    EXPECT_FALSE(policyForAppSSOPerformed); // The delegate isn't registered, so this won't be set.

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

// { Custom delegate method, With App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed3)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    EXPECT_TRUE(gAuthorization.enableEmbeddedAuthorizationViewController);

    // Force App Links with a request.URL that has a different host than the current one (empty host) and ShouldOpenExternalURLsPolicy::ShouldAllow.
    auto testURL = URL(URL(), "https://www.example.com");
#if PLATFORM(MAC)
    [webView _loadRequest:[NSURLRequest requestWithURL:testURL] shouldOpenExternalURLs:YES];
#elif PLATFORM(IOS)
    // Here we force RedirectSOAuthorizationSession to go to the with user gestures route.
    [webView evaluateJavaScript: [NSString stringWithFormat:@"location = '%@'", (id)testURL.string()] completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
#if PLATFORM(MAC)
    checkAuthorizationOptions(false, "", 0);
#elif PLATFORM(IOS)
    checkAuthorizationOptions(true, "null", 0);
#endif
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

// { _WKNavigationActionPolicyAllowWithoutTryingAppLink, No App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed4)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    // A separate delegate that implements decidePolicyForNavigationAction.
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setIsDefaultPolicy:false];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithOtherHttpStatusCode)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithCookie)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;", @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);

    navigationCompleted = false;
    [webView evaluateJavaScript: @"document.cookie" completionHandler:^(NSString *result, NSError *) {
        EXPECT_WK_STREQ("sessionid=38afes7a8", [result UTF8String]);
        navigationCompleted = true;
    }];
    Util::run(&navigationCompleted);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithCookies)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8, qwerty=219ffwef9w0f, id=a3fWa;", @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);

    navigationCompleted = false;
    [webView evaluateJavaScript: @"document.cookie" completionHandler:^(NSString *result, NSError *) {
        EXPECT_WK_STREQ("id=a3fWa; qwerty=219ffwef9w0f; sessionid=38afes7a8", [result UTF8String]);
        navigationCompleted = true;
    }];
    Util::run(&navigationCompleted);
}

// The redirected URL has a different domain than the test URL.
TEST(SOAuthorizationRedirect, InterceptionSucceedWithRedirectionAndCookie)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    auto testURL = URL(URL(), "https://www.example.com");
#if PLATFORM(MAC)
    [webView loadRequest:[NSURLRequest requestWithURL:testURL]];
#elif PLATFORM(IOS)
    // Here we force RedirectSOAuthorizationSession to go to the with user gestures route.
    [webView evaluateJavaScript: [NSString stringWithFormat:@"location = '%@'", (id)testURL.string()] completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
#if PLATFORM(MAC)
    checkAuthorizationOptions(false, "", 0);
#elif PLATFORM(IOS)
    checkAuthorizationOptions(true, "null", 0);
#endif
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;", @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);

    navigationCompleted = false;
    [webView evaluateJavaScript: @"document.cookie" completionHandler:^(NSString *result, NSError *) {
        EXPECT_WK_STREQ("", [result UTF8String]);
        navigationCompleted = true;
    }];
    Util::run(&navigationCompleted);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithDifferentOrigin)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto redirectURL = URL(URL(), "https://www.example.com");
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:redirectURL statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : redirectURL.string() }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithWaitingSession)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // The session will be waiting since the web view is is not in the window.
    [webView removeFromSuperview];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_FALSE(authorizationPerformed);

    // Should activate the session.
    [webView addToTestWindow];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionAbortedWithWaitingSession)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // The session will be waiting since the web view is is not in the window.
    [webView removeFromSuperview];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL1.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_FALSE(authorizationPerformed);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL2.get()]];
    // Should activate the session.
    [webView addToTestWindow];

    // The waiting session should be aborted as the previous navigation is overwritten by a new navigation.
    Util::run(&navigationCompleted);
    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_FALSE(authorizationPerformed);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithActiveSessionDidMoveWindow)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Should be a no op.
    [webView addToTestWindow];

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedTwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        policyForAppSSOPerformed = false;
        [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
        Util::run(&authorizationPerformed);
        checkAuthorizationOptions(false, "", 0);
        EXPECT_TRUE(policyForAppSSOPerformed);

        navigationCompleted = false;
        RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
        Util::run(&navigationCompleted);
#if PLATFORM(IOS)
        navigationCompleted = false;
        Util::run(&navigationCompleted);
#endif
        EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
    }
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSuppressActiveSession)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(cancelAuthorization), reinterpret_cast<IMP>(overrideCancelAuthorization));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Suppress the last active session.
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationCancelled);
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSuppressWaitingSession)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // The session will be waiting since the web view is is not int the window.
    [webView removeFromSuperview];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_FALSE(policyForAppSSOPerformed);

    // Suppress the last waiting session.
    navigationPolicyDecided = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_FALSE(policyForAppSSOPerformed);

    // Activate the last session.
    [webView addToTestWindow];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSAML)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // Add a http body to the request to mimic a SAML request.
    auto request = adoptNS([NSMutableURLRequest requestWithURL:testURL.get()]);
    [request setHTTPBody:adoptNS([[NSData alloc] init]).get()];
    haveHttpBody = true;

    [webView loadRequest:request.get()];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Pass a HTTP 200 response with a html to mimic a SAML response.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:samlResponse length:strlen(samlResponse)]).get()];
    [webView waitForMessage:@"SAML"];
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSAMLWithPSON)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple3" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:baseURL.get()]];
    Util::run(&navigationCompleted);

    // PSON: file:/// => example.com
    [webView loadRequest:[NSURLRequest requestWithURL:(NSURL *)testURL]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    navigationCompleted = false;
    // Pass a HTTP 200 response with a html to mimic a SAML response.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:(NSURL *)testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:samlResponse length:strlen(samlResponse)]).get()];
    Util::run(&navigationCompleted);

    authorizationPerformed = false;
    navigationPolicyDecided = false;
    policyForAppSSOPerformed = false;
    [webView _evaluateJavaScriptWithoutUserGesture:@"location = 'http://www.example.com'" completionHandler:nil];
    Util::run(&navigationPolicyDecided);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationRedirect, AuthorizationOptions)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:@"" baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    [webView evaluateJavaScript: @"location = 'http://www.example.com'" completionHandler:nil];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "http://www.webkit.org", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationRedirect, InterceptionDidNotHandleTwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Test passes if no crashes.
    [gDelegate authorizationDidNotHandle:gAuthorization];
    [gDelegate authorizationDidNotHandle:gAuthorization];
}

TEST(SOAuthorizationRedirect, InterceptionCompleteTwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Test passes if no crashes.
    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setAllowSOAuthorizationLoad:false];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_TRUE(policyForAppSSOPerformed);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyAllowAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setAllowSOAuthorizationLoad:false];
    [delegate setIsAsyncExecution:true];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_TRUE(policyForAppSSOPerformed);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}


// FIXME(175204): Enable the iOS tests once the bug is fixed.
#if PLATFORM(MAC)
TEST(SOAuthorizationRedirect, InterceptionSucceedWithUI)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
    EXPECT_FALSE(uiShowed);
}

TEST(SOAuthorizationRedirect, InterceptionCancelWithUI)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    [gDelegate authorizationDidCancel:gAuthorization];
    // FIXME: Find a delegate method that can detect load cancels.
    Util::sleep(0.5);
    EXPECT_WK_STREQ("", webView.get()._committedURL.absoluteString);
    EXPECT_FALSE(uiShowed);
}

TEST(SOAuthorizationRedirect, InterceptionErrorWithUI)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
    EXPECT_FALSE(uiShowed);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSuppressActiveSessionWithUI)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(cancelAuthorization), reinterpret_cast<IMP>(overrideCancelAuthorization));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    // Suppress the last active session.
    authorizationPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationCancelled);
    EXPECT_TRUE(policyForAppSSOPerformed);
    Util::run(&authorizationPerformed);
    EXPECT_FALSE(uiShowed);

    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, ShowUITwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    uiShowed = false;
    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *error) {
        EXPECT_FALSE(success);
        EXPECT_EQ(error.code, kSOErrorAuthorizationPresentationFailed);
        EXPECT_WK_STREQ(error.domain, "com.apple.AppSSO.AuthorizationError");
        uiShowed = true;
    }];
    Util::run(&uiShowed);
}
#endif

#if PLATFORM(MAC)
TEST(SOAuthorizationRedirect, NSNotificationCenter)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    {
        InstanceMethodSwizzler swizzler4([NSNotificationCenter class], @selector(addObserverForName:object:queue:usingBlock:), reinterpret_cast<IMP>(overrideAddObserverForName));
        [viewController setView:view.get()];
        [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
            EXPECT_TRUE(success);
        }];
        Util::run(&uiShowed);
    }

    gNotificationCallback(nullptr);
    EXPECT_FALSE(uiShowed);
}

TEST(SOAuthorizationRedirect, DismissUIDuringMiniaturization)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    bool didEndSheet = false;
    auto *hostWindow = [webView hostWindow];
    auto observer = adoptNS([[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEndSheetNotification object:hostWindow queue:nil usingBlock:[&didEndSheet] (NSNotification *) {
        didEndSheet = true;
    }]);
    [hostWindow miniaturize:hostWindow];

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];
    EXPECT_FALSE(didEndSheet);

    // UI is only dimissed after the hostWindow is deminimized.
    [hostWindow deminiaturize:hostWindow];
    EXPECT_TRUE(didEndSheet);
}

TEST(SOAuthorizationRedirect, DismissUIDuringHiding)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    bool didEndSheet = false;
    auto observer = adoptNS([[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEndSheetNotification object:[webView hostWindow] queue:nil usingBlock:[&didEndSheet] (NSNotification *) {
        didEndSheet = true;
    }]);

    [NSApp hide:NSApp];

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];
    EXPECT_FALSE(didEndSheet);

    // UI is only dimissed after the hostApp is unhidden.
    [NSApp unhide:NSApp];
    EXPECT_TRUE(didEndSheet);
}

TEST(SOAuthorizationRedirect, DismissUIDuringMiniaturizationThenAnother)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController1 = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view1 = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController1 setView:view1.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController1.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    bool didEndSheet = false;
    auto *hostWindow = [webView hostWindow];
    auto observer = adoptNS([[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEndSheetNotification object:hostWindow queue:nil usingBlock:[&didEndSheet] (NSNotification *) {
        didEndSheet = true;
    }]);
    [hostWindow miniaturize:hostWindow];

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];
    EXPECT_FALSE(didEndSheet);

    // Load another AppSSO request.
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // UI is only dimissed after the hostApp is unhidden.
    [hostWindow deminiaturize:hostWindow];
    EXPECT_TRUE(didEndSheet);
}

TEST(SOAuthorizationRedirect, DismissUIDuringHidingThenAnother)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto viewController = adoptNS([[TestSOAuthorizationViewController alloc] init]);
    auto view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [viewController setView:view.get()];

    [gDelegate authorization:gAuthorization presentViewController:viewController.get() withCompletion:^(BOOL success, NSError *) {
        EXPECT_TRUE(success);
    }];
    Util::run(&uiShowed);

    bool didEndSheet = false;
    auto observer = adoptNS([[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEndSheetNotification object:[webView hostWindow] queue:nil usingBlock:[&didEndSheet] (NSNotification *) {
        didEndSheet = true;
    }]);

    [NSApp hide:NSApp];

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];
    EXPECT_FALSE(didEndSheet);

    // Load another AppSSO request.
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // UI is only dimissed after the hostApp is unhidden.
    [NSApp unhide:NSApp];
    EXPECT_TRUE(didEndSheet);
}
#endif

TEST(SOAuthorizationPopUp, NoInterceptions)
{
    resetState();

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:testURL.get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    navigationCompleted = false;
#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&newWindowCreated);
    Util::run(&navigationCompleted);
    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

// FIXME(172614): Enable the following test for iOS once the bug is fixed.
#if PLATFORM(MAC)
TEST(SOAuthorizationPopUp, NoInterceptionsSubFrame)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto iframeTestHtml = generateHtml(openerTemplate, testURL.get().absoluteString);
    auto testHtml = makeString("<iframe style='width:400px;height:400px' srcdoc=\"", iframeTestHtml, "\" />");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    // The new window will not navigate to the testURL as the iframe has unique origin.
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
    Util::run(&newWindowCreated);
    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_FALSE(authorizationPerformed);
}
#endif

TEST(SOAuthorizationPopUp, NoInterceptionsWithoutUserGesture)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    // The default value of javaScriptCanOpenWindowsAutomatically is NO on iOS, and YES on macOS.
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView _evaluateJavaScriptWithoutUserGesture: @"window.open('http://www.example.com')" completionHandler:nil];
    Util::run(&newWindowCreated);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_FALSE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationPopUp, InterceptionError)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    authorizationPerformed = false;
    navigationCompleted = false;
    policyForAppSSOPerformed = false;
    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&newWindowCreated);
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
    EXPECT_FALSE(authorizationPerformed); // Don't intercept the first navigation in the new window.
    EXPECT_FALSE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationPopUp, InterceptionCancel)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorizationDidCancel:gAuthorization];
    }
    [webView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, InterceptionSucceedCloseByItself)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "window.close();").utf8(); // The pop up closes itself.
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [webView waitForMessage:@"Hello."];
    [webView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, InterceptionSucceedCloseByParent)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string(), "", "event.source.close();"); // The parent closes the pop up.

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [webView waitForMessage:@"Hello."];
    [webView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, InterceptionSucceedCloseByWebKit)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [webView waitForMessage:@"Hello."];
    [webView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, InterceptionSucceedWithOtherHttpStatusCode)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Will fallback to web path.
    navigationCompleted = false;
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    Util::run(&newWindowCreated);
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_FALSE(policyForAppSSOPerformed);
}

// Setting cookie is ensured by other tests. Here is to cover if the whole authentication handshake can be completed.
TEST(SOAuthorizationPopUp, InterceptionSucceedWithCookie)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;"}]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [webView waitForMessage:@"Hello."];
    [webView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, InterceptionSucceedTwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        policyForAppSSOPerformed = false;
#if PLATFORM(MAC)
        [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
        [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
        Util::run(&authorizationPerformed);
        checkAuthorizationOptions(true, "file://", 1);
        EXPECT_TRUE(policyForAppSSOPerformed);

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
        auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
        // The secret WKWebView needs to be destroyed right the way.
        @autoreleasepool {
            [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
        }
        [webView waitForMessage:@"Hello."];
        [webView waitForMessage:@"WindowClosed."];
    }
}

TEST(SOAuthorizationPopUp, InterceptionSucceedSuppressActiveSession)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(cancelAuthorization), reinterpret_cast<IMP>(overrideCancelAuthorization));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Suppress the last active session.
    auto newWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:webView.get().configuration]);
    configureSOAuthorizationWebView(newWebView.get(), delegate.get());

    navigationCompleted = false;
    [newWebView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
#if PLATFORM(MAC)
    [newWebView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [newWebView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationCancelled);
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    navigationCompleted = false;
    [webView evaluateJavaScript: @"newWindow" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE(result == adoptNS([NSNull null]).get());
        navigationCompleted = true;
    }];
    Util::run(&navigationCompleted);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [newWebView waitForMessage:@"Hello."];
    [newWebView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, InterceptionSucceedNewWindowNavigation)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string(), makeString("newWindow.location = '", baseURL.get().absoluteString.UTF8String, "';")); // Starts a new navigation on the new window.

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [webView waitForMessage:@"Hello."];
    [webView waitForMessage:@"WindowClosed."];
}

TEST(SOAuthorizationPopUp, AuthorizationOptions)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "http://www.webkit.org", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&newWindowCreated);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyAllowAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler4(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "window.close();").utf8(); // The pop up closes itself.
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    [webView waitForMessage:@"Hello."];
    [webView waitForMessage:@"WindowClosed."];
}


TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&newWindowCreated);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationSubFrame, NoInterceptions)
{
    resetState();
    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(parentTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    [webView waitForMessage:@""];
    EXPECT_FALSE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationSubFrame, NoInterceptionsNonAppleFirstPartyMainFrame)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testHtml = generateHtml(parentTemplate, URL(URL(), "http://www.example.com").string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    // Try to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_FALSE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationSubFrame, InterceptionError)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(parentTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    [webView waitForMessage:@"null"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "file://", 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    [webView waitForMessage:@"null"];
    [webView waitForMessage:@"SOAuthorizationDidCancel"];
    [webView waitForMessage:@""];
    // Trys to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't load the request of the iframe to the main frame.
    EXPECT_WK_STREQ("", finalURL);
}

TEST(SOAuthorizationSubFrame, InterceptionCancel)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(parentTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    [webView waitForMessage:@"null"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "file://", 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidCancel:gAuthorization];
    [webView waitForMessage:@"null"];
    [webView waitForMessage:@"SOAuthorizationDidCancel"];
    [webView waitForMessage:@""];
    // Trys to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't load the request of the iframe to the main frame.
    EXPECT_WK_STREQ("", finalURL);
}

TEST(SOAuthorizationSubFrame, InterceptionSuccess)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:nil];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "null", 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"Hello."];
}

TEST(SOAuthorizationSubFrame, InterceptionSucceedWithOtherHttpStatusCode)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(parentTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    [webView waitForMessage:@"null"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "file://", 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Will fallback to web path.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    [webView waitForMessage:@"null"];
    [webView waitForMessage:@"SOAuthorizationDidCancel"];
    [webView waitForMessage:@""];
    // Trys to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't load the request of the iframe to the main frame.
    EXPECT_WK_STREQ("", finalURL);
}

// Setting cookie is ensured by other tests. Here is to cover if the whole authentication handshake can be completed.
TEST(SOAuthorizationSubFrame, InterceptionSucceedWithCookie)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:nil];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "null", 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;"}]);
    auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"Hello."];
}

TEST(SOAuthorizationSubFrame, InterceptionSuccessTwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        navigationCompleted = false;
        policyForAppSSOPerformed = false;

        [webView loadHTMLString:testHtml baseURL:nil];
        [webView waitForMessage:@"http://www.example.com"];
        [webView waitForMessage:@"SOAuthorizationDidStart"];
        checkAuthorizationOptions(false, "null", 2);
        EXPECT_TRUE(policyForAppSSOPerformed);

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
        auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
        [webView waitForMessage:@"http://www.example.com"];
        [webView waitForMessage:@"Hello."];
    }
}

TEST(SOAuthorizationSubFrame, AuthorizationOptions)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.apple.com")];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "http://www.apple.com", 2);
}

TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.apple.com")];
    // Try to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
}

TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyAllowAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml baseURL:nil];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "null", 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"Hello."];
}

TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.apple.com")];
    // Try to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
}

TEST(SOAuthorizationSubFrame, InterceptionErrorWithReferrer)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    HTTPServer server([parentHtml = generateHtml(parentTemplate, "simple.html"), frameHtml = generateHtml(iframeTemplate, "parent.postMessage('Referrer: ' + document.referrer, '*');")] (const Connection& connection) {
        RetainPtr<NSString> firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentHtml.length(),
            (id)parentHtml
        ];
        RetainPtr<NSString> secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            frameHtml.length(),
            (id)frameHtml
        ];

        connection.receiveHTTPRequest([=](Vector<char>&&) {
            connection.send(firstResponse.get(), [=] {
                connection.receiveHTTPRequest([=](Vector<char>&&) {
                    connection.send(secondResponse.get());
                });
            });
        });
    });

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    auto origin = makeString("http://127.0.0.1:", server.port());
    [webView _loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(id)origin]] shouldOpenExternalURLs:NO];
    [webView waitForMessage:(id)origin];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    [webView waitForMessage:(id)origin];
    [webView waitForMessage:@"SOAuthorizationDidCancel"];
    [webView waitForMessage:(id)makeString("Referrer: ", origin, "/")]; // Referrer policy requires '/' after origin.
}

TEST(SOAuthorizationSubFrame, InterceptionErrorMessageOrder)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> baseURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"GetSessionCookie" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto testHtml = generateHtml(parentTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);
    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionSuccessMessageOrder)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));
    InstanceMethodSwizzler swizzler5(PAL::getSOAuthorizationClass(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:nil];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

} // namespace TestWebKitAPI

#endif
