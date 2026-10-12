/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/DataDetectorsCoreSPI.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(MAC)
#import "Helpers/mac/DataDetectorHighlightTestUtilities.h"
#endif

#if PLATFORM(IOS_FAMILY)

@interface WKWebView (DataDetection)
- (void)synchronouslyDetectDataWithTypes:(WKDataDetectorTypes)types;
- (void)synchronouslyRemoveDataDetectedLinks;
@end

@implementation WKWebView (DataDetection)

- (void)synchronouslyDetectDataWithTypes:(WKDataDetectorTypes)types
{
    __block bool done = false;
    [self _detectDataWithTypes:types completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)synchronouslyRemoveDataDetectedLinks
{
    __block bool done = false;
    [self _removeDataDetectedLinks:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

@end

static bool ranScript;

@interface DataDetectionUIDelegate : NSObject <WKUIDelegate>

@property (nonatomic, retain) NSDate *referenceDate;

@end

@implementation DataDetectionUIDelegate

- (NSDictionary *)_dataDetectionContextForWebView:(WKWebView *)webView
{
    if (!_referenceDate)
        return nil;

    return @{
        @"ReferenceDate": _referenceDate,
        @"unused": [NSUUID UUID]
    };
}

@end

void expectLinkCount(WKWebView *webView, NSString *HTMLString, unsigned linkCount)
{
    [webView loadHTMLString:HTMLString baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.querySelectorAll('a[x-apple-data-detectors=true]').length" completionHandler:^(id value, NSError *error) {
        EXPECT_EQ(linkCount, [value unsignedIntValue]);
        ranScript = true;
    }];

    TestWebKitAPI::Util::run(&ranScript);
    ranScript = false;
}

TEST(WebKit, DataDetectionReferenceDate)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setDataDetectorTypes:WKDataDetectorTypeCalendarEvent];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<DataDetectionUIDelegate> UIDelegate = adoptNS([[DataDetectionUIDelegate alloc] init]);
    [webView setUIDelegate:UIDelegate.get()];

    expectLinkCount(webView.get(), @"tomorrow at 6PM", 1);
    expectLinkCount(webView.get(), @"yesterday at 6PM", 0);
    expectLinkCount(webView.get(), @"<a href='about:blank'>tomorrow at 6PM</a>", 0);
    expectLinkCount(webView.get(), @"<a href='about:blank'>tomorrow</a> at <a href='about:blank'>6PM</a>", 0);


    NSTimeInterval week = 60 * 60 * 24 * 7;

    [UIDelegate setReferenceDate:[NSDate dateWithTimeIntervalSinceNow:-week]];
    expectLinkCount(webView.get(), @"tomorrow at 6PM", 0);
    expectLinkCount(webView.get(), @"yesterday at 6PM", 0);

    [UIDelegate setReferenceDate:[NSDate dateWithTimeIntervalSinceNow:week]];
    expectLinkCount(webView.get(), @"tomorrow at 6PM", 1);
    expectLinkCount(webView.get(), @"yesterday at 6PM", 1);
}

#if PLATFORM(IOS) || PLATFORM(VISION)

TEST(WebKit, AddAndRemoveDataDetectors)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"data-detectors"];
    [webView expectElementCount:0 querySelector:@"a[x-apple-data-detectors=true]"];
    EXPECT_EQ(0U, [webView _dataDetectionResults].count);

    auto checkDataDetectionResults = [] (NSArray<DDScannerResult *> *results) {
        EXPECT_EQ(3U, results.count);
        EXPECT_TRUE([results[0].value containsString:@"+1-234-567-8900"]);
        EXPECT_TRUE([results[0].value containsString:@"https://www.apple.com"]);
        EXPECT_TRUE([results[0].value containsString:@"2 Apple Park Way, Cupertino 95014"]);
        EXPECT_WK_STREQ("SignatureBlock", results[0].type);
        EXPECT_WK_STREQ("Date", results[1].type);
        EXPECT_WK_STREQ("December 21, 2099", results[1].value);
        EXPECT_WK_STREQ("FlightInformation", results[2].type);
        EXPECT_WK_STREQ("AC780", results[2].value);

        EXPECT_EQ(DDResultCategoryUnknown, results[0].category);
        EXPECT_EQ(DDResultCategoryCalendarEvent, results[1].category);
        EXPECT_EQ(DDResultCategoryMisc, results[2].category);
    };

    [webView synchronouslyDetectDataWithTypes:WKDataDetectorTypeAll];
    [webView expectElementCount:5 querySelector:@"a[x-apple-data-detectors=true]"];
    checkDataDetectionResults([webView _dataDetectionResults]);

    [webView synchronouslyRemoveDataDetectedLinks];
    [webView expectElementCount:0 querySelector:@"a[x-apple-data-detectors=true]"];
    EXPECT_EQ(0U, [webView _dataDetectionResults].count);

    [webView synchronouslyDetectDataWithTypes:WKDataDetectorTypeAddress | WKDataDetectorTypePhoneNumber];
    [webView expectElementCount:2 querySelector:@"a[x-apple-data-detectors=true]"];
    checkDataDetectionResults([webView _dataDetectionResults]);
}

TEST(WebKit, DoNotCrashWhenDetectingDataAfterWebProcessTerminates)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"data-detectors"];
    [webView _killWebContentProcessAndResetState];
    [webView synchronouslyDetectDataWithTypes:WKDataDetectorTypeAll];
    [webView synchronouslyRemoveDataDetectedLinks];
}

#endif // PLATFORM(IOS) || PLATFORM(VISION)

#endif

#if PLATFORM(MAC) && ENABLE(TELEPHONE_NUMBER_DETECTION) && ENABLE(DATA_DETECTION)

namespace TestWebKitAPI {

static constexpr CGFloat telephoneNumberRectTolerance = 2;

class TelephoneNumberInSubframe : public testing::TestWithParam<SiteIsolationParams> {
public:
    static constexpr int iframeLeft = 100;
    static constexpr int iframeTop = 120;
    static constexpr int iframeWidth = 400;
    static constexpr int iframeHeight = 300;
    static constexpr int iframeBorder = 3;
    static constexpr int iframePadding = 5;
    static constexpr int iframeBorderAndPadding = iframeBorder + iframePadding;
    static constexpr NSRect iframeContentBox { { iframeLeft + iframeBorderAndPadding, iframeTop + iframeBorderAndPadding }, { iframeWidth, iframeHeight } };
    static constexpr int nestedFrameLeft = 30;
    static constexpr int nestedFrameTop = 40;
    static constexpr NSPoint nestedFrameContentOrigin { iframeLeft + iframeBorderAndPadding + nestedFrameLeft, iframeTop + iframeBorderAndPadding + nestedFrameTop };

    void SetUp() override
    {
        auto iframe = [](int left, const String& url) {
            return makeString("<iframe style='position:absolute; left:"_s, left, "px; top:"_s, iframeTop, "px; width:"_s, iframeWidth, "px; height:"_s, iframeHeight,
                "px; border:"_s, iframeBorder, "px solid black; padding:"_s, iframePadding, "px' src='"_s, url, "'></iframe>"_s);
        };
        auto mainHTML = [](const String& iframes) {
            return makeString("<body style='margin:0'>"_s, iframes, "<div style='height:2000px'></div></body>"_s);
        };
        auto nestedFrameHTML = [](const String& url) {
            return makeString("<body style='margin:0'><iframe style='position:absolute; left:"_s, nestedFrameLeft, "px; top:"_s, nestedFrameTop, "px; width:300px; height:200px; border:0' src='"_s, url, "'></iframe></body>"_s);
        };
        auto html = [](const String& body) {
            return HTTPResponse { { { "Content-Type"_s, "text/html"_s } }, body };
        };

        server = makeUnique<HTTPServer>(std::initializer_list<std::pair<String, HTTPResponse>> {
            { "/main"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/telephone-number.html"_s)))) },
            { "/main-with-nested-frame"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/nested.html"_s)))) },
            { "/main-with-scrollable-iframe"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/tall-telephone-number.html"_s)))) },
            { "/nested.html"_s, html(nestedFrameHTML("telephone-number.html"_s)) },
            { "/telephone-number.html"_s, html("<body style='margin:0; font:20px Helvetica'><p style='margin:40px'>Call <span id='phone'>408-996-1010</span> today</p></body>"_s) },
            { "/tall-telephone-number.html"_s, html("<body style='margin:0; font:20px Helvetica'><p style='margin:100px'>Call <span id='phone'>408-996-1010</span> today</p><div style='height:1000px'></div></body>"_s) },
        }, HTTPServer::Protocol::HttpsProxy);

        RetainPtr configuration { adoptNS([[WKWebViewConfiguration alloc] init]) };
        [configuration setWebsiteDataStore:[server->httpsProxyConfiguration() websiteDataStore]];
        [[configuration preferences] _setTelephoneNumberDetectionIsEnabled:YES];
        [configuration _setServiceControlsEnabled:YES];
        GetParam().applyTo(configuration.get());

        navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [navigationDelegate allowAnyTLSCertificate];
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 600) configuration:configuration.get() addToWindow:YES]);
        [webView _setWindowOcclusionDetectionEnabled:NO];
        [webView setNavigationDelegate:navigationDelegate.get()];
        [[webView window] makeKeyAndOrderFront:nil];
        [[webView window] orderFrontRegardless];
    }

    void load(NSString *path)
    {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[@"https://example.com" stringByAppendingString:path]]]];
        [navigationDelegate waitForDidFinishNavigation];
        [webView waitForNextPresentationUpdate];
    }

    RetainPtr<WKFrameInfo> nestedFrame()
    {
        RetainPtr<WKFrameInfo> frame;
        EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
            frame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
            return !!frame;
        }));
        return frame;
    }

    CGRect telephoneNumberRect(WKFrameInfo *frame, CGPoint frameContentOrigin)
    {
        RetainPtr rect { dynamic_objc_cast<NSArray>([webView objectByEvaluatingJavaScript:@"(() => {"
            "  const range = document.createRange();"
            "  range.selectNodeContents(document.getElementById('phone'));"
            "  const rect = range.getBoundingClientRect();"
            "  return [rect.x, rect.y, rect.width, rect.height];"
            "})()" inFrame:frame]) };
        return CGRectMake(frameContentOrigin.x + [[rect objectAtIndex:0] doubleValue], frameContentOrigin.y + [[rect objectAtIndex:1] doubleValue],
            [[rect objectAtIndex:2] doubleValue], [[rect objectAtIndex:3] doubleValue]);
    }

    static CGPoint center(CGRect rect)
    {
        return CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
    }

    bool selectionContainsTelephoneNumber(WKFrameInfo *frame)
    {
        return [[webView objectByEvaluatingJavaScript:@"getSelection().toString().includes('408-996-1010')" inFrame:frame] boolValue];
    }

    void moveMouse(CGPoint pointInWebView)
    {
        [webView mouseMoveToPoint:[webView convertPoint:pointInWebView toView:nil] withFlags:0];
        [webView waitForPendingMouseEvents];
    }

    void click(CGPoint pointInWebView)
    {
        moveMouse(pointInWebView);
        [webView sendClickAtPoint:[webView convertPoint:pointInWebView toView:nil]];
        [webView waitForPendingMouseEvents];
    }

    void selectTelephoneNumber(WKFrameInfo *frame, CGRect telephoneNumberRect)
    {
        moveMouse(center(telephoneNumberRect));
        [webView sendClicksAtPoint:[webView convertPoint:center(telephoneNumberRect) toView:nil] numberOfClicks:3];
        [webView waitForPendingMouseEvents];
        EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
            return selectionContainsTelephoneNumber(frame);
        }));
    }

    struct Menu {
        CGRect telephoneNumberRect { CGRectNull };
        CGPoint location { CGPointZero };
    };

    std::optional<Menu> clickButton(CGPoint pointOverButton)
    {
        Class highlightDelegateClass { NSClassFromString(@"WKEmptyPresenterHighlightDelegate") };
        EXPECT_NOT_NULL(highlightDelegateClass);
        if (!highlightDelegateClass)
            return std::nullopt;

        __block bool didShowMenu { false };
        __block Menu menu;

        SEL initWithRect { NSSelectorFromString(@"initWithRect:") };
        auto originalInitWithRect { reinterpret_cast<id (*)(id, SEL, NSRect)>(method_getImplementation(class_getInstanceMethod(highlightDelegateClass, initWithRect))) };
        InstanceMethodSwizzler initWithRectSwizzler {
            highlightDelegateClass,
            initWithRect,
            imp_implementationWithBlock(^(id delegate, NSRect rect) {
                menu.telephoneNumberRect = rect;
                return originalInitWithRect(delegate, initWithRect, rect);
            })
        };
        InstanceMethodSwizzler popUpSwizzler {
            [NSMenu class],
            @selector(popUpMenuPositioningItem:atLocation:inView:),
            imp_implementationWithBlock(^(NSMenu *, NSMenuItem *, NSPoint location, NSView *) {
                menu.location = location;
                didShowMenu = true;
                return NO;
            })
        };

        click(pointOverButton);
        if (!TestWebKitAPI::Util::runFor(&didShowMenu, 5_s))
            return std::nullopt;
        return menu;
    }

    void clickButtonAndExpectMenu(CGPoint pointOverButton, CGRect telephoneNumberRect)
    {
        auto menu { clickButton(pointOverButton) };
        ASSERT_TRUE(menu.has_value());

        EXPECT_TRUE(rectsAreNear(menu->telephoneNumberRect, telephoneNumberRect, telephoneNumberRectTolerance)) << "Expected " << NSStringFromRect(telephoneNumberRect).UTF8String << ", got " << NSStringFromRect(menu->telephoneNumberRect).UTF8String;
        EXPECT_NEAR(menu->location.x, pointOverButton.x, 1);
        EXPECT_NEAR(menu->location.y, pointOverButton.y, 1);
    }

    void showHighlightAndMenu(CGRect telephoneNumberRect, CGRect visibleRect)
    {
        auto highlight { dataDetectorHighlightGeometry(telephoneNumberRect, visibleRect) };
        ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), telephoneNumberRect, highlight.boundingRect, telephoneNumberRectTolerance));
        clickButtonAndExpectMenu(highlight.buttonCenter, telephoneNumberRect);
    }

    static std::string testNameGenerator(testing::TestParamInfo<SiteIsolationParams> info)
    {
        return info.param.testName();
    }

    std::unique_ptr<HTTPServer> server;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<TestWKWebView> webView;
};

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAreAtTheTelephoneNumber)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, [webView bounds]);
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAccountForMainFrameScroll)
{
    load(@"/main");

    static constexpr int scrollOffset = 40;
    bool scrolled { TestWebKitAPI::Util::waitFor([&] {
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(0, %d)", scrollOffset]];
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == scrollOffset;
    }) };
    EXPECT_TRUE(scrolled);
    [webView waitForNextPresentationUpdate];

    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), NSMakePoint(CGRectGetMinX(iframeContentBox), CGRectGetMinY(iframeContentBox) - scrollOffset)) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, [webView bounds]);
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAccountForSubframeScroll)
{
    load(@"/main-with-scrollable-iframe");
    RetainPtr frame { [webView firstChildFrame] };
    selectTelephoneNumber(frame.get(), telephoneNumberRect(frame.get(), iframeContentBox.origin));

    static constexpr int scrollOffset = 30;
    bool scrolled { TestWebKitAPI::Util::waitFor([&] {
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(0, %d)", scrollOffset] inFrame:frame.get()];
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:frame.get()] intValue] == scrollOffset;
    }) };
    EXPECT_TRUE(scrolled);
    [webView waitForNextPresentationUpdate];

    showHighlightAndMenu(telephoneNumberRect(frame.get(), iframeContentBox.origin), [webView bounds]);
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAreAtTheTelephoneNumberInNestedFrame)
{
    load(@"/main-with-nested-frame");
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameContentOrigin) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, [webView bounds]);
}

// FIXME: Run in every configuration once highlights work in a cross-origin subframe's process (https://bugs.webkit.org/show_bug.cgi?id=326784).
static std::vector<SiteIsolationParams> telephoneNumberInSubframeParams()
{
    std::vector<SiteIsolationParams> params;
    for (auto& siteIsolationParameters : siteIsolationParams) {
        if (!siteIsolationParameters.isCrossOriginWithSiteIsolation())
            params.push_back(siteIsolationParameters);
    }
    return params;
}

INSTANTIATE_TEST_SUITE_P(DataDetection, TelephoneNumberInSubframe, testing::ValuesIn(telephoneNumberInSubframeParams()), &TelephoneNumberInSubframe::testNameGenerator);

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(TELEPHONE_NUMBER_DETECTION) && ENABLE(DATA_DETECTION)
