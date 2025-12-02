/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#if HAVE(APP_SSO)

#import "ClassMethodSwizzler.h"
#import "HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/cocoa/AuthKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/StringPrintStream.h>
#import <wtf/URL.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

#import <pal/cocoa/AppSSOSoftLink.h>

static bool navigationCompleted = false;
static bool appSSONavigationFailed = false;
static bool policyForAppSSOPerformed = false;
static bool authorizationPerformed = false;
static bool authorizationCancelled = false;
static bool uiShowed = false;
static bool newWindowCreated = false;
static bool haveHttpBody = false;
static RetainPtr<NSData> httpBody;
static bool navigationPolicyDecided = false;
static bool allMessagesReceived = false;
static String finalURL;
static SOAuthorization* gAuthorization;
static id<SOAuthorizationDelegate> gDelegate;
#if PLATFORM(MAC)
static BlockPtr<void(NSNotification *)> gNotificationCallback;
#endif
static RetainPtr<WKWebView> gNewWindow;

static constexpr auto openerTemplate =
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

static constexpr auto newWindowResponseTemplate =
"<html>"
"<script>"
"window.opener.postMessage('Hello.', '*');"
"%s"
"</script>"
"</html>";

static constexpr auto parentTemplate =
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

static constexpr auto iframeTemplate =
"<html>"
"<script>"
"parent.postMessage('Hello.', '*');"
"%s"
"</script>"
"</html>";

static constexpr auto samlResponse =
"<html>"
"<script>"
"window.webkit.messageHandlers.testHandler.postMessage('SAML');"
"</script>"
"</html>";

constexpr Seconds actionAbsenceTimeout = 300_ms;
constexpr Seconds actionDoneTimeout = 1000_ms;

static RetainPtr<WKNavigationAction> delegateNavigationAction;

namespace {

class ScopedNotificationCenterObserveOnce {
    WTF_MAKE_NONCOPYABLE(ScopedNotificationCenterObserveOnce);
public:
    ScopedNotificationCenterObserveOnce() = default;
    ScopedNotificationCenterObserveOnce(NSNotificationName name, NSObject* object)
    {
        m_observer = [m_center addObserverForName:name object:object queue:nil usingBlock:[this] (NSNotification *) {
            [m_center removeObserver:m_observer.get()];
            m_observer = nullptr;
            m_didObserve = true;
            m_center = nullptr;
        }];
    }

    ~ScopedNotificationCenterObserveOnce()
    {
        if (!m_observer)
            return;
        [m_center removeObserver:m_observer.get()];
    }

    ScopedNotificationCenterObserveOnce(ScopedNotificationCenterObserveOnce&&) = default;
    ScopedNotificationCenterObserveOnce& operator=(ScopedNotificationCenterObserveOnce&&) = default;

    bool waitFor(Seconds timeout)
    {
        return TestWebKitAPI::Util::runFor(&m_didObserve, timeout);
    }

    explicit operator bool() const { return m_didObserve; }

private:
    NSNotificationCenter * __weak m_center { [NSNotificationCenter defaultCenter] };
    RetainPtr<NSObject> m_observer;
    bool m_didObserve { false };
};

}

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
    delegateNavigationAction = navigationAction;
    navigationPolicyDecided = true;
    EXPECT_EQ(navigationAction._shouldOpenExternalSchemes, self.shouldOpenExternalSchemes);
    if (self.isDefaultPolicy) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }
    decisionHandler(_WKNavigationActionPolicyAllowWithoutTryingAppLink);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    EXPECT_FALSE(navigationCompleted);
    appSSONavigationFailed = true;
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
    dispatch_async(mainDispatchQueueSingleton(), ^() {
        if (allowSOAuthorizationLoad)
            completionHandler(_WKSOAuthorizationLoadPolicyAllow);
        else
            completionHandler(_WKSOAuthorizationLoadPolicyIgnore);
    });
}

#if PLATFORM(IOS) || PLATFORM(VISION)
- (UIViewController *)_presentingViewControllerForWebView:(WKWebView *)webView
{
    return nil;
}
#endif

@end

#if PLATFORM(MAC)
@interface TestSOAuthorizationViewController : NSViewController
#elif PLATFORM(IOS) || PLATFORM(VISION)
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
    RetainPtr<NSArray> _expectedMessages;
}

- (instancetype)initWithExpectation:(NSArray *)expectedMessages
{
    self = [super init];
    if (!self)
        return nil;
    _messages = adoptNS([[NSMutableArray alloc] init]);
    _expectedMessages = expectedMessages;
    return self;
}

- (void)extendExpectations:(NSArray *)additionalMessages
{
    _expectedMessages = [_expectedMessages arrayByAddingObjectsFromArray:additionalMessages];
    allMessagesReceived = false;
}

- (void)resetExpectations:(NSArray *)expectedMessages
{
    _messages = adoptNS([[NSMutableArray alloc] init]);
    _expectedMessages = expectedMessages;
    allMessagesReceived = false;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    auto curIndex = [_messages count];
    [_messages addObject:message.body];
    if ([_messages count] == [_expectedMessages count])
        allMessagesReceived = true;
    if (curIndex < [_expectedMessages count] && [_expectedMessages objectAtIndex:curIndex] != [NSNull null])
        EXPECT_WK_STREQ([_expectedMessages objectAtIndex:curIndex], [_messages objectAtIndex:curIndex]);
}

@end

using CanPerformAuthorizationWithURLCallback = void (^)(BOOL);
static void overrideCanPerformAuthorizationWithURLCompletion(id, SEL, NSURL *url, NSInteger, NSString *, BOOL, CanPerformAuthorizationWithURLCallback completion)
{
    if (![url.lastPathComponent isEqual:@"simple.html"] && ![url.host isEqual:@"www.example.com"] && ![url.lastPathComponent isEqual:@"GetSessionCookie.html"]) {
        completion(NO);
        return;
    }
    completion(YES);
}

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
    if (haveHttpBody)
        httpBody = body;
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
    appSSONavigationFailed = false;
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

#if !COMPILER_HAS_ATTRIBUTE(format_matches)
// FIXME: Remove when oldest macOS supported is (__MAC_OS_X_VERSION_MIN_REQUIRED >= 260000).
ALLOW_NONLITERAL_FORMAT_BEGIN
#endif

WTF_ATTRIBUTE_PRINTF_MATCHES(1, "%s")
static String generateHTML(const char* templateHTML, const String& substitute)
{
    StringPrintStream stream;
    stream.printf(templateHTML, substitute.utf8().data());
    return stream.toString();
}

WTF_ATTRIBUTE_PRINTF_MATCHES(1, "%s %s %s")
static String generateOpenerHTML(const char* templateHTML, const String& substitute, const String& optionalSubstitute1 = emptyString(), const String& optionalSubstitute2 = emptyString())
{
    StringPrintStream stream;
    stream.printf(templateHTML, substitute.utf8().data(), optionalSubstitute1.utf8().data(), optionalSubstitute2.utf8().data());
    return stream.toString();
}

#if !COMPILER_HAS_ATTRIBUTE(format_matches)
ALLOW_NONLITERAL_FORMAT_END
#endif

static void checkAuthorizationOptions(bool userActionInitiated, String initiatorOrigin, int initiatingAction, String path = { })
{
    EXPECT_TRUE(gAuthorization);
    EXPECT_EQ(((NSNumber *)[gAuthorization authorizationOptions][SOAuthorizationOptionUserActionInitiated]).boolValue, userActionInitiated);
    EXPECT_WK_STREQ([gAuthorization authorizationOptions][SOAuthorizationOptionInitiatorOrigin], initiatorOrigin);
    EXPECT_EQ(((NSNumber *)[gAuthorization authorizationOptions][SOAuthorizationOptionInitiatingAction]).intValue, initiatingAction);
    if (!path.isNull())
        EXPECT_WK_STREQ([gAuthorization authorizationOptions][kSOAuthorizationOptionInitiatingPath], path);
}

#define SWIZZLE_SOAUTH(SOAuthClass) \
    ClassMethodSwizzler swizzler0(SOAuthClass, @selector(canPerformAuthorizationWithURL:responseCode:callerBundleIdentifier:useInternalExtensions:completion:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURLCompletion)); \
    ClassMethodSwizzler swizzler1(SOAuthClass, @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL)); \
    InstanceMethodSwizzler swizzler2(SOAuthClass, @selector(setDelegate:), reinterpret_cast<IMP>(overrideSetDelegate)); \
    InstanceMethodSwizzler swizzler3(SOAuthClass, @selector(beginAuthorizationWithURL:httpHeaders:httpBody:), reinterpret_cast<IMP>(overrideBeginAuthorizationWithURL)); \
    InstanceMethodSwizzler swizzler4(SOAuthClass, @selector(cancelAuthorization), reinterpret_cast<IMP>(overrideCancelAuthorization)); \
    InstanceMethodSwizzler swizzler5(SOAuthClass, @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

#define SWIZZLE_AKAUTH() \
    ClassMethodSwizzler swizzler6([AKAuthorizationController class], @selector(isURLFromAppleOwnedDomain:), reinterpret_cast<IMP>(overrideIsURLFromAppleOwnedDomain)); \

namespace TestWebKitAPI {

TEST(SOAuthorizationRedirect, NoInterceptions)
{
    resetState();
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    // This test relies on us not swizzling most of the SOAuthorizationClass methods.
    ClassMethodSwizzler swizzler0(PAL::getSOAuthorizationClassSingleton(), @selector(canPerformAuthorizationWithURL:responseCode:callerBundleIdentifier:useInternalExtensions:completion:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURLCompletion));
    ClassMethodSwizzler swizzler1(PAL::getSOAuthorizationClassSingleton(), @selector(canPerformAuthorizationWithURL:responseCode:), reinterpret_cast<IMP>(overrideCanPerformAuthorizationWithURL));
    InstanceMethodSwizzler swizzler2(PAL::getSOAuthorizationClassSingleton(), @selector(getAuthorizationHintsWithURL:responseCode:completion:), reinterpret_cast<IMP>(overrideGetAuthorizationHintsWithURL));

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidNotHandle:gAuthorization];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionCancel)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidCancel:gAuthorization];
    Util::run(&appSSONavigationFailed);
    EXPECT_WK_STREQ("", webView.get()._committedURL.absoluteString);
    EXPECT_FALSE(webView.get().loading);
    EXPECT_FALSE(navigationCompleted);
}

TEST(SOAuthorizationRedirect, InterceptionCompleteWithoutData)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorizationDidComplete:gAuthorization];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionUnexpectedCompletion)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorization:gAuthorization didCompleteWithHTTPAuthorizationHeaders:adoptNS([[NSDictionary alloc] init]).get()];
    Util::run(&navigationCompleted);

    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

// { Default delegate method, No App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed1)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationBasicDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_FALSE(policyForAppSSOPerformed); // The delegate isn't registered, so this won't be set.
#if PLATFORM(MAC) || PLATFORM(IOS)
    EXPECT_TRUE(gAuthorization.enableEmbeddedAuthorizationViewController);
#elif PLATFORM(VISION)
    EXPECT_FALSE(gAuthorization.enableEmbeddedAuthorizationViewController);
#endif
    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

static constexpr auto SimpleHtml =
"<html>"
"<body>"
"  Simple HTML file."
"<div id='test'></div>"
"<script>"
"function submitForm(formAsText)"
"{"
"    test.innerHTML = formAsText;"
"    document.forms[0].submit();"
"}"
"</script>"
"</body>"
"</html>"_s;

// { Default delegate method, With App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed2)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationBasicDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    // Force App Links with a request.URL that has a different host than the current one (empty host) and ShouldOpenExternalURLsPolicy::ShouldAllow.
    URL testURL { "https://www.example.com"_str };
#if PLATFORM(MAC)
    [webView _loadRequest:[NSURLRequest requestWithURL:testURL.createNSURL().get()] shouldOpenExternalURLs:YES];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    // Here we force RedirectSOAuthorizationSession to go to the with user gestures route.
    [webView evaluateJavaScript:adoptNS([[NSString alloc] initWithFormat:@"location = '%@'", testURL.string().createNSString().get()]).get() completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
#if PLATFORM(MAC)
    checkAuthorizationOptions(false, emptyString(), 0);
#elif PLATFORM(IOS) || PLATFORM(VISION)
    checkAuthorizationOptions(true, emptyString(), 0);
#endif
    EXPECT_FALSE(policyForAppSSOPerformed); // The delegate isn't registered, so this won't be set.

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

// { Custom delegate method, With App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed3)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // Force App Links with a request.URL that has a different host than the current one (empty host) and ShouldOpenExternalURLsPolicy::ShouldAllow.
    URL testURL { "https://www.example.com"_str };
#if PLATFORM(MAC)
    [webView _loadRequest:[NSURLRequest requestWithURL:testURL.createNSURL().get()] shouldOpenExternalURLs:YES];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    // Here we force RedirectSOAuthorizationSession to go to the with user gestures route.
    [webView evaluateJavaScript:adoptNS([[NSString alloc] initWithFormat:@"location = '%@'", testURL.string().createNSString().get()]).get() completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(gAuthorization.enableEmbeddedAuthorizationViewController);
#if PLATFORM(MAC)
    checkAuthorizationOptions(false, emptyString(), 0);
#elif PLATFORM(IOS) || PLATFORM(VISION)
    checkAuthorizationOptions(true, emptyString(), 0);
#endif
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

// { _WKNavigationActionPolicyAllowWithoutTryingAppLink, No App Links }
TEST(SOAuthorizationRedirect, InterceptionSucceed4)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    // A separate delegate that implements decidePolicyForNavigationAction.
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setIsDefaultPolicy:false];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithOtherHttpStatusCode)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWith302POST)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { SimpleHtml } },
        { "/simple.html"_s, { SimpleHtml } },
        { "/simple2.html"_s, { SimpleHtml } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [webView loadRequest:server.request()];
    Util::run(&navigationCompleted);

    haveHttpBody = true;

    RetainPtr<NSString> formSubmissionScript = [NSString stringWithFormat:@"submitForm('<form action=\"%@\" method=\"POST\"><input type=\"text\" name=\"name\" id=\"name\" value=\"test\"></input><input type=\"submit\" name=\"submitButton\" value=\"Submit\"></form>')", [[server.request("/simple.html"_s) URL] absoluteString]];
    authorizationPerformed = false;
    [webView evaluateJavaScript: formSubmissionScript.get() completionHandler:^(NSString *result, NSError *) { }];
    Util::run(&authorizationPerformed);

    navigationCompleted = false;
    delegateNavigationAction = nullptr;

    auto simple2URLString = [[server.request("/simple2.html"_s) URL] absoluteString];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:[server.request("/simple.html"_s) URL] statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : simple2URLString }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);

    haveHttpBody = false;

    EXPECT_EQ(WKNavigationTypeOther, delegateNavigationAction.get().navigationType);
    EXPECT_WK_STREQ("GET", delegateNavigationAction.get().request.HTTPMethod);
    EXPECT_WK_STREQ(simple2URLString, delegateNavigationAction.get().request.URL.absoluteString);
    EXPECT_WK_STREQ(adoptNS([[NSString alloc] initWithData:delegateNavigationAction.get().request.HTTPBody encoding:NSUTF8StringEncoding]).get(), "");
    EXPECT_WK_STREQ(adoptNS([[NSString alloc] initWithData:httpBody.get() encoding:NSUTF8StringEncoding]).get(), "name=test");
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWith302AfterRedirection)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { SimpleHtml } },
        { "/simple.html"_s, { SimpleHtml } },
        { "/simple2.html"_s, { SimpleHtml } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    HashMap<String, String> redirectHeaders;
    auto simpleURL = server.request("/simple.html"_s).URL;
    redirectHeaders.add("location"_s, simpleURL.absoluteString);

    TestWebKitAPI::HTTPResponse redirectResponse(302, WTFMove(redirectHeaders));

    server.addResponse("/redirection.html"_s, WTFMove(redirectResponse));

    navigationCompleted = false;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    // A separate delegate that implements decidePolicyForNavigationAction.
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);
    [delegate setIsDefaultPolicy:false];

    [webView loadRequest:server.request("/redirection.html"_s)];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, ""_s, 0);
    EXPECT_TRUE(policyForAppSSOPerformed);
    auto simpleURL2String = server.request("/simple2.html"_s).URL.absoluteString;
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:simpleURL statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : simpleURL2String }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    navigationCompleted = false;

#if PLATFORM(IOS_FAMILY)
    Util::run(&navigationCompleted);
    navigationCompleted = false;
#endif

    EXPECT_WK_STREQ(finalURL, simpleURL2String);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWith307Simple)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    TestWebKitAPI::HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/simple2.html"_s) {
                request.append(0);
                EXPECT_TRUE(contains(request.span(), "POST /simple2.html HTTP/1.1\r\n"_span));
                EXPECT_TRUE(contains(request.span(), "\r\nContent-Type: application/x-www-form-urlencoded\r\n"_span));
                EXPECT_TRUE(contains(request.span(), "\r\n\r\nname=test"_span));
            }
            co_await connection.awaitableSend(HTTPResponse(SimpleHtml).serialize());
        }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [webView loadRequest:server.request()];
    Util::run(&navigationCompleted);

    haveHttpBody = true;

    auto simpleURL = server.request("/simple.html"_s).URL;
    auto simpleURL2String = server.request("/simple2.html"_s).URL.absoluteString;
    RetainPtr<NSString> formSubmissionScript = [NSString stringWithFormat:@"submitForm('<form action=\"%@\" method=\"POST\"><input type=\"text\" name=\"name\" id=\"name\" value=\"test\"></input><input type=\"submit\" name=\"submitButton\" value=\"Submit\"></form>')", simpleURL.absoluteString];
    authorizationPerformed = false;
    [webView evaluateJavaScript: formSubmissionScript.get() completionHandler:^(NSString *result, NSError *) { }];
    Util::run(&authorizationPerformed);

    navigationCompleted = false;
    delegateNavigationAction = nullptr;

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:simpleURL statusCode:307 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : simpleURL2String }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);

    haveHttpBody = false;

    EXPECT_EQ(WKNavigationTypeFormSubmitted, delegateNavigationAction.get().navigationType);
    EXPECT_TRUE(delegateNavigationAction.get().sourceFrame == delegateNavigationAction.get().targetFrame);
    EXPECT_WK_STREQ("POST", delegateNavigationAction.get().request.HTTPMethod);
    EXPECT_WK_STREQ(simpleURL2String, delegateNavigationAction.get().request.URL.absoluteString);
    EXPECT_WK_STREQ(adoptNS([[NSString alloc] initWithData:delegateNavigationAction.get().request.HTTPBody encoding:NSUTF8StringEncoding]).get(), "name=test");
    EXPECT_WK_STREQ(adoptNS([[NSString alloc] initWithData:httpBody.get() encoding:NSUTF8StringEncoding]).get(), "name=test");
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWith307CrossOrigin)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { SimpleHtml } },
        { "/simple.html"_s, { SimpleHtml } },
        { "/simple2.html"_s, { SimpleHtml } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);
    TestWebKitAPI::HTTPServer server2(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/simple2.html"_s) {
                request.append(0);
                EXPECT_TRUE(contains(request.span(), "POST /simple2.html HTTP/1.1\r\n"_span));
                EXPECT_TRUE(contains(request.span(), "\r\nContent-Type: application/x-www-form-urlencoded\r\n"_span));
                EXPECT_TRUE(contains(request.span(), "\r\n\r\nname=test"_span));
            }
            co_await connection.awaitableSend(HTTPResponse(SimpleHtml).serialize());
        }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [webView loadRequest:server1.request()];
    Util::run(&navigationCompleted);

    haveHttpBody = true;

    auto server1SimpleURL = server1.request("/simple.html"_s).URL;
    RetainPtr<NSString> formSubmissionScript = [NSString stringWithFormat:@"submitForm('<form action=\"%@\" method=\"POST\"><input type=\"text\" name=\"name\" id=\"name\" value=\"test\"></input><input type=\"submit\" name=\"submitButton\" value=\"Submit\"></form>')", server1SimpleURL.absoluteString];
    authorizationPerformed = false;
    [webView evaluateJavaScript: formSubmissionScript.get() completionHandler:^(NSString *result, NSError *) { }];
    Util::run(&authorizationPerformed);

    navigationCompleted = false;
    delegateNavigationAction = nullptr;

    auto server2Simple2URLString = server2.request("/simple2.html"_s).URL.absoluteString;
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:server1SimpleURL statusCode:307 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : server2Simple2URLString }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);

    haveHttpBody = false;

    EXPECT_EQ(WKNavigationTypeFormSubmitted, delegateNavigationAction.get().navigationType);
    EXPECT_TRUE(delegateNavigationAction.get().sourceFrame == delegateNavigationAction.get().targetFrame);
    EXPECT_WK_STREQ("POST", delegateNavigationAction.get().request.HTTPMethod);
    EXPECT_WK_STREQ(server2Simple2URLString, delegateNavigationAction.get().request.URL.absoluteString);
    EXPECT_WK_STREQ(adoptNS([[NSString alloc] initWithData:delegateNavigationAction.get().request.HTTPBody encoding:NSUTF8StringEncoding]).get(), "name=test");
    EXPECT_WK_STREQ(adoptNS([[NSString alloc] initWithData:httpBody.get() encoding:NSUTF8StringEncoding]).get(), "name=test");
}

// FIXME: change this test once enabling the support for all 307 redirections.
TEST(SOAuthorizationRedirect, InterceptionFailedWith307PUT)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { SimpleHtml } },
        { "/simple.html"_s, { SimpleHtml } },
        { "/simple.html?name=test"_s, { "<html><body>FAIL</body></html>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);
    TestWebKitAPI::HTTPServer server2(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/simple2.html"_s) {
                request.append(0);
                EXPECT_TRUE(contains(request.span(), "POST /simple2.html HTTP/1.1\r\n"_span));
                EXPECT_TRUE(contains(request.span(), "\r\nContent-Type: application/x-www-form-urlencoded\r\n"_span));
                EXPECT_TRUE(contains(request.span(), "\r\n\r\nname=test"_span));
            }
            co_await connection.awaitableSend(HTTPResponse(SimpleHtml).serialize());
        }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    [webView loadRequest:server1.request()];
    Util::run(&navigationCompleted);

    haveHttpBody = false;

    auto server1SimpleURL = server1.request("/simple.html"_s).URL;
    RetainPtr<NSString> formSubmissionScript = [NSString stringWithFormat:@"submitForm('<form action=\"%@\" method=\"PUT\"><input type=\"text\" name=\"name\" id=\"name\" value=\"test\"></input><input type=\"submit\" name=\"submitButton\" value=\"Submit\"></form>')", server1SimpleURL.absoluteString];
    authorizationPerformed = false;
    [webView evaluateJavaScript: formSubmissionScript.get() completionHandler:^(NSString *result, NSError *) { }];
    Util::run(&authorizationPerformed);

    navigationCompleted = false;
    delegateNavigationAction = nullptr;

    auto server2Simple2URLString = server2.request("/simple2.html"_s).URL.absoluteString;
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:server1SimpleURL statusCode:307 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : server2Simple2URLString }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);

    navigationCompleted = false;
    [webView evaluateJavaScript: @"document.body.innerHTML" completionHandler:^(NSString *result, NSError *) {
        EXPECT_WK_STREQ("FAIL", [result UTF8String]);
        navigationCompleted = true;
    }];
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithCookie)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;", @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8, qwerty=219ffwef9w0f, id=a3fWa;", @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    URL testURL { "https://www.example.com"_str };
#if PLATFORM(MAC)
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.createNSURL().get()]];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    // Here we force RedirectSOAuthorizationSession to go to the with user gestures route.
    [webView evaluateJavaScript:adoptNS([[NSString alloc] initWithFormat:@"location = '%@'", testURL.string().createNSString().get()]).get() completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
#if PLATFORM(MAC)
    checkAuthorizationOptions(false, emptyString(), 0);
#elif PLATFORM(IOS) || PLATFORM(VISION)
    checkAuthorizationOptions(true, emptyString(), 0);
#endif
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;", @"Location" : [redirectURL absoluteString] }]);
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    URL redirectURL { "https://www.example.com"_str };
    RetainPtr response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:redirectURL.createNSURL().get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : redirectURL.string().createNSString().get() }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(testURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedWithWaitingSession)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionAbortedWithWaitingSession)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL1 = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr<NSURL> testURL2 = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Should be a no op.
    [webView addToTestWindow];

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedTwice)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        policyForAppSSOPerformed = false;
        [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
        Util::run(&authorizationPerformed);
        checkAuthorizationOptions(false, i ? "file://"_s : ""_s, 0);
        EXPECT_TRUE(policyForAppSSOPerformed);

        navigationCompleted = false;
        RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
        Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
        navigationCompleted = false;
        Util::run(&navigationCompleted);
#endif
        EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
    }
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSuppressActiveSession)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Suppress the last active session.
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationCancelled);
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSuppressWaitingSession)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSAML)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"SAML"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    // Add a http body to the request to mimic a SAML request.
    RetainPtr request = [NSMutableURLRequest requestWithURL:testURL.get()];
    [request setHTTPBody:adoptNS([[NSData alloc] init]).get()];
    haveHttpBody = true;

    [webView loadRequest:request.get()];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, emptyString(), 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Pass a HTTP 200 response with a html to mimic a SAML response.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:samlResponse length:strlen(samlResponse)]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationRedirect, InterceptionSucceedSAMLWithPSON)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple3" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:baseURL.get()]];
    Util::run(&navigationCompleted);

    // PSON: file:/// => example.com
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.createNSURL().get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    navigationCompleted = false;
    // Pass a HTTP 200 response with a html to mimic a SAML response.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:@"" baseURL:URL { "http://www.webkit.org"_str }.createNSURL().get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    [webView evaluateJavaScript: @"location = 'http://www.example.com'" completionHandler:nil];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "http://www.webkit.org"_s, 0);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationRedirect, AuthorizationOptionsWithPath)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:@"" baseURL:URL { "http://www.webkit.org/some/path"_str }.createNSURL().get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    [webView evaluateJavaScript: @"location = 'http://www.example.com/test/path'" completionHandler:nil];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "http://www.webkit.org"_s, 0, "/some/path"_s);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationRedirect, AuthorizationOptionsAboutBlank)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"about:blank"]];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    [webView evaluateJavaScript: @"location = 'http://www.example.com'" completionHandler:nil];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, ""_s, 0);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationRedirect, InterceptionDidNotHandleTwice)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Test passes if no crashes.
    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get(), OpenExternalSchemesPolicy::Allow);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(false, ""_s, 0);
    EXPECT_TRUE(policyForAppSSOPerformed);

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
#if PLATFORM(IOS) || PLATFORM(VISION)
    navigationCompleted = false;
    Util::run(&navigationCompleted);
#endif
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
    EXPECT_FALSE(uiShowed);
}

TEST(SOAuthorizationRedirect, InterceptionCancelWithUI)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    Util::runFor(0.5_s);
    EXPECT_WK_STREQ("", webView.get()._committedURL.absoluteString);
    EXPECT_FALSE(uiShowed);
}

TEST(SOAuthorizationRedirect, InterceptionErrorWithUI)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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

    RetainPtr<NSURL> redirectURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:302 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Location" : [redirectURL absoluteString] }]);
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] init]).get()];
    Util::run(&navigationCompleted);
    EXPECT_WK_STREQ(redirectURL.get().absoluteString, finalURL);
}

TEST(SOAuthorizationRedirect, ShowUITwice)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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

    auto *hostWindow = [webView hostWindow];
    ScopedNotificationCenterObserveOnce didEndSheet { NSWindowDidEndSheetNotification, hostWindow };

    ScopedNotificationCenterObserveOnce didMiniaturize { NSWindowDidMiniaturizeNotification, hostWindow };
    [hostWindow miniaturize:hostWindow];
    EXPECT_TRUE(didMiniaturize.waitFor(actionDoneTimeout));

    // Dimiss the UI while it is miniaturized.
    [gDelegate authorizationDidCancel:gAuthorization];

    // This is the condition that the test tests: it is expected that the sheet is not ended because the view is miniaturized.
    EXPECT_FALSE(didEndSheet.waitFor(actionAbsenceTimeout));

    ScopedNotificationCenterObserveOnce didDeminiaturize { NSWindowDidDeminiaturizeNotification, hostWindow };
    [hostWindow deminiaturize:hostWindow];
    EXPECT_TRUE(didDeminiaturize.waitFor(actionDoneTimeout));

    EXPECT_TRUE(didEndSheet.waitFor(actionDoneTimeout));
}

TEST(SOAuthorizationRedirect, DismissUIDuringHiding)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    auto *hostWindow = [webView hostWindow];

    ScopedNotificationCenterObserveOnce didEndSheet { NSWindowDidEndSheetNotification, hostWindow };

    [NSApp hide:NSApp];

    // Wait for hiding to finish.
    while (!NSApp.hidden)
        Util::spinRunLoop();

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];

    // This is the condition that the test tests: it is expected that the sheet is not ended because the view is hidden.
    EXPECT_FALSE(didEndSheet.waitFor(actionAbsenceTimeout));

    // UI is only dimissed after the hostApp is unhidden.
    [NSApp unhide:NSApp];
    while (NSApp.hidden)
        Util::spinRunLoop();

    EXPECT_TRUE(didEndSheet.waitFor(actionDoneTimeout));
}

TEST(SOAuthorizationRedirect, DismissUIDuringMiniaturizationThenAnother)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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
    auto *hostWindow = [webView hostWindow];

    ScopedNotificationCenterObserveOnce didEndSheet { NSWindowDidEndSheetNotification, hostWindow };
    ScopedNotificationCenterObserveOnce didMiniaturize { NSWindowDidMiniaturizeNotification, hostWindow };
    [hostWindow miniaturize:hostWindow];
    didMiniaturize.waitFor(actionDoneTimeout);

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];

    // This is the condition that the test tests: it is expected that the sheet is not ended because the view is miniaturized.
    EXPECT_FALSE(didEndSheet.waitFor(actionAbsenceTimeout));

    // Load another AppSSO request.
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // This is the condition that the test tests: it is expected that the sheet is not ended because the view is miniaturized.
    EXPECT_FALSE(didEndSheet.waitFor(actionAbsenceTimeout));

    // UI is only dimissed after the window is deminiaturized.
    ScopedNotificationCenterObserveOnce didDeminiaturize { NSWindowDidDeminiaturizeNotification, hostWindow };
    [hostWindow deminiaturize:hostWindow];
    EXPECT_TRUE(didDeminiaturize.waitFor(actionDoneTimeout));

    EXPECT_TRUE(didEndSheet.waitFor(actionDoneTimeout));
}

TEST(SOAuthorizationRedirect, DismissUIDuringHidingThenAnother)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

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

    ScopedNotificationCenterObserveOnce didEndSheet { NSWindowDidEndSheetNotification, [webView hostWindow] };

    [NSApp hide:NSApp];
    while (!NSApp.hidden)
        Util::spinRunLoop();

    // Dimiss the UI
    [gDelegate authorizationDidCancel:gAuthorization];

    // This is the condition that the test tests: it is expected that the sheet is not ended because the view is hidden.
    EXPECT_FALSE(didEndSheet.waitFor(actionAbsenceTimeout));

    // Load another AppSSO request.
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [NSApp unhide:NSApp];
    while (NSApp.hidden)
        Util::spinRunLoop();

    EXPECT_TRUE(didEndSheet.waitFor(actionDoneTimeout));
}
#endif

TEST(SOAuthorizationPopUp, NoInterceptions)
{
    resetState();

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:testURL.get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
    navigationCompleted = false;
#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    auto iframeTestHtml = generateOpenerHTML(openerTemplate, testURL.get().absoluteString);
    auto testHtml = makeString("<iframe style='width:400px;height:400px' srcdoc=\""_s, iframeTestHtml, "\" />"_s);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    // The new window will not navigate to the testURL as the iframe has an opaque origin.
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
    Util::run(&newWindowCreated);
    EXPECT_FALSE(policyForAppSSOPerformed);
    EXPECT_FALSE(authorizationPerformed);
}
#endif

TEST(SOAuthorizationPopUp, NoInterceptionsWithoutUserGesture)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorizationDidCancel:gAuthorization];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, InterceptionSucceedCloseByItself)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, "window.close();"_s).utf8(); // The pop up closes itself.
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, InterceptionSucceedCloseByParent)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string(), emptyString(), "event.source.close();"_s); // The parent closes the pop up.

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, InterceptionSucceedCloseByWebKit)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, InterceptionSucceedWithOtherHttpStatusCode)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.get().absoluteString);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    [delegate setShouldOpenExternalSchemes:true];
#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Will fallback to web path.
    navigationCompleted = false;
    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
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
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;" }]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, InterceptionSucceedTwice)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        policyForAppSSOPerformed = false;
#if PLATFORM(MAC)
        [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
        [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
        Util::run(&authorizationPerformed);
        checkAuthorizationOptions(true, "file://"_s, 1);
        EXPECT_TRUE(policyForAppSSOPerformed);

        [messageHandler resetExpectations:@[@"Hello.", @"WindowClosed."]];

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
        auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
        // The secret WKWebView needs to be destroyed right the way.
        @autoreleasepool {
            [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
        }
        Util::run(&allMessagesReceived);
    }
}

TEST(SOAuthorizationPopUp, InterceptionSucceedSuppressActiveSession)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    // Suppress the last active session.

    auto configuration = adoptNS([webView.get().configuration copy]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto newWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    configureSOAuthorizationWebView(newWebView.get(), delegate.get());

    navigationCompleted = false;
    [newWebView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

    authorizationPerformed = false;
    policyForAppSSOPerformed = false;
#if PLATFORM(MAC)
    [newWebView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [newWebView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationCancelled);
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    navigationCompleted = false;
    [webView evaluateJavaScript: @"newWindow" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE(result == [NSNull null]);
        navigationCompleted = true;
    }];
    Util::run(&navigationCompleted);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, InterceptionSucceedNewWindowNavigation)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string(), makeString("newWindow.location = '"_s, unsafeSpan(baseURL.get().absoluteString.UTF8String), "';"_s)); // Starts a new navigation on the new window.

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, emptyString()).utf8();
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationPopUp, AuthorizationOptions)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL { "http://www.webkit.org"_str }.createNSURL().get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "http://www.webkit.org"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL { "http://www.webkit.org"_str }.createNSURL().get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&newWindowCreated);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyAllowAsync)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"Hello.", @"WindowClosed."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&authorizationPerformed);
    checkAuthorizationOptions(true, "file://"_s, 1);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto resonseHtmlCString = generateHTML(newWindowResponseTemplate, "window.close();"_s).utf8(); // The pop up closes itself.
    // The secret WKWebView needs to be destroyed right the way.
    @autoreleasepool {
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:resonseHtmlCString.data() length:resonseHtmlCString.length()]).get()];
    }
    Util::run(&allMessagesReceived);
}


TEST(SOAuthorizationPopUp, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateOpenerHTML(openerTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL { "http://www.webkit.org"_str }.createNSURL().get()];
    Util::run(&navigationCompleted);

#if PLATFORM(MAC)
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [webView evaluateJavaScript: @"clickMe()" completionHandler:nil];
#endif
    Util::run(&newWindowCreated);
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationSubFrame, NoInterceptions)
{
    resetState();
    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"GetSessionCookie" withExtension:@"html"];
    auto testHtml = generateHTML(parentTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@""]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&allMessagesReceived);
    EXPECT_FALSE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationSubFrame, NoInterceptionsNonAppleFirstPartyMainFrame)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    auto testHtml = generateHTML(parentTemplate, URL { "http://www.example.com"_str }.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL { "http://www.webkit.org"_str }.createNSURL().get()];
    // Try to wait until the iframe load is finished.
    Util::runFor(0.5_s);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
    EXPECT_FALSE(policyForAppSSOPerformed);
}

TEST(SOAuthorizationSubFrame, InterceptionError)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"GetSessionCookie" withExtension:@"html"];
    auto testHtml = generateHTML(parentTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[[NSNull null], @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "file://"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"null", @"SOAuthorizationDidCancel", @""]];

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&allMessagesReceived);
    // Make sure we don't load the request of the iframe to the main frame.
    EXPECT_WK_STREQ("", finalURL);
}

TEST(SOAuthorizationSubFrame, InterceptionCancel)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"GetSessionCookie" withExtension:@"html"];
    auto testHtml = generateHTML(parentTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[[NSNull null], @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "file://"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"null", @"SOAuthorizationDidCancel", @""]];

    [gDelegate authorizationDidCancel:gAuthorization];
    Util::run(&allMessagesReceived);
    // Make sure we don't load the request of the iframe to the main frame.
    EXPECT_WK_STREQ("", finalURL);
}

TEST(SOAuthorizationSubFrame, InterceptionSuccess)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"http://www.example.com", @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:nil];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "null"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"http://www.example.com", @"Hello."]];

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, emptyString()).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionSucceedWithOtherHttpStatusCode)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"GetSessionCookie" withExtension:@"html"];
    auto testHtml = generateHTML(parentTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[[NSNull null], @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "file://"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"null", @"SOAuthorizationDidCancel", @""]];

    // Will fallback to web path.
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.get() statusCode:400 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, emptyString()).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
    // Make sure we don't load the request of the iframe to the main frame.
    EXPECT_WK_STREQ("", finalURL);
}

// Setting cookie is ensured by other tests. Here is to cover if the whole authentication handshake can be completed.
TEST(SOAuthorizationSubFrame, InterceptionSucceedWithCookie)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"http://www.example.com", @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:adoptNS([[NSURL alloc] initWithString:@"http://example.com"]).get()];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "http://example.com"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);
    [messageHandler extendExpectations:@[@"http://www.example.com", @"Hello.", @"http://www.example.com", @"Cookies: sessionid=38afes7a8"]];

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;" }]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, "parent.postMessage('Cookies: ' + document.cookie, '*');"_s).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionSucceedWithCookieButCSPDeny)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"http://www.example.com", @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:nil];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "null"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"http://www.example.com", @"SOAuthorizationDidCancel"]];

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;", @"Content-Security-Policy" : @"frame-ancestors 'none';" }]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, "parent.postMessage('Cookies: ' + document.cookie, '*');"_s).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionSucceedWithCookieButXFrameDeny)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"http://www.example.com", @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:nil];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "null"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"http://www.example.com", @"SOAuthorizationDidCancel"]];

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{ @"Set-Cookie" : @"sessionid=38afes7a8;", @"X-Frame-Options" : @"DENY" }]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, "parent.postMessage('Cookies: ' + document.cookie, '*');"_s).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionSuccessTwice)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    for (int i = 0; i < 2; i++) {
        authorizationPerformed = false;
        navigationCompleted = false;
        policyForAppSSOPerformed = false;

        [messageHandler resetExpectations:@[@"http://www.example.com", @"SOAuthorizationDidStart"]];

        [webView loadHTMLString:testHtml.createNSString().get() baseURL:nil];
        Util::run(&allMessagesReceived);

        checkAuthorizationOptions(false, "null"_s, 2);
        EXPECT_TRUE(policyForAppSSOPerformed);

        [messageHandler extendExpectations:@[@"http://www.example.com", @"Hello."]];

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
        auto iframeHtmlCString = generateHTML(iframeTemplate, emptyString()).utf8();
        [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
        Util::run(&allMessagesReceived);
    }
}

TEST(SOAuthorizationSubFrame, AuthorizationOptions)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"http://www.example.com", @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL { "http://www.apple.com"_str }.createNSURL().get()];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "http://www.apple.com"_s, 2);
}

TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyIgnore)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL { "http://www.apple.com"_str }.createNSURL().get()];
    // Try to wait until the iframe load is finished.
    Util::runFor(0.5_s);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
}

// FIX ME WHEN rdar://107855114 is resolved. 
#if PLATFORM(IOS) || PLATFORM(VISION)
TEST(SOAuthorizationSubFrame, DISABLED_SOAuthorizationLoadPolicyAllowAsync)
#else
TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyAllowAsync)
#endif
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[@"http://www.example.com", @"SOAuthorizationDidStart"]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:nil];
    Util::run(&allMessagesReceived);

    checkAuthorizationOptions(false, "null"_s, 2);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [messageHandler extendExpectations:@[@"http://www.example.com", @"Hello."]];

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, emptyString()).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, SOAuthorizationLoadPolicyIgnoreAsync)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());
    [delegate setAllowSOAuthorizationLoad:false];
    [delegate setIsAsyncExecution:true];

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:URL("http://www.apple.com"_s).createNSURL().get()];
    // Try to wait until the iframe load is finished.
    Util::runFor(0.5_s);
    // Make sure we don't intercept the iframe.
    EXPECT_FALSE(authorizationPerformed);
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=239311
TEST(SOAuthorizationSubFrame, InterceptionErrorWithReferrer)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    HTTPServer server([parentHtml = generateHTML(parentTemplate, "simple.html"_s), frameHtml = generateHTML(iframeTemplate, "parent.postMessage('Referrer: ' + document.referrer, '*');"_s)] (const Connection& connection) {
        RetainPtr<NSString> firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentHtml.length(),
            parentHtml.createNSString().get()
        ];
        RetainPtr<NSString> secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            frameHtml.length(),
            frameHtml.createNSString().get()
        ];

        connection.receiveHTTPRequest([=](Vector<char>&&) {
            connection.send(firstResponse.get(), [=] {
                connection.receiveHTTPRequest([=](Vector<char>&&) {
                    connection.send(secondResponse.get());
                });
            });
        });
    });
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto origin = makeString("http://127.0.0.1:"_s, server.port());
    RetainPtr nsOrigin = origin.createNSString();

    RetainPtr messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[nsOrigin.get(), @"SOAuthorizationDidStart", nsOrigin.get(), @"SOAuthorizationDidCancel", nsOrigin.get(), @"Hello.", nsOrigin.get(), makeString("Referrer: "_s, origin, '/').createNSString().get()]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView _loadRequest:adoptNS([[NSURLRequest alloc] initWithURL:adoptNS([[NSURL alloc] initWithString:nsOrigin.get()]).get()]).get() shouldOpenExternalURLs:NO];

    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionErrorMessageOrder)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    RetainPtr<NSURL> baseURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"GetSessionCookie" withExtension:@"html"];
    auto testHtml = generateHTML(parentTemplate, testURL.get().absoluteString);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[[NSNull null], @"SOAuthorizationDidStart", [NSNull null], @"SOAuthorizationDidCancel", @""]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:baseURL.get()];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);
    [gDelegate authorization:gAuthorization didCompleteWithError:adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]).get()];
    Util::run(&allMessagesReceived);
}

TEST(SOAuthorizationSubFrame, InterceptionSuccessMessageOrder)
{
    resetState();
    SWIZZLE_SOAUTH(PAL::getSOAuthorizationClassSingleton());
    SWIZZLE_AKAUTH();

    URL testURL { "http://www.example.com"_str };
    auto testHtml = generateHTML(parentTemplate, testURL.string());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[TestSOAuthorizationScriptMessageHandler alloc] initWithExpectation:@[[NSNull null], @"SOAuthorizationDidStart", [NSNull null], @"Hello."]]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSOAuthorizationDelegate alloc] init]);
    configureSOAuthorizationWebView(webView.get(), delegate.get());

    [webView loadHTMLString:testHtml.createNSString().get() baseURL:nil];
    Util::run(&authorizationPerformed);
    EXPECT_TRUE(policyForAppSSOPerformed);

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:testURL.createNSURL().get() statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:nil]);
    auto iframeHtmlCString = generateHTML(iframeTemplate, emptyString()).utf8();
    [gDelegate authorization:gAuthorization didCompleteWithHTTPResponse:response.get() httpBody:adoptNS([[NSData alloc] initWithBytes:iframeHtmlCString.data() length:iframeHtmlCString.length()]).get()];
    Util::run(&allMessagesReceived);
}

} // namespace TestWebKitAPI

#undef SWIZZLE_SOAUTH
#undef SWIZZLE_AKAUTH

#endif
