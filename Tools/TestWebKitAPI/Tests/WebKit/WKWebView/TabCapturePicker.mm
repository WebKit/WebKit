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

#if PLATFORM(MAC) && ENABLE(MEDIA_STREAM) && HAVE(SCREEN_CAPTURE_KIT)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static constexpr NSRect webViewFrame = { { 0, 0 }, { 400, 300 } };

static constexpr auto tabMarkup = R"HTML(
<body style="margin: 0; height: 2000px">
<script>
var clickCount = 0;
document.body.addEventListener('click', () => ++clickCount);
</script>
</body>
)HTML";

static void loadTab(TestWKWebView *webView)
{
    [webView synchronouslyLoadHTMLString:@(tabMarkup) baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
}

static RetainPtr<TestWKWebView> createTab()
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:webViewFrame configuration:adoptNS([[WKWebViewConfiguration alloc] init])]);
    loadTab(webView);
    return webView;
}

static RetainPtr<TestWKWebView> createTabInWindowOf(TestWKWebView *webView, BOOL hidden)
{
    RetainPtr newWebView = adoptNS([[TestWKWebView alloc] initWithFrame:webViewFrame configuration:adoptNS([[WKWebViewConfiguration alloc] init]) addToWindow:NO]);
    [newWebView setHidden:hidden];
    [[[webView hostWindow] contentView] addSubview:newWebView];
    loadTab(newWebView);
    return newWebView;
}

static NSView *overlayInWebView(WKWebView *webView)
{
    Class overlayClass = NSClassFromString(@"WKTabCapturePickerOverlay");
    NSView *result = nil;
    for (NSView *subview in webView.subviews) {
        if (![subview isKindOfClass:overlayClass])
            continue;
        EXPECT_NULL(result) << "More than one overlay in a web view";
        result = subview;
    }
    return result;
}

static bool hasOverlay(WKWebView *webView)
{
    return !!overlayInWebView(webView);
}

static bool isSelected(NSView *overlay)
{
    // The selected overlay is tinted and has a border.
    return overlay.layer.borderWidth > 0;
}

static void collectSubviews(NSView *view, Class viewClass, NSMutableArray *result)
{
    for (NSView *subview in view.subviews) {
        if ([subview isKindOfClass:viewClass])
            [result addObject:subview];
        collectSubviews(subview, viewClass, result);
    }
}

static RetainPtr<NSArray> subviewsOfClass(NSView *view, Class viewClass)
{
    RetainPtr result = adoptNS([[NSMutableArray alloc] init]);
    collectSubviews(view, viewClass, result);
    return result;
}

static RetainPtr<NSArray<NSString *>> buttonTitles(NSView *overlay)
{
    RetainPtr titles = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr buttons = subviewsOfClass(overlay, NSButton.class);
    for (NSButton *button in buttons.get())
        [titles addObject:button.title];
    return titles;
}

static RetainPtr<NSArray<NSString *>> labelTexts(NSView *overlay)
{
    RetainPtr texts = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr labels = subviewsOfClass(overlay, NSTextField.class);
    for (NSTextField *label in labels.get())
        [texts addObject:label.stringValue];
    return texts;
}

static void clickButton(WKWebView *webView, NSString *title)
{
    RetainPtr buttons = subviewsOfClass(overlayInWebView(webView), NSButton.class);
    for (NSButton *button in buttons.get()) {
        if ([button.title isEqualToString:title]) {
            [button performClick:nil];
            return;
        }
    }
    ADD_FAILURE() << "No button titled " << title.UTF8String;
}

static void share(WKWebView *webView)
{
    clickButton(webView, @"Share This Tab");
}

static void cancel(WKWebView *webView)
{
    clickButton(webView, @"Cancel");
}

static void hover(WKWebView *webView)
{
    NSEvent *syntheticEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:webView.window.windowNumber context:nil eventNumber:0 trackingNumber:0 userData:nil];
    [overlayInWebView(webView) mouseEntered:syntheticEvent];
}

struct PickerResult {
    bool done { false };
    RetainPtr<WKWebView> pickedWebView;
};

static RetainPtr<_WKTabCapturePickerForTesting> showPicker(WKWebView *requestingWebView, PickerResult& result)
{
    PickerResult* resultPointer = &result;
    return [requestingWebView _showTabCapturePickerForTesting:^(WKWebView *pickedWebView) {
        resultPointer->pickedWebView = pickedWebView;
        resultPointer->done = true;
    }];
}

TEST(TabCapturePicker, ShowsAnOverlayInEachWindow)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    ASSERT_NOT_NULL(picker.get());
    EXPECT_FALSE(result.done);

    RetainPtr overlay1 = overlayInWebView(webView1);
    RetainPtr overlay2 = overlayInWebView(webView2);
    ASSERT_NOT_NULL(overlay1.get());
    ASSERT_NOT_NULL(overlay2.get());

    EXPECT_TRUE([labelTexts(overlay1) containsObject:@"Share this tab with “webkit.org”?"]);
    EXPECT_TRUE([buttonTitles(overlay1) isEqualToArray:(@[ @"Share This Tab", @"Cancel" ])]);
    EXPECT_TRUE(NSEqualRects([webView1 bounds], [overlay1 frame]));
    EXPECT_TRUE(NSEqualRects([webView2 bounds], [overlay2 frame]));

    EXPECT_TRUE(isSelected(overlay1));
    EXPECT_FALSE(isSelected(overlay2));

    EXPECT_TRUE([webView1 _ignoresMouseMoveEvents]);
    EXPECT_TRUE([webView2 _ignoresMouseMoveEvents]);

    [picker cancel];
    EXPECT_TRUE(result.done);
    EXPECT_NULL(result.pickedWebView.get());
    EXPECT_FALSE(hasOverlay(webView1));
    EXPECT_FALSE(hasOverlay(webView2));
}

TEST(TabCapturePicker, ShareReturnsTheWebViewOfTheOverlay)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    share(webView2);

    EXPECT_TRUE(result.done);
    EXPECT_EQ(webView2.get(), result.pickedWebView.get());
    EXPECT_FALSE(hasOverlay(webView1));
    EXPECT_FALSE(hasOverlay(webView2));
    EXPECT_FALSE([webView1 _ignoresMouseMoveEvents]);
    EXPECT_FALSE([webView2 _ignoresMouseMoveEvents]);
}

TEST(TabCapturePicker, CancelFromAnOverlayReturnsNoWebView)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    cancel(webView2);

    EXPECT_TRUE(result.done);
    EXPECT_NULL(result.pickedWebView.get());
    EXPECT_FALSE(hasOverlay(webView1));
    EXPECT_FALSE(hasOverlay(webView2));
    EXPECT_FALSE([webView1 _ignoresMouseMoveEvents]);
    EXPECT_FALSE([webView2 _ignoresMouseMoveEvents]);
}

TEST(TabCapturePicker, PickerRestoresIgnoresMouseMoveEvents)
{
    RetainPtr webView = createTab();
    [webView _setIgnoresMouseMoveEvents:YES];

    PickerResult result;
    RetainPtr picker = showPicker(webView, result);
    [picker cancel];

    EXPECT_TRUE([webView _ignoresMouseMoveEvents]);
}

TEST(TabCapturePicker, HoveringAnOverlaySelectsItsWindow)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);

    hover(webView2);
    EXPECT_FALSE(isSelected(overlayInWebView(webView1)));
    EXPECT_TRUE(isSelected(overlayInWebView(webView2)));

    hover(webView1);
    EXPECT_TRUE(isSelected(overlayInWebView(webView1)));
    EXPECT_FALSE(isSelected(overlayInWebView(webView2)));

    [picker cancel];
}

TEST(TabCapturePicker, NewPickerCancelsThePreviousOne)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult firstResult;
    RetainPtr firstPicker = showPicker(webView1, firstResult);

    PickerResult secondResult;
    RetainPtr secondPicker = showPicker(webView2, secondResult);

    EXPECT_TRUE(firstResult.done);
    EXPECT_NULL(firstResult.pickedWebView.get());
    EXPECT_FALSE(secondResult.done);

    EXPECT_FALSE(isSelected(overlayInWebView(webView1)));
    EXPECT_TRUE(isSelected(overlayInWebView(webView2)));

    share(webView1);
    EXPECT_NULL(firstResult.pickedWebView.get());
    EXPECT_EQ(webView1.get(), secondResult.pickedWebView.get());
}

TEST(TabCapturePicker, NewWindowGetsAnOverlay)
{
    RetainPtr webView1 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    EXPECT_TRUE(hasOverlay(webView1));

    RetainPtr webView2 = createTab();
    EXPECT_TRUE(Util::waitFor([&] { return hasOverlay(webView2); }));
    EXPECT_TRUE(NSEqualRects([webView2 bounds], [overlayInWebView(webView2) frame]));
    EXPECT_FALSE(isSelected(overlayInWebView(webView2)));
    EXPECT_TRUE([webView2 _ignoresMouseMoveEvents]);
    EXPECT_TRUE(hasOverlay(webView1));

    [picker cancel];
    EXPECT_FALSE(hasOverlay(webView2));
}

TEST(TabCapturePicker, WindowShownLaterGetsAnOverlay)
{
    RetainPtr webView1 = createTab();

    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:webViewFrame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [window setReleasedWhenClosed:NO];
    RetainPtr webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:webViewFrame configuration:adoptNS([[WKWebViewConfiguration alloc] init]) addToWindow:NO]);
    [[window contentView] addSubview:webView2];
    loadTab(webView2);

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    EXPECT_FALSE(hasOverlay(webView2));

    [window orderFront:nil];
    EXPECT_TRUE(Util::waitFor([&] { return hasOverlay(webView2); }));

    [picker cancel];
    [window close];
}

TEST(TabCapturePicker, ClosingAWindowRemovesItsOverlay)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    EXPECT_TRUE(hasOverlay(webView2));

    [[webView2 hostWindow] close];
    EXPECT_TRUE(Util::waitFor([&] { return !hasOverlay(webView2); }));
    EXPECT_FALSE([webView2 _ignoresMouseMoveEvents]);
    EXPECT_TRUE(hasOverlay(webView1));
    EXPECT_FALSE(result.done);

    [picker cancel];
}

TEST(TabCapturePicker, OverlayFollowsTheVisibleTabOfAWindow)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTabInWindowOf(webView1, YES);

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    EXPECT_TRUE(hasOverlay(webView1));
    EXPECT_FALSE(hasOverlay(webView2));

    // Switch tabs.
    [webView1 setHidden:YES];
    [webView2 setHidden:NO];
    EXPECT_TRUE(Util::waitFor([&] { return hasOverlay(webView2); }));
    EXPECT_FALSE(hasOverlay(webView1));
    EXPECT_FALSE([webView1 _ignoresMouseMoveEvents]);

    EXPECT_TRUE(isSelected(overlayInWebView(webView2)));

    share(webView2);
    EXPECT_EQ(webView2.get(), result.pickedWebView.get());
}

TEST(TabCapturePicker, WindowWithOnlyHiddenTabsHasNoOverlay)
{
    RetainPtr webView1 = createTab();
    RetainPtr webView2 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    EXPECT_TRUE(hasOverlay(webView2));

    [webView2 setHidden:YES];
    EXPECT_TRUE(Util::waitFor([&] { return !hasOverlay(webView2); }));

    [webView2 setHidden:NO];
    EXPECT_TRUE(Util::waitFor([&] { return hasOverlay(webView2); }));

    [picker cancel];
}

TEST(TabCapturePicker, SecondVisibleWebViewDoesNotTakeTheOverlay)
{
    RetainPtr webView1 = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView1, result);
    EXPECT_TRUE(hasOverlay(webView1));

    RetainPtr webView2 = createTabInWindowOf(webView1, NO);

    int count = 0;
    do {
        Util::runFor(0.01_s);
    } while (!hasOverlay(webView1) && ++count < 50);

    EXPECT_TRUE(hasOverlay(webView1));
    EXPECT_FALSE(hasOverlay(webView2));

    share(webView1);
    EXPECT_EQ(webView1.get(), result.pickedWebView.get());
}

TEST(TabCapturePicker, OverlayFrameFollowsObscuredContentInsets)
{
    RetainPtr webView = createTab();

    PickerResult result;
    RetainPtr picker = showPicker(webView, result);
    EXPECT_TRUE(NSEqualRects([webView bounds], [overlayInWebView(webView) frame]));

    [webView _setObscuredContentInsets:NSEdgeInsetsMake(50, 10, 0, 20) immediate:YES];
    EXPECT_TRUE(Util::waitFor([&] {
        NSSize size = [overlayInWebView(webView) frame].size;
        return size.width == webViewFrame.size.width - 30 && size.height == webViewFrame.size.height - 50;
    }));

    [picker cancel];
}

static void waitForClickCount(WKWebView *webView, int value)
{
    size_t count = 0;
    do {
        Util::runFor(0.01_s);
    } while (++count < 50 && [[webView objectByEvaluatingJavaScript:@"clickCount"] intValue] != value);
}

TEST(TabCapturePicker, ClicksDoNotReachThePage)
{
    RetainPtr webView = createTab();

    NSPoint corner = NSMakePoint(50, 50);
    [webView sendClickAtPoint:corner];
    waitForClickCount(webView, 1);
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"clickCount"] intValue]);

    PickerResult result;
    RetainPtr picker = showPicker(webView, result);

    [webView sendClickAtPoint:corner];
    Util::runFor(0.5_s);
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"clickCount"] intValue]);
    EXPECT_FALSE(result.done);

    [picker cancel];

    [webView sendClickAtPoint:corner];
    waitForClickCount(webView, 2);
    EXPECT_EQ(2, [[webView objectByEvaluatingJavaScript:@"clickCount"] intValue]);
    EXPECT_TRUE(result.done);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(MEDIA_STREAM) && HAVE(SCREEN_CAPTURE_KIT)
