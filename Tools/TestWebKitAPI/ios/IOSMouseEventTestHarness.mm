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

#import "config.h"
#import "IOSMouseEventTestHarness.h"

#import <wtf/MonotonicTime.h>

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)

@implementation WKTestingTouch {
    NSUInteger _tapCount;
    CGPoint _overriddenLocation;
    UITouchPhase _overriddenPhase;
}

- (CGPoint)locationInView:(UIView *)view
{
    return _overriddenLocation;
}

- (void)setLocationInView:(CGPoint)location
{
    _overriddenLocation = location;
}

- (void)setTapCount:(NSUInteger)tapCount
{
    _tapCount = tapCount;
}

- (void)setPhase:(UITouchPhase)phase
{
    _overriddenPhase = phase;
}

- (UITouchPhase)phase
{
    return _overriddenPhase;
}

- (NSUInteger)tapCount
{
    return _tapCount;
}

- (NSTimeInterval)timestamp
{
    return MonotonicTime::now().secondsSinceEpoch().value();
}

- (BOOL)_isPointerTouch
{
    return YES;
}

@end

namespace TestWebKitAPI {

MouseEventTestHarness::MouseEventTestHarness(TestWKWebView *webView)
    : m_webView(webView)
{
    auto contentView = m_webView.wkContentView;
    for (id<UIInteraction> interaction in contentView.interactions) {
        if ([interaction respondsToSelector:@selector(_hoverGestureRecognized:)]) {
            m_mouseInteraction = static_cast<id<WKMouseInteractionForTesting>>(interaction);
            break;
        }
    }

    for (UIGestureRecognizer *gestureRecognizer in contentView.gestureRecognizers) {
        if ([gestureRecognizer.name isEqualToString:@"WKMouseHover"])
            m_hoverGestureRecognizer = dynamic_objc_cast<UIHoverGestureRecognizer>(gestureRecognizer);
        else if ([gestureRecognizer.name isEqualToString:@"WKMouseTouch"])
            m_mouseTouchGestureRecognizer = gestureRecognizer;
    }

    RELEASE_ASSERT(m_mouseInteraction);
    RELEASE_ASSERT(m_hoverGestureRecognizer);
    RELEASE_ASSERT(m_mouseTouchGestureRecognizer);

    auto overrideLocationInView = imp_implementationWithBlock([&](UIGestureRecognizer *) {
        return m_locationInRootView;
    });
    m_gestureLocationSwizzler = makeUnique<InstanceMethodSwizzler>(UIGestureRecognizer.class, @selector(locationInView:), overrideLocationInView);
    m_hoverGestureLocationSwizzler = makeUnique<InstanceMethodSwizzler>(UIHoverGestureRecognizer.class, @selector(locationInView:), overrideLocationInView);
    m_gestureButtonMaskSwizzler = makeUnique<InstanceMethodSwizzler>(UIGestureRecognizer.class, @selector(buttonMask), imp_implementationWithBlock([&](UIGestureRecognizer *) {
        return m_buttonMask;
    }));
    m_gestureModifierFlagsSwizzler = makeUnique<InstanceMethodSwizzler>(UIGestureRecognizer.class, @selector(modifierFlags), imp_implementationWithBlock([&](UIGestureRecognizer *) {
        return m_modifierFlags;
    }));
    m_unusedEvent = adoptNS([UIEvent new]);
}

void MouseEventTestHarness::mouseMove(CGFloat x, CGFloat y)
{
    // FIXME(262757): This test helper should handle mouse drags.
    if (!m_activeTouch) {
        m_activeTouch = adoptNS([[WKTestingTouch alloc] init]);
        EXPECT_TRUE([m_mouseInteraction gestureRecognizer:m_hoverGestureRecognizer shouldReceiveTouch:m_activeTouch.get()]);
    }

    m_locationInRootView = CGPointMake(x, y);
    [m_activeTouch setLocationInView:m_locationInRootView];

    bool shouldBeginGesture = std::exchange(m_needsToDispatchHoverGestureBegan, false);
    [m_activeTouch setPhase:shouldBeginGesture ? UITouchPhaseRegionEntered : UITouchPhaseRegionMoved];
    m_hoverGestureRecognizer.state = shouldBeginGesture ? UIGestureRecognizerStateBegan : UIGestureRecognizerStateChanged;
    [m_mouseInteraction _hoverGestureRecognized:m_hoverGestureRecognizer];
}

void MouseEventTestHarness::mouseDown(UIEventButtonMask buttons, UIKeyModifierFlags modifierFlags)
{
    [m_activeTouch setPhase:UITouchPhaseBegan];
    [m_activeTouch setTapCount:1];
    m_buttonMask = buttons;
    m_modifierFlags = modifierFlags;
    [m_mouseTouchGestureRecognizer touchesBegan:activeTouches() withEvent:m_unusedEvent.get()];
    EXPECT_EQ(m_mouseTouchGestureRecognizer.state, UIGestureRecognizerStateBegan);
}

void MouseEventTestHarness::mouseUp()
{
    [m_activeTouch setPhase:UITouchPhaseEnded];
    [m_activeTouch setTapCount:0];
    m_buttonMask = 0;
    [m_mouseTouchGestureRecognizer touchesEnded:activeTouches() withEvent:m_unusedEvent.get()];
    m_modifierFlags = 0;
    EXPECT_EQ(m_mouseTouchGestureRecognizer.state, UIGestureRecognizerStateEnded);
}

void MouseEventTestHarness::mouseCancel()
{
    [m_activeTouch setPhase:UITouchPhaseCancelled];
    [m_activeTouch setTapCount:0];
    m_buttonMask = 0;
    [m_mouseTouchGestureRecognizer touchesCancelled:activeTouches() withEvent:m_unusedEvent.get()];
    m_modifierFlags = 0;
    EXPECT_EQ(m_mouseTouchGestureRecognizer.state, UIGestureRecognizerStateCancelled);
}

};

#endif
