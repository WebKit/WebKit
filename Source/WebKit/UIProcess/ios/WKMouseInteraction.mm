/*
* Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WKMouseInteraction.h"

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import "UIKitSPI.h"
#import "WebIOSEventFactory.h"
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/Compiler.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>

@interface WKMouseInteraction () <UIGestureRecognizerDelegate>
- (void)_updateMouseTouches:(NSSet<UITouch *> *)touches;
@end

@interface WKMouseTouchGestureRecognizer : UIGestureRecognizer
- (instancetype)initWithInteraction:(WKMouseInteraction *)interaction;
@end

@implementation WKMouseTouchGestureRecognizer {
    __weak WKMouseInteraction *_interaction;
}

- (instancetype)initWithInteraction:(WKMouseInteraction *)interaction
{
    if (self = [super init])
        _interaction = interaction;
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [_interaction _updateMouseTouches:touches];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [_interaction _updateMouseTouches:touches];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [_interaction _updateMouseTouches:touches];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [_interaction _updateMouseTouches:touches];
}

@end

@implementation WKMouseInteraction {
    RetainPtr<WKMouseTouchGestureRecognizer> _mouseTouchGestureRecognizer;
    RetainPtr<UIHoverGestureRecognizer> _mouseHoverGestureRecognizer;
    RetainPtr<UIHoverGestureRecognizer> _pencilHoverGestureRecognizer;
    RetainPtr<UITouch> _currentHoverTouch;
    RetainPtr<UITouch> _currentMouseTouch;
    __weak UIView *_view;
    std::optional<CGPoint> _lastLocation;
    std::optional<UIEventButtonMask> _pressedButtonMask;
    BOOL _enabled;
    BOOL _touching;
    BOOL _cancelledOrExited;
}

- (instancetype)initWithDelegate:(id<WKMouseInteractionDelegate>)delegate
{
    if (!(self = [super init]))
        return nil;

    _mouseTouchGestureRecognizer = adoptNS([[WKMouseTouchGestureRecognizer alloc] initWithInteraction:self]);
    _pencilHoverGestureRecognizer = adoptNS([[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(_hoverGestureRecognized:)]);
    _mouseHoverGestureRecognizer = adoptNS([[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(_hoverGestureRecognized:)]);
    [self _forEachGesture:^(UIGestureRecognizer *gesture) {
        gesture.delegate = self;
    }];
    [_pencilHoverGestureRecognizer setAllowedTouchTypes:@[ @(UITouchTypePencil) ]];
    [_mouseTouchGestureRecognizer setAllowedTouchTypes:@[ @(UITouchTypeDirect), @(UITouchTypeIndirectPointer) ]];
    [_mouseHoverGestureRecognizer setAllowedTouchTypes:@[ @(UITouchTypeDirect), @(UITouchTypeIndirectPointer) ]];
    _delegate = delegate;
    _enabled = YES;
    _cancelledOrExited = YES;
    return self;
}

- (void)_resetCachedState
{
    _cancelledOrExited = YES;
    _touching = NO;
    _lastLocation = { };
    _pressedButtonMask = { };
    _currentHoverTouch = { };
    _currentMouseTouch = { };
}

- (BOOL)hasGesture:(UIGestureRecognizer *)gesture
{
    return _mouseTouchGestureRecognizer == gesture || _mouseHoverGestureRecognizer == gesture || _pencilHoverGestureRecognizer == gesture;
}

- (UIView *)view
{
    return _view;
}

- (BOOL)isEnabled
{
    return _enabled;
}

- (void)_forEachGesture:(void(^)(UIGestureRecognizer *))callback
{
    callback(_mouseTouchGestureRecognizer.get());
    callback(_mouseHoverGestureRecognizer.get());
    callback(_pencilHoverGestureRecognizer.get());
}

- (void)setEnabled:(BOOL)enabled
{
    if (_enabled == enabled)
        return;

    _enabled = enabled;

    [self _forEachGesture:^(UIGestureRecognizer *gesture) {
        gesture.enabled = enabled;
    }];

    if (!enabled)
        [self _resetCachedState];
}

- (UITouch *)mouseTouch
{
    return _touching ? _currentMouseTouch.get() : _currentHoverTouch.get();
}

- (UIGestureRecognizer *)mouseTouchGestureRecognizer
{
    return _mouseTouchGestureRecognizer.get();
}

- (UIGestureRecognizer *)_activeGesture
{
    if (_touching)
        return _mouseTouchGestureRecognizer.get();

    if ([_currentHoverTouch type] == UITouchTypePencil)
        return _pencilHoverGestureRecognizer.get();

    return _mouseHoverGestureRecognizer.get();
}

inline static String pointerType(UITouchType type)
{
#if ENABLE(PENCIL_HOVER)
    if (type == UITouchTypePencil)
        return WebCore::penPointerEventType();
#endif
    return WebCore::mousePointerEventType();
}

- (WebKit::NativeWebMouseEvent)createMouseEventWithType:(WebKit::WebEventType)type wasCancelled:(BOOL)cancelled
{
    auto modifiers = WebIOSEventFactory::webEventModifiersForUIKeyModifierFlags(self._activeGesture.modifierFlags);
    BOOL isRightButton = modifiers.contains(WebKit::WebEventModifier::ControlKey) || (_pressedButtonMask.value_or(0) & UIEventButtonMaskSecondary);

    auto button = [&] {
        if (!_touching)
            return WebKit::WebMouseEventButton::NoButton;
        if (isRightButton)
            return WebKit::WebMouseEventButton::RightButton;
        return WebKit::WebMouseEventButton::LeftButton;
    }();

    // FIXME: 'buttons' should report any buttons that are still down in the case when one button is released from a chord.
    auto buttons = [&] {
        if (!_touching || type == WebKit::WebEventType::MouseUp)
            return 0;
        if (isRightButton)
            return 2;
        return 1;
    }();

    auto currentTouch = self.mouseTouch;
    WebCore::IntPoint point { [self locationInView:self.view] };
    auto delta = point - WebCore::IntPoint { [currentTouch previousLocationInView:self.view] };
    // UITouch's timestamp uses mach_absolute_time as its timebase, same as MonotonicTime.
    return {
        type,
        button,
        static_cast<unsigned short>(buttons),
        point,
        point,
        static_cast<float>(delta.width()),
        static_cast<float>(delta.height()),
        0,
        static_cast<int>(currentTouch.tapCount),
        modifiers,
        MonotonicTime::fromRawSeconds(currentTouch.timestamp).approximateWallTime(),
        0,
        cancelled ? WebKit::GestureWasCancelled::Yes : WebKit::GestureWasCancelled::No,
        pointerType(currentTouch.type)
    };
}

#pragma mark - UIInteraction

- (void)willMoveToView:(UIView *)view
{
    if (_view == view)
        return;

    [self _forEachGesture:^(UIGestureRecognizer *gesture) {
        [_view removeGestureRecognizer:gesture];
    }];
    [self _resetCachedState];
}

- (void)didMoveToView:(UIView *)view
{
    if (_view == view)
        return;

    _view = view;

    [self _forEachGesture:^(UIGestureRecognizer *gesture) {
        [view addGestureRecognizer:gesture];
    }];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    ASSERT([self hasGesture:gestureRecognizer]);
    return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    if (gestureRecognizer != _pencilHoverGestureRecognizer && !touch._isPointerTouch)
        return NO;

    if (gestureRecognizer == _mouseHoverGestureRecognizer || gestureRecognizer == _pencilHoverGestureRecognizer)
        _currentHoverTouch = touch;

    return YES;
}

- (void)_hoverGestureRecognized:(UIHoverGestureRecognizer *)gestureRecognizer
{
    if (!_currentHoverTouch)
        return;

    auto state = gestureRecognizer.state;
    BOOL isCancelled = state == UIGestureRecognizerStateCancelled;
    switch (state) {
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        _cancelledOrExited = NO;
        break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        _cancelledOrExited = YES;
        break;
    case UIGestureRecognizerStatePossible:
        break;
    }

    if (gestureRecognizer.buttonMask) {
        // Let WKMouseTouchGestureRecognizer recognize mouse presses.
        return;
    }

    auto location = [gestureRecognizer locationInView:self.view];
    if (!isCancelled && _lastLocation && CGPointEqualToPoint(*_lastLocation, location))
        return;

    _lastLocation = location;
    [_delegate mouseInteraction:self changedWithEvent:[self createMouseEventWithType:WebKit::WebEventType::MouseMove wasCancelled:isCancelled]];
}

- (void)_updateMouseTouches:(NSSet<UITouch *> *)touches
{
    _currentMouseTouch = touches.anyObject;
    _lastLocation = [_mouseTouchGestureRecognizer locationInView:_view];

    auto eventType = WebKit::WebEventType::NoType;
    auto phase = [_currentMouseTouch phase];
    switch (phase) {
    case UITouchPhaseBegan:
        _touching = YES;
        _pressedButtonMask = [_mouseTouchGestureRecognizer buttonMask];
        eventType = WebKit::WebEventType::MouseDown;
        break;
    case UITouchPhaseEnded:
    case UITouchPhaseCancelled:
        eventType = WebKit::WebEventType::MouseUp;
        break;
    case UITouchPhaseMoved:
        _touching = YES;
        _pressedButtonMask = [_mouseTouchGestureRecognizer buttonMask];
        eventType = WebKit::WebEventType::MouseMove;
        break;
    case UITouchPhaseRegionEntered:
    case UITouchPhaseRegionMoved:
    case UITouchPhaseRegionExited:
    case UITouchPhaseStationary:
        break;
    }

    if (eventType != WebKit::WebEventType::NoType)
        [_delegate mouseInteraction:self changedWithEvent:[self createMouseEventWithType:eventType wasCancelled:phase == UITouchPhaseCancelled]];

    if (eventType == WebKit::WebEventType::MouseUp) {
        _touching = NO;
        _pressedButtonMask = std::nullopt;
    }
}

- (CGPoint)locationInView:(UIView *)view
{
    auto touch = self.mouseTouch;
    return touch && !_cancelledOrExited ? [touch locationInView:view] : CGPointMake(-1, -1);
}

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)
