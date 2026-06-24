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

#import "Helpers/mac/AppKitSPI.h"
#import "Helpers/cocoa/CGImagePixelReader.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestProtocol.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
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
- (instancetype)initWithInsertText:(NSString *)insertedText replacementRange:(NSRange)replacementRange;

@property (nonatomic) double delay;
@property (nonatomic, assign) NSString *markedText;
@property (nonatomic, assign) NSString *insertedText;
@property (nonatomic) NSRange selectedRange;
@property (nonatomic) NSRange replacementRange;
// If pollSelectedRangeAfterAction is YES, the mock asks the input client for selectedRange
// after performing this action but before returning from handleEventByInputMethod's completion.
// The result is appended to MockTextInputContext.polledSelectedRanges. This exercises the
// selectedRange staging in WebViewImpl while the queued insertText: is still in the queue.
@property (nonatomic) BOOL pollSelectedRangeAfterAction;
// Same idea for attributedSubstring. attributedSubstringPollRange.location == NSNotFound
// disables the poll. The returned plain text is appended to polledAttributedSubstrings.
@property (nonatomic) NSRange attributedSubstringPollRange;
// Controls the handled value returned by the completion handler. Defaults to YES (IM consumed
// the key). Set to NO to simulate a commit-only action where the IM fires insertText: as a
// side effect of committing a prior composition but does not consume the key itself
// (e.g. Korean 2-Set pressing space to commit a syllable).
@property (nonatomic) BOOL handledByIM;
@end

@implementation MockTextInputContextAction

- (instancetype)initWithMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    if (self = [super init]) {
        _markedText = markedText;
        _selectedRange = selectedRange;
        _replacementRange = replacementRange;
        _attributedSubstringPollRange = NSMakeRange(NSNotFound, 0);
        _handledByIM = YES;
    }
    return self;
}

- (instancetype)initWithInsertText:(NSString *)insertedText replacementRange:(NSRange)replacementRange
{
    if (self = [super init]) {
        _insertedText = insertedText;
        _replacementRange = replacementRange;
        _attributedSubstringPollRange = NSMakeRange(NSNotFound, 0);
        _handledByIM = YES;
    }
    return self;
}

@end

@interface MockTextInputContext : NSTextInputContext
@property (nonatomic, assign) NSMutableArray<MockTextInputContextAction *> *actions;
@property (nonatomic, assign) NSMutableArray<MockTextInputContextAction *> *layoutActions;
@property (nonatomic, assign) NSMutableArray<NSString *> *eventLog;
@property (nonatomic, assign) NSMutableArray<NSValue *> *polledSelectedRanges;
@property (nonatomic, assign) NSMutableArray<NSString *> *polledAttributedSubstrings;
@end

@implementation MockTextInputContext

- (void)handleEventByInputMethod:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    [super handleEventByInputMethod:event completionHandler:^(BOOL handled) {
        if (!_actions.count || event.type != NSEventTypeKeyDown) {
            completionHandler(NO);
            return;
        }
        MockTextInputContextAction *lastItem = _actions.firstObject;
        [_actions removeObjectAtIndex:0];
        NSString *label = lastItem.markedText ?: lastItem.insertedText;
        [_eventLog addObject:[NSString stringWithFormat:@"received_%@", label]];
        double delay = lastItem ? lastItem.delay : 10;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), mainDispatchQueueSingleton(), ^{
            [_eventLog addObject:[NSString stringWithFormat:@"fired_%@", label]];
            if (lastItem.markedText)
                [self.client setMarkedText:lastItem.markedText selectedRange:lastItem.selectedRange replacementRange:lastItem.replacementRange];
            else
                [self.client insertText:lastItem.insertedText replacementRange:lastItem.replacementRange];
            // Polls run while the IM is still inside its callback — the queued setMarkedText:/insertText:
            // hasn't reached the web process yet, so they exercise the staging paths in
            // selectedRangeWithCompletionHandler / attributedSubstringForProposedRange.
            if (lastItem.pollSelectedRangeAfterAction) {
                [(id<NSTextInputClient_Async>)self.client selectedRangeWithCompletionHandler:^(NSRange selectedRange) {
                    [_polledSelectedRanges addObject:[NSValue valueWithRange:selectedRange]];
                }];
            }
            if (lastItem.attributedSubstringPollRange.location != NSNotFound) {
                [(id<NSTextInputClient_Async>)self.client attributedSubstringForProposedRange:lastItem.attributedSubstringPollRange completionHandler:^(NSAttributedString *string, NSRange) {
                    [_polledAttributedSubstrings addObject:string.string ?: @""];
                }];
            }
            completionHandler(lastItem.handledByIM);
        });
    }];
}

- (BOOL)handleEventByKeyboardLayout:(NSEvent *)event
{
    // Tests that exercise the keyboard-layout pass (e.g. the Hindi InScript suffix case)
    // queue layout actions here. Tests that don't set layoutActions fall through to super,
    // preserving the existing behavior where the layout pass produces commands that get
    // discarded by interpretKeyEvent's append gate.
    if (!_layoutActions.count)
        return [super handleEventByKeyboardLayout:event];
    MockTextInputContextAction *layoutItem = _layoutActions.firstObject;
    [_layoutActions removeObjectAtIndex:0];
    if (layoutItem.insertedText)
        [self.client insertText:layoutItem.insertedText replacementRange:layoutItem.replacementRange];
    return YES;
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

// Borderless windows return canBecomeKeyWindow=NO by default, which makes
// [NSApp sendEvent:] silently drop the keydown re-send that
// WebViewImpl::doneWithKeyEvent issues on eventWasHandled=false. This subclass
// (a) forces canBecomeKeyWindow=YES so re-sent keydowns reach the firstResponder,
// and (b) counts -noResponderFor:keyDown:: when an unhandled keydown propagates
// up the responder chain past the webView and bottoms out at the window, AppKit's
// default -noResponderFor: would call NSBeep — our override suppresses the beep
// and counts so tests can assert the IPC reply correctly reports handled.
@interface KeyableBorderlessWindow : NSWindow
@property (nonatomic, readonly) NSUInteger unhandledKeyDownCount;
@end

@implementation KeyableBorderlessWindow {
    NSUInteger _unhandledKeyDownCount;
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (NSUInteger)unhandledKeyDownCount
{
    return _unhandledKeyDownCount;
}

- (void)noResponderFor:(SEL)selector
{
    if (selector == @selector(keyDown:))
        ++_unhandledKeyDownCount;
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
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u4F60" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
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
    [webView typeCharacter:'\r'];
    Util::runFor(1_s);

    EXPECT_STREQ("keydown,compositionstart,compositionupdate,input,keyup,keydown,compositionupdate,input,keyup,keydown,compositionupdate,input,keyup,keydown,input,input,compositionend,keyup",
        [webView stringByEvaluatingJavaScript:@"logs.map((event) => event.type).join(',')"].UTF8String);
}

TEST(WKWebViewMacEditingTests, KeyDownInsertAccentedCharacterOnce)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"\u00B4" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u00E9" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<body contenteditable></body>"]];
    [webView stringByEvaluatingJavaScript:@"const target = document.body; const logs = [];"
        "['keydown', 'keyup', 'compositionstart', 'compositionend', 'compositionupdate']"
        ".forEach((type) => { target.addEventListener(type, (event) => logs.push(event)); }); document.body.focus()"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'e' modifiers:NSEventModifierFlagOption];
    Util::runFor(1_s);
    [webView typeCharacter:'e'];
    Util::runFor(1_s);

    EXPECT_STREQ(@"\u00E9".UTF8String, [webView stringByEvaluatingJavaScript:@"document.body.textContent"].UTF8String);

    EXPECT_STREQ("keydown,compositionstart,compositionupdate,keyup,keydown,compositionend,keyup",
        [webView stringByEvaluatingJavaScript:@"logs.map((event) => event.type).join(',')"].UTF8String);
}

TEST(WKWebViewMacEditingTests, KeyDownForInputMethodCommitUsesCompositionKeyCode)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"\u3093" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u3093" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];
    [webView synchronouslyLoadHTMLString:@"<input type='text' id='q'>"];
    // When the input method commits a composition via insertText:, keydown
    // must fire with CompositionEventKeyCode (229), not the real key's
    // windowsVirtualKeyCode. Otherwise:
    //  - Sites like google.com submit search on keydown when they see
    //    event.keyCode === 13, treating the Enter-to-commit as a real Enter.
    //  - The key event would be reported as unhandled, re-sent to the
    //    responder chain, and trigger an unhandled-keyDown: alert sound.
    [webView stringByEvaluatingJavaScript:@"window.keyCodes = [];"
        "document.addEventListener('keydown', (event) => window.keyCodes.push(event.keyCode), true);"
        "document.getElementById('q').focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'n'];
    Util::runFor(1_s);
    [webView typeCharacter:'\r'];
    Util::runFor(1_s);

    EXPECT_STREQ(@"\u3093".UTF8String, [webView stringByEvaluatingJavaScript:@"document.getElementById('q').value"].UTF8String);
    EXPECT_STREQ("229,229", [webView stringByEvaluatingJavaScript:@"window.keyCodes.join(',')"].UTF8String);
}

TEST(WKWebViewMacEditingTests, KeyDownForModelessInputMethodInsertUsesRealKeyCode)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    // Modeless IME: calls insertText: directly (no setMarkedText:) with no prior composition.
    // This is Vietnamese Simple Telex / Korean Hangul typing a character that commits inline.
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"v" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];
    [webView synchronouslyLoadHTMLString:@"<input type='text' id='q'>"];
    // The keydown should report the real keyCode (not 229), a keypress should fire, and the
    // text insertion should happen during the Char phase with underlyingEvent=keypress. Without
    // the modeless routing, the keydown would be reported as handled-by-input-method, keyCode
    // would be 229, no keypress would fire, and the Char-phase TextInput event would trip the
    // underlyingEvent=keydown assertion in EventHandler::handleTextInputEvent.
    [webView stringByEvaluatingJavaScript:@"window.events = [];"
        "const q = document.getElementById('q');"
        "q.addEventListener('keydown', (event) => window.events.push('keydown:' + event.keyCode), true);"
        "q.addEventListener('keypress', (event) => window.events.push('keypress:' + event.keyCode), true);"
        "q.focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'v'];
    Util::runFor(1_s);

    EXPECT_STREQ("v", [webView stringByEvaluatingJavaScript:@"document.getElementById('q').value"].UTF8String);
    // keydown reports 'V' (86); keypress reports 'v' (118).
    EXPECT_STREQ("keydown:86,keypress:118", [webView stringByEvaluatingJavaScript:@"window.events.join(',')"].UTF8String);
}

TEST(WKWebViewMacEditingTests, ModelessInputMethodInsertTextWithReplacementRangeInsertsCorrectly)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    // Modeless IME continuation: first keystroke inserts 'v', second keystroke extends the
    // syllable by inserting "vi" with replacementRange=(0,1) to replace the prior 'v'. The
    // replacement-range path must not trip the queue's assertion and must end up with "vi".
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"v" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"vi" replacementRange:NSMakeRange(0, 1)] autorelease],
    ].mutableCopy autorelease];
    [webView synchronouslyLoadHTMLString:@"<input type='text' id='q'>"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById('q').focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'v'];
    Util::runFor(1_s);
    [webView typeCharacter:'i'];
    Util::runFor(1_s);

    EXPECT_STREQ("vi", [webView stringByEvaluatingJavaScript:@"document.getElementById('q').value"].UTF8String);
}

TEST(WKWebViewMacEditingTests, ModelessInputMethodEventOrderingMatchesNormalTyping)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    // Two modeless keystrokes: 'v' as a plain insertText:, then 'i' as a replacement-range
    // insertText: that extends the syllable to "vi" (mirroring Vietnamese Simple Telex). Both
    // should produce the same JS event ordering as plain typing — keydown, keypress, then input
    // from the queued execution during the keydown default handler in the web process. No
    // composition events should fire (no setMarkedText: was called). 297270@main's keydown-first
    // ordering must hold for both the plain and replacement-range insertText: paths.
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"v" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"vi" replacementRange:NSMakeRange(0, 1)] autorelease],
    ].mutableCopy autorelease];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView stringByEvaluatingJavaScript:@"window.events = [];"
        "['keydown', 'keypress', 'keyup', 'input', 'compositionstart', 'compositionupdate', 'compositionend']"
        ".forEach((type) => { document.body.addEventListener(type, (event) => window.events.push(type), true); });"
        "document.body.focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'v'];
    Util::runFor(1_s);
    [webView typeCharacter:'i'];
    Util::runFor(1_s);

    EXPECT_STREQ("vi", [webView stringByEvaluatingJavaScript:@"document.body.textContent"].UTF8String);
    // Per keystroke: keydown -> keypress -> input -> keyup. No composition events for modeless.
    EXPECT_STREQ("keydown,keypress,input,keyup,keydown,keypress,input,keyup",
        [webView stringByEvaluatingJavaScript:@"window.events.join(',')"].UTF8String);
}

TEST(WKWebViewMacEditingTests, HindiInScriptViramaConjunctEmitsSuffixFromKeyboardLayoutPass)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    // Hindi InScript "kd/e" types DEVANAGARI KA + VIRAMA + YA + VOWEL SIGN AA — the
    // syllable "kya" — as U+0915 U+094D U+092F U+093E. Each keystroke maps as:
    //   k → U+0915 (KA)        — modeless commit
    //   d → U+094D (VIRAMA)    — IM marks for composition
    //   / → U+092F (YA) layout output, but the IM uses this keystroke to commit the
    //         marked virama via insertText:U+094D — the NSEvent's characters string
    //         is U+094D U+092F (the marked prefix + the new layout char), so the IM
    //         has covered only a 1-char prefix of a 2-char keystroke. The trailing
    //         U+092F must come from the keyboard-layout pass appended after the
    //         partial-prefix commit.
    //   e → U+093E (VOWEL SIGN AA) — modeless commit
    //
    // Without the partial-prefix detection in interpretKeyEvent the layout pass is
    // dropped (because the IM did insertText:), so U+092F is lost and the body ends
    // up as U+0915 U+094D U+093E instead of U+0915 U+094D U+092F U+093E.
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u0915" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"\u094D" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u094D" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u093E" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];
    // collectKeyboardLayoutCommandsForEvent runs whenever the IM completion ends with
    // handled=NO — that's the modeless 'k' and 'e' commits and the partial-prefix '/'.
    // The 'd' marked-text keystroke early-returns with handled=YES and never asks the
    // mock for a layout action.
    [webView _web_superInputContext].layoutActions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u0915" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u092F" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u093E" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];

    [webView sendKey:@"\u0915" code:0x28 isDown:YES modifiers:0];
    [webView sendKey:@"\u0915" code:0x28 isDown:NO modifiers:0];
    Util::runFor(1_s);
    [webView sendKey:@"\u094D" code:0x02 isDown:YES modifiers:0];
    [webView sendKey:@"\u094D" code:0x02 isDown:NO modifiers:0];
    Util::runFor(1_s);
    [webView sendKey:@"\u094D\u092F" code:0x2C isDown:YES modifiers:0];
    [webView sendKey:@"\u094D\u092F" code:0x2C isDown:NO modifiers:0];
    Util::runFor(1_s);
    [webView sendKey:@"\u093E" code:0x0E isDown:YES modifiers:0];
    [webView sendKey:@"\u093E" code:0x0E isDown:NO modifiers:0];
    Util::runFor(1_s);

    EXPECT_STREQ(@"\u0915\u094D\u092F\u093E".UTF8String, [webView stringByEvaluatingJavaScript:@"document.body.textContent"].UTF8String);
}

TEST(WKWebViewMacEditingTests, HindiInScriptViramaConjunctReportsKeystrokeHandled)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView removeFromSuperview];
    RetainPtr window = adoptNS([[KeyableBorderlessWindow alloc] initWithContentRect:NSMakeRect(100, 100, 800, 600) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:webView.get()];

    // The Hindi InScript "/" keystroke goes through the partial-prefix path: the input method commits
    // the marked virama via insertText:"\u094D" and the layout pass appends insertText:"\u092F".
    // We need to prevent NSEvent from bubbling up to [NSApp sendEvent:] which beeps.
    [webView _web_superInputContext].actions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u0915" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithMarkedText:@"\u094D" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u094D" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u093E" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];
    [webView _web_superInputContext].layoutActions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u0915" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u092F" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"\u093E" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView waitForNextPresentationUpdate];

    [webView sendKey:@"\u0915" code:0x28 isDown:YES modifiers:0];
    [webView sendKey:@"\u0915" code:0x28 isDown:NO modifiers:0];
    Util::runFor(1_s);
    [webView sendKey:@"\u094D" code:0x02 isDown:YES modifiers:0];
    [webView sendKey:@"\u094D" code:0x02 isDown:NO modifiers:0];
    Util::runFor(1_s);
    [webView sendKey:@"\u094D\u092F" code:0x2C isDown:YES modifiers:0];
    [webView sendKey:@"\u094D\u092F" code:0x2C isDown:NO modifiers:0];
    Util::runFor(1_s);
    [webView sendKey:@"\u093E" code:0x0E isDown:YES modifiers:0];
    [webView sendKey:@"\u093E" code:0x0E isDown:NO modifiers:0];
    Util::runFor(1_s);

    EXPECT_STREQ(@"\u0915\u094D\u092F\u093E".UTF8String, [webView stringByEvaluatingJavaScript:@"document.body.textContent"].UTF8String);
    EXPECT_EQ(0u, [window unhandledKeyDownCount]);
}

TEST(WKWebViewMacEditingTests, ModelessInputMethodStagingReportsPostKeystrokeCursorAndContent)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    // Vietnamese Simple Telex modeless extension: the IM commits 'v' first, then on the
    // 'i' keystroke commits "vi" via insertText:"vi" replacementRange:(0,1) \u2014 replacing
    // the prior 'v' with the syllable "vi". The IM polls selectedRange and
    // attributedSubstring during its handleEventByInputMethod: callback to verify the
    // commit landed; both polls happen while the queued command is still in
    // m_collectedKeypressCommands and exercise the staging paths in WebViewImpl.
    //
    // Cursor staging: text.length() alone is wrong for replacementRange:(0,1) \u2014 the net
    // advance from the pre-keystroke cursor (1) is text.length() \u2212 replacementRange.length
    // = 1, so the IM-visible cursor must be 2, not 3. Without the subtraction, the IM
    // caches an inflated cursor, the next keystroke's setMarkedText: addresses past the
    // document end, and the IM abandons modeless mode for the rest of the session.
    //
    // Content staging: the IM's attributedSubstring poll must reflect the document
    // post-execution ("v" then "vi"), not the stale pre-keystroke content the web
    // process still has at poll time. Without staging, the second poll returns "v"
    // (length 1) instead of "vi" (length 2) \u2014 the IM concludes its commit didn't take
    // effect and falls back to setMarkedText: for the rest of the session.
    RetainPtr action1 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"v" replacementRange:NSMakeRange(NSNotFound, 0)]);
    [action1 setPollSelectedRangeAfterAction:YES];
    [action1 setAttributedSubstringPollRange:NSMakeRange(0, 1)];
    RetainPtr action2 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"vi" replacementRange:NSMakeRange(0, 1)]);
    [action2 setPollSelectedRangeAfterAction:YES];
    [action2 setAttributedSubstringPollRange:NSMakeRange(0, 2)];
    [webView _web_superInputContext].actions = [@[action1.get(), action2.get()].mutableCopy autorelease];
    [webView _web_superInputContext].polledSelectedRanges = [NSMutableArray array];
    [webView _web_superInputContext].polledAttributedSubstrings = [NSMutableArray array];

    [webView synchronouslyLoadHTMLString:@"<input type='text' id='q'>"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById('q').focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'v'];
    Util::runFor(1_s);
    [webView typeCharacter:'i'];
    Util::runFor(1_s);

    EXPECT_STREQ("vi", [webView stringByEvaluatingJavaScript:@"document.getElementById('q').value"].UTF8String);

    NSArray<NSValue *> *ranges = [webView _web_superInputContext].polledSelectedRanges;
    EXPECT_EQ(2u, ranges.count);
    if (ranges.count >= 1) {
        EXPECT_EQ(1u, [ranges[0] rangeValue].location);
        EXPECT_EQ(0u, [ranges[0] rangeValue].length);
    }
    if (ranges.count >= 2) {
        // Without the replacementRange.length subtraction, this would be 3 \u2014 the bug
        // that drove Vietnamese out of modeless mode on the third keystroke.
        EXPECT_EQ(2u, [ranges[1] rangeValue].location);
        EXPECT_EQ(0u, [ranges[1] rangeValue].length);
    }

    NSArray<NSString *> *substrings = [webView _web_superInputContext].polledAttributedSubstrings;
    EXPECT_EQ(2u, substrings.count);
    if (substrings.count >= 1)
        EXPECT_TRUE([substrings[0] isEqualToString:@"v"]);
    if (substrings.count >= 2) {
        // Without attributedSubstring staging, this would be "v" \u2014 the second cause of
        // Vietnamese falling out of modeless mode (IM concludes its insert didn't land).
        EXPECT_TRUE([substrings[1] isEqualToString:@"vi"]);
    }
}

TEST(WKWebViewMacEditingTests, ModelessInputMethodStagingToleratesExternalSetMarkedText)
{
    // Simulates the 3-keystroke Korean 2-Set (\ub450\ubc8c\uc2dd) sequence that inserts "\uc15b" (\u3145+\u3155+\u3137):
    //   't' \u2192 insertText:"\u3145"
    //   'u' \u2192 insertText:"\uc154" replacementRange:(0,1)      [replaces \u3145 with \u3145+\u3155=\uc154]
    //   'e' \u2192 setMarkedText:"\uc15b" selectedRange:{0,1} replacementRange:{0,1}
    //         (simulates an inline prediction or Touch Bar setMarkedText firing while the Korean
    //          IME's handleEventByInputMethod: XPC is in flight)
    //
    // The poll after 'e''s setMarkedText exercises the bug path. Because no composition exists in
    // the web process yet (modeless insertText was used for the first two keys), compositionRange.location
    // is notFound (SIZE_MAX). The original code computed:
    //   compositionRange.location + stagedSelectedRange.location = SIZE_MAX + 0 \u2192 overflow \u2192 NSNotFound
    // The Korean 2-Set IME interprets NSNotFound as "can't determine cursor" and abandons modeless
    // mode; the sequence then produces "\uc154\u3137" instead of "\uc15b" in Mail (where _markedTextInputEnabled=YES
    // enables inline predictions that trigger the spurious setMarkedText).
    //
    // Fix: when compositionRange.location is notFound, fall through to the unstaged cursor
    // (editingRangeResult), preventing the overflow and returning the correct cursor position (1).
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    // Key 1 ('t' \u2192 \u3145): insertText:"\u3145" with no replacement.
    RetainPtr action1 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"\u3145" replacementRange:NSMakeRange(NSNotFound, 0)]);

    // Key 2 ('u' \u2192 \uc154): insertText:"\uc154" replacing the committed "\u3145" at position (0,1).
    // pollSelectedRange verifies the staged cursor advance is correct (1, not 2).
    RetainPtr action2 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"\uc154" replacementRange:NSMakeRange(0, 1)]);
    [action2 setPollSelectedRangeAfterAction:YES];

    // Key 3 ('e'): an external setMarkedText fires (simulating inline predictions landing while
    // m_collectedKeypressCommands is non-empty for this keydown). pollSelectedRange fires after
    // the setMarkedText is queued. At that point compositionRange.location is notFound because the
    // web process still has no composition \u2014 only committed "\uc154" from the modeless insertText.
    // The fix returns editingRangeResult (1), NOT SIZE_MAX + 0 (overflow).
    RetainPtr action3 = adoptNS([[MockTextInputContextAction alloc] initWithMarkedText:@"\uc15b" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(0, 1)]);
    [action3 setPollSelectedRangeAfterAction:YES];

    [webView _web_superInputContext].actions = [@[action1.get(), action2.get(), action3.get()].mutableCopy autorelease];
    [webView _web_superInputContext].polledSelectedRanges = [NSMutableArray array];

    [webView synchronouslyLoadHTMLString:@"<input type='text' id='q'>"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById('q').focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'t'];
    Util::runFor(1_s);
    [webView typeCharacter:'u'];
    Util::runFor(1_s);
    [webView typeCharacter:'e'];
    Util::runFor(1_s);

    NSArray<NSValue *> *ranges = [webView _web_superInputContext].polledSelectedRanges;
    EXPECT_EQ(2u, ranges.count);
    if (ranges.count >= 1) {
        EXPECT_EQ(1u, [ranges[0] rangeValue].location);
        EXPECT_EQ(0u, [ranges[0] rangeValue].length);
    }
    if (ranges.count >= 2) {
        // Without the overflow fix: compositionRange.location (SIZE_MAX) + stagedSelectedRange.location (0)
        // = SIZE_MAX, which is NSNotFound \u2014 the Korean IME interprets this as "unknown cursor" and
        // abandons modeless mode. With the fix, the code falls through to editingRangeResult = 1.
        EXPECT_EQ(1u, [ranges[1] rangeValue].location);
        EXPECT_EQ(0u, [ranges[1] rangeValue].length);
    }
}

TEST(WKWebViewMacEditingTests, ModelessInputMethodCommitOnDelimiterInsertsCommittedTextAndDelimiter)
{
    // Simulates the Korean 2-Set (두벌식) behavior when a delimiter key (space) is pressed
    // during a modeless composition. The IM fires insertText: to commit the in-progress
    // syllable but returns handled=NO — the space was not consumed, just triggered the commit.
    // WebKit must run the keyboard-layout pass for the space key and append its insertText:" "
    // so both the committed Korean syllable and the space end up in the document.
    //
    // Sequence:
    //   't' → insertText:"ㅅ"                    (IM starts composition, returns handled=YES)
    //   'm' → insertText:"스" rep=(0,1)           (IM extends, returns handled=YES)
    //  ' ' → insertText:"스" rep=(0,1)            (IM commits same syllable, returns handled=NO)
    //       + keyboard layout → insertText:" "   (space must be appended)
    //
    // Without the fix, the handled=NO path still had hasInsertText=true (because the commit
    // insertText: was queued), which blocked the layout pass — only "스" was inserted, not "스 ".
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    // 't': IM inserts "ㅅ", returns handled=YES (composing key).
    RetainPtr action1 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"ㅅ" replacementRange:NSMakeRange(NSNotFound, 0)]);

    // 'm': IM extends to "스", returns handled=YES (composing key).
    RetainPtr action2 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"스" replacementRange:NSMakeRange(0, 1)]);

    // ' ': IM commits "스" (replacing the in-progress char), returns handled=NO — the space
    // itself was not consumed by the IM. The keyboard layout must insert the space.
    RetainPtr action3 = adoptNS([[MockTextInputContextAction alloc] initWithInsertText:@"스" replacementRange:NSMakeRange(0, 1)]);
    [action3 setHandledByIM:NO];

    [webView _web_superInputContext].actions = [@[action1.get(), action2.get(), action3.get()].mutableCopy autorelease];

    // collectKeyboardLayoutCommandsForEvent is called for every key whose completion ends with
    // handled=NO — that includes the forced-to-NO composing keys ('t', 'm') as well as the
    // naturally-NO commit key (space). The layout action is consumed for each even though the
    // composing-key results are discarded (imHandled=YES suppresses appending). Three layout
    // actions are needed: two no-op Korean chars for 't'/'m', then the space for ' '.
    [webView _web_superInputContext].layoutActions = [@[
        [[[MockTextInputContextAction alloc] initWithInsertText:@"ㅅ" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@"ㅡ" replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
        [[[MockTextInputContextAction alloc] initWithInsertText:@" " replacementRange:NSMakeRange(NSNotFound, 0)] autorelease],
    ].mutableCopy autorelease];

    [webView synchronouslyLoadHTMLString:@"<input type='text' id='q'>"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById('q').focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];
    [webView typeCharacter:'t'];
    Util::runFor(1_s);
    [webView typeCharacter:'m'];
    Util::runFor(1_s);
    [webView typeCharacter:' '];
    Util::runFor(1_s);

    // "스 " — Korean syllable followed by a space. Without the fix only "스" is inserted.
    EXPECT_STREQ("스 ", [webView stringByEvaluatingJavaScript:@"document.getElementById('q').value"].UTF8String);
}

TEST(WKWebViewMacEditingTests, ConcurrentInputMethodKeyDownsScopeQueueingPerKey)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"InputMethodUsesCorrectKeyEventOrder"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWebViewWithMockTextInputContext alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    // Two keydowns dispatched back-to-back. The mock's 0.1s delay on each keydown action
    // means both keydowns must reach the IM (super.handleEventByInputMethod:) before
    // either action fires. With the rdar://177042301 fix (Zhuyin Traditional stall),
    // keydowns flow through to the IM directly and each gets its own deque entry, so
    // both are claimed before action 1 runs. Without the fix, the second keydown is
    // held in WebKit until the first keydown's IM completion fires (after action 1
    // runs), serializing the events.
    RetainPtr action1 = adoptNS([[MockTextInputContextAction alloc] initWithMarkedText:@"a" selectedRange:NSMakeRange(0, 1) replacementRange:NSMakeRange(NSNotFound, 0)]);
    [action1 setDelay:0.1];
    RetainPtr action2 = adoptNS([[MockTextInputContextAction alloc] initWithMarkedText:@"ab" selectedRange:NSMakeRange(0, 2) replacementRange:NSMakeRange(NSNotFound, 0)]);
    [action2 setDelay:0.1];
    [webView _web_superInputContext].actions = [@[action1.get(), action2.get()].mutableCopy autorelease];
    [webView _web_superInputContext].eventLog = [NSMutableArray array];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus();"];
    [webView waitForNextPresentationUpdate];
    [webView removeFromSuperview];

    [webView typeCharacter:'a'];
    [webView typeCharacter:'b'];
    Util::runFor(2_s);

    EXPECT_STREQ("ab", [webView stringByEvaluatingJavaScript:@"document.body.textContent"].UTF8String);
    // The mock log records "received_<text>" when super.handleEventByInputMethod: completes
    // for a keydown (the IM has been told about it) and "fired_<text>" when the action's
    // dispatch_after block runs (the IM responds with setMarkedText:). With per-key queue
    // scoping, both keydowns are received before either action fires.
    EXPECT_STREQ("received_a,received_ab,fired_a,fired_ab",
        [[[webView _web_superInputContext].eventLog componentsJoinedByString:@","] UTF8String]);
}

TEST(WKWebViewMacEditingTests, DoNotCrashWhenInterpretingKeyEventWhileDeallocatingView)
{
    __block bool isDone = false;

    @autoreleasepool {
        RetainPtr webView = adoptNS([[SlowInputWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
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

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setProcessSwapsOnNavigation:YES];

    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient, NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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

    RetainPtr string = adoptNS([[NSAttributedString alloc] initWithString:@"xie wen sheng" attributes:@{
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

TEST(WKWebViewMacEditingTests, InlinePredictionsShouldSuppressAutocorrection)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient, NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
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

    RetainPtr typedString = adoptNS([[NSAttributedString alloc] initWithString:@"wedn"]);
    RetainPtr predictedString = adoptNS([[NSAttributedString alloc] initWithString:@"esday" attributes:@{
        NSForegroundColorAttributeName : NSColor.grayColor
    }]);

    RetainPtr string = adoptNS([[NSMutableAttributedString alloc] init]);
    [string appendAttributedString:typedString.get()];
    [string appendAttributedString:predictedString.get()];

    [webView setMarkedText:string.get() selectedRange:NSMakeRange(4, 0) replacementRange:NSMakeRange(6, 4)];

    NSString *hasSpellingMarker = [webView stringByEvaluatingJavaScript:@"internals.hasSpellingMarker(6, 9) ? 'true' : 'false'"];
    EXPECT_STREQ("false", hasSpellingMarker.UTF8String);
}

TEST(WKWebViewMacEditingTests, SetMarkedTextWithNoAttributedString)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient, NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
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
        { { { 8.f, 502.f }, { 232.f, 18.f } }, { 409, 36 } }
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

// Mimics the prosemirror.net Japanese-IME bug: when an IME auto-commits a portion of a
// marked composition and immediately starts a new composition, ProseMirror's
// compositionstart handler rebuilds the affected DOM (wrapping the just-confirmed text
// in a span descriptor) and then sets the DOM Selection to a parent-anchored offset
// (e.g. <P>:1) via setBaseAndExtent. TypingCommand::insertText then runs to install the
// new marked text. Without flushing layout in Editor::setComposition between the
// JS-driven mutation and TypingCommand::insertText, Position::upstream() can fail to
// find a text-node candidate in the freshly-mutated tree and the marked text gets
// inserted at offset 0 of the wrong sibling text node - landing BEFORE the auto-
// confirmed text instead of after it.
TEST(WKWebViewMacEditingTests, JapaneseAutoCommitWithDOMRebuildInCompositionStartLandsTextAtCursor)
{
    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:NSMakeRect(0, 0, 400, 200)]);
    [webView synchronouslyLoadHTMLString:@"<div id='editor' contenteditable style='min-height: 50px'><p id='p'><br></p></div>"];
    [webView stringByEvaluatingJavaScript:@""
        "const editor = document.getElementById('editor');"
        "const p = document.getElementById('p');"
        "editor.focus();"
        "const selection = window.getSelection();"
        "const range = document.createRange();"
        "range.setStart(p, 0); range.setEnd(p, 0);"
        "selection.removeAllRanges(); selection.addRange(range);"];
    [webView waitForNextPresentationUpdate];

    [webView setMarkedText:@"abc" selectedRange:NSMakeRange(3, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    [webView insertText:@"abc" replacementRange:NSMakeRange(NSNotFound, 0)];

    [webView stringByEvaluatingJavaScript:@""
        "window.compositionLog = [];"
        "editor.addEventListener('compositionstart', () => {"
        "    const firstChild = p.firstChild;"
        "    if (firstChild && firstChild.nodeType === Node.TEXT_NODE && firstChild.nodeValue.length) {"
        "        const newText = document.createTextNode(firstChild.nodeValue);"
        "        p.insertBefore(newText, firstChild);"
        "        p.removeChild(firstChild);"
        "        window.compositionLog.push('mutated');"
        "    } else {"
        "        window.compositionLog.push('skipped:firstChild=' + (firstChild && firstChild.nodeType));"
        "    }"
        "}, { once: true });"];

    [webView setMarkedText:@"X" selectedRange:NSMakeRange(1, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
    [webView waitForNextPresentationUpdate];

    EXPECT_STREQ("mutated", [webView stringByEvaluatingJavaScript:@"window.compositionLog.join('|')"].UTF8String);
    EXPECT_STREQ("abcX", [webView stringByEvaluatingJavaScript:@"document.getElementById('p').textContent"].UTF8String);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
