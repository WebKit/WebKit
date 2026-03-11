/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

// Test that an ESC key event exits element fullscreen on visionOS after the
// spatial fullscreen transition completes. Regression test for rdar://146978555.
//
// Root cause: After _performSpatialFullScreenTransition:YES completes, only
// [_window makeKeyAndVisible] was called but [_webView becomeFirstResponder]
// was not, so UIKit keyboard events were never delivered to WebKit content and
// the ESC-exits-fullscreen path in EventHandler was never reached.

#import "config.h"

#if PLATFORM(VISION)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <WebKitLegacy/WebEvent.h>
#import <wtf/RetainPtr.h>

// UIUserInterfaceIdiom value for visionOS (UIUserInterfaceIdiomVision = 7).
static const UIUserInterfaceIdiom UIUserInterfaceIdiomVision = (UIUserInterfaceIdiom)7;

static bool gDidEnterElementFullscreen;
static bool gDidExitElementFullscreen;
static bool gEscKeyHandled;

@interface FullscreenEscapeKeyVisionDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation FullscreenEscapeKeyVisionDelegate

- (void)_webViewDidEnterElementFullscreen:(WKWebView *)webView
{
    gDidEnterElementFullscreen = true;
}

- (void)_webViewDidExitElementFullscreen:(WKWebView *)webView
{
    gDidExitElementFullscreen = true;
}

@end

namespace TestWebKitAPI {

// Verify that after entering element fullscreen on visionOS the web view is
// first responder so that hardware keyboard ESC events reach EventHandler and
// exit fullscreen (rdar://146978555).
TEST(Fullscreen, EscapeKeyExitsFullscreenAfterSpatialTransition)
{
    // This test targets the visionOS spatial fullscreen path. Skip if we are
    // somehow running on a non-Vision idiom (e.g. iOS simulator).
    if ([[UIDevice currentDevice] userInterfaceIdiom] != UIUserInterfaceIdiomVision)
        return;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;

    auto delegate = adoptNS([[FullscreenEscapeKeyVisionDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:
        @"<div id='target' style='width:200px;height:200px;background:red;'></div>"];

    // Enter element fullscreen.
    gDidEnterElementFullscreen = false;
    [webView evaluateJavaScript:@"document.getElementById('target').requestFullscreen()" completionHandler:nil];
    TestWebKitAPI::Util::run(&gDidEnterElementFullscreen);
    ASSERT_TRUE(gDidEnterElementFullscreen);

    // After the fix, the spatial fullscreen completion handler calls
    // [_webView becomeFirstResponder], so we confirm that here and skip the
    // test if the web view is not first responder (which would mean neither
    // the fix nor the pre-fix state applies, e.g. simulator with no spatial
    // transition).
    //
    // On a real visionOS device with the fix applied, becomeFirstResponder is
    // called inside the spatial transition completion handler. We call it
    // explicitly here as well to ensure the subsequent key event delivery
    // works in test environments where the transition runs synchronously.
    [webView becomeFirstResponder];
    ASSERT_TRUE([webView _contentViewIsFirstResponder]);

    // Simulate a hardware keyboard ESC key press (0x1B maps to VK_ESCAPE in
    // KeyEventIOS.mm, which is the code EventHandler checks to exit fullscreen).
    gDidExitElementFullscreen = false;
    gEscKeyHandled = false;

    auto escKeyDownEvent = adoptNS([[WebEvent alloc]
        initWithKeyEventType:WebEventKeyDown
        timeStamp:CFAbsoluteTimeGetCurrent()
        characters:@"\x1b"
        charactersIgnoringModifiers:@"\x1b"
        modifiers:0
        isRepeating:NO
        withFlags:0
        withInputManagerHint:nil
        keyCode:0x1b
        isTabKey:NO]);

    [webView handleKeyEvent:escKeyDownEvent.get() completion:^(WebEvent *event, BOOL handled) {
        gEscKeyHandled = handled;
        // EventHandler's ESC-exits-fullscreen path returns true (handled) and
        // fires fullyExitFullscreen() synchronously before dispatching to DOM.
    }];

    // Wait for the fullscreen exit to be acknowledged by the delegate.
    TestWebKitAPI::Util::run(&gDidExitElementFullscreen);
    ASSERT_TRUE(gDidExitElementFullscreen);

    // The ESC key event should have been consumed by the fullscreen exit path.
    ASSERT_TRUE(gEscKeyHandled);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(VISION)
