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

#import "AdditionalButtonMasksIOS.h"
#import "Logging.h"
#import "UIKitSPI.h"
#import "WebIOSEventFactory.h"
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Compiler.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if ENABLE(POINTER_LOCK)

#import "WKMouseDeviceObserver.h"
#import <GameController/GameController.h>
#import <WebCore/GameControllerSoftLink.h>

struct PointerLockState {
    bool isActive { false };
    bool isObservingNotifications { false };
    RetainPtr<GCMouse> currentMouse;
    GCMouseMoved originalMouseMovedHandler { nil };
    std::optional<CGPoint> lockedCursorPosition;

    void reset()
    {
        isActive = false;
        isObservingNotifications = false;
        currentMouse = nil;
        originalMouseMovedHandler = nil;
        lockedCursorPosition = std::nullopt;
    }
};

#endif

@interface WKMouseInteraction () <UIGestureRecognizerDelegate>
- (void)_updateMouseTouches:(NSSet<UITouch *> *)touches;

#if ENABLE(POINTER_LOCK)
- (void)_startObservingMouseNotifications;
- (void)_stopObservingMouseNotifications;
- (void)_mouseDidStopBeingCurrent:(NSNotification *)notification;
- (void)_setupMouseMovedHandlerForMouse:(GCMouse *)mouse;
#endif
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
    self.state = UIGestureRecognizerStateBegan;

    [protect(_interaction) _updateMouseTouches:touches];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    self.state = UIGestureRecognizerStateChanged;

    [protect(_interaction) _updateMouseTouches:touches];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    self.state = UIGestureRecognizerStateEnded;

    [protect(_interaction) _updateMouseTouches:touches];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    self.state = UIGestureRecognizerStateCancelled;

    [protect(_interaction) _updateMouseTouches:touches];
}

#if PLATFORM(MACCATALYST)

- (UIEventButtonMask)_defaultAllowedMouseButtons
{
    return UIEventButtonMaskPrimary | UIEventButtonMaskSecondary;
}

#endif

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

#if ENABLE(POINTER_LOCK)
    PointerLockState _pointerLockState;
#endif
}

- (instancetype)initWithDelegate:(id<WKMouseInteractionDelegate>)delegate
{
    if (!(self = [super init]))
        return nil;

    _mouseTouchGestureRecognizer = adoptNS([[WKMouseTouchGestureRecognizer alloc] initWithInteraction:self]);
    [_mouseTouchGestureRecognizer setName:@"WKMouseTouch"];

    _pencilHoverGestureRecognizer = adoptNS([[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(_hoverGestureRecognized:)]);
    [_pencilHoverGestureRecognizer setName:@"WKPencilHover"];

    _mouseHoverGestureRecognizer = adoptNS([[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(_hoverGestureRecognized:)]);
    [_mouseHoverGestureRecognizer setName:@"WKMouseHover"];

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

#if ENABLE(POINTER_LOCK)
    _pointerLockState.reset();
#endif
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

- (std::optional<WebKit::NativeWebMouseEvent>)createMouseEventWithType:(std::optional<WebKit::WebEventType>)type wasCancelled:(BOOL)cancelled
{
    if (!type)
        return std::nullopt;

    auto modifiers = WebKit::WebIOSEventFactory::webEventModifiersForUIKeyModifierFlags(self._activeGesture.modifierFlags);
    UIEventButtonMask currentButtonMask = _pressedButtonMask.value_or(0);
    BOOL isControlClick = modifiers.contains(WebKit::WebEventModifier::ControlKey);

    auto button = [&] {
        if (!_touching)
            return WebKit::WebMouseEventButton::None;
        if (isControlClick || (currentButtonMask & UIEventButtonMaskSecondary))
            return WebKit::WebMouseEventButton::Right;
        if (currentButtonMask & WebKit::UIEventButtonMaskTertiary)
            return WebKit::WebMouseEventButton::Middle;
        if (currentButtonMask & WebKit::UIEventButtonMaskQuaternary)
            return WebKit::WebMouseEventButton::Back;
        if (currentButtonMask & WebKit::UIEventButtonMaskQuinary)
            return WebKit::WebMouseEventButton::Forward;
        return WebKit::WebMouseEventButton::Left;
    }();

    // FIXME: 'buttons' should report any buttons that are still down in the case when one button is released from a chord.
    auto buttons = [&] {
        unsigned short buttonsBitmask = 0;

        if (!_touching || type == WebKit::WebEventType::MouseUp)
            return buttonsBitmask;

        UIEventButtonMask buttonMaskToCheck = (type == WebKit::WebEventType::MouseUp)
            ? [_mouseTouchGestureRecognizer buttonMask]
            : currentButtonMask;

        if (buttonMaskToCheck & UIEventButtonMaskPrimary)
            buttonsBitmask |= 1;
        if ((buttonMaskToCheck & UIEventButtonMaskSecondary) || isControlClick)
            buttonsBitmask |= 2;
        if (buttonMaskToCheck & WebKit::UIEventButtonMaskTertiary)
            buttonsBitmask |= 4;
        if (buttonMaskToCheck & WebKit::UIEventButtonMaskQuaternary)
            buttonsBitmask |= 8;
        if (buttonMaskToCheck & WebKit::UIEventButtonMaskQuinary)
            buttonsBitmask |= 16;

        return buttonsBitmask;
    }();

    auto currentTouch = self.mouseTouch;
    auto point = [&] -> WebCore::DoublePoint {
        if (_pointerLockState.isActive)
            return _pointerLockState.lockedCursorPosition.value_or(CGPointZero);
        return [self locationInView:self.view];
    }();
    auto delta = point - WebCore::DoublePoint { [currentTouch previousLocationInView:self.view] };
    // UITouch's timestamp uses mach_absolute_time as its timebase, same as MonotonicTime.
    return WebKit::NativeWebMouseEvent {
        *type,
        button,
        static_cast<unsigned short>(buttons),
        point,
        point,
        static_cast<float>(delta.width()),
        static_cast<float>(delta.height()),
        0,
        static_cast<int>(currentTouch.tapCount),
        modifiers,
        MonotonicTime::fromRawSeconds(currentTouch.timestamp),
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
        [protect(_view) removeGestureRecognizer:gesture];
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
    auto mouseEvent = [self createMouseEventWithType:WebKit::WebEventType::MouseMove wasCancelled:isCancelled];
    if (mouseEvent)
        [protect(_delegate) mouseInteraction:self changedWithEvent:*mouseEvent];
}

- (void)_updateMouseTouches:(NSSet<UITouch *> *)touches
{
    _currentMouseTouch = touches.anyObject;
    _lastLocation = [_mouseTouchGestureRecognizer locationInView:protect(_view).get()];

    std::optional<WebKit::WebEventType> eventType;
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

    auto mouseEvent = [self createMouseEventWithType:eventType wasCancelled:phase == UITouchPhaseCancelled];
    if (!mouseEvent)
        return;

    if (eventType)
        [protect(_delegate) mouseInteraction:self changedWithEvent:*mouseEvent];

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

- (void)dealloc
{
#if ENABLE(POINTER_LOCK)
    [self _stopObservingMouseNotifications];
    [self endPointerLockMouseTracking];
#endif
    [super dealloc];
}

#pragma mark - GameController Pointer Lock Integration

#if ENABLE(POINTER_LOCK)

- (void)beginPointerLockMouseTracking
{
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    if (![[WKMouseDeviceObserver sharedInstance] hasMouseDevice])
        return;
#endif

    _pointerLockState.currentMouse = dynamic_objc_cast<GCMouse>([WebCore::getGCMouseClassSingleton() current]);
    if (!_pointerLockState.currentMouse)
        return;

    _pointerLockState.lockedCursorPosition = [self locationInView:self.view];
    [self _startObservingMouseNotifications];
    [self _setupMouseMovedHandlerForMouse:_pointerLockState.currentMouse.get()];
    [_mouseHoverGestureRecognizer setEnabled:NO];
    _pointerLockState.isActive = true;

    RELEASE_LOG_DEBUG(PointerLock, "Pointer lock mouse tracking enabled with GCMouse: %" PRIVATE_LOG_STRING, [_pointerLockState.currentMouse productCategory].UTF8String);
}

- (void)endPointerLockMouseTracking
{
    if (!_pointerLockState.isActive)
        return;

    [self _stopObservingMouseNotifications];

    [[_pointerLockState.currentMouse mouseInput] setMouseMovedHandler:_pointerLockState.originalMouseMovedHandler];
    _pointerLockState.originalMouseMovedHandler = nil;
    _pointerLockState.currentMouse = nil;

    [_mouseHoverGestureRecognizer setEnabled:YES];
    _pointerLockState.isActive = false;
    _pointerLockState.lockedCursorPosition = std::nullopt;

    LOG(PointerLock, "Pointer lock mouse tracking disabled");
}

- (void)handleGameControllerMouseMove:(float)deltaX deltaY:(float)deltaY
{
    if (!_pointerLockState.isActive)
        return;

    WebCore::DoublePoint lockedPoint { _pointerLockState.lockedCursorPosition.value_or(CGPointZero) };
    WebKit::NativeWebMouseEvent mouseEvent {
        WebKit::WebEventType::MouseMove,
        WebKit::WebMouseEventButton::None,
        0,
        lockedPoint,
        lockedPoint,
        static_cast<float>(deltaX),
        static_cast<float>(-deltaY), // GCMouseInput y-axis aligns with macOS conventions, so we flip.
        0,
        0,
        WebKit::WebIOSEventFactory::webEventModifiersForUIKeyModifierFlags(0),
        MonotonicTime::now(),
        0,
        WebKit::GestureWasCancelled::No,
        WebCore::mousePointerEventType()
    };

    [protect(_delegate) mouseInteraction:self changedWithEvent:mouseEvent];
}

- (void)_startObservingMouseNotifications
{
    if (_pointerLockState.isObservingNotifications)
        return;
    // Only observe when current mouse stops being current - we'll exit pointer lock
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_mouseDidStopBeingCurrent:) name:WebCore::get_GameController_GCMouseDidStopBeingCurrentNotificationSingleton() object:nil];
    _pointerLockState.isObservingNotifications = true;
}

- (void)_stopObservingMouseNotifications
{
    if (!_pointerLockState.isObservingNotifications)
        return;
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebCore::get_GameController_GCMouseDidStopBeingCurrentNotificationSingleton() object:nil];
    _pointerLockState.isObservingNotifications = false;
}

- (void)_mouseDidStopBeingCurrent:(NSNotification *)notification
{
    if (!_pointerLockState.isActive)
        return;
    LOG(PointerLock, "Handling mouse disconnection during pointer lock");
    [protect(_delegate) mouseInteractionDidLoseMouseDeviceDuringPointerLock:self];
    [self endPointerLockMouseTracking];
}

- (void)_setupMouseMovedHandlerForMouse:(GCMouse *)mouse
{
    if (!mouse)
        return;
    _pointerLockState.originalMouseMovedHandler = [[mouse mouseInput] mouseMovedHandler];
    [[mouse mouseInput] setMouseMovedHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKMouseInteraction> { self }](GCMouseInput *mouseInput, float deltaX, float deltaY) {
        if (RetainPtr strongSelf = weakSelf.get())
            [strongSelf handleGameControllerMouseMove:deltaX deltaY:deltaY];
    }).get()];
}

#endif

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)
