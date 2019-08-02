/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
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
static bool authorizationPerformed = false;
static bool authorizationCancelled = false;
static bool uiShowed = false;
static bool newWindowCreated = false;
static bool haveHttpBody = false;
static bool navigationPolicyDecided = false;
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
    gNewWindow = [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];
    [gNewWindow setNavigationDelegate:self];
    return gNewWindow.get();
}

- (void)_webView:(WKWebView *)webView decidePolicyForSOAuthorizationLoadWithCurrentPolicy:(_WKSOAuthorizationLoadPolicy)policy forExtension:(NSString *)extension completionHandler:(void (^)(_WKSOAuthorizationLoadPolicy policy))completionHandler
{
    EXPECT_EQ(policy, _WKSOAuthorizationLoadPolicyAllow);
    EXPECT_TRUE([extension isEqual:@""]);
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
    authorizationPerformed = false;
    authorizationCancelled = false;
    uiShowed = false;
    newWindowCreated = false;
    haveHttpBody = false;
    navigationPolicyDecided = false;
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

// FIXME<rdar://problem/48909336>: Replace the below with AppSSO constants.
static void checkAuthorizationOptions(bool userActionInitiated, String initiatorOrigin, int initiatingAction)
{
    EXPECT_TRUE(gAuthorization);
    // FIXME: Remove the selector once the iOS bots has been updated to a recent SDK.
    auto selector = NSSelectorFromString(@"authorizationOptions");
    if ([gAuthorization respondsToSelector:selector]) {
        EXPECT_EQ(((NSNumber *)[gAuthorization performSelector:selector][SOAuthorizationOptionUserActionInitiated]).boolValue, userActionInitiated);
        EXPECT_WK_STREQ([gAuthorization performSelector:selector][@"initiatorOrigin"], initiatorOrigin);
        EXPECT_EQ(((NSNumber *)[gAuthorization performSelector:selector][@"initiatingAction"]).intValue, initiatingAction);
    }
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

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionError)
{
    resetState();
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionDoNotHandle)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationBasicDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);
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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    // A separate delegate that implements decidePolicyForNavigationAction.
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setIsDefaultPolicy:false];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // The session will be waiting since the web view is is not in the window.
    [webView removeFromSuperview];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(authorizationPerformed);

    // Should activate the session.
    [webView addToTestWindow];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> testURL2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // The session will be waiting since the web view is is not in the window.
    [webView removeFromSuperview];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL1.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(authorizationPerformed);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL2.get()]];
    // Should activate the session.
    [webView addToTestWindow];

    // The waiting session should be aborted as the previous navigation is overwritten by a new navigation.
    Util::run(&navigationCompleted);
    EXPECT_FALSE(authorizationPerformed);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithActiveSessionDidMoveWindow)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
        Util::run(&authorizationPerformed);
        checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

    // Suppress the last active session.
    authorizationPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationCancelled);
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // The session will be waiting since the web view is is not int the window.
    [webView removeFromSuperview];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(authorizationPerformed);

    // Suppress the last waiting session.
    navigationPolicyDecided = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationPolicyDecided);
    EXPECT_FALSE(authorizationPerformed);

    // Activate the last session.
    [webView addToTestWindow];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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

    // Pass a HTTP 200 response with a html to mimic a SAML response.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:samlResponse length:strlen(samlResponse)]).get()];
    [webView waitForMessage:@"SAML"];
}

TEST(SOAuthorizationRedirect, AuthorizationOptions)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:@"" baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    [webView evaluateJavaScript: @"location = 'http://www.example.com'" completionHandler:nil];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "http://www.webkit.org", 0);
}

TEST(SOAuthorizationRedirect, InterceptionDidNotHandleTwice)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

    // Test passes if no crashes.
    RetainPtr<NSURL> redirectURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setAllowSOAuthorizationLoad:false];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyAllowAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, "", 0);

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
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setAllowSOAuthorizationLoad:false];
    [delegate setIsAsyncExecution:true];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationCompleted);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);

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
    EXPECT_FALSE(authorizationPerformed);
}
#endif

TEST(SOAuthorizationPopUp, NoInterceptionsWithoutUserGesture)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    // The default value of javaScriptCanOpenWindowsAutomatically is NO on iOS, and YES on macOS.
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView _evaluateJavaScriptWithoutUserGesture: @"window.open('http://www.example.com')" completionHandler:nil];
    Util::run(&newWindowCreated);
    EXPECT_FALSE(authorizationPerformed);
}

TEST(SOAuthorizationPopUp, InterceptionError)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

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

    authorizationPerformed = false;
    navigationCompleted = false;
    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&newWindowCreated);
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
    EXPECT_FALSE(authorizationPerformed); // Don't intercept the first navigation in the new window.
}

TEST(SOAuthorizationPopUp, InterceptionCancel)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

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

    // Will fallback to web path.
    navigationCompleted = false;
    authorizationPerformed = false;
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHtml(newWindowResponseTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    Util::run(&newWindowCreated);
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
    EXPECT_FALSE(authorizationPerformed);
}

// Setting cookie is ensured by other tests. Here is to cover if the whole authentication handshake can be completed.
TEST(SOAuthorizationPopUp, InterceptionSucceedWithCookie)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

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
#if PLATFORM(MAC)
        [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
        [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
        Util::run(&authorizationPerformed);
        checkAuthorizationOptions(true, "file://", 1);

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

    // Suppress the last active session.
    auto newWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:webView.get().configuration]);
    configureSOAuthorizationWebView(newWebView.get(), delegate.get());

    navigationCompleted = false;
    [newWebView loadHTMLString:testHtml baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    authorizationPerformed = false;
#if PLATFORM(MAC)
    [newWebView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS)
    [newWebView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationCancelled);
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://", 1);

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
}

TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

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
}

TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyAllowAsync)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

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
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

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
}

TEST(SOAuthorizationSubFrame, NoInterceptionsNonAppleFirstPartyMainFrame)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));

    auto testHtml = generateHtml(parentTemplate, URL(URL(), "http://www.example.com").string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:(NSURL *)URL(URL(), "http://www.webkit.org")];
    // Try to wait until the iframe load is finished.
    Util::sleep(0.5);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
}

TEST(SOAuthorizationSubFrame, InterceptionError)
{
    resetState();
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClass(), @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate));
    InstanceMethodSwizzler swizzler3(PAL::getSOAuthorizationClass(), @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL));
    ClassMethodSwizzler swizzler4([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain));

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

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
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

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:nil];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "null", 2);

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

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml baseURL:nil];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"SOAuthorizationDidStart"];
    checkAuthorizationOptions(false, "null", 2);

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

    auto testURL = URL(URL(), "http://www.example.com");
    auto testHtml = generateHtml(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        navigationCompleted = false;

        [webView loadHTMLString:testHtml baseURL:nil];
        [webView waitForMessage:@"http://www.example.com"];
        [webView waitForMessage:@"SOAuthorizationDidStart"];
        checkAuthorizationOptions(false, "null", 2);

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
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

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

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHtml(iframeTemplate, "").utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    [webView waitForMessage:@"http://www.example.com"];
    [webView waitForMessage:@"Hello."];
}

TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    ClassMethodSwizzler swizzler(PAL::getSOAuthorizationClass(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));

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

    TCPServer server([parentHtml = generateHtml(parentTemplate, "simple.html"), frameHtml = generateHtml(iframeTemplate, "parent.postMessage('Referrer: ' + document.referrer, '*');")] (int socket) {
        NSString *firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentHtml.length(),
            (id)parentHtml
        ];
        NSString *secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            frameHtml.length(),
            (id)frameHtml
        ];

        TCPServer::read(socket);
        TCPServer::write(socket, firstResponse.UTF8String, firstResponse.length);
        TCPServer::read(socket);
        TCPServer::write(socket, secondResponse.UTF8String, secondResponse.length);
    });

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    auto origin = makeString("http://127.0.0.1:", static_cast<unsigned>(server.port()));
    [webView _loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(id)origin]] shouldOpenExternalURLs:NO];
    [webView waitForMessage:(id)origin];
    [webView waitForMessage:@"SOAuthorizationDidStart"];

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    [webView waitForMessage:(id)origin];
    [webView waitForMessage:@"SOAuthorizationDidCancel"];
    [webView waitForMessage:(id)makeString("Referrer: ", origin, "/")]; // Referrer policy requires '/' after origin.
}

} // namespace TestWebKitAPI

#endif
