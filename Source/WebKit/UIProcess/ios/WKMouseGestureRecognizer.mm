/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WKMouseGestureRecognizer.h"

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import "NativeWebMouseEvent.h"
#import "UIKitSPI.h"
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/Compiler.h>
#import <wtf/MonotonicTime.h>

static OptionSet<WebKit::WebEvent::Modifier> webEventModifiersForUIKeyModifierFlags(UIKeyModifierFlags flags)
{
    OptionSet<WebKit::WebEvent::Modifier> modifiers;
    if (flags & UIKeyModifierShift)
        modifiers.add(WebKit::WebEvent::Modifier::ShiftKey);
    if (flags & UIKeyModifierControl)
        modifiers.add(WebKit::WebEvent::Modifier::ControlKey);
    if (flags & UIKeyModifierAlternate)
        modifiers.add(WebKit::WebEvent::Modifier::AltKey);
    if (flags & UIKeyModifierCommand)
        modifiers.add(WebKit::WebEvent::Modifier::MetaKey);
    if (flags & UIKeyModifierAlphaShift)
        modifiers.add(WebKit::WebEvent::Modifier::CapsLockKey);
    return modifiers;
}

@implementation WKMouseGestureRecognizer {
    RetainPtr<UIEvent> _currentHoverEvent;
    RetainPtr<UITouch> _currentTouch;

    BOOL _touching;

    std::unique_ptr<WebKit::NativeWebMouseEvent> _lastEvent;
    std::optional<CGPoint> _lastLocation;
    std::optional<UIEventButtonMask> _pressedButtonMask;
}

- (void)setEnabled:(BOOL)enabled
{
    [super setEnabled:enabled];

    if (!enabled) {
        _currentHoverEvent = nil;
        _currentTouch = nil;
        _touching = NO;
        _lastEvent = nil;
        _lastLocation = std::nullopt;
        _pressedButtonMask = std::nullopt;
    }
}

- (BOOL)_shouldReceiveTouch:(UITouch *)touch forEvent:(UIEvent *)event recognizerView:(id)gestureOwner
{
    // FIXME: We should move off of this UIKit IPI method once we have a viable SPI or API alternative
    // for opting a UIHoverGestureRecognizer subclass into receiving UITouches. See also: rdar://80700227.
    return touch == _currentTouch;
}

- (WebKit::NativeWebMouseEvent *)lastMouseEvent
{
    return _lastEvent.get();
}

- (std::optional<CGPoint>)lastMouseLocation
{
    return _lastLocation;
}

- (UITouch *)mouseTouch
{
    return _currentTouch.get();
}

- (void)reset
{
    [super reset];
    _currentHoverEvent = nil;
    _currentTouch = nil;
}

- (std::unique_ptr<WebKit::NativeWebMouseEvent>)createMouseEventWithType:(WebKit::WebEvent::Type)type wasCancelled:(BOOL)cancelled
{
    auto modifiers = webEventModifiersForUIKeyModifierFlags(self.modifierFlags);
    BOOL isRightButton = modifiers.contains(WebKit::WebEvent::Modifier::ControlKey) || (_pressedButtonMask && (*_pressedButtonMask & UIEventButtonMaskSecondary));

    auto button = [&] {
        if (!_touching)
            return WebKit::WebMouseEvent::NoButton;
        if (isRightButton)
            return WebKit::WebMouseEvent::RightButton;
        return WebKit::WebMouseEvent::LeftButton;
    }();

    // FIXME: 'buttons' should report any buttons that are still down in the case when one button is released from a chord.
    auto buttons = [&] {
        if (!_touching || type == WebKit::WebEvent::Type::MouseUp)
            return 0;
        if (isRightButton)
            return 2;
        return 1;
    }();

    WebCore::IntPoint point { [self locationInView:self.view] };
    auto delta = point - WebCore::IntPoint { [_currentTouch previousLocationInView:self.view] };
    // UITouch's timestamp uses mach_absolute_time as its timebase, same as MonotonicTime.
    auto timestamp = MonotonicTime::fromRawSeconds([_currentTouch timestamp]).approximateWallTime();
    return WTF::makeUnique<WebKit::NativeWebMouseEvent>(type, button, buttons, point, point, delta.width(), delta.height(), 0, [_currentTouch tapCount], modifiers, timestamp, 0, cancelled ? WebKit::GestureWasCancelled::Yes : WebKit::GestureWasCancelled::No);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _touching = YES;
    _pressedButtonMask = [event _buttonMask];

    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseDown wasCancelled:NO];
    _lastLocation = [self locationInView:self.view];

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove wasCancelled:NO];
    _lastLocation = [self locationInView:self.view];

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseUp wasCancelled:NO];
    _lastLocation = [self locationInView:self.view];

    _touching = NO;
    _pressedButtonMask = std::nullopt;

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseUp wasCancelled:YES];
    _lastLocation = [self locationInView:self.view];

    _touching = NO;
    _pressedButtonMask = std::nullopt;

    self.state = UIGestureRecognizerStateChanged;
}

- (void)_hoverEntered:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove wasCancelled:NO];

    if (_currentHoverEvent == nil && touches.count == 1 && [event isKindOfClass:NSClassFromString(@"UIHoverEvent")]) {
        _currentHoverEvent = event;
        _currentTouch = touches.anyObject;
        _lastLocation = [self locationInView:self.view];
        self.state = UIGestureRecognizerStateBegan;
    }
}

- (void)_hoverMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (_touching) {
        _lastEvent = nullptr;
        return;
    }

    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove wasCancelled:NO];
    _lastLocation = [self locationInView:self.view];

    if (_currentHoverEvent == event && [touches containsObject:_currentTouch.get()])
        self.state = UIGestureRecognizerStateChanged;
}

- (void)_hoverExited:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove wasCancelled:NO];
    _lastLocation = [self locationInView:self.view];

    if (_currentHoverEvent == event) {
        _currentHoverEvent = nil;
        _currentTouch = nil;
        self.state = UIGestureRecognizerStateEnded;
    }
}

- (void)_hoverCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _hoverExited:touches withEvent:event];
}

- (CGPoint)locationInView:(UIView *)view
{
    if (!_currentTouch)
        return CGPointMake(-1, -1);
    return [_currentTouch locationInView:view];
}

@end

#endif
