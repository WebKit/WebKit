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
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>
#import <wtf/MonotonicTime.h>
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

class TelephoneNumberInSubframe : public DataDetectorInSubframeTest {
public:
    static constexpr int nestedFrameWidth = 300;
    static constexpr int nestedFrameHeight = 200;
    static constexpr NSRect nestedFrameContentBox { nestedFrameContentOrigin, { nestedFrameWidth, nestedFrameHeight } };
    static constexpr NSRect nestedFrameAtOriginBox { { 300, 120 }, { 300, 200 } };
    static constexpr NSRect iframeNextToTelephoneNumberBox { { 300, 120 }, { 400, 300 } };

    void SetUp() override
    {
        String secondIframeOrigin { GetParam().crossOrigin ? "https://apple.com"_s : "https://example.com"_s };
        setUpWebView({
            { "/main"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/telephone-number.html"_s)))) },
            { "/main-without-scroll-bars"_s, html(makeString("<html style='overflow:hidden'><body style='margin:0'>"_s, iframe(iframeLeft, GetParam().subframeURL("/telephone-number.html"_s)), "</body></html>"_s)) },
            { "/main-with-two-iframes"_s, html(mainHTML(makeString(iframe(iframeLeft, GetParam().subframeURL("/telephone-number.html"_s)), iframe(secondIframeLeft, makeString(secondIframeOrigin, "/text.html"_s))))) },
            { "/main-with-two-telephone-numbers"_s, html(mainHTML(makeString(iframe(iframeLeft, GetParam().subframeURL("/telephone-number.html"_s)), iframe(secondIframeLeft, makeString(secondIframeOrigin, "/telephone-number.html"_s))))) },
            { "/main-with-nested-frame"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/nested.html"_s)))) },
            { "/main-with-nested-main-site-frame"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/nested-main-site.html"_s)))) },
            { "/main-with-telephone-number-and-nested-main-site-frame"_s, html(mainHTML(makeString("<p style='margin:40px; font:20px Helvetica'>Call <span id='phone'>408-996-1010</span> today</p>"_s, iframe(iframeLeft, GetParam().subframeURL("/nested-main-site.html"_s))))) },
            { "/main-with-telephone-number-next-to-nested-main-site-frame"_s, html(makeString("<body style='margin:0'><span id='phone' style='position:absolute; right:706px; top:130px; font:20px Helvetica; white-space:nowrap'>408-996-1010</span>"
                "<iframe style='position:absolute; left:300px; top:120px; width:400px; height:300px; border:0' src='"_s, GetParam().subframeURL("/nested-main-site-at-origin.html"_s), "'></iframe></body>"_s)) },
            { "/main-with-telephone-number-overlapping-nested-main-site-frame"_s, html(makeString("<body style='margin:0'><span id='phone' style='position:absolute; right:702px; top:130px; font:20px Helvetica; white-space:nowrap'>408-996-1010</span>"
                "<iframe style='position:absolute; left:300px; top:120px; width:400px; height:300px; border:0' src='"_s, GetParam().subframeURL("/nested-main-site-at-origin-with-telephone-number-at-left.html"_s), "'></iframe></body>"_s)) },
            { "/main-with-telephone-number-next-to-iframe"_s, html(makeString("<body style='margin:0'><span id='phone' style='position:absolute; right:706px; top:130px; font:20px Helvetica; white-space:nowrap'>408-996-1010</span>"
                "<iframe style='position:absolute; left:300px; top:120px; width:400px; height:300px; border:0' src='"_s, GetParam().subframeURL("/text.html"_s), "'></iframe></body>"_s)) },
            { "/main-with-scrollable-iframe"_s, html(mainHTML(iframe(iframeLeft, GetParam().subframeURL("/tall-telephone-number.html"_s)))) },
            { "/main-with-text-and-iframe"_s, html(mainHTML(makeString("<p style='margin:40px; font:20px Helvetica'>Select these words</p>"_s, iframe(iframeLeft, GetParam().subframeURL("/text.html"_s))))) },
            { "/nested.html"_s, html(nestedFrameHTML("telephone-number.html"_s, nestedFrameWidth, nestedFrameHeight)) },
            { "/nested-main-site.html"_s, html(nestedFrameHTML("https://example.com/telephone-number.html"_s, nestedFrameWidth, nestedFrameHeight)) },
            { "/nested-main-site-at-origin.html"_s, html("<body style='margin:0'><iframe style='position:absolute; left:0; top:0; width:300px; height:200px; border:0' src='https://example.com/telephone-number.html'></iframe></body>"_s) },
            { "/nested-main-site-at-origin-with-telephone-number-at-left.html"_s, html("<body style='margin:0'><iframe style='position:absolute; left:0; top:0; width:300px; height:200px; border:0' src='https://example.com/telephone-number-at-left.html'></iframe></body>"_s) },
            { "/telephone-number.html"_s, html("<body style='margin:0; font:20px Helvetica'><p style='margin:40px'>Call <span id='phone'>408-996-1010</span> today</p></body>"_s) },
            { "/telephone-number-at-left.html"_s, html("<body style='margin:0'><span id='phone' style='position:absolute; left:4px; top:10px; font:20px Helvetica; white-space:nowrap'>408-996-1010</span></body>"_s) },
            { "/tall-telephone-number.html"_s, html("<body style='margin:0; font:20px Helvetica'><p style='margin:100px'>Call <span id='phone'>408-996-1010</span> today</p><div style='height:1000px'></div></body>"_s) },
            { "/text.html"_s, html("<body style='margin:0; font:20px Helvetica'><p style='margin:40px'>No telephone number here</p></body>"_s) },
        }, [](WKWebViewConfiguration *configuration) {
            [[configuration preferences] _setTelephoneNumberDetectionIsEnabled:YES];
            [configuration _setServiceControlsEnabled:YES];
        });
    }

    CGRect telephoneNumberRect(WKFrameInfo *frame, CGPoint frameContentOrigin)
    {
        RetainPtr rect { dynamic_objc_cast<NSArray>([webView objectByEvaluatingJavaScript:@"(() => {"
            "  const range = document.createRange();"
            "  range.selectNodeContents(document.getElementById('phone'));"
            "  const rect = range.getBoundingClientRect();"
            "  return [rect.x, rect.y, rect.width, rect.height];"
            "})()" inFrame:frame]) };
        return rectInWebView(rect.get(), frameContentOrigin);
    }

    bool frameHasFocus(WKFrameInfo *frame)
    {
        return [[webView objectByEvaluatingJavaScript:@"document.hasFocus()" inFrame:frame] boolValue];
    }

    bool selectionContainsTelephoneNumber(WKFrameInfo *frame)
    {
        return [[webView objectByEvaluatingJavaScript:@"getSelection().toString().includes('408-996-1010')" inFrame:frame] boolValue];
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

    bool hasVisibleHighlightLayer(CGRect highlightBoundingRect)
    {
        return static_cast<bool>(hasVisibleLayerNear([webView layer], highlightBoundingRect, telephoneNumberRectTolerance));
    }

    float highlightOpacity(CGRect highlightBoundingRect)
    {
        RetainPtr layer { visibleLayerNear([webView layer], highlightBoundingRect, telephoneNumberRectTolerance) };
        return layer ? [layer opacity] : 0;
    }

    bool waitUntilHighlightIsOpaque(CGRect highlightBoundingRect)
    {
        return TestWebKitAPI::Util::waitFor([&] {
            [webView waitForNextPresentationUpdate];
            return highlightOpacity(highlightBoundingRect) == 1;
        });
    }

    bool moveAwayUntilHighlightIsHidden(CGPoint point, CGRect highlightBoundingRect)
    {
        moveMouse(point);
        return TestWebKitAPI::Util::waitFor([&] {
            [webView waitForNextPresentationUpdate];
            return !hasVisibleHighlightLayer(highlightBoundingRect);
        });
    }

    bool highlightStaysHidden(CGRect highlightBoundingRect)
    {
        TestWebKitAPI::Util::runFor(500_ms);
        [webView waitForNextPresentationUpdate];
        return !hasVisibleHighlightLayer(highlightBoundingRect);
    }

    bool hoveringShowsHighlight(CGPoint point, CGRect highlightBoundingRect)
    {
        for (unsigned i = 0; i < 5; ++i) {
            moveMouse(CGPointMake(point.x + (i % 2), point.y));
            [webView waitForNextPresentationUpdate];
            if (hasVisibleHighlightLayer(highlightBoundingRect))
                return true;
        }
        TestWebKitAPI::Util::runFor(500_ms);
        [webView waitForNextPresentationUpdate];
        return hasVisibleHighlightLayer(highlightBoundingRect);
    }

    struct Menu {
        CGRect telephoneNumberRect { CGRectNull };
        CGPoint location { CGPointZero };
    };

    std::optional<Menu> presentedMenu(NOESCAPE const Function<void()>& trigger, Seconds timeout)
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

        trigger();
        if (!didShowMenu && timeout)
            TestWebKitAPI::Util::runFor(&didShowMenu, timeout);
        if (!didShowMenu)
            return std::nullopt;
        return menu;
    }

    std::optional<Menu> clickButton(CGPoint pointOverButton)
    {
        return presentedMenu([&] {
            click(pointOverButton);
        }, 5_s);
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
};

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAreAtTheTelephoneNumber)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAccountForMainFrameScroll)
{
    load(@"/main");

    static constexpr int scrollOffset = 40;
    EXPECT_TRUE(scrollTo(nil, scrollOffset));

    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), NSMakePoint(CGRectGetMinX(iframeContentBox), CGRectGetMinY(iframeContentBox) - scrollOffset)) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, rootVisibleRect(NSOffsetRect(iframeContentBox, 0, -scrollOffset)));
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAccountForSubframeScroll)
{
    load(@"/main-with-scrollable-iframe");
    RetainPtr frame { [webView firstChildFrame] };
    selectTelephoneNumber(frame.get(), telephoneNumberRect(frame.get(), iframeContentBox.origin));

    static constexpr int scrollOffset = 30;
    EXPECT_TRUE(scrollTo(frame.get(), scrollOffset));

    showHighlightAndMenu(telephoneNumberRect(frame.get(), iframeContentBox.origin), rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightButtonStaysInViewWhenFrameIsPartlyOutOfView)
{
    load(@"/main-without-scroll-bars");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };

    CGRect visibleRect { [webView bounds] };
    int offset { static_cast<int>(std::round(CGRectGetMaxX(visibleRect) - 10 - CGRectGetMaxX(rect))) };
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('iframe').style.left = '%dpx'", iframeLeft + offset]];
    CGRect contentBox { NSOffsetRect(iframeContentBox, offset, 0) };
    ASSERT_TRUE(waitUntilWindowClipRectWidthIs(webView.get(), frame.get(), static_cast<int>(CGRectGetMaxX(visibleRect) - CGRectGetMinX(contentBox))));
    rect = telephoneNumberRect(frame.get(), contentBox.origin);

    auto highlight { dataDetectorHighlightGeometry(rect, visibleRect) };
    ASSERT_FALSE(rectsAreNear(highlight.boundingRect, dataDetectorHighlightGeometry(rect, contentBox).boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(rectsAreNear(highlight.boundingRect, dataDetectorHighlightGeometry(rect, CGRectIntersection(contentBox, visibleRect)).boundingRect, telephoneNumberRectTolerance));

    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, visibleRect);
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAreAtTheTelephoneNumberInNestedFrame)
{
    load(@"/main-with-nested-frame");
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameContentOrigin) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuInFrameNestedUnderCrossSiteFrame)
{
    load(@"/main-with-nested-main-site-frame");
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameContentOrigin) };
    selectTelephoneNumber(frame.get(), rect);
    showHighlightAndMenu(rect, rootVisibleRect(nestedFrameContentBox));
    if (HasFatalFailure())
        return;

    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(nestedFrameContentBox)) };
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(nestedFrameContentOrigin.x + 150, nestedFrameContentOrigin.y + 180), highlight.boundingRect));

    EXPECT_FALSE(hoveringShowsHighlight(CGPointMake(CGRectGetMidX(rect) - nestedFrameContentOrigin.x, CGRectGetMidY(rect) - nestedFrameContentOrigin.y), highlight.boundingRect));

    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));
    [webView objectByEvaluatingJavaScript:@"document.querySelector('iframe').remove()"];
    CGRect highlightRelativeToMainFrame { CGRectOffset(highlight.boundingRect, -nestedFrameContentOrigin.x, -nestedFrameContentOrigin.y) };
    bool wasShownRelativeToMainFrame { false };
    auto removalTime { MonotonicTime::now() };
    while (MonotonicTime::now() - removalTime < 500_ms) {
        [webView waitForNextPresentationUpdate];
        wasShownRelativeToMainFrame |= hasVisibleHighlightLayer(highlightRelativeToMainFrame);
    }
    EXPECT_FALSE(wasShownRelativeToMainFrame);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return !hasVisibleHighlightLayer(highlight.boundingRect);
    }));
    TestWebKitAPI::Util::runFor(500_ms);
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE(hasVisibleHighlightLayer(highlight.boundingRect));
    EXPECT_FALSE(hasVisibleHighlightLayer(highlightRelativeToMainFrame));
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"document.querySelectorAll('iframe').length"] intValue], 0);
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuInFrameNestedUnderCrossSiteFrameWithHighlightInMainFrame)
{
    load(@"/main-with-telephone-number-and-nested-main-site-frame");
    CGRect mainFrameRect { telephoneNumberRect(nil, CGPointZero) };
    selectTelephoneNumber(nil, mainFrameRect);
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameContentOrigin) };
    selectTelephoneNumber(frame.get(), rect);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return selectionContainsTelephoneNumber(nil) && selectionContainsTelephoneNumber(frame.get());
    }));

    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(nestedFrameContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(waitUntilHighlightIsOpaque(highlight.boundingRect));
    moveMouse(CGPointMake(CGRectGetMidX(rect) + 3, CGRectGetMidY(rect)));
    float minimumOpacity { 1 };
    for (unsigned i = 0; i < 5; ++i) {
        TestWebKitAPI::Util::runFor(30_ms);
        [webView waitForNextPresentationUpdate];
        minimumOpacity = std::min(minimumOpacity, highlightOpacity(highlight.boundingRect));
    }
    EXPECT_EQ(minimumOpacity, 1.f);
    clickButtonAndExpectMenu(highlight.buttonCenter, rect);
    if (HasFatalFailure())
        return;

    showHighlightAndMenu(mainFrameRect, [webView bounds]);
}

TEST_P(TelephoneNumberInSubframe, HighlightInMainFrameStaysWhileMouseIsOverNestedFrame)
{
    load(@"/main-with-telephone-number-next-to-nested-main-site-frame");
    CGRect mainFrameRect { telephoneNumberRect(nil, CGPointZero) };
    selectTelephoneNumber(nil, mainFrameRect);
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameAtOriginBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return selectionContainsTelephoneNumber(nil) && selectionContainsTelephoneNumber(frame.get());
    }));

    auto highlight { dataDetectorHighlightGeometry(mainFrameRect, [webView bounds]) };
    CGPoint pointOverNestedFrame { std::max<CGFloat>(CGRectGetMinX(nestedFrameAtOriginBox) + 2, CGRectGetMidX(highlight.buttonRect)), std::floor(CGRectGetMidY(highlight.buttonRect)) };
    ASSERT_TRUE(CGRectContainsPoint(highlight.buttonRect, pointOverNestedFrame) && CGRectContainsPoint(nestedFrameAtOriginBox, pointOverNestedFrame)) << NSStringFromRect(highlight.buttonRect).UTF8String;

    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), mainFrameRect, highlight.boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(waitUntilHighlightIsOpaque(highlight.boundingRect));
    float minimumOpacity { 1 };
    for (unsigned i = 0; i < 2; ++i) {
        moveMouse(CGPointMake(pointOverNestedFrame.x, pointOverNestedFrame.y + i));
        for (unsigned j = 0; j < 3; ++j) {
            TestWebKitAPI::Util::runFor(30_ms);
            [webView waitForNextPresentationUpdate];
            minimumOpacity = std::min(minimumOpacity, highlightOpacity(highlight.boundingRect));
        }
    }
    EXPECT_EQ(minimumOpacity, 1.f);
    clickButtonAndExpectMenu(pointOverNestedFrame, mainFrameRect);
}

TEST_P(TelephoneNumberInSubframe, HighlightInMainFrameStaysWhileMouseIsOverCrossSiteFrame)
{
    load(@"/main-with-telephone-number-next-to-iframe");
    CGRect mainFrameRect { telephoneNumberRect(nil, CGPointZero) };
    selectTelephoneNumber(nil, mainFrameRect);

    auto highlight { dataDetectorHighlightGeometry(mainFrameRect, [webView bounds]) };
    CGPoint pointOverFrame { std::max<CGFloat>(CGRectGetMinX(iframeNextToTelephoneNumberBox) + 2, CGRectGetMidX(highlight.buttonRect)), std::floor(CGRectGetMidY(highlight.buttonRect)) };
    ASSERT_TRUE(CGRectContainsPoint(highlight.buttonRect, pointOverFrame) && CGRectContainsPoint(iframeNextToTelephoneNumberBox, pointOverFrame)) << NSStringFromRect(highlight.buttonRect).UTF8String;

    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), mainFrameRect, highlight.boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(waitUntilHighlightIsOpaque(highlight.boundingRect));
    float minimumOpacity { 1 };
    for (unsigned i = 0; i < 2; ++i) {
        moveMouse(CGPointMake(pointOverFrame.x, pointOverFrame.y + i));
        for (unsigned j = 0; j < 3; ++j) {
            TestWebKitAPI::Util::runFor(30_ms);
            [webView waitForNextPresentationUpdate];
            minimumOpacity = std::min(minimumOpacity, highlightOpacity(highlight.boundingRect));
        }
    }
    EXPECT_EQ(minimumOpacity, 1.f);
    clickButtonAndExpectMenu(pointOverFrame, mainFrameRect);
}

TEST_P(TelephoneNumberInSubframe, OverlappingHighlightsInNestedFramesDoNotTakeTurns)
{
    load(@"/main-with-telephone-number-overlapping-nested-main-site-frame");
    CGRect mainFrameRect { telephoneNumberRect(nil, CGPointZero) };
    selectTelephoneNumber(nil, mainFrameRect);
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameAtOriginBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return selectionContainsTelephoneNumber(nil) && selectionContainsTelephoneNumber(frame.get());
    }));

    CGRect mainFrameVisibleRect { [webView bounds] };
    CGRect visibleRect { rootVisibleRect(nestedFrameAtOriginBox) };
    auto mainFrameHighlight { dataDetectorHighlightGeometry(mainFrameRect, mainFrameVisibleRect) };
    auto highlight { dataDetectorHighlightGeometry(rect, visibleRect) };
    RetainPtr mainFrameDDHighlight { createDataDetectorHighlight(mainFrameRect, mainFrameVisibleRect) };
    RetainPtr ddHighlight { createDataDetectorHighlight(rect, visibleRect) };
    auto isOverNestedFrameAndBothHighlights = [&](CGPoint point) {
        return CGRectContainsPoint(nestedFrameAtOriginBox, point) && pointIsOnDataDetectorHighlight(mainFrameDDHighlight.get(), point) && pointIsOnDataDetectorHighlight(ddHighlight.get(), point);
    };
    Vector<CGPoint> candidatePoints;
    CGRect overlap { CGRectIntegral(CGRectIntersection(mainFrameHighlight.boundingRect, highlight.boundingRect)) };
    for (auto y = CGRectGetMinY(overlap); y < CGRectGetMaxY(overlap); ++y) {
        for (auto x = CGRectGetMinX(overlap); x + 1 < CGRectGetMaxX(overlap); ++x) {
            if (isOverNestedFrameAndBothHighlights(CGPointMake(x, y)) && isOverNestedFrameAndBothHighlights(CGPointMake(x + 1, y)))
                candidatePoints.append(CGPointMake(x, y));
        }
    }
    ASSERT_FALSE(candidatePoints.isEmpty()) << NSStringFromRect(mainFrameHighlight.boundingRect).UTF8String << " and " << NSStringFromRect(highlight.boundingRect).UTF8String;
    CGPoint firstPoint { candidatePoints[candidatePoints.size() / 2] };
    std::array points { firstPoint, CGPointMake(firstPoint.x + 1, firstPoint.y) };

    unsigned moveCount { 0 };
    ASSERT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        moveMouse(points[moveCount++ % points.size()]);
        [webView waitForNextPresentationUpdate];
        return hasVisibleHighlightLayer(mainFrameHighlight.boundingRect) || hasVisibleHighlightLayer(highlight.boundingRect);
    })) << hasVisibleLayerNear([webView layer], mainFrameHighlight.boundingRect, telephoneNumberRectTolerance).message() << ' ' << hasVisibleLayerNear([webView layer], highlight.boundingRect, telephoneNumberRectTolerance).message();
    ASSERT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return GetParam().isCrossOriginWithSiteIsolation() ? highlightOpacity(mainFrameHighlight.boundingRect) == 1 : std::max(highlightOpacity(mainFrameHighlight.boundingRect), highlightOpacity(highlight.boundingRect)) == 1;
    }));

    bool mainFrameHighlightIsShown { highlightOpacity(mainFrameHighlight.boundingRect) == 1 };
    if (GetParam().isCrossOriginWithSiteIsolation())
        EXPECT_TRUE(mainFrameHighlightIsShown);
    CGRect shownHighlightBoundingRect { mainFrameHighlightIsShown ? mainFrameHighlight.boundingRect : highlight.boundingRect };
    CGRect otherHighlightBoundingRect { mainFrameHighlightIsShown ? highlight.boundingRect : mainFrameHighlight.boundingRect };
    float minimumShownOpacity { 1 };
    float maximumOtherOpacity { 0 };
    for (unsigned i = 0; i < 6; ++i) {
        moveMouse(points[i % points.size()]);
        for (unsigned j = 0; j < 3; ++j) {
            TestWebKitAPI::Util::runFor(30_ms);
            [webView waitForNextPresentationUpdate];
            minimumShownOpacity = std::min(minimumShownOpacity, highlightOpacity(shownHighlightBoundingRect));
            maximumOtherOpacity = std::max(maximumOtherOpacity, highlightOpacity(otherHighlightBoundingRect));
        }
    }
    EXPECT_EQ(minimumShownOpacity, 1.f);
    EXPECT_EQ(maximumOtherOpacity, 0.f);
}

TEST_P(TelephoneNumberInSubframe, MouseOverAnotherFrameDoesNotShowHighlight)
{
    load(@"/main-with-two-iframes");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(CGRectGetMidX(iframeContentBox), CGRectGetMaxY(iframeContentBox) - 20), highlight.boundingRect));

    EXPECT_FALSE(hoveringShowsHighlight(pointInSecondFrame(center(rect)), highlight.boundingRect));
}

TEST_P(TelephoneNumberInSubframe, HighlightGoesAwayWhenMouseMovesIntoAnotherFrame)
{
    load(@"/main-with-two-telephone-numbers");
    RetainPtr frame { [webView firstChildFrame] };
    RetainPtr secondFrame { [webView secondChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    CGRect secondRect { telephoneNumberRect(secondFrame.get(), secondIframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    selectTelephoneNumber(secondFrame.get(), secondRect);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return selectionContainsTelephoneNumber(frame.get()) && selectionContainsTelephoneNumber(secondFrame.get());
    }));

    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));

    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(CGRectGetMidX(secondIframeContentBox), CGRectGetMaxY(secondIframeContentBox) - 20), highlight.boundingRect));
}

TEST_P(TelephoneNumberInSubframe, HighlightGoesAwayWhenMouseMovesIntoMainFrame)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));

    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(50, CGRectGetMidY(rect)), highlight.boundingRect));
    EXPECT_TRUE(highlightStaysHidden(highlight.boundingRect));

    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightGoesAwayWhenMouseMovesIntoFrameWithoutTelephoneNumber)
{
    load(@"/main-with-two-iframes");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));

    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(pointInSecondFrame(center(rect)), highlight.boundingRect));
    EXPECT_TRUE(highlightStaysHidden(highlight.boundingRect));

    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightInNestedFrameGoesAwayWhenMouseMovesIntoMainFrame)
{
    load(@"/main-with-telephone-number-and-nested-main-site-frame");
    CGRect mainFrameRect { telephoneNumberRect(nil, CGPointZero) };
    selectTelephoneNumber(nil, mainFrameRect);
    RetainPtr frame { nestedFrame() };
    CGRect rect { telephoneNumberRect(frame.get(), nestedFrameContentOrigin) };
    selectTelephoneNumber(frame.get(), rect);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return selectionContainsTelephoneNumber(nil) && selectionContainsTelephoneNumber(frame.get());
    }));

    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(nestedFrameContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));

    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(50, CGRectGetMidY(rect)), highlight.boundingRect));
    EXPECT_TRUE(highlightStaysHidden(highlight.boundingRect));

    showHighlightAndMenu(mainFrameRect, [webView bounds]);
}

TEST_P(TelephoneNumberInSubframe, HighlightGoesAwayWhenMouseLeavesWebView)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));

    CGPoint pointBelowWebView { CGRectGetMidX(rect), CGRectGetMaxY([webView bounds]) + 10 };
    RetainPtr exitEvent { [webView _mouseEventWithType:NSEventTypeMouseExited atLocation:[webView convertPoint:pointBelowWebView toView:nil]] };
    [webView _simulateMouseExit:exitEvent.get()];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return !hasVisibleHighlightLayer(highlight.boundingRect);
    }));
    EXPECT_TRUE(highlightStaysHidden(highlight.boundingRect));
}

TEST_P(TelephoneNumberInSubframe, HighlightGoesAwayWhenSelectionMovesOffTelephoneNumber)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(waitUntilHighlightIsOpaque(highlight.boundingRect));

    [webView objectByEvaluatingJavaScript:@"(() => {"
        "  const text = document.querySelector('p').lastChild;"
        "  getSelection().setBaseAndExtent(text, 1, text, text.length);"
        "})()" inFrame:frame.get()];
    EXPECT_WK_STREQ("today", [webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:frame.get()]);

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return !hasVisibleHighlightLayer(highlight.boundingRect);
    }));
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAfterFocusMovesToMainFrame)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);

    click(CGPointMake(50, 60));
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return frameHasFocus(nil) && !frameHasFocus(frame.get()) && selectionContainsTelephoneNumber(frame.get());
    }));

    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuForNumberSelectedByScriptInUnfocusedFrame)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };

    click(CGPointMake(50, 60));
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return frameHasFocus(nil);
    }));
    [webView objectByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.querySelector('p'))" inFrame:frame.get()];
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return frameHasFocus(nil) && !frameHasFocus(frame.get()) && selectionContainsTelephoneNumber(frame.get());
    }));

    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightAndMenuAfterFocusMovesToAnotherFrame)
{
    load(@"/main-with-two-iframes");
    RetainPtr frame { [webView firstChildFrame] };
    RetainPtr secondFrame { [webView secondChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    selectTelephoneNumber(frame.get(), rect);
    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), rect, highlight.boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(waitUntilHighlightIsOpaque(highlight.boundingRect));
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(CGRectGetMidX(iframeContentBox), CGRectGetMaxY(iframeContentBox) - 20), highlight.boundingRect));

    click(pointInSecondFrame(center(rect)));
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return frameHasFocus(secondFrame.get()) && !frameHasFocus(frame.get()) && selectionContainsTelephoneNumber(frame.get());
    }));

    EXPECT_FALSE(hoveringShowsHighlight(pointInSecondFrame(center(rect)), highlight.boundingRect));

    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
}

TEST_P(TelephoneNumberInSubframe, HighlightsInTwoFramesWithSelectedTelephoneNumbers)
{
    load(@"/main-with-two-telephone-numbers");
    RetainPtr frame { [webView firstChildFrame] };
    RetainPtr secondFrame { [webView secondChildFrame] };
    CGRect rect { telephoneNumberRect(frame.get(), iframeContentBox.origin) };
    CGRect secondRect { telephoneNumberRect(secondFrame.get(), secondIframeContentBox.origin) };

    selectTelephoneNumber(frame.get(), rect);
    selectTelephoneNumber(secondFrame.get(), secondRect);
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return selectionContainsTelephoneNumber(frame.get()) && selectionContainsTelephoneNumber(secondFrame.get());
    }));

    showHighlightAndMenu(rect, rootVisibleRect(iframeContentBox));
    if (HasFatalFailure())
        return;

    auto highlight { dataDetectorHighlightGeometry(rect, rootVisibleRect(iframeContentBox)) };
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(CGRectGetMidX(iframeContentBox), CGRectGetMaxY(iframeContentBox) - 20), highlight.boundingRect));

    auto secondHighlight { dataDetectorHighlightGeometry(secondRect, rootVisibleRect(secondIframeContentBox)) };
    ASSERT_TRUE(hoverUntilHighlightIsShown(webView.get(), secondRect, secondHighlight.boundingRect, telephoneNumberRectTolerance));
    EXPECT_FALSE(hasVisibleHighlightLayer(highlight.boundingRect));
    clickButtonAndExpectMenu(secondHighlight.buttonCenter, secondRect);
}

INSTANTIATE_TEST_SUITE_P(DataDetection, TelephoneNumberInSubframe, testing::ValuesIn(siteIsolationParams), &TelephoneNumberInSubframe::testNameGenerator);

class SelectionServicesInSubframe : public TelephoneNumberInSubframe {
public:
    void makeSelectionServicesAvailable(WKFrameInfo *frame)
    {
        [webView objectByEvaluatingJavaScript:@"internals.setHasRelevantSelectionServicesForTesting(true); true" inFrame:frame];
    }

    CGRect wordRect(WKFrameInfo *frame, CGPoint frameContentOrigin, NSString *word)
    {
        RetainPtr rect { dynamic_objc_cast<NSArray>([webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => {"
            "  const text = document.querySelector('p').firstChild;"
            "  const start = text.data.indexOf('%@');"
            "  const range = document.createRange();"
            "  range.setStart(text, start);"
            "  range.setEnd(text, start + %lu);"
            "  const rect = range.getBoundingClientRect();"
            "  return [rect.x, rect.y, rect.width, rect.height];"
            "})()", word, static_cast<unsigned long>(word.length)] inFrame:frame]) };
        return rectInWebView(rect.get(), frameContentOrigin);
    }

    bool selectionIs(WKFrameInfo *frame, NSString *text)
    {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:frame] isEqualToString:text];
    }

    void selectWord(WKFrameInfo *frame, CGRect wordRect, NSString *word)
    {
        moveMouse(center(wordRect));
        [webView sendClicksAtPoint:[webView convertPoint:center(wordRect) toView:nil] numberOfClicks:2];
        [webView waitForPendingMouseEvents];
        EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
            return selectionIs(frame, word) && frameHasFocus(frame);
        }));
    }

    CGRect selectionRect(WKFrameInfo *frame, CGPoint frameContentOrigin)
    {
        RetainPtr rect { dynamic_objc_cast<NSArray>([webView objectByEvaluatingJavaScript:@"(() => {"
            "  const rect = internals.selectionBounds();"
            "  return [rect.x, rect.y, rect.width, rect.height];"
            "})()" inFrame:frame]) };
        return rectInWebView(rect.get(), frameContentOrigin);
    }

    static DataDetectorHighlightGeometry selectionHighlightGeometry(CGRect selectionRect, CGRect visibleRect)
    {
        return dataDetectorHighlightGeometry(selectionRect, visibleRect, DataDetectorHighlightStyle::SelectionButton);
    }

    testing::AssertionResult hoverButtonUntilHighlightIsShown(const DataDetectorHighlightGeometry& highlight)
    {
        return hoverUntilHighlightIsShown(webView.get(), { highlight.buttonCenter, CGSizeZero }, highlight.boundingRect, telephoneNumberRectTolerance);
    }

    void clickServicesButtonAndExpectMenu(CGPoint pointOverButton, NSString *selectedText)
    {
        __block RetainPtr<NSString> menuText;

        SEL initWithItems { @selector(initWithItems:) };
        auto originalInitWithItems { reinterpret_cast<id (*)(id, SEL, NSArray *)>(method_getImplementation(class_getInstanceMethod(NSSharingServicePicker.class, initWithItems))) };
        InstanceMethodSwizzler initWithItemsSwizzler {
            NSSharingServicePicker.class,
            initWithItems,
            imp_implementationWithBlock(^(id picker, NSArray *items) {
                menuText = dynamic_objc_cast<NSAttributedString>(items.firstObject).string;
                return originalInitWithItems(picker, initWithItems, items);
            })
        };
        InstanceMethodSwizzler pickerMenuSwizzler {
            NSSharingServicePicker.class,
            @selector(menu),
            imp_implementationWithBlock(^NSMenu *(id) {
                return adoptNS([[NSMenu alloc] init]).autorelease();
            })
        };

        auto menu { clickButton(pointOverButton) };
        ASSERT_TRUE(menu.has_value());

        EXPECT_WK_STREQ(selectedText, menuText.get());
        EXPECT_NEAR(menu->location.x, pointOverButton.x, 1);
        EXPECT_NEAR(menu->location.y, pointOverButton.y, 1);
    }
};

TEST_P(SelectionServicesInSubframe, ButtonAndMenuAreAtTheEndOfTheSelection)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    makeSelectionServicesAvailable(frame.get());
    selectWord(frame.get(), wordRect(frame.get(), iframeContentBox.origin, @"Call"), @"Call");

    auto highlight { selectionHighlightGeometry(selectionRect(frame.get(), iframeContentBox.origin), rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverButtonUntilHighlightIsShown(highlight));
    clickServicesButtonAndExpectMenu(highlight.buttonCenter, @"Call");
}

TEST_P(SelectionServicesInSubframe, ButtonStaysInViewWhenFrameIsPartlyOutOfView)
{
    load(@"/main-without-scroll-bars");
    RetainPtr frame { [webView firstChildFrame] };
    makeSelectionServicesAvailable(frame.get());

    CGRect visibleRect { [webView bounds] };
    CGRect word { wordRect(frame.get(), iframeContentBox.origin, @"Call") };
    int offset { static_cast<int>(std::round(CGRectGetMaxX(visibleRect) - 10 - CGRectGetMaxX(word))) };
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('iframe').style.left = '%dpx'", iframeLeft + offset]];
    CGRect contentBox { NSOffsetRect(iframeContentBox, offset, 0) };
    ASSERT_TRUE(waitUntilWindowClipRectWidthIs(webView.get(), frame.get(), static_cast<int>(CGRectGetMaxX(visibleRect) - CGRectGetMinX(contentBox))));

    selectWord(frame.get(), wordRect(frame.get(), contentBox.origin, @"Call"), @"Call");
    CGRect selection { selectionRect(frame.get(), contentBox.origin) };

    auto highlight { selectionHighlightGeometry(selection, visibleRect) };
    ASSERT_FALSE(rectsAreNear(highlight.boundingRect, selectionHighlightGeometry(selection, contentBox).boundingRect, telephoneNumberRectTolerance));
    ASSERT_TRUE(rectsAreNear(highlight.boundingRect, selectionHighlightGeometry(selection, CGRectIntersection(contentBox, visibleRect)).boundingRect, telephoneNumberRectTolerance));

    ASSERT_TRUE(hoverButtonUntilHighlightIsShown(highlight));
    clickServicesButtonAndExpectMenu(highlight.buttonCenter, @"Call");
}

TEST_P(SelectionServicesInSubframe, ButtonIsNotShownForMainFrameSelectionAfterSelectingInSubframe)
{
    load(@"/main-with-text-and-iframe");
    RetainPtr frame { [webView firstChildFrame] };
    makeSelectionServicesAvailable(nil);
    makeSelectionServicesAvailable(frame.get());

    selectWord(nil, wordRect(nil, CGPointZero, @"these"), @"these");
    auto mainFrameHighlight { selectionHighlightGeometry(selectionRect(nil, CGPointZero), [webView bounds]) };
    ASSERT_TRUE(hoverButtonUntilHighlightIsShown(mainFrameHighlight));
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(50, 300), mainFrameHighlight.boundingRect));

    selectWord(frame.get(), wordRect(frame.get(), iframeContentBox.origin, @"telephone"), @"telephone");
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.activeElement === document.querySelector('iframe')"] boolValue];
    }));
    EXPECT_TRUE(selectionIs(nil, @"these"));
    EXPECT_FALSE(hoveringShowsHighlight(mainFrameHighlight.buttonCenter, mainFrameHighlight.boundingRect));

    auto highlight { selectionHighlightGeometry(selectionRect(frame.get(), iframeContentBox.origin), rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverButtonUntilHighlightIsShown(highlight));
    clickServicesButtonAndExpectMenu(highlight.buttonCenter, @"telephone");
}

TEST_P(SelectionServicesInSubframe, ButtonIsNotShownForSubframeSelectionAfterSelectingInMainFrame)
{
    load(@"/main-with-text-and-iframe");
    RetainPtr frame { [webView firstChildFrame] };
    makeSelectionServicesAvailable(nil);
    makeSelectionServicesAvailable(frame.get());

    selectWord(frame.get(), wordRect(frame.get(), iframeContentBox.origin, @"telephone"), @"telephone");
    auto highlight { selectionHighlightGeometry(selectionRect(frame.get(), iframeContentBox.origin), rootVisibleRect(iframeContentBox)) };
    ASSERT_TRUE(hoverButtonUntilHighlightIsShown(highlight));
    EXPECT_TRUE(moveAwayUntilHighlightIsHidden(CGPointMake(CGRectGetMidX(iframeContentBox), CGRectGetMaxY(iframeContentBox) - 20), highlight.boundingRect));

    selectWord(nil, wordRect(nil, CGPointZero, @"these"), @"these");
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return !frameHasFocus(frame.get());
    }));
    EXPECT_TRUE(selectionIs(frame.get(), @"telephone"));
    EXPECT_FALSE(hoveringShowsHighlight(highlight.buttonCenter, highlight.boundingRect));

    auto mainFrameHighlight { selectionHighlightGeometry(selectionRect(nil, CGPointZero), [webView bounds]) };
    ASSERT_TRUE(hoverButtonUntilHighlightIsShown(mainFrameHighlight));
    clickServicesButtonAndExpectMenu(mainFrameHighlight.buttonCenter, @"these");
}

INSTANTIATE_TEST_SUITE_P(DataDetection, SelectionServicesInSubframe, testing::ValuesIn(siteIsolationParams), &SelectionServicesInSubframe::testNameGenerator);

#if ENABLE(IPC_TESTING_API)

class TelephoneNumberMenuRequestFromSubframe : public TelephoneNumberInSubframe {
public:
    void adjustConfiguration(WKWebViewConfiguration *configuration) override
    {
        setFeatureEnabled(configuration, @"IPCTestingAPIEnabled", true);
        setFeatureEnabled(configuration, @"IgnoreInvalidMessageWhenIPCTestingAPIEnabled", true);
    }

    RetainPtr<NSString> requestMenuNamingFrame(WKFrameInfo *sender, NSString *frameIDExpression)
    {
        return [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => {"
            "  IPC.sendMessage('UI', IPC.pageID, IPC.messages.WebPageProxy_ShowTelephoneNumberMenu.name, ["
            "    { type: 'FrameID', value: %@ },"
            "    { type: 'String', value: '408-996-1010' },"
            "    { type: 'IntPoint', x: 20, y: 20 },"
            "    { type: 'IntRect', x: 10, y: 10, width: 120, height: 20 },"
            "  ]);"
            "  const reply = IPC.sendSyncMessage('UI', 0, IPC.messages.WebProcessProxy_TakeInvalidMessageStringForTesting.name, 1000, []);"
            "  return reply ? (reply.arguments && reply.arguments[0] ? reply.arguments[0].value : '') : '<no reply>';"
            "})()", frameIDExpression] inFrame:sender];
    }
};

TEST_P(TelephoneNumberMenuRequestFromSubframe, NamingTheMainFrameShowsNoMenu)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    ASSERT_NE(frame.get()._processIdentifier, [webView _webProcessIdentifier]);
    RetainPtr mainFrameID { [webView stringByEvaluatingJavaScript:@"String(IPC.frameID[0])"] };

    RetainPtr<NSString> invalidMessage;
    auto menu { presentedMenu([&] {
        invalidMessage = requestMenuNamingFrame(frame.get(), [NSString stringWithFormat:@"[BigInt('%@')]", mainFrameID.get()]);
    }, 0_s) };
    EXPECT_FALSE(menu.has_value());
    EXPECT_WK_STREQ(@"", invalidMessage.get());
}

TEST_P(TelephoneNumberMenuRequestFromSubframe, NamingItsOwnFrameShowsMenuAtTheGivenPoint)
{
    load(@"/main");
    RetainPtr frame { [webView firstChildFrame] };
    ASSERT_NE(frame.get()._processIdentifier, [webView _webProcessIdentifier]);

    RetainPtr<NSString> invalidMessage;
    auto menu { presentedMenu([&] {
        invalidMessage = requestMenuNamingFrame(frame.get(), @"IPC.frameID");
    }, 0_s) };
    ASSERT_TRUE(menu.has_value());
    EXPECT_WK_STREQ(@"", invalidMessage.get());
    EXPECT_EQ(menu->location.x, 20);
    EXPECT_EQ(menu->location.y, 20);
    EXPECT_TRUE(rectsAreNear(menu->telephoneNumberRect, CGRectMake(10, 10, 120, 20), telephoneNumberRectTolerance)) << NSStringFromRect(menu->telephoneNumberRect).UTF8String;
}

INSTANTIATE_TEST_SUITE_P(DataDetection, TelephoneNumberMenuRequestFromSubframe, testing::ValuesIn(crossOriginWithSiteIsolationParams), &TelephoneNumberMenuRequestFromSubframe::testNameGenerator);

#endif // ENABLE(IPC_TESTING_API)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(TELEPHONE_NUMBER_DETECTION) && ENABLE(DATA_DETECTION)
