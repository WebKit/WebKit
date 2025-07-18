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

#import "WebEventConversion.h"
#import <WebCore/KeyboardEvent.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSEventSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/ASCIICType.h>
#import <wtf/UUID.h>

namespace WebKit {

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
    NSPoint position = WebCore::pointForEvent(event, windowView);
    NSPoint globalPosition = WebCore::globalPointForEvent(event);

    WebEventType type = kit(WebCore::mouseEventTypeForEvent(event));
    if ([event type] == NSEventTypePressure) {
        // Since AppKit doesn't send mouse events for force down or force up, we have to use the current pressure
        // event and lastPressureEvent to detect if this is MouseForceDown, MouseForceUp, or just MouseForceChanged.
        if (lastPressureEvent.stage == 1 && event.stage == 2)
            type = WebEventType::MouseForceDown;
        else if (lastPressureEvent.stage == 2 && event.stage == 1)
            type = WebEventType::MouseForceUp;
        else
            type = WebEventType::MouseForceChanged;
    }

    WebMouseEventButton button = kit(WebCore::mouseButtonForEvent(event));
    unsigned short buttons = WebCore::currentlyPressedMouseButtons();
    float deltaX = [event deltaX];
    float deltaY = [event deltaY];
    float deltaZ = [event deltaZ];
    int clickCount = WebCore::clickCountForEvent(event);
    auto modifiers = kit(WebCore::modifiersForEvent(event));
    auto timestamp = WebCore::eventTimeStampSince1970(event.timestamp);
    int eventNumber = [event eventNumber];
    int menuTypeForEvent = typeForEvent(event);

    int stage = [event type] == NSEventTypePressure ? event.stage : lastPressureEvent.stage;
    double pressure = [event type] == NSEventTypePressure ? event.pressure : lastPressureEvent.pressure;
    double force = pressure + stage;

    auto unadjustedMovementDelta = WebCore::unadjustedMovementForEvent(event);

    return WebMouseEvent({ type, modifiers, timestamp, WTF::UUID::createVersion4() }, button, buttons, WebCore::IntPoint(position), WebCore::IntPoint(globalPosition), deltaX, deltaY, deltaZ, clickCount, force, WebMouseEventSyntheticClickType::NoTap, eventNumber, menuTypeForEvent, GestureWasCancelled::No, unadjustedMovementDelta);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(NSEvent *event, NSView *windowView)
{
    NSPoint position = WebCore::pointForEvent(event, windowView);
    NSPoint globalPosition = WebCore::globalPointForEvent(event);

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

    auto modifiers = kit(WebCore::modifiersForEvent(event));
    auto timestamp = WebCore::eventTimeStampSince1970(event.timestamp);
    
    auto ioHIDEventWallTime = timestamp;
    std::optional<WebCore::FloatSize> rawPlatformDelta;
    auto momentumEndType = WebWheelEvent::MomentumEndType::Unknown;
    
    ([&] {
        RetainPtr<CGEventRef> cgEvent = event.CGEvent;
        if (!cgEvent)
            return;

        auto ioHIDEvent = adoptCF(CGEventCopyIOHIDEvent(cgEvent.get()));
        if (!ioHIDEvent)
            return;

        auto ioHIDEventTimestamp = IOHIDEventGetTimeStamp(ioHIDEvent.get()); // IOEventRef timestamp is mach_absolute_time units.
        auto monotonicIOHIDEventTimestamp = MonotonicTime::fromMachAbsoluteTime(ioHIDEventTimestamp).secondsSinceEpoch().seconds();
        ioHIDEventWallTime = WebCore::eventTimeStampSince1970(monotonicIOHIDEventTimestamp);
        
        rawPlatformDelta = { WebCore::FloatSize(-IOHIDEventGetFloatValue(ioHIDEvent.get(), kIOHIDEventFieldScrollX), -IOHIDEventGetFloatValue(ioHIDEvent.get(), kIOHIDEventFieldScrollY)) };

        if (IOHIDEventGetScrollMomentum(ioHIDEvent.get()) & kIOHIDEventScrollMomentumWillBegin) {
            ASSERT(momentumPhase == WebWheelEvent::Phase::PhaseNone && phase == WebWheelEvent::Phase::PhaseEnded);
            momentumPhase = WebWheelEvent::Phase::PhaseWillBegin;
        }

        bool momentumWasInterrupted = IOHIDEventGetScrollMomentum(ioHIDEvent.get()) & kIOHIDEventScrollMomentumInterrupted;
        momentumEndType = momentumWasInterrupted ? WebWheelEvent::MomentumEndType::Interrupted : WebWheelEvent::MomentumEndType::Natural;
    })();

    if (phase == WebWheelEvent::PhaseCancelled) {
        deltaX = 0;
        deltaY = 0;
        wheelTicksX = 0;
        wheelTicksY = 0;
        unacceleratedScrollingDelta = { };
        rawPlatformDelta = std::nullopt;
    }

    return WebWheelEvent({ WebEventType::Wheel, modifiers, timestamp, WTF::UUID::createVersion4() }, WebCore::IntPoint(position), WebCore::IntPoint(globalPosition), WebCore::FloatSize(deltaX, deltaY), WebCore::FloatSize(wheelTicksX, wheelTicksY),
        granularity, directionInvertedFromDevice, phase, momentumPhase, hasPreciseScrollingDeltas,
        scrollCount, unacceleratedScrollingDelta, ioHIDEventWallTime, rawPlatformDelta, momentumEndType);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(NSEvent *event, bool handledByInputMethod, bool replacesSoftSpace, const Vector<WebCore::KeypressCommand>& commands)
{
    WebEventType type = WebCore::isKeyUpEvent(event) ? WebEventType::KeyUp : WebEventType::KeyDown;
    String text = WebCore::textFromEvent(event, replacesSoftSpace);
    String unmodifiedText = WebCore::unmodifiedTextFromEvent(event, replacesSoftSpace);
    String key = WebCore::keyForKeyEvent(event);
    String code = WebCore::codeForKeyEvent(event);
    String keyIdentifier = WebCore::keyIdentifierForKeyEvent(event);
    int windowsVirtualKeyCode = WebCore::windowsKeyCodeForKeyEvent(event);
    int nativeVirtualKeyCode = [event keyCode];
    int macCharCode = WebCore::keyCharForEvent(event);
    bool autoRepeat = [event type] != NSEventTypeFlagsChanged && [event isARepeat];
    bool isKeypad = WebCore::isKeypadEvent(event);
    bool isSystemKey = false; // SystemKey is always false on the Mac.
    auto modifiers = kit(WebCore::modifiersForEvent(event));
    auto timestamp = WebCore::eventTimeStampSince1970(event.timestamp);

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

    return WebKeyboardEvent({ type, modifiers, timestamp, WTF::UUID::createVersion4() }, text, unmodifiedText, key, code, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, handledByInputMethod, commands, autoRepeat, isKeypad, isSystemKey);
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
    case WebKit::WebMouseEventButton::None:
        return 0;
    case WebKit::WebMouseEventButton::Left:
        return 1 << 0;
    case WebKit::WebMouseEventButton::Right:
        return 1 << 1;
    case WebKit::WebMouseEventButton::Middle:
        return 1 << 2;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebKit

#endif // USE(APPKIT)
