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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentWorld.h>
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

@interface KeyboardTestMenu : NSMenu
@end

@implementation KeyboardTestMenu

- (BOOL)_containsItemMatchingEvent:(NSEvent *)event includingDisabledItems:(BOOL)includingDisabledItems
{
    return [event.charactersIgnoringModifiers isEqualToString:@"e"] && event.modifierFlags & NSEventModifierFlagFunction;
}

@end

@interface KeyboardTestMenuItem : NSMenuItem

@end

@implementation KeyboardTestMenuItem

- (BOOL)_isSystemMenuItem
{
    return YES;
}

@end

@interface NSViewWithKeyDownOverride : NSView

@property (nonatomic) NSUInteger keyDownCount;

@end

@implementation NSViewWithKeyDownOverride

- (void)keyDown:(NSEvent *)event
{
    _keyDownCount++;
}

@end

namespace TestWebKitAPI {

static void arrowKeyDownWithKeyRepeat(WKWebView *webView, unsigned keyRepeatCount)
{
    NSString *arrowString = [NSString stringWithFormat:@"%C", (unichar)NSDownArrowFunctionKey];
    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSMakePoint(5, 5) modifierFlags:0 timestamp:[[NSDate date] timeIntervalSince1970] windowNumber:[[webView window] windowNumber] context:[NSGraphicsContext currentContext] characters:arrowString charactersIgnoringModifiers:arrowString isARepeat:NO keyCode:0x7D];

    [webView keyDown:event];

    for (unsigned i = 0; i < keyRepeatCount; i++) {
        event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSMakePoint(5, 5) modifierFlags:0 timestamp:[[NSDate date] timeIntervalSince1970] windowNumber:[[webView window] windowNumber] context:[NSGraphicsContext currentContext] characters:arrowString charactersIgnoringModifiers:arrowString isARepeat:YES keyCode:0x7D];

        [webView keyDown:event];
    }
}

TEST(KeyboardEventTests, FunctionKeyCommand)
{
    auto menu = adoptNS([[KeyboardTestMenu alloc] initWithTitle:@"Test menu"]);
    auto menuItem = adoptNS([[KeyboardTestMenuItem alloc] initWithTitle:@"Emojis & Symbols" action:@selector(description) keyEquivalent:@"e"]);
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagFunction];
    [menu setItemArray:@[ menuItem.get() ]];
    NSApp.mainMenu = menu.get();

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<script>addEventListener('load', () => document.body.focus())</script><body contenteditable></body>"];
    [webView typeCharacter:'e' modifiers:NSEventModifierFlagFunction];
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(KeyboardEventTests, SmoothKeyboardScrolling)
{
    auto window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);

    auto view = adoptNS([[NSViewWithKeyDownOverride alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    [view addSubview:webView.get()];

    [[window contentView] addSubview:view.get()];
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:webView.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];

    arrowKeyDownWithKeyRepeat(webView.get(), 3);

    Util::runFor(Seconds(3));
    EXPECT_EQ([view keyDownCount], 0UL);
}

TEST(KeyboardEventTests, TerminateWebContentProcessDuringKeyEventHandling)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@""];

    RunLoop::mainSingleton().dispatchAfter(5_ms, [&] {
        [webView _killWebContentProcessAndResetState];
    });
    for (unsigned i = 0; i < 10; ++i) {
        [webView typeCharacter:'a'];
        Util::runFor(1_ms);
    }
    Util::runFor(25_ms);
}

TEST(KeyboardEventTests, UserTextInputEvent)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);
    [webView addToTestWindow];

    RetainPtr configuration = adoptNS([_WKContentWorldConfiguration new]);
    configuration.get().allowAutofill = YES;
    RetainPtr autofillWorld = [WKContentWorld _worldWithConfiguration:configuration.get()];
    NSString *pageWorldJS = @"window.addEventListener('webkitusertextinput', () => alert('fail') )";
    NSString *autofillWorldJS = @"window.addEventListener('webkitusertextinput', (e) => { setTimeout(() => alert('pass ' + e.target.id), 50)})";
    RetainPtr pageWorldScript = adoptNS([[WKUserScript alloc] initWithSource:pageWorldJS injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
    RetainPtr autofillWorldScript = adoptNS([[WKUserScript alloc] initWithSource:autofillWorldJS injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES inContentWorld:autofillWorld.get()]);
    RetainPtr<WKUserContentController> userContentController = [webView configuration].userContentController;
    [userContentController addUserScript:pageWorldScript.get()];
    [userContentController addUserScript:autofillWorldScript.get()];

    [webView synchronouslyLoadHTMLString:@"<input id='input' type='text'><textarea id='textarea'></textarea>"];

    [webView objectByEvaluatingJavaScript:@"input.focus()"];
    [webView typeCharacter:'a'];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "pass input");

    [webView objectByEvaluatingJavaScript:@"textarea.focus()"];
    [webView typeCharacter:'c'];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "pass textarea");

    // Untrusted input changes should not cause webkitusertextinput to fire.
    [webView objectByEvaluatingJavaScript:@"window.addEventListener('input', (e) => { setTimeout(() => alert('input without webkitusertextinput ' + e.target.id), 50)})"];

    [webView objectByEvaluatingJavaScript:@"input.value += 'c'; input.dispatchEvent(new InputEvent('input', { data: 'c', inputType: 'insertText', bubbles: true }));"];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "input without webkitusertextinput input");

    [webView objectByEvaluatingJavaScript:@"textarea.value += 'c'; textarea.dispatchEvent(new InputEvent('input', { data: 'c', inputType: 'insertText', bubbles: true }));"];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "input without webkitusertextinput textarea");
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
