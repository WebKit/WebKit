/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)

#import "InstanceMethodSwizzler.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

@protocol WKMouseInteractionForTesting<UIGestureRecognizerDelegate>
@property (readonly, nonatomic, getter=isEnabled) BOOL enabled;
- (void)_hoverGestureRecognized:(UIHoverGestureRecognizer *)gestureRecognizer;
@end

@interface WKTestingTouch : UITouch
@end

namespace TestWebKitAPI {

class MouseEventTestHarness {
public:
    MouseEventTestHarness(TestWKWebView *);

    MouseEventTestHarness() = delete;

    void mouseMove(CGFloat x, CGFloat y);

    void mouseDown(UIEventButtonMask buttons = UIEventButtonMaskPrimary, UIKeyModifierFlags = 0);

    void mouseUp();

    void mouseCancel();

    NSSet *activeTouches() const { return [NSSet setWithObject:m_activeTouch.get()]; }
    TestWKWebView *webView() const { return m_webView; }
    id<WKMouseInteractionForTesting> mouseInteraction() const { return m_mouseInteraction; }

private:
    std::unique_ptr<InstanceMethodSwizzler> m_gestureLocationSwizzler;
    std::unique_ptr<InstanceMethodSwizzler> m_hoverGestureLocationSwizzler;
    std::unique_ptr<InstanceMethodSwizzler> m_gestureButtonMaskSwizzler;
    std::unique_ptr<InstanceMethodSwizzler> m_gestureModifierFlagsSwizzler;
    RetainPtr<WKTestingTouch> m_activeTouch;
    RetainPtr<UIEvent> m_unusedEvent;
    __weak UIHoverGestureRecognizer *m_hoverGestureRecognizer { nil };
    __weak UIGestureRecognizer *m_mouseTouchGestureRecognizer { nil };
    __weak id<WKMouseInteractionForTesting> m_mouseInteraction;
    __weak TestWKWebView *m_webView { nil };
    CGPoint m_locationInRootView { CGPointZero };
    UIEventButtonMask m_buttonMask { 0 };
    UIKeyModifierFlags m_modifierFlags { 0 };
    bool m_needsToDispatchHoverGestureBegan { true };
};

}

#endif

