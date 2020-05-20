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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UserInterfaceSwizzler.h"
#import <WebKit/WKWebpagePreferences.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/WTFString.h>

@interface ContentModeNavigationDelegate : TestNavigationDelegate
@property (nonatomic, copy) void (^decidePolicyForNavigationActionWithPreferences)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *));
@end

@implementation ContentModeNavigationDelegate {
    BlockPtr<void(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))> _decidePolicyForNavigationActionWithPreferences;
}

- (void)setDecidePolicyForNavigationActionWithPreferences:(void (^)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *)))decidePolicyForNavigationActionWithPreferences
{
    _decidePolicyForNavigationActionWithPreferences = decidePolicyForNavigationActionWithPreferences;
}

- (void (^)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *)))decidePolicyForNavigationActionWithPreferences
{
    return _decidePolicyForNavigationActionWithPreferences.get();
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    if (_decidePolicyForNavigationActionWithPreferences)
        _decidePolicyForNavigationActionWithPreferences(navigationAction, preferences, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
}

@end

@implementation WKWebpagePreferences (PreferredContentMode)

+ (instancetype)preferencesWithContentMode:(WKContentMode)mode
{
    auto preferences = adoptNS([[self alloc] init]);
    [preferences setPreferredContentMode:mode];
    return preferences.autorelease();
}

@end

@implementation WKWebView (PreferredContentMode)

- (NSString *)navigatorUserAgent
{
    return [self stringByEvaluatingJavaScript:@"navigator.userAgent"];
}

- (NSString *)navigatorPlatform
{
    return [self stringByEvaluatingJavaScript:@"navigator.platform"];
}

- (void)loadTestPageNamed:(NSString *)pageName withPolicyDecisionHandler:(void (^)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *)))decisionHandler
{
    ContentModeNavigationDelegate *navigationDelegate = (ContentModeNavigationDelegate *)self.navigationDelegate;
    navigationDelegate.decidePolicyForNavigationActionWithPreferences = decisionHandler;
    [self loadTestPageNamed:pageName];
    [navigationDelegate waitForDidFinishNavigation];
}

- (void)loadHTMLString:(NSString *)htmlString withPolicyDecisionHandler:(void (^)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *)))decisionHandler
{
    ContentModeNavigationDelegate *navigationDelegate = (ContentModeNavigationDelegate *)self.navigationDelegate;
    navigationDelegate.decidePolicyForNavigationActionWithPreferences = decisionHandler;
    [self loadHTMLString:htmlString baseURL:nil];
    [navigationDelegate waitForDidFinishNavigation];
}

- (void)loadTestPageNamed:(NSString *)pageName andExpectEffectiveContentMode:(WKContentMode)expectedContentMode withPolicyDecisionHandler:(void (^)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *)))decisionHandler
{
    ContentModeNavigationDelegate *navigationDelegate = (ContentModeNavigationDelegate *)self.navigationDelegate;
    navigationDelegate.decidePolicyForNavigationActionWithPreferences = decisionHandler;

    auto didFailProvisionalNavigation = makeBlockPtr(navigationDelegate.didFailProvisionalNavigation);
    navigationDelegate.didFailProvisionalNavigation = [expectedContentMode, didFailProvisionalNavigation] (WKWebView *webView, WKNavigation *navigation, NSError *error) {
        EXPECT_EQ(expectedContentMode, navigation.effectiveContentMode);
        if (didFailProvisionalNavigation)
            didFailProvisionalNavigation(webView, navigation, error);
    };

    auto didStartProvisionalNavigation = makeBlockPtr(navigationDelegate.didStartProvisionalNavigation);
    navigationDelegate.didStartProvisionalNavigation = [expectedContentMode, didStartProvisionalNavigation] (WKWebView *webView, WKNavigation *navigation) {
        EXPECT_EQ(expectedContentMode, navigation.effectiveContentMode);
        if (didStartProvisionalNavigation)
            didStartProvisionalNavigation(webView, navigation);
    };

    auto didCommitNavigation = makeBlockPtr(navigationDelegate.didCommitNavigation);
    navigationDelegate.didCommitNavigation = [expectedContentMode, didCommitNavigation] (WKWebView *webView, WKNavigation *navigation) {
        EXPECT_EQ(expectedContentMode, navigation.effectiveContentMode);
        if (didCommitNavigation)
            didCommitNavigation(webView, navigation);
    };

    bool finishedNavigation = false;
    auto didFinishNavigation = makeBlockPtr(navigationDelegate.didFinishNavigation);
    navigationDelegate.didFinishNavigation = [expectedContentMode, didFinishNavigation, &finishedNavigation] (WKWebView *webView, WKNavigation *navigation) {
        EXPECT_EQ(expectedContentMode, navigation.effectiveContentMode);
        if (didFinishNavigation)
            didFinishNavigation(webView, navigation);
        finishedNavigation = true;
    };

    [self loadTestPageNamed:pageName];
    TestWebKitAPI::Util::run(&finishedNavigation);

    navigationDelegate.didFailProvisionalNavigation = didFailProvisionalNavigation.get();
    navigationDelegate.didStartProvisionalNavigation = didStartProvisionalNavigation.get();
    navigationDelegate.didCommitNavigation = didCommitNavigation.get();
    navigationDelegate.didFinishNavigation = didFinishNavigation.get();
}

@end

@implementation NSString (PreferredContentMode)

- (void)shouldContainStrings:(NSString *)firstString, ...
{
    va_list args;
    va_start(args, firstString);
    for (NSString *string = firstString; string; string = va_arg(args, NSString *)) {
        BOOL containsString = [self containsString:string];
        EXPECT_TRUE(containsString);
        if (!containsString)
            NSLog(@"Expected '%@' to contain '%@'", self, string);
    }
    va_end(args);
}

@end

namespace TestWebKitAPI {

template <typename ViewClass>
RetainPtr<ViewClass> setUpWebViewForPreferredContentModeTestingWithoutNavigationDelegate(Optional<WKContentMode> defaultContentMode = { }, Optional<String> applicationNameForUserAgent = { "TestWebKitAPI" }, CGSize size = CGSizeMake(1024, 768))
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    if (defaultContentMode)
        [configuration setDefaultWebpagePreferences:[WKWebpagePreferences preferencesWithContentMode:defaultContentMode.value()]];
    if (applicationNameForUserAgent)
        [configuration setApplicationNameForUserAgent:applicationNameForUserAgent->isNull() ? nil : (NSString *)applicationNameForUserAgent.value()];
    auto webView = adoptNS([[ViewClass alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height) configuration:configuration.get()]);
    EXPECT_TRUE([webView isKindOfClass:WKWebView.class]);
    return webView;
}

template <typename ViewClass>
std::pair<RetainPtr<ViewClass>, RetainPtr<ContentModeNavigationDelegate>> setUpWebViewForPreferredContentModeTesting(Optional<WKContentMode> defaultContentMode = { }, Optional<String> applicationNameForUserAgent = { "TestWebKitAPI" }, CGSize size = CGSizeMake(1024, 768))
{
    auto webView = setUpWebViewForPreferredContentModeTestingWithoutNavigationDelegate<ViewClass>(defaultContentMode, applicationNameForUserAgent, size);
    auto navigationDelegate = adoptNS([[ContentModeNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    return { webView, navigationDelegate };
}

static auto makeContentModeDecisionHandler(WKContentMode mode)
{
    return [mode] (WKNavigationAction *action, WKWebpagePreferences *, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        decisionHandler(WKNavigationActionPolicyAllow, [WKWebpagePreferences preferencesWithContentMode:mode]);
    };
};

TEST(PreferredContentMode, SetDefaultWebpagePreferences)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    {
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();
        [webView loadHTMLString:@"<pre>Foo</pre>" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
            EXPECT_EQ(WKContentModeRecommended, defaultPreferences.preferredContentMode);
            decisionHandler(WKNavigationActionPolicyAllow, defaultPreferences);
        }];
        [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
        EXPECT_WK_STREQ("MacIntel", [webView navigatorPlatform]);
    }

    {
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>(WKContentModeDesktop);
        [webView loadHTMLString:@"<pre>Bar</pre>" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
            EXPECT_EQ(WKContentModeDesktop, defaultPreferences.preferredContentMode);
            decisionHandler(WKNavigationActionPolicyAllow, defaultPreferences);
        }];
        [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
        EXPECT_WK_STREQ("MacIntel", [webView navigatorPlatform]);

        [webView loadHTMLString:@"<pre>Baz</pre>" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
            EXPECT_EQ(WKContentModeDesktop, defaultPreferences.preferredContentMode);
            decisionHandler(WKNavigationActionPolicyAllow, [WKWebpagePreferences preferencesWithContentMode:WKContentModeMobile]);
        }];
        [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (iP", @"TestWebKitAPI", nil];
        EXPECT_TRUE([[webView navigatorPlatform] containsString:@"iP"]);
    }
}

TEST(PreferredContentMode, DesktopModeWithoutNavigationDelegate)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto webView = setUpWebViewForPreferredContentModeTestingWithoutNavigationDelegate<TestWKWebView>(WKContentModeDesktop);
    [webView loadHTMLString:@"<body>Hello world</body>" baseURL:nil];

    __block bool finished = false;
    [webView performAfterLoading:^{
        finished = true;
    }];
    Util::run(&finished);

    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
    EXPECT_WK_STREQ("MacIntel", [webView navigatorPlatform]);
}

TEST(PreferredContentMode, DesktopModeOnPhone)
{
    IPhoneUserInterfaceSwizzler iPhoneUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();
    [webView loadHTMLString:@"<pre>Foo</pre>" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(WKContentModeRecommended, defaultPreferences.preferredContentMode);
        decisionHandler(WKNavigationActionPolicyAllow, [WKWebpagePreferences preferencesWithContentMode:WKContentModeDesktop]);
    }];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
    EXPECT_WK_STREQ("MacIntel", [webView navigatorPlatform]);
    EXPECT_EQ(980, [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue]);

    webView = setUpWebViewForPreferredContentModeTestingWithoutNavigationDelegate<WKWebView>(WKContentModeMobile);
    [webView synchronouslyLoadHTMLString:@"<body>Hello world</body>"];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (iP", @"TestWebKitAPI", nil];
    EXPECT_TRUE([[webView navigatorPlatform] containsString:@"iP"]);
    EXPECT_EQ(980, [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue]);
}

TEST(PreferredContentMode, DesktopModeTopLevelFrameSupercedesSubframe)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();

    __block BOOL decidedPolicyForMainFrame = NO;
    __block BOOL decidedPolicyForSubFrame = NO;
    [webView loadHTMLString:@"<p>Foo bar</p><iframe src='data:text/html,<pre>Hello</pre>'></iframe>" withPolicyDecisionHandler:^(WKNavigationAction *action, WKWebpagePreferences *, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        WKContentMode contentMode;
        if (action.targetFrame.mainFrame) {
            decidedPolicyForMainFrame = YES;
            contentMode = WKContentModeDesktop;
        } else {
            decidedPolicyForSubFrame = YES;
            contentMode = WKContentModeMobile;
        }
        decisionHandler(WKNavigationActionPolicyAllow, [WKWebpagePreferences preferencesWithContentMode:contentMode]);
    }];

    EXPECT_TRUE(decidedPolicyForMainFrame);
    EXPECT_TRUE(decidedPolicyForSubFrame);
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
    EXPECT_WK_STREQ("MacIntel", [webView navigatorPlatform]);
}

TEST(PreferredContentMode, DesktopModeUsesNativeViewportByDefault)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();

    [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:makeContentModeDecisionHandler(WKContentModeDesktop)];
    EXPECT_EQ(1024, [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue]);

    [webView loadTestPageNamed:@"simple2" withPolicyDecisionHandler:makeContentModeDecisionHandler(WKContentModeMobile)];
    EXPECT_EQ(980, [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue]);

    ((ContentModeNavigationDelegate *)[webView navigationDelegate]).decidePolicyForNavigationActionWithPreferences = makeContentModeDecisionHandler(WKContentModeDesktop);
    [webView goBack];
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ(1024, [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue]);
}

TEST(PreferredContentMode, CustomUserAgentOverridesDesktopContentModeUserAgent)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<TestWKWebView>(WKContentModeDesktop);

    NSString *customUserAgent = @"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36";
    [webView setCustomUserAgent:customUserAgent];
    [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:nil];
    EXPECT_WK_STREQ(customUserAgent, [webView navigatorUserAgent]);

    [webView setCustomUserAgent:@""];
    [webView loadTestPageNamed:@"simple2" withPolicyDecisionHandler:nil];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
}

TEST(PreferredContentMode, DoNotAllowChangingDefaultWebpagePreferencesInDelegateMethod)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<TestWKWebView>(WKContentModeDesktop);

    [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(WKContentModeDesktop, defaultPreferences.preferredContentMode);
        defaultPreferences.preferredContentMode = WKContentModeMobile;
        decisionHandler(WKNavigationActionPolicyAllow, defaultPreferences);
    }];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (iP", @"TestWebKitAPI", nil];

    [webView loadTestPageNamed:@"simple2" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_EQ(WKContentModeDesktop, defaultPreferences.preferredContentMode);
        decisionHandler(WKNavigationActionPolicyAllow, defaultPreferences);
    }];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Mac", @"TestWebKitAPI", nil];
}

TEST(PreferredContentMode, EffectiveContentModeOnIPad)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();

    [webView loadTestPageNamed:@"simple" andExpectEffectiveContentMode:WKContentModeDesktop withPolicyDecisionHandler:nil];
    [webView loadTestPageNamed:@"simple2" andExpectEffectiveContentMode:WKContentModeDesktop withPolicyDecisionHandler:makeContentModeDecisionHandler(WKContentModeDesktop)];
    [webView loadTestPageNamed:@"simple3" andExpectEffectiveContentMode:WKContentModeMobile withPolicyDecisionHandler:makeContentModeDecisionHandler(WKContentModeMobile)];
}

TEST(PreferredContentMode, EffectiveContentModeOnPhone)
{
    IPhoneUserInterfaceSwizzler iPhoneUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();

    [webView loadTestPageNamed:@"simple" andExpectEffectiveContentMode:WKContentModeMobile withPolicyDecisionHandler:nil];
    [webView loadTestPageNamed:@"simple2" andExpectEffectiveContentMode:WKContentModeDesktop withPolicyDecisionHandler:makeContentModeDecisionHandler(WKContentModeDesktop)];
    [webView loadTestPageNamed:@"simple3" andExpectEffectiveContentMode:WKContentModeMobile withPolicyDecisionHandler:makeContentModeDecisionHandler(WKContentModeMobile)];
}

TEST(PreferredContentMode, RecommendedContentModeAtVariousViewWidths)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>({ }, { "TestWebKitAPI" }, CGSizeZero);
    [webView loadTestPageNamed:@"simple" andExpectEffectiveContentMode:WKContentModeDesktop withPolicyDecisionHandler:nil];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Macintosh", @"TestWebKitAPI", nil];

    [webView setFrame:CGRectMake(0, 0, 320, 768)];
    [webView loadTestPageNamed:@"simple2" andExpectEffectiveContentMode:WKContentModeMobile withPolicyDecisionHandler:nil];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (iP", @"TestWebKitAPI", nil];

    [webView setFrame:CGRectMake(0, 0, 1024, 768)];
    [webView loadTestPageNamed:@"simple3" andExpectEffectiveContentMode:WKContentModeDesktop withPolicyDecisionHandler:nil];
    [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Macintosh", @"TestWebKitAPI", nil];
}

TEST(PreferredContentMode, ApplicationNameForDesktopUserAgent)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    {
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>();
        [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:^(WKNavigationAction *, WKWebpagePreferences *defaultPreferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
            defaultPreferences._applicationNameForUserAgentWithModernCompatibility = @"DesktopBrowser";
            decisionHandler(WKNavigationActionPolicyAllow, defaultPreferences);
        }];
        [[webView navigatorUserAgent] shouldContainStrings:@"Mozilla/5.0 (Macintosh", @"DesktopBrowser", nil];
        EXPECT_WK_STREQ("MacIntel", [webView navigatorPlatform]);
    }

    {
        // Don't attempt to change the application name, but still opt into desktop-class browsing;
        // the application name should not default to "Mobile/15E148".
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>(WKContentModeDesktop, { });
        [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:nil];
        NSString *userAgent = [webView navigatorUserAgent];
        EXPECT_FALSE([userAgent containsString:@"Mobile"]);
        EXPECT_TRUE([userAgent containsString:@"Macintosh"]);
    }
    {
        // Don't attempt to change the application name, but this time, opt into mobile content. The application
        // name should default to "Mobile/15E148".
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>(WKContentModeMobile, { });
        [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:nil];
        NSString *userAgent = [webView navigatorUserAgent];
        EXPECT_TRUE([userAgent containsString:@"Mobile"]);
        EXPECT_FALSE([userAgent containsString:@"Macintosh"]);
    }
    {
        // Manually set the application name for user agent to the default value, Mobile/15E148.
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>(WKContentModeDesktop, { "Mobile/15E148" });
        [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:nil];
        NSString *userAgent = [webView navigatorUserAgent];
        EXPECT_TRUE([userAgent containsString:@"Mobile"]);
        EXPECT_TRUE([userAgent containsString:@"Macintosh"]);
    }
    {
        // Manually set the application name for user agent to nil.
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>(WKContentModeDesktop, {{ }});
        [webView loadTestPageNamed:@"simple" withPolicyDecisionHandler:nil];
        NSString *userAgent = [webView navigatorUserAgent];
        EXPECT_FALSE([userAgent containsString:@"Mobile"]);
        EXPECT_TRUE([userAgent containsString:@"Macintosh"]);
    }
}

TEST(PreferredContentMode, IdempotentModeAutosizingOnlyHonorsPercentages)
{
    IPadUserInterfaceSwizzler iPadUserInterface;
    {
        auto [webView, delegate] = setUpWebViewForPreferredContentModeTesting<WKWebView>(WKContentModeMobile);
        [webView loadTestPageNamed:@"idempotent-mode-autosizing-only-honors-percentages" andExpectEffectiveContentMode:WKContentModeMobile withPolicyDecisionHandler:nil];
        EXPECT_EQ(static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"run1()"]).intValue, 12);
        EXPECT_EQ(static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"run2()"]).intValue, 6);
        EXPECT_EQ(static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"run3()"]).intValue, 6);
        EXPECT_EQ(static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"run4()"]).intValue, 12);
    }
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
