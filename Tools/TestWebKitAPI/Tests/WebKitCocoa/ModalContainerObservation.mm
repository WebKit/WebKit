/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/_WKModalContainerInfo.h>
#import <objc/runtime.h>
#import <wtf/FastMalloc.h>
#import <wtf/SetForScope.h>

@interface NSBundle (TestSupport)
- (NSURL *)swizzled_URLForResource:(NSString *)name withExtension:(NSString *)extension;
@end

@implementation NSBundle (TestSupport)

- (NSURL *)swizzled_URLForResource:(NSString *)name withExtension:(NSString *)extension
{
    if ([name isEqualToString:@"ModalContainerControls"] && [extension isEqualToString:@"mlmodelc"]) {
        // Override the default CoreML model with a smaller dataset that's limited to testing purposes.
        return [NSBundle.mainBundle URLForResource:@"TestModalContainerControls" withExtension:@"mlmodelc" subdirectory:@"TestWebKitAPI.resources"];
    }
    // Call through to the original method implementation if we're not specifically requesting ModalContainerControls.mlmodelc.
    return [self swizzled_URLForResource:name withExtension:extension];
}

@end

namespace TestWebKitAPI {

class ClassifierModelSwizzler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClassifierModelSwizzler()
        : m_originalMethod(class_getInstanceMethod(NSBundle.class, @selector(URLForResource:withExtension:)))
        , m_swizzledMethod(class_getInstanceMethod(NSBundle.class, @selector(swizzled_URLForResource:withExtension:)))
    {
        m_originalImplementation = method_getImplementation(m_originalMethod);
        m_swizzledImplementation = method_getImplementation(m_swizzledMethod);
        class_replaceMethod(NSBundle.class, @selector(swizzled_URLForResource:withExtension:), m_originalImplementation, method_getTypeEncoding(m_originalMethod));
        class_replaceMethod(NSBundle.class, @selector(URLForResource:withExtension:), m_swizzledImplementation, method_getTypeEncoding(m_swizzledMethod));
    }

    ~ClassifierModelSwizzler()
    {
        class_replaceMethod(NSBundle.class, @selector(swizzled_URLForResource:withExtension:), m_swizzledImplementation, method_getTypeEncoding(m_originalMethod));
        class_replaceMethod(NSBundle.class, @selector(URLForResource:withExtension:), m_originalImplementation, method_getTypeEncoding(m_swizzledMethod));
    }

private:
    Method m_originalMethod;
    Method m_swizzledMethod;
    IMP m_originalImplementation;
    IMP m_swizzledImplementation;
};

} // namespace TestWebKitAPI

@interface ModalContainerWebView : TestWKWebView <WKUIDelegatePrivate>
@property (nonatomic) _WKModalContainerDecision decision;
@property (nonatomic, readonly) _WKModalContainerInfo *lastModalContainerInfo;
@end

@implementation ModalContainerWebView {
    std::unique_ptr<TestWebKitAPI::ClassifierModelSwizzler> _classifierModelSwizzler;
    RetainPtr<TestNavigationDelegate> _navigationDelegate;
    RetainPtr<_WKModalContainerInfo> _lastModalContainerInfo;
    bool _doneWaitingForDecisionHandler;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        [TestProtocol registerWithScheme:@"http"];
    });

    _decision = _WKModalContainerDecisionShow;
    _classifierModelSwizzler = makeUnique<TestWebKitAPI::ClassifierModelSwizzler>();
    _navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    _doneWaitingForDecisionHandler = true;
    [self setNavigationDelegate:_navigationDelegate.get()];
    [self setUIDelegate:self];
    return self;
}

- (void)loadBundlePage:(NSString *)page andDecidePolicy:(_WKModalContainerDecision)decision
{
    SetForScope decisionScope { _decision, decision };
    _doneWaitingForDecisionHandler = false;

    [self loadBundlePage:page];

    TestWebKitAPI::Util::run(&_doneWaitingForDecisionHandler);
    [self waitForNextPresentationUpdate];
}

- (void)evaluate:(NSString *)script andDecidePolicy:(_WKModalContainerDecision)decision
{
    SetForScope decisionScope { _decision, decision };
    _doneWaitingForDecisionHandler = false;

    [self objectByEvaluatingJavaScript:script];

    TestWebKitAPI::Util::run(&_doneWaitingForDecisionHandler);
    [self waitForNextPresentationUpdate];
}

- (void)loadBundlePage:(NSString *)page
{
    NSURL *bundleURL = [NSBundle.mainBundle URLForResource:page withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURLRequest *fakeRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [self loadSimulatedRequest:fakeRequest responseHTMLString:[NSString stringWithContentsOfURL:bundleURL]];

    auto preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences _setModalContainerObservationPolicy:_WKWebsiteModalContainerObservationPolicyPrompt];
    [_navigationDelegate waitForDidFinishNavigationWithPreferences:preferences.get()];
}

- (void)loadHTML:(NSString *)html
{
    NSURLRequest *fakeRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [self loadSimulatedRequest:fakeRequest responseHTMLString:html];

    auto preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences _setModalContainerObservationPolicy:_WKWebsiteModalContainerObservationPolicyPrompt];
    [_navigationDelegate waitForDidFinishNavigationWithPreferences:preferences.get()];
}

- (void)waitForText:(NSString *)text
{
    TestWebKitAPI::Util::waitForConditionWithLogging([&]() -> bool {
        return [self.contentsAsString containsString:text];
    }, 3, @"Timed out waiting for '%@'", text);
}

- (void)_webView:(WKWebView *)webView decidePolicyForModalContainer:(_WKModalContainerInfo *)containerInfo decisionHandler:(void (^)(_WKModalContainerDecision))handler
{
    handler(_decision);
    _lastModalContainerInfo = containerInfo;
    _doneWaitingForDecisionHandler = true;
}

- (_WKModalContainerInfo *)lastModalContainerInfo
{
    return _lastModalContainerInfo.get();
}

@end

namespace TestWebKitAPI {

static RetainPtr<ModalContainerWebView> createModalContainerWebView()
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    return adoptNS([[ModalContainerWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration]);
}

constexpr auto allControlTypes = _WKModalContainerControlTypeNeutral | _WKModalContainerControlTypePositive | _WKModalContainerControlTypeNegative;

TEST(ModalContainerObservation, HideAndAllowModalContainer)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container" andDecidePolicy:_WKModalContainerDecisionHideAndAllow];
    NSString *visibleText = [webView contentsAsString];
    EXPECT_TRUE([visibleText containsString:@"Clicked on \"Yes\""]);
    EXPECT_FALSE([visibleText containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, allControlTypes);
}

TEST(ModalContainerObservation, HideAndDisallowModalContainer)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container" andDecidePolicy:_WKModalContainerDecisionHideAndDisallow];
    NSString *visibleText = [webView contentsAsString];
    EXPECT_TRUE([visibleText containsString:@"Clicked on \"No\""]);
    EXPECT_FALSE([visibleText containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, allControlTypes);
}

TEST(ModalContainerObservation, HideAndIgnoreModalContainer)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container" andDecidePolicy:_WKModalContainerDecisionHideAndIgnore];
    NSString *visibleText = [webView contentsAsString];
    EXPECT_FALSE([visibleText containsString:@"Clicked on"]);
    EXPECT_FALSE([visibleText containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, allControlTypes);
}

TEST(ModalContainerObservation, ShowModalContainer)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container" andDecidePolicy:_WKModalContainerDecisionShow];
    NSString *visibleText = [webView contentsAsString];
    EXPECT_FALSE([visibleText containsString:@"Clicked on"]);
    EXPECT_TRUE([visibleText containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, allControlTypes);
}

TEST(ModalContainerObservation, ClassifyMultiplySymbol)
{
    auto webView = createModalContainerWebView();
    auto runTest = [&] (NSString *symbol) {
        [webView loadBundlePage:@"modal-container-custom"];
        NSString *scriptToEvaluate = [NSString stringWithFormat:@"show(`<p>Hello world</p><button>%@</button>`)", symbol];
        [webView evaluate:scriptToEvaluate andDecidePolicy:_WKModalContainerDecisionHideAndIgnore];

        EXPECT_FALSE([[webView contentsAsString] containsString:@"Hello world"]);
        EXPECT_EQ([webView lastModalContainerInfo].availableTypes, _WKModalContainerControlTypeNeutral);
    };
    runTest(@"✕");
    runTest(@"⨯");
}

TEST(ModalContainerObservation, DetectSearchTermInBoldTag)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container-custom"];
    [webView evaluate:@"show(`<b>Hello world</b><a href='#'>Yes</a><a href='#'>No</a>`)" andDecidePolicy:_WKModalContainerDecisionHideAndIgnore];

    EXPECT_FALSE([[webView contentsAsString] containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, _WKModalContainerControlTypePositive | _WKModalContainerControlTypeNegative);
}

TEST(ModalContainerObservation, HideUserInteractionBlockingElementAndMakeDocumentScrollable)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container-with-overlay" andDecidePolicy:_WKModalContainerDecisionHideAndIgnore];

    EXPECT_FALSE([[webView contentsAsString] containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, _WKModalContainerControlTypePositive);

    NSString *hitTestedText = [webView stringByEvaluatingJavaScript:@"document.elementFromPoint(50, 50).textContent"];
    EXPECT_TRUE([hitTestedText containsString:@"Lorem"]);
    EXPECT_WK_STREQ("auto", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.documentElement).overflowY"]);
    EXPECT_WK_STREQ("auto", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.body).overflowY"]);
}

TEST(ModalContainerObservation, IgnoreFixedDocumentElement)
{
    auto webView = createModalContainerWebView();
    [webView setDecision:_WKModalContainerDecisionHideAndIgnore];
    [webView loadHTML:@("<head>"
        "<script>internals.overrideModalContainerSearchTermForTesting('foo')</script>"
        "<style>html { position: fixed; width: 100%; height: 100%; top: 0; left: 0; }</style>"
        "</head><body><h1>foo bar</h1></body>")];

    EXPECT_TRUE([[webView contentsAsString] containsString:@"foo bar"]);
    EXPECT_NULL([webView lastModalContainerInfo]);
}

TEST(ModalContainerObservation, IgnoreNavigationElements)
{
    auto webView = createModalContainerWebView();
    [webView setDecision:_WKModalContainerDecisionHideAndIgnore];
    [webView loadBundlePage:@"modal-container-custom"];
    [webView objectByEvaluatingJavaScript:@"show(`<nav>hello world 1</nav><div role='navigation'>hello world 2</div>`)"];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView contentsAsString] containsString:@"hello world 1"]);
    EXPECT_TRUE([[webView contentsAsString] containsString:@"hello world 2"]);
    EXPECT_NULL([webView lastModalContainerInfo]);
}

TEST(ModalContainerObservation, ShowModalContainerAfterFalsePositive)
{
    auto webView = createModalContainerWebView();
    [webView setDecision:_WKModalContainerDecisionHideAndIgnore];
    [webView loadBundlePage:@"modal-container-custom"];
    [webView objectByEvaluatingJavaScript:@"show(`<div>hello world</div><a href='https://apple.com'>Click here</a>`)"];
    [webView waitForNextPresentationUpdate];
    [webView waitForText:@"hello world"];
    EXPECT_NULL([webView lastModalContainerInfo]);
}

TEST(ModalContainerObservation, ModalContainerInSubframe)
{
    auto webView = createModalContainerWebView();
    [webView setDecision:_WKModalContainerDecisionHideAndIgnore];
    [webView loadBundlePage:@"modal-container-custom"];
    [webView evaluate:@"show(`<p>subframe test</p><iframe srcdoc='<h2>hello world</h2><button>YES</button>'></iframe>`)" andDecidePolicy:_WKModalContainerDecisionHideAndIgnore];
    EXPECT_FALSE([[webView contentsAsString] containsString:@"subframe test"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, _WKModalContainerControlTypePositive);
}

TEST(ModalContainerObservation, DetectModalContainerAfterSettingText)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container-custom"];
    [webView objectByEvaluatingJavaScript:@"show(`<div id='content'></div>`)"];
    [webView waitForNextPresentationUpdate];
    [webView evaluate:@"document.getElementById('content').innerHTML = `hello world <a href='#'>no</a>`" andDecidePolicy:_WKModalContainerDecisionHideAndIgnore];
    EXPECT_FALSE([[webView contentsAsString] containsString:@"hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, _WKModalContainerControlTypeNegative);
}

TEST(ModalContainerObservation, DetectControlsWithEventListenersOnModalContainer)
{
    auto webView = createModalContainerWebView();
    [webView loadBundlePage:@"modal-container-custom"];
    auto script = @"showWithEventListener(`<div>Hello world <span style='cursor: pointer;'>yes</span></div>`, 'click', () => window.testPassed = true)";
    [webView evaluate:script andDecidePolicy:_WKModalContainerDecisionHideAndAllow];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([[webView contentsAsString] containsString:@"Hello world"]);
    EXPECT_EQ([webView lastModalContainerInfo].availableTypes, _WKModalContainerControlTypePositive);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.testPassed"] boolValue]);
}

} // namespace TestWebKitAPI
