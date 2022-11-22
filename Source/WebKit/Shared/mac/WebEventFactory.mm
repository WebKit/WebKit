/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#import "WebEventFactory.h"

#if USE(APPKIT)

#import <WebCore/KeyboardEvent.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/ASCIICType.h>

@interface NSEvent (WebNSEventDetails)
- (NSInteger)_scrollCount;
- (CGFloat)_unacceleratedScrollingDeltaX;
- (CGFloat)_unacceleratedScrollingDeltaY;
@end

namespace WebKit {

// FIXME: This is a huge copy/paste from WebCore/PlatformEventFactoryMac.mm. The code should be merged.

static WebMouseEventButton currentMouseButton()
{
    NSUInteger pressedMouseButtons = [NSEvent pressedMouseButtons];
    if (!pressedMouseButtons)
        return WebMouseEventButton::NoButton;
    if (pressedMouseButtons == 1 << 0)
        return WebMouseEventButton::LeftButton;
    if (pressedMouseButtons == 1 << 1)
        return WebMouseEventButton::RightButton;
    return WebMouseEventButton::MiddleButton;
}

static WebMouseEventButton mouseButtonForEvent(NSEvent *event)
{
    switch ([event type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
        return WebMouseEventButton::LeftButton;
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
        return WebMouseEventButton::RightButton;
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
        return WebMouseEventButton::MiddleButton;
    case NSEventTypePressure:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
        return currentMouseButton();
    default:
        return WebMouseEventButton::NoButton;
    }
}

static unsigned short currentlyPressedMouseButtons()
{
    return static_cast<unsigned short>([NSEvent pressedMouseButtons]);
}

static WebEvent::Type mouseEventTypeForEvent(NSEvent* event)
{
    switch ([event type]) {
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeRightMouseDragged:
        return WebEvent::MouseMove;
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
        return WebEvent::MouseDown;
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
        return WebEvent::MouseUp;
    default:
        return WebEvent::MouseMove;
    }
}

static int clickCountForEvent(NSEvent *event)
{
    switch ([event type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
        return [event clickCount];
    default:
        return 0;
    }
}

static NSPoint globalPointForEvent(NSEvent *event)
{
    switch ([event type]) {
    case NSEventTypePressure:
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeScrollWheel:
        return WebCore::globalPoint([event locationInWindow], [event window]);
    default:
        return NSZeroPoint;
    }
}

static NSPoint pointForEvent(NSEvent *event, NSView *windowView)
{
    switch ([event type]) {
    case NSEventTypePressure:
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeScrollWheel: {
        // Note: This will have its origin at the bottom left of the window unless windowView is flipped.
        // In those cases, the Y coordinate gets flipped by Widget::convertFromContainingWindow.
        NSPoint location = [event locationInWindow];
        if (windowView)
            location = [windowView convertPoint:location fromView:nil];
        return location;
    }
    default:
        return NSZeroPoint;
    }
}

static WebWheelEvent::Phase phaseForEvent(NSEvent *event)
{
    uint32_t phase = WebWheelEvent::PhaseNone;
    if ([event phase] & NSEventPhaseBegan)
        phase |= WebWheelEvent::PhaseBegan;
    if ([event phase] & NSEventPhaseStationary)
        phase |= WebWheelEvent::PhaseStationary;
    if ([event phase] & NSEventPhaseChanged)
        phase |= WebWheelEvent::PhaseChanged;
    if ([event phase] & NSEventPhaseEnded)
        phase |= WebWheelEvent::PhaseEnded;
    if ([event phase] & NSEventPhaseCancelled)
        phase |= WebWheelEvent::PhaseCancelled;
    if ([event phase] & NSEventPhaseMayBegin)
        phase |= WebWheelEvent::PhaseMayBegin;

    return static_cast<WebWheelEvent::Phase>(phase);
}

static WebWheelEvent::Phase momentumPhaseForEvent(NSEvent *event)
{
    uint32_t phase = WebWheelEvent::PhaseNone; 

    if ([event momentumPhase] & NSEventPhaseBegan)
        phase |= WebWheelEvent::PhaseBegan;
    if ([event momentumPhase] & NSEventPhaseStationary)
        phase |= WebWheelEvent::PhaseStationary;
    if ([event momentumPhase] & NSEventPhaseChanged)
        phase |= WebWheelEvent::PhaseChanged;
    if ([event momentumPhase] & NSEventPhaseEnded)
        phase |= WebWheelEvent::PhaseEnded;
    if ([event momentumPhase] & NSEventPhaseCancelled)
        phase |= WebWheelEvent::PhaseCancelled;

    return static_cast<WebWheelEvent::Phase>(phase);
}

static inline String textFromEvent(NSEvent* event, bool replacesSoftSpace)
{
    if (replacesSoftSpace)
        return emptyString();
    if ([event type] == NSEventTypeFlagsChanged)
        return emptyString();
    return String([event characters]);
}

static inline String unmodifiedTextFromEvent(NSEvent* event, bool replacesSoftSpace)
{
    if (replacesSoftSpace)
        return emptyString();
    if ([event type] == NSEventTypeFlagsChanged)
        return emptyString();
    return String([event charactersIgnoringModifiers]);
}

static bool isKeypadEvent(NSEvent* event)
{
    // Check that this is the type of event that has a keyCode.
    switch ([event type]) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged:
        break;
    default:
        return false;
    }

    switch ([event keyCode]) {
    case 71: // Clear
    case 81: // =
    case 75: // /
    case 67: // *
    case 78: // -
    case 69: // +
    case 76: // Enter
    case 65: // .
    case 82: // 0
    case 83: // 1
    case 84: // 2
    case 85: // 3
    case 86: // 4
    case 87: // 5
    case 88: // 6
    case 89: // 7
    case 91: // 8
    case 92: // 9
        return true;
    }
     
     return false;
}

static inline bool isKeyUpEvent(NSEvent *event)
{
    if ([event type] != NSEventTypeFlagsChanged)
        return [event type] == NSEventTypeKeyUp;
    // FIXME: This logic fails if the user presses both Shift keys at once, for example:
    // we treat releasing one of them as keyDown.
    switch ([event keyCode]) {
    case 54: // Right Command
    case 55: // Left Command
        return !([event modifierFlags] & NSEventModifierFlagCommand);

    case 57: // Capslock
        return !([event modifierFlags] & NSEventModifierFlagCapsLock);

    case 56: // Left Shift
    case 60: // Right Shift
        return !([event modifierFlags] & NSEventModifierFlagShift);

    case 58: // Left Alt
    case 61: // Right Alt
        return !([event modifierFlags] & NSEventModifierFlagOption);

    case 59: // Left Ctrl
    case 62: // Right Ctrl
        return !([event modifierFlags] & NSEventModifierFlagControl);

    case 63: // Function
        return !([event modifierFlags] & NSEventModifierFlagFunction);
    }
    return false;
}

static int typeForEvent(NSEvent *event)
{
    return static_cast<int>([NSMenu menuTypeForEvent:event]);
}

bool WebEventFactory::shouldBeHandledAsContextClick(const WebCore::PlatformMouseEvent& event)
{
    return (static_cast<NSMenuType>(event.menuTypeForEvent()) == NSMenuTypeContextMenu);
}

WebMouseEvent WebEventFactory::createWebMouseEvent(NSEvent *event, NSEvent *lastPressureEvent, NSView *windowView)
{
    NSPoint position = pointForEvent(event, windowView);
    NSPoint globalPosition = globalPointForEvent(event);

    WebEvent::Type type = mouseEventTypeForEvent(event);
    if ([event type] == NSEventTypePressure) {
        // Since AppKit doesn't send mouse events for force down or force up, we have to use the current pressure
        // event and lastPressureEvent to detect if this is MouseForceDown, MouseForceUp, or just MouseForceChanged.
        if (lastPressureEvent.stage == 1 && event.stage == 2)
            type = WebEvent::MouseForceDown;
        else if (lastPressureEvent.stage == 2 && event.stage == 1)
            type = WebEvent::MouseForceUp;
        else
            type = WebEvent::MouseForceChanged;
    }

    WebMouseEventButton button = mouseButtonForEvent(event);
    unsigned short buttons = currentlyPressedMouseButtons();
    float deltaX = [event deltaX];
    float deltaY = [event deltaY];
    float deltaZ = [event deltaZ];
    int clickCount = clickCountForEvent(event);
    auto modifiers = webEventModifiersForNSEventModifierFlags(event.modifierFlags);
    auto timestamp = WebCore::eventTimeStampSince1970(event.timestamp);
    int eventNumber = [event eventNumber];
    int menuTypeForEvent = typeForEvent(event);

    int stage = [event type] == NSEventTypePressure ? event.stage : lastPressureEvent.stage;
    double pressure = [event type] == NSEventTypePressure ? event.pressure : lastPressureEvent.pressure;
    double force = pressure + stage;

    return WebMouseEvent({ type, modifiers, timestamp }, button, buttons, WebCore::IntPoint(position), WebCore::IntPoint(globalPosition), deltaX, deltaY, deltaZ, clickCount, force, WebMouseEventSyntheticClickType::NoTap, eventNumber, menuTypeForEvent);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(NSEvent *event, NSView *windowView)
{
    NSPoint position = pointForEvent(event, windowView);
    NSPoint globalPosition = globalPointForEvent(event);

    BOOL continuous;
    float deltaX = 0;
    float deltaY = 0;
    float wheelTicksX = 0;
    float wheelTicksY = 0;

    WebCore::getWheelEventDeltas(event, deltaX, deltaY, continuous);
    
    if (continuous) {
        // smooth scroll events
        wheelTicksX = deltaX / static_cast<float>(WebCore::Scrollbar::pixelsPerLineStep());
        wheelTicksY = deltaY / static_cast<float>(WebCore::Scrollbar::pixelsPerLineStep());
    } else {
        // plain old wheel events
        wheelTicksX = deltaX;
        wheelTicksY = deltaY;
        deltaX *= static_cast<float>(WebCore::Scrollbar::pixelsPerLineStep());
        deltaY *= static_cast<float>(WebCore::Scrollbar::pixelsPerLineStep());
    }

    WebWheelEvent::Granularity granularity  = WebWheelEvent::ScrollByPixelWheelEvent;
    bool directionInvertedFromDevice        = [event isDirectionInvertedFromDevice];
    WebWheelEvent::Phase phase              = phaseForEvent(event);
    WebWheelEvent::Phase momentumPhase      = momentumPhaseForEvent(event);
    bool hasPreciseScrollingDeltas          = continuous;

    uint32_t scrollCount;
    WebCore::FloatSize unacceleratedScrollingDelta;

    static bool nsEventSupportsScrollCount = [NSEvent instancesRespondToSelector:@selector(_scrollCount)];
    if (nsEventSupportsScrollCount) {
        scrollCount = [event _scrollCount];
        unacceleratedScrollingDelta = WebCore::FloatSize([event _unacceleratedScrollingDeltaX], [event _unacceleratedScrollingDeltaY]);
    } else {
        scrollCount = 0;
        unacceleratedScrollingDelta = WebCore::FloatSize(deltaX, deltaY);
    }

    auto modifiers = webEventModifiersForNSEventModifierFlags(event.modifierFlags);
    auto timestamp = WebCore::eventTimeStampSince1970(event.timestamp);
    
    auto ioHIDEventWallTime = timestamp;
    std::optional<WebCore::FloatSize> rawPlatformDelta;
    auto momentumEndType = WebWheelEvent::MomentumEndType::Unknown;
    
    ([&] {
        auto cgEvent = event.CGEvent;
        if (!cgEvent)
            return;

        auto ioHIDEvent = adoptCF(CGEventCopyIOHIDEvent(cgEvent));
        if (!ioHIDEvent)
            return;

        auto ioHIDEventTimestamp = IOHIDEventGetTimeStamp(ioHIDEvent.get()); // IOEventRef timestamp is mach_absolute_time units.
        auto monotonicIOHIDEventTimestamp = MonotonicTime::fromMachAbsoluteTime(ioHIDEventTimestamp).secondsSinceEpoch().seconds();
        ioHIDEventWallTime = WebCore::eventTimeStampSince1970(monotonicIOHIDEventTimestamp);
        
        rawPlatformDelta = { WebCore::FloatSize(-IOHIDEventGetFloatValue(ioHIDEvent.get(), kIOHIDEventFieldScrollX), -IOHIDEventGetFloatValue(ioHIDEvent.get(), kIOHIDEventFieldScrollY)) };

#if HAVE(PLATFORM_SCROLL_MOMENTUM_INTERRUPTION_REASON)
        bool momentumWasInterrupted = IOHIDEventGetScrollMomentum(ioHIDEvent.get()) & kIOHIDEventScrollMomentumInterrupted;
        momentumEndType = momentumWasInterrupted ? WebWheelEvent::MomentumEndType::Interrupted : WebWheelEvent::MomentumEndType::Natural;
#endif
    })();

    if (phase == WebWheelEvent::PhaseCancelled) {
        deltaX = 0;
        deltaY = 0;
        wheelTicksX = 0;
        wheelTicksY = 0;
        unacceleratedScrollingDelta = { };
        rawPlatformDelta = std::nullopt;
    }

    return WebWheelEvent({ WebEvent::Wheel, modifiers, timestamp }, WebCore::IntPoint(position), WebCore::IntPoint(globalPosition), WebCore::FloatSize(deltaX, deltaY), WebCore::FloatSize(wheelTicksX, wheelTicksY),
        granularity, directionInvertedFromDevice, phase, momentumPhase, hasPreciseScrollingDeltas,
        scrollCount, unacceleratedScrollingDelta, ioHIDEventWallTime, rawPlatformDelta, momentumEndType);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(NSEvent *event, bool handledByInputMethod, bool replacesSoftSpace, const Vector<WebCore::KeypressCommand>& commands)
{
    WebEvent::Type type             = isKeyUpEvent(event) ? WebEvent::KeyUp : WebEvent::KeyDown;
    String text                     = textFromEvent(event, replacesSoftSpace);
    String unmodifiedText           = unmodifiedTextFromEvent(event, replacesSoftSpace);
    String key                      = WebCore::keyForKeyEvent(event);
    String code                     = WebCore::codeForKeyEvent(event);
    String keyIdentifier            = WebCore::keyIdentifierForKeyEvent(event);
    int windowsVirtualKeyCode       = WebCore::windowsKeyCodeForKeyEvent(event);
    int nativeVirtualKeyCode        = [event keyCode];
    int macCharCode                 = WebCore::keyCharForEvent(event);
    bool autoRepeat                 = [event type] != NSEventTypeFlagsChanged && [event isARepeat];
    bool isKeypad                   = isKeypadEvent(event);
    bool isSystemKey                = false; // SystemKey is always false on the Mac.
    auto modifiers = webEventModifiersForNSEventModifierFlags(event.modifierFlags);
    auto timestamp                  = WebCore::eventTimeStampSince1970(event.timestamp);

    // Always use 13 for Enter/Return -- we don't want to use AppKit's different character for Enter.
    if (windowsVirtualKeyCode == VK_RETURN) {
        text = "\r"_s;
        unmodifiedText = text;
    }

    // AppKit sets text to "\x7F" for backspace, but the correct KeyboardEvent character code is 8.
    if (windowsVirtualKeyCode == VK_BACK) {
        text = "\x8"_s;
        unmodifiedText = text;
    }

    // Always use 9 for Tab -- we don't want to use AppKit's different character for shift-tab.
    if (windowsVirtualKeyCode == VK_TAB) {
        text = "\x9"_s;
        unmodifiedText = text;
    }

    return WebKeyboardEvent({ type, modifiers, timestamp }, text, unmodifiedText, key, code, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, handledByInputMethod, commands, autoRepeat, isKeypad, isSystemKey);
}

OptionSet<WebKit::WebEventModifier> WebEventFactory::webEventModifiersForNSEventModifierFlags(NSEventModifierFlags modifierFlags)
{
    OptionSet<WebEventModifier> modifiers;
    if (modifierFlags & NSEventModifierFlagCapsLock)
        modifiers.add(WebEventModifier::CapsLockKey);
    if (modifierFlags & NSEventModifierFlagShift)
        modifiers.add(WebEventModifier::ShiftKey);
    if (modifierFlags & NSEventModifierFlagControl)
        modifiers.add(WebEventModifier::ControlKey);
    if (modifierFlags & NSEventModifierFlagOption)
        modifiers.add(WebEventModifier::AltKey);
    if (modifierFlags & NSEventModifierFlagCommand)
        modifiers.add(WebEventModifier::MetaKey);
    return modifiers;
}

NSEventModifierFlags WebEventFactory::toNSEventModifierFlags(OptionSet<WebKit::WebEventModifier> modifiers)
{
    NSEventModifierFlags modifierFlags = 0;
    if (modifiers.contains(WebKit::WebEventModifier::CapsLockKey))
        modifierFlags |= NSEventModifierFlagCapsLock;
    if (modifiers.contains(WebKit::WebEventModifier::ShiftKey))
        modifierFlags |= NSEventModifierFlagShift;
    if (modifiers.contains(WebKit::WebEventModifier::ControlKey))
        modifierFlags |= NSEventModifierFlagControl;
    if (modifiers.contains(WebKit::WebEventModifier::AltKey))
        modifierFlags |= NSEventModifierFlagOption;
    if (modifiers.contains(WebKit::WebEventModifier::MetaKey))
        modifierFlags |= NSEventModifierFlagCommand;
    return modifierFlags;
}

NSInteger WebEventFactory::toNSButtonNumber(WebKit::WebMouseEventButton mouseButton)
{
    switch (mouseButton) {
    case WebKit::WebMouseEventButton::NoButton:
        return 0;
    case WebKit::WebMouseEventButton::LeftButton:
        return 1 << 0;
    case WebKit::WebMouseEventButton::RightButton:
        return 1 << 1;
    case WebKit::WebMouseEventButton::MiddleButton:
        return 1 << 2;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebKit

#endif // USE(APPKIT)
