/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "CGImagePixelReader.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/Color.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@interface SlowTextInputContext : NSTextInputContext
@property (nonatomic) BlockPtr<void()> handledInputMethodEventBlock;
@end

@implementation SlowTextInputContext

- (void)handleEventByInputMethod:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    [super handleEventByInputMethod:event completionHandler:^(BOOL handled) {
        dispatch_async(mainDispatchQueueSingleton(), ^() {
            completionHandler(handled);
            if (_handledInputMethodEventBlock)
                _handledInputMethodEventBlock();
        });
    }];
}

- (void)handleEvent:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    [super handleEvent:event completionHandler:^(BOOL handled) {
        dispatch_async(mainDispatchQueueSingleton(), ^() {
            completionHandler(handled);
        });
    }];
}

@end

@interface SlowInputWebView : TestWKWebView {
    RetainPtr<SlowTextInputContext> _slowInputContext;
}
@end

@implementation SlowInputWebView

- (NSTextInputContext *)inputContext
{
    return self._web_superInputContext;
}

- (SlowTextInputContext *)_web_superInputContext
{
    if (!_slowInputContext)
        _slowInputContext = adoptNS([[SlowTextInputContext alloc] initWithClient:(id<NSTextInputClient>)self]);
    return _slowInputContext.get();
}

@end

@interface MockTextInputContextAction : NSObject

- (instancetype)initWithMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange;

@property (nonatomic) double delay;
@property (nonatomic, assign) NSString *markedText;
@property (nonatomic) NSRange selectedRange;
@property (nonatomic) NSRange replacementRange;
@end

@implementation MockTextInputContextAction

- (instancetype)initWithMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    if (self = [super init]) {
        _markedText = markedText;
        _selectedRange = selectedRange;
        _replacementRange = replacementRange;
    }
    return self;
}

@end

@interface MockTextInputContext : NSTextInputContext
@property (nonatomic, assign) NSMutableArray<MockTextInputContextAction *> *actions;
@end

@implementation MockTextInputContext

- (void)handleEventByInputMethod:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    [super handleEventByInputMethod:event completionHandler:^(BOOL handled) {
        if (!_actions.count) {
            completionHandler(NO);
            return;
        }
        MockTextInputContextAction *lastItem = _actions.lastObject;
        [_actions removeLastObject];
        double delay = lastItem ? lastItem.delay : 10;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), mainDispatchQueueSingleton(), ^{
            if (lastItem)
                [self.client setMarkedText:lastItem.markedText selectedRange:lastItem.selectedRange replacementRange:lastItem.replacementRange];
            completionHandler(!!lastItem);
        });
    }];
}

@end

@interface TestWebViewWithMockTextInputContext : TestWKWebView {
    RetainPtr<MockTextInputContext> _mockInputContext;
}

- (MockTextInputContext *)mockInputContext;
@end

@implementation TestWebViewWithMockTextInputContext

- (MockTextInputContext *)mockInputContext
{
    return _mockInputContext.get();
}

- (NSTextInputContext *)inputContext
{
    return self._web_superInputContext;
}

- (MockTextInputContext *)_web_superInputContext
{
    if (!_mockInputContext)
        _mockInputContext = adoptNS([[MockTextInputContext alloc] initWithClient:(id<NSTextInputClient>)self]);
    return _mockInputContext.get();
}

@end

@interface WKWebView (MacEditingTests)
- (std::pair<NSRect, NSRange>)_firstRectForCharacterRange:(NSRange)characterRange;
@end

@implementation WKWebView (MacEditingTests)

- (std::pair<NSRect, NSRange>)_firstRectForCharacterRange:(NSRange)characterRange
{
    __block bool done = false;
    __block std::pair<NSRect, NSRange> result;
    [static_cast<id<NSTextInputClient_Async>>(self) firstRectForCharacterRange:characterRange completionHandler:^(NSRect firstRect, NSRange actualRange) {
        result = { firstRect, actualRange };
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (NSRange)_selectedRange
{
    __block bool done = false;
    __block NSRange result;
    [static_cast<id<NSTextInputClient_Async>>(self) selectedRangeWithCompletionHandler:^(NSRange selectedRange) {
        result = selectedRange;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

@end

@interface SetMarkedTextWithNoAttributedStringTestCandidate : NSTextCheckingResult
@end

@implementation SetMarkedTextWithNoAttributedStringTestCandidate {
    RetainPtr<NSString> _string;
    NSRange _range;
}

- (instancetype)initWithReplacementString:(NSString *)string inRange:(NSRange)range
{
    if (self = [super init]) {
        _string = string;
        _range = range;
    }
    return self;
}

- (NSString *)replacementString
{
    return _string.get();
}

- (NSTextCheckingType)resultType
{
    return NSTextCheckingTypeReplacement;
}

- (NSRange)range
{
    return _range;
}

@end

namespace TestWebKitAPI {

TEST(WKWebViewMacEditingTests, DoubleClickDoesNotSelectTrailingSpace)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"double-click-does-not-select-trailing-space"];

    __block bool finishedSelectingText = false;
    [webView performAfterReceivingMessage:@"selected" action:^() {
        finishedSelectingText = true;
    }];
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:2];
    Util::run(&finishedSelectingText);

    NSString *selectedText = [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"];
    EXPECT_STREQ("Hello", selectedText.UTF8String);
}

TEST(WKWebViewMacEditingTests, DoNotCrashWhenCallingTextInputClientMethodsWhileDeallocatingView)
{
    NSString *textContent = @"This test should not cause us to dereference null.";

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<p>%@</p>", textContent]];
    [webView removeFromSuperview];

    __unsafe_unretained id <NSTextInputClient_Async> inputClient = (id <NSTextInputClient_Async>)webView.get();
    [inputClient hasMarkedTextWithCompletionHandler:^(BOOL) {
        [inputClient selectedRangeWithCompletionHandler:^(NSRange) {
            [inputClient markedRangeWithCompletionHandler:^(NSRange) { }];
        }];
    }];

    EXPECT_WK_STREQ(textContent, [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(WKWebViewMacEditingTests, KeyDownFiresBeforeCompositionEvent)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"n" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"ni" selectedRange:NSMakeRange(0, 2) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"\u4F60" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<body contenteditable>Hello world</body>"]];
    [webView stringByEvaluatingJavaScript:@"const target = document.body; const logs = [];"
        "['keydown', 'keyup', 'input', 'compositionstart', 'compositionend', 'compositionupdate']"
        ".forEach((type) => { target.addEventListener(type, (event) => logs.push(event)); }); document.body.focus()"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'n'];
    Util::runFor(1_s);
    [webView typeCharacter:'i'];
    Util::runFor(1_s);
    [webView typeCharacter:' '];
    Util::runFor(1_s);

    EXPECT_STREQ("keydown,compositionstart,compositionupdate,input,keyup,keydown,compositionupdate,input,keyup,keydown,input,input,compositionend,keyup",
        [webView stringByEvaluatingJavaScript:@"logs.map((event) => event.type).join(',')"].UTF8String);
}

TEST(WKWebViewMacEditingTests, DoNotCrashWhenInterpretingKeyEventWhileDeallocatingView)
{
    __block bool isDone = false;

    @autoreleasepool {
        auto webView = adoptNS([[SlowInputWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
        [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<body contenteditable>Hello world</body>"]];
        [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
        [webView waitForNextPresentationUpdate];
        [webView removeFromSuperview];
        [webView typeCharacter:'a'];

        webView.get()._web_superInputContext.handledInputMethodEventBlock = ^() {
            isDone = true;
        };
    }

    Util::run(&isDone);
}

TEST(WKWebViewMacEditingTests, ProcessSwapAfterSettingMarkedText)
{
    [TestProtocol registerWithScheme:@"https"];

    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setProcessSwapsOnNavigation:YES];

    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView<NSTextInputClient, NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView _setEditable:YES];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/simple.html"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView selectAll:nil];
    [webView setMarkedText:@"Simple" selectedRange:NSMakeRange(0, 6) replacementRange:NSMakeRange(0, 6)];
    [webView waitForNextPresentationUpdate];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [webView hasMarkedTextWithCompletionHandler:^(BOOL hasMarkedText) {
        EXPECT_FALSE(hasMarkedText);
        done = true;
    }];
    Util::run(&done);
}

TEST(WKWebViewMacEditingTests, DoNotRenderInlinePredictionsForRegularMarkedText)
{
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:NSMakeRect(0, 0, 200, 100)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<body style='caret-color: transparent;'></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView waitForNextPresentationUpdate];

    auto string = adoptNS([[NSAttributedString alloc] initWithString:@"xie wen sheng" attributes:@{
        NSMarkedClauseSegmentAttributeName: @0,
        NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle),
        NSUnderlineColorAttributeName: NSColor.controlAccentColor
    }]);

    [webView setMarkedText:string.get() selectedRange:NSMakeRange(13, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    [webView waitForNextPresentationUpdate];

    bool foundNonWhitePixel = false;
    CGImagePixelReader reader { [webView snapshotAfterScreenUpdates] };
    for (unsigned x = 0; x < reader.width(); ++x) {
        for (unsigned y = 0; y < reader.height(); ++y) {
            if (reader.at(x, y) != WebCore::Color::white)
                foundNonWhitePixel = true;
        }
    }
    EXPECT_TRUE(foundNonWhitePixel);
}

#if HAVE(INLINE_PREDICTIONS)
TEST(WKWebViewMacEditingTests, InlinePredictionsShouldSuppressAutocorrection)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView<NSTextInputClient, NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView synchronouslyLoadHTMLString:@"<body id='p' contenteditable>Is it &nbsp;</body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _setEditable:YES];
    [webView waitForNextPresentationUpdate];

    NSString *modifySelectionJavascript = @""
    "(() => {"
    "  const node = document.getElementById('p').firstChild;"
    "  const range = document.createRange();"
    "  range.setStart(node, 7);"
    "  range.setEnd(node, 7);"
    "  "
    "  var selection = window.getSelection();"
    "  selection.removeAllRanges();"
    "  selection.addRange(range);"
    "})();";

    [webView stringByEvaluatingJavaScript:modifySelectionJavascript];

    auto typedString = adoptNS([[NSAttributedString alloc] initWithString:@"wedn"]);
    auto predictedString = adoptNS([[NSAttributedString alloc] initWithString:@"esday" attributes:@{
        NSForegroundColorAttributeName : NSColor.grayColor
    }]);

    auto string = adoptNS([[NSMutableAttributedString alloc] init]);
    [string appendAttributedString:typedString.get()];
    [string appendAttributedString:predictedString.get()];

    [webView setMarkedText:string.get() selectedRange:NSMakeRange(4, 0) replacementRange:NSMakeRange(6, 4)];

    NSString *hasSpellingMarker = [webView stringByEvaluatingJavaScript:@"internals.hasSpellingMarker(6, 9) ? 'true' : 'false'"];
    EXPECT_STREQ("false", hasSpellingMarker.UTF8String);
}

TEST(WKWebViewMacEditingTests, SetMarkedTextWithNoAttributedString)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView<NSTextInputClient, NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView synchronouslyLoadHTMLString:@"<body id='p' contenteditable>Is it &nbsp;</body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _setEditable:YES];
    [webView waitForNextPresentationUpdate];

    NSString *modifySelectionJavascript = @""
    "(() => {"
    "  const node = document.getElementById('p').firstChild;"
    "  const range = document.createRange();"
    "  range.setStart(node, 7);"
    "  range.setEnd(node, 7);"
    "  "
    "  var selection = window.getSelection();"
    "  selection.removeAllRanges();"
    "  selection.addRange(range);"
    "})();";

    [webView stringByEvaluatingJavaScript:modifySelectionJavascript];

    [webView _handleAcceptedCandidate:adoptNS([[SetMarkedTextWithNoAttributedStringTestCandidate alloc] initWithReplacementString:@"test" inRange:NSMakeRange(0, 0)]).get()];

    [webView setMarkedText:@"hello" selectedRange:NSMakeRange(4, 0) replacementRange:NSMakeRange(6, 4)];
}
#endif

TEST(WKWebViewMacEditingTests, FirstRectForCharacterRange)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<body id='p' contenteditable>First Line<br>Second Line<br>Third Line<br></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _setEditable:YES];
    [webView waitForNextPresentationUpdate];
    {
        auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:NSMakeRange(0, 31)];
        EXPECT_EQ(8, firstRect.origin.x);
        EXPECT_EQ(574, firstRect.origin.y);
        EXPECT_EQ(62, firstRect.size.width);
        EXPECT_EQ(18, firstRect.size.height);

        EXPECT_EQ(0U, actualRange.location);
        EXPECT_EQ(11U, actualRange.length);
    }
    {
        auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:NSMakeRange(0, 5)];
        EXPECT_EQ(8, firstRect.origin.x);
        EXPECT_EQ(574, firstRect.origin.y);
        EXPECT_EQ(30, firstRect.size.width);
        EXPECT_EQ(18, firstRect.size.height);

        EXPECT_EQ(0U, actualRange.location);
        EXPECT_EQ(5U, actualRange.length);
    }
    {
        auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:NSMakeRange(17, 4)];
        EXPECT_EQ(55, firstRect.origin.x);
        EXPECT_EQ(556, firstRect.origin.y);
        EXPECT_EQ(27, firstRect.size.width);
        EXPECT_EQ(18, firstRect.size.height);

        EXPECT_EQ(17U, actualRange.location);
        EXPECT_EQ(4U, actualRange.length);
    }
}

TEST(WKWebViewMacEditingTests, FirstRectForCharacterRangeInTextArea)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<textarea></textarea>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('textarea').focus()"];
    [webView waitForNextPresentationUpdate];

    auto [rectBeforeTyping, rangeBeforeTyping] = [webView _firstRectForCharacterRange:NSMakeRange(0, 0)];
    EXPECT_GT(rectBeforeTyping.origin.x, 0);
    EXPECT_GT(rectBeforeTyping.origin.y, 0);
    EXPECT_GT(rectBeforeTyping.size.height, 0);
    EXPECT_EQ(rangeBeforeTyping.location, 0U);
    EXPECT_EQ(rangeBeforeTyping.length, 0U);

    [webView insertText:@"a"];
    [webView waitForNextPresentationUpdate];

    auto [rectAfterTyping, rangeAfterTyping] = [webView _firstRectForCharacterRange:NSMakeRange(1, 0)];
    EXPECT_GT(rectAfterTyping.origin.x, rectBeforeTyping.origin.x);
    EXPECT_GT(rectAfterTyping.origin.y, 0);
    EXPECT_GT(rectAfterTyping.size.height, 0);
    EXPECT_EQ(rangeAfterTyping.location, 1U);
    EXPECT_EQ(rangeAfterTyping.length, 0U);
}

TEST(WKWebViewMacEditingTests, FirstRectForCharacterRangeWithNewlinesAndWrapping)
{
    Vector<std::pair<NSRect, NSRange>> expectedRectsAndRanges = {
        { { { 8.f, 574.f }, { 328.f, 18.f } }, { 0, 51 } },
        { { { 8.f, 556.f }, { 719.f, 18.f } }, { 51, 111 } },
        { { { 8.f, 538.f }, { 770.f, 18.f } }, { 162, 122 } },
        { { { 8.f, 520.f }, { 764.f, 18.f } }, { 284, 125 } },
        { { { 8.f, 502.f }, { 232.f, 18.f } }, { 409, 36 } }
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body><div>Lorem ipsum dolor sit amet, consectetur adipiscing</div><div>elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</div></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];

    [webView selectAll:nil];

    NSRange selectedRange = [webView _selectedRange];
    NSRange remainingRange = selectedRange;

    NSUInteger lineCount = 0;
    while (remainingRange.length) {
        auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:remainingRange];

        auto [expectedRect, expectedRange] = expectedRectsAndRanges[lineCount];

        EXPECT_TRUE(NSEqualRects(expectedRect, firstRect));
        EXPECT_TRUE(NSEqualRanges(expectedRange, actualRange));

        remainingRange.location += actualRange.length;
        remainingRange.length -= actualRange.length;

        lineCount++;
    }

    EXPECT_EQ(5U, lineCount);
}

TEST(WKWebViewMacEditingTests, FirstRectForCharacterRangeForPartialLineWithNewlinesAndWrapping)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body><div>Lorem ipsum dolor sit amet, consectetur adipiscing</div><div>elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</div></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];

    [webView selectAll:nil];

    NSRange characterRange = NSMakeRange(175, 31);
    NSRect expectedRect = NSMakeRect(87.f, 538.f, 192.f, 18.f);

    auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:characterRange];
    EXPECT_TRUE(NSEqualRects(expectedRect, firstRect));
    EXPECT_TRUE(NSEqualRanges(characterRange, actualRange));
}

TEST(WKWebViewMacEditingTests, FirstRectForCharacterRangeWithNewlinesAndWrappingLineBreakAfterWhiteSpace)
{
    Vector<std::pair<NSRect, NSRange>> expectedRectsAndRanges = {
        { { { 8.f, 574.f }, { 328.f, 18.f } }, { 0, 51 } },
        { { { 8.f, 556.f }, { 723.f, 18.f } }, { 51, 111 } },
        { { { 8.f, 538.f }, { 774.f, 18.f } }, { 162, 122 } },
        { { { 8.f, 520.f }, { 768.f, 18.f } }, { 284, 125 } },
        // FIXME: <http://webkit.org/b/278181> The size of the rect for the last line is incorrect.
        { { { 8.f, 502.f }, { 768.f, 36.f } }, { 409, 36 } }
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body style='line-break: after-white-space;'><div>Lorem ipsum dolor sit amet, consectetur adipiscing</div><div>elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</div></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];

    [webView selectAll:nil];

    NSRange selectedRange = [webView _selectedRange];
    NSRange remainingRange = selectedRange;

    NSUInteger lineCount = 0;
    while (remainingRange.length) {
        auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:remainingRange];

        auto [expectedRect, expectedRange] = expectedRectsAndRanges[lineCount];

        EXPECT_TRUE(NSEqualRects(expectedRect, firstRect));
        EXPECT_TRUE(NSEqualRanges(expectedRange, actualRange));

        remainingRange.location += actualRange.length;
        remainingRange.length -= actualRange.length;

        lineCount++;
    }

    EXPECT_EQ(5U, lineCount);
}

TEST(WKWebViewMacEditingTests, FirstRectForCharacterRangeForPartialLineWithNewlinesAndWrappingLineBreakAfterWhiteSpace)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body style='line-break: after-white-space;'><div>Lorem ipsum dolor sit amet, consectetur adipiscing</div><div>elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</div></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];

    [webView selectAll:nil];

    NSRange characterRange = NSMakeRange(175, 31);
    NSRect expectedRect = NSMakeRect(87.f, 538.f, 192.f, 18.f);

    auto [firstRect, actualRange] = [webView _firstRectForCharacterRange:characterRange];
    EXPECT_TRUE(NSEqualRects(expectedRect, firstRect));
    EXPECT_TRUE(NSEqualRanges(characterRange, actualRange));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
