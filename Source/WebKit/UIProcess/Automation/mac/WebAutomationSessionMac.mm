/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#import "WebAutomationSession.h"

#if PLATFORM(MAC)

#import "Logging.h"
#import "WebAutomationSessionMacros.h"
#import "WebEventConversion.h"
#import "WebEventFactory.h"
#import "WebInspectorUIProxy.h"
#import "WebPageProxy.h"
#import "WKWebViewPrivate.h"
#import "_WKAutomationSession.h"
#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>
#import <WebCore/IntPoint.h>
#import <WebCore/IntSize.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/PlatformMouseEvent.h>
#import <objc/runtime.h>
#import <pal/spi/mac/NSEventSPI.h>
#import <wtf/Scope.h>

namespace WebKit {
using namespace WebCore;

#pragma mark Commands for 'PLATFORM(MAC)'

void WebAutomationSession::inspectBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<bool>&& enableAutoCapturing, Inspector::CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    if (auto pendingCallback = m_pendingInspectorCallbacksPerPage.take(page->identifier()))
        pendingCallback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout)));

    m_pendingInspectorCallbacksPerPage.set(page->identifier(), WTF::move(callback));

    // Don't bring the inspector to front since this may be done automatically.
    // We just want it loaded so it can pause if a breakpoint is hit during a command.
    if (RefPtr inspector = page->inspector()) {
        inspector->show();

        // Start collecting profile information immediately so the entire session is captured.
        if (enableAutoCapturing && *enableAutoCapturing)
            inspector->togglePageProfiling();
    }
}

#pragma mark AppKit Event Simulation Support

static const NSInteger synthesizedMouseEventMagicEventNumber = 0;
static const void *synthesizedAutomationEventAssociatedObjectKey = &synthesizedAutomationEventAssociatedObjectKey;

void WebAutomationSession::sendSynthesizedEventsToPage(WebPageProxy& page, NSArray *eventsToSend)
{
    RetainPtr window = page.platformWindow();
    auto webView = page.cocoaView();

    // +[NSEvent pressedMouseButtons] does not account for the NSEvent objects created through eventSender JS in tests.
    // As such, that method always returns 0. To fix this, we swizzle out +[NSEvent pressedMouseButtons], keep track of
    // the mouse button currently being pressed down, and supply the appropriate return value as specified in documentation.

    auto methodToSwizzle = class_getClassMethod(RetainPtr { objc_getMetaClass(NSStringFromClass([NSEvent class]).UTF8String) }.get(), @selector(pressedMouseButtons));
    // FIXME: This looks like a safer cpp false positive. It wants me to retain the Objective C block passed to imp_implementationWithBlock(),
    // which gets implicitly constructed from a C++ lambda on this line.
    SUPPRESS_UNRETAINED_ARG auto originalImplementation = method_setImplementation(methodToSwizzle, imp_implementationWithBlock([&mouseButtonsCurrentlyDown = m_mouseButtonsCurrentlyDown] {
        NSUInteger mouseButtons = 0;
        static constexpr std::array<MouseButton, 5> potentialMouseButtons { MouseButton::Left, MouseButton::Right, MouseButton::Middle, MouseButton::Back, MouseButton::Forward };
        for (std::size_t idx = 0; idx < potentialMouseButtons.size(); ++idx) {
            if (mouseButtonsCurrentlyDown.getOptional(potentialMouseButtons[idx]).value_or(false))
                mouseButtons += (1 << idx);
        }
        return mouseButtons;
    }));
    auto patchOriginalFunction = makeScopeExit([&methodToSwizzle, &originalImplementation] {
        method_setImplementation(methodToSwizzle, originalImplementation);
    });

    for (NSEvent *event in eventsToSend) {
        LOG(Automation, "Sending event[%p] to window[%p]: %@", event, window.get(), event);

        // Take focus back in case the Inspector became focused while the prior command or
        // NSEvent was delivered to the window.
        [window makeKeyAndOrderFront:nil];
        page.makeFirstResponder();

        markEventAsSynthesizedForAutomation(event);
        [window sendEvent:event];

        // NSEventTypeMouseMoved events are not forwarded from the WKWebView to the underlying view implementation,
        // which prevents these synthetic events from being dispatched as events in JavaScript. We still dispatch the
        // event to the window as well to avoid any side effects of providing incomplete events to other parts of the
        // window. NSEventTypeMouseEntered and NSEventTypeMouseExited events are also affected, but we do not currently
        // dispatch events with those types.
        if (event.type == NSEventTypeMouseMoved) {
            LOG(Automation, "Simulating event[%p] of type NSEventTypeMouseMoved for web view[%p]: %@", event, webView.get(), event);
            [webView _simulateMouseMove:event];
        }
    }
}

void WebAutomationSession::markEventAsSynthesizedForAutomation(NSEvent *event)
{
    objc_setAssociatedObject(event, &synthesizedAutomationEventAssociatedObjectKey, m_sessionIdentifier.createNSString().get(), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

bool WebAutomationSession::wasEventSynthesizedForAutomation(NSEvent *event)
{
    RetainPtr<NSString> senderSessionIdentifier = objc_getAssociatedObject(event, &synthesizedAutomationEventAssociatedObjectKey);
    if ([senderSessionIdentifier isEqualToString:m_sessionIdentifier.createNSString().get()])
        return true;

    switch (event.type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeMouseMoved:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeRightMouseUp:
        // Use this as a backup for checking mouse events, which are frequently copied
        // and/or faked by AppKit, causing them to lose their associated object tag.
        return event.eventNumber == synthesizedMouseEventMagicEventNumber;
    default:
        break;
    }

    return false;
}

#pragma mark Platform-dependent Implementations

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) || ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

static WebCore::IntPoint viewportLocationToWindowLocation(WebCore::IntPoint locationInViewport, WebPageProxy& page)
{
    IntRect windowRect;

    auto obscuredContentInsets = page.obscuredContentInsets();
    IntPoint locationInView = locationInViewport + IntPoint(obscuredContentInsets.left(), obscuredContentInsets.top());
    page.rootViewToWindow(IntRect(locationInView, IntSize()), windowRect);
    return windowRect.location();
}

#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) || ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

static unsigned nsEventButtonNumberFromAutomationMouseButton(MouseButton button)
{
    switch (button) {
    case MouseButton::Right:
        return 1;
    case MouseButton::Middle:
        return 2;
    case MouseButton::Back:
        return 3;
    case MouseButton::Forward:
        return 4;
    default:
        return 0;
    }
}

static WebMouseEventButton automationMouseButtonToPlatformMouseButton(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:   return WebMouseEventButton::Left;
    case MouseButton::Middle: return WebMouseEventButton::Middle;
    case MouseButton::Right:  return WebMouseEventButton::Right;
    case MouseButton::None:   return WebMouseEventButton::None;
    default: RELEASE_ASSERT_NOT_REACHED();
    }
}

static RetainPtr<NSEvent> simulatedMouseEvent(NSEventType type, WebCore::IntPoint locationInWindow, NSEventModifierFlags modifiers, NSTimeInterval timestamp, NSInteger windowNumber, unsigned clickCount, float pressure, MouseButton button)
{
    RetainPtr event = [NSEvent mouseEventWithType:type location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:synthesizedMouseEventMagicEventNumber clickCount:clickCount pressure:pressure];
    if (type != NSEventTypeOtherMouseDown && type != NSEventTypeOtherMouseUp)
        return event;

    RetainPtr cgEvent = [event CGEvent];
    CGEventSetIntegerValueField(cgEvent.get(), kCGMouseEventButtonNumber, nsEventButtonNumberFromAutomationMouseButton(button));
    return [NSEvent eventWithCGEvent:cgEvent.get()];
}

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, MouseButton button, const WebCore::IntPoint& locationInViewport, OptionSet<WebEventModifier> keyModifiers, const String& pointerType)
{
    UNUSED_PARAM(pointerType);

    auto locationInWindow = viewportLocationToWindowLocation(locationInViewport, page);

    NSEventModifierFlags modifiers = WebEventFactory::toNSEventModifierFlags(keyModifiers);
    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    RetainPtr window = page.platformWindow();
    NSInteger windowNumber = window.get().windowNumber;

    NSEventType downEventType = (NSEventType)0;
    NSEventType dragEventType = (NSEventType)0;
    NSEventType upEventType = (NSEventType)0;
    switch (automationMouseButtonToPlatformMouseButton(button)) {
    case WebMouseEventButton::None:
        downEventType = upEventType = dragEventType = NSEventTypeMouseMoved;
        break;
    case WebMouseEventButton::Left:
        downEventType = NSEventTypeLeftMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeLeftMouseUp;
        break;
    case WebMouseEventButton::Back:
    case WebMouseEventButton::Forward:
    case WebMouseEventButton::Middle:
        downEventType = NSEventTypeOtherMouseDown;
        dragEventType = NSEventTypeOtherMouseDragged;
        upEventType = NSEventTypeOtherMouseUp;
        break;
    case WebMouseEventButton::Right:
        downEventType = NSEventTypeRightMouseDown;
        dragEventType = NSEventTypeRightMouseDragged;
        upEventType = NSEventTypeRightMouseUp;
        break;
    }

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    switch (interaction) {
    case MouseInteraction::Move: {
        ASSERT(dragEventType);
        RetainPtr event = simulatedMouseEvent(dragEventType, locationInWindow, modifiers, timestamp, windowNumber, 0, 0, button);
        RetainPtr<CGEventRef> cgEvent = event.get().CGEvent;
        if (!m_lastPosition)
            updateLastPosition(locationInWindow);
        CGEventSetIntegerValueField(cgEvent.get(), kCGMouseEventDeltaX, locationInWindow.x() - m_lastPosition->x());
        CGEventSetIntegerValueField(cgEvent.get(), kCGMouseEventDeltaY, -1 * (locationInWindow.y() - m_lastPosition->y()));
        event = [NSEvent eventWithCGEvent:cgEvent.get()];
        [eventsToBeSent addObject:event.get()];
        break;
    }
    case MouseInteraction::Down: {
        ASSERT(downEventType);
        updateClickCount(button, locationInWindow);
        m_mouseButtonsCurrentlyDown.set(button, true);
        RetainPtr event = simulatedMouseEvent(downEventType, locationInWindow, modifiers, timestamp, windowNumber, m_clickCount, WebCore::ForceAtClick, button);
        [eventsToBeSent addObject:event.get()];
        break;
    }
    case MouseInteraction::Up: {
        ASSERT(upEventType);
        m_mouseButtonsCurrentlyDown.set(button, false);
        RetainPtr event = simulatedMouseEvent(upEventType, locationInWindow, modifiers, timestamp, windowNumber, m_clickCount, 0.f, button);
        [eventsToBeSent addObject:event.get()];
        break;
    }
    case MouseInteraction::SingleClick: {
        ASSERT(upEventType);
        ASSERT(downEventType);

        RetainPtr downEvent = simulatedMouseEvent(downEventType, locationInWindow, modifiers, timestamp, windowNumber, 1, WebCore::ForceAtClick, button);
        RetainPtr upEvent = simulatedMouseEvent(downEventType, locationInWindow, modifiers, timestamp, windowNumber, 1, 0.f, button);

        // Send separate down and up events. WebCore will see this as a single-click event.
        [eventsToBeSent addObject:downEvent.get()];
        [eventsToBeSent addObject:upEvent.get()];
        break;
    }
    case MouseInteraction::DoubleClick: {
        ASSERT(upEventType);
        ASSERT(downEventType);

        RetainPtr downEventOne = simulatedMouseEvent(downEventType, locationInWindow, modifiers, timestamp, windowNumber, 1, WebCore::ForceAtClick, button);
        RetainPtr upEventOne = simulatedMouseEvent(upEventType, locationInWindow, modifiers, timestamp, windowNumber, 1, 0.f, button);
        RetainPtr downEventTwo = simulatedMouseEvent(downEventType, locationInWindow, modifiers, timestamp, windowNumber, 2, WebCore::ForceAtClick, button);
        RetainPtr upEventTwo = simulatedMouseEvent(upEventType, locationInWindow, modifiers, timestamp, windowNumber, 2, 0.f, button);

        // Send multiple down and up events with proper click count.
        // WebCore will see this as a single-click event then double-click event.
        [eventsToBeSent addObject:downEventOne.get()];
        [eventsToBeSent addObject:upEventOne.get()];
        [eventsToBeSent addObject:downEventTwo.get()];
        [eventsToBeSent addObject:upEventTwo.get()];
    }
    }

    updateLastPosition(locationInWindow);

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

OptionSet<WebEventModifier> WebAutomationSession::platformWebModifiersFromRaw(WebPageProxy&, unsigned modifiers)
{
    return kit(WebCore::modifiersForModifierFlags(modifiers));
}

#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
static bool virtualKeyHasStickyModifier(VirtualKey key)
{
    // Returns whether the key's modifier flags should affect other events while pressed down.
    switch (key) {
    case VirtualKey::Shift:
    case VirtualKey::ShiftRight:
    case VirtualKey::Control:
    case VirtualKey::ControlRight:
    case VirtualKey::Alternate:
    case VirtualKey::AlternateRight:
    case VirtualKey::Meta:
    case VirtualKey::MetaRight:
    case VirtualKey::Command:
    case VirtualKey::CommandRight:
        return true;

    default:
        return false;
    }
}

// `keyCode` 0 is `kVK_ANSI_A` in <Carbon/Events.h>, which means events with that `keyCode` will incorrectly report
// themselves as having a `KeyboardEvent.prototype.code` of `KeyA` in JavaScript due to the mapping in
// PlatformEventFactoryMac::codeForKeyEvent.
static constexpr unsigned short unknownKeyCode = USHRT_MAX;

static int keyCodeForCharKey(CharKey charKey)
{
    if (charKey.length() != 1)
        return unknownKeyCode;

    switch (charKey[0]) {
    case 'q':
    case 'Q':
        return kVK_ANSI_Q;
    case 'w':
    case 'W':
        return kVK_ANSI_W;
    case 'e':
    case 'E':
        return kVK_ANSI_E;
    case 'r':
    case 'R':
        return kVK_ANSI_R;
    case 't':
    case 'T':
        return kVK_ANSI_T;
    case 'y':
    case 'Y':
        return kVK_ANSI_Y;
    case 'u':
    case 'U':
        return kVK_ANSI_U;
    case 'i':
    case 'I':
        return kVK_ANSI_I;
    case 'o':
    case 'O':
        return kVK_ANSI_O;
    case 'p':
    case 'P':
        return kVK_ANSI_P;
    case 'a':
    case 'A':
        return kVK_ANSI_A;
    case 's':
    case 'S':
        return kVK_ANSI_S;
    case 'd':
    case 'D':
        return kVK_ANSI_D;
    case 'f':
    case 'F':
        return kVK_ANSI_F;
    case 'g':
    case 'G':
        return kVK_ANSI_G;
    case 'h':
    case 'H':
        return kVK_ANSI_H;
    case 'j':
    case 'J':
        return kVK_ANSI_J;
    case 'k':
    case 'K':
        return kVK_ANSI_K;
    case 'l':
    case 'L':
        return kVK_ANSI_L;
    case 'z':
    case 'Z':
        return kVK_ANSI_Z;
    case 'x':
    case 'X':
        return kVK_ANSI_X;
    case 'c':
    case 'C':
        return kVK_ANSI_C;
    case 'v':
    case 'V':
        return kVK_ANSI_V;
    case 'b':
    case 'B':
        return kVK_ANSI_B;
    case 'n':
    case 'N':
        return kVK_ANSI_N;
    case 'm':
    case 'M':
        return kVK_ANSI_M;
    case '1':
        return kVK_ANSI_1;
    case '2':
        return kVK_ANSI_2;
    case '3':
        return kVK_ANSI_3;
    case '4':
        return kVK_ANSI_4;
    case '5':
        return kVK_ANSI_5;
    case '6':
        return kVK_ANSI_6;
    case '7':
        return kVK_ANSI_7;
    case '8':
        return kVK_ANSI_8;
    case '9':
        return kVK_ANSI_9;
    case '0':
        return kVK_ANSI_0;
    case '=':
    case '+':
        return kVK_ANSI_Equal;
    case '-':
    case '_':
        return kVK_ANSI_Minus;
    case '[':
    case '{':
        return kVK_ANSI_LeftBracket;
    case ']':
    case '}':
        return kVK_ANSI_RightBracket;
    case '\'':
    case '"':
        return kVK_ANSI_Quote;
    case ';':
    case ':':
        return kVK_ANSI_Semicolon;
    case '\\':
    case '|':
        return kVK_ANSI_Backslash;
    case ',':
    case '<':
        return kVK_ANSI_Comma;
    case '.':
    case '>':
        return kVK_ANSI_Period;
    case '/':
    case '?':
        return kVK_ANSI_Slash;
    case '`':
    case '~':
        return kVK_ANSI_Grave;
    }

    return unknownKeyCode;
}

static unsigned short keyCodeForVirtualKey(VirtualKey key)
{
    // The likely keyCode for the virtual key as defined in <HIToolbox/Events.h>.
    switch (key) {
    case VirtualKey::Shift:
        return kVK_Shift;
    case VirtualKey::ShiftRight:
        return kVK_RightShift;
    case VirtualKey::Control:
        return kVK_Control;
    case VirtualKey::ControlRight:
        return kVK_RightControl;
    case VirtualKey::Alternate:
        return kVK_Option;
    case VirtualKey::AlternateRight:
        return kVK_RightOption;
    case VirtualKey::Meta:
        // The 'meta' keys (left and right) do not exist on Apple keyboards and are usually
        // mapped to the Command keys when using third-party keyboards.
    case VirtualKey::Command:
        return kVK_Command;
    case VirtualKey::MetaRight:
    case VirtualKey::CommandRight:
        return kVK_RightCommand;
    case VirtualKey::Help:
        return kVK_Help;
    case VirtualKey::Backspace:
        return kVK_Delete;
    case VirtualKey::Tab:
        return kVK_Tab;
    case VirtualKey::Clear:
        return kVK_ANSI_KeypadClear;
    case VirtualKey::Enter:
        return kVK_ANSI_KeypadEnter;
    case VirtualKey::Pause:
        // The 'pause' key does not exist on Apple keyboards and has no keyCode.
        // The semantics are unclear so just abort and do nothing.
        return unknownKeyCode;
    case VirtualKey::Cancel:
        // The 'cancel' key does not exist on Apple keyboards and has no keyCode.
        // According to the internet its functionality is similar to 'Escape'.
    case VirtualKey::Escape:
        return kVK_Escape;
    case VirtualKey::PageUp:
    case VirtualKey::PageUpRight:
        return kVK_PageUp;
    case VirtualKey::PageDown:
    case VirtualKey::PageDownRight:
        return kVK_PageDown;
    case VirtualKey::End:
    case VirtualKey::EndRight:
        return kVK_End;
    case VirtualKey::Home:
    case VirtualKey::HomeRight:
        return kVK_Home;
    case VirtualKey::LeftArrow:
    case VirtualKey::LeftArrowRight:
        return kVK_LeftArrow;
    case VirtualKey::UpArrow:
    case VirtualKey::UpArrowRight:
        return kVK_UpArrow;
    case VirtualKey::RightArrow:
    case VirtualKey::RightArrowRight:
        return kVK_RightArrow;
    case VirtualKey::DownArrow:
    case VirtualKey::DownArrowRight:
        return kVK_DownArrow;
    case VirtualKey::Insert:
    case VirtualKey::InsertRight:
        // The 'insert' key does not exist on Apple keyboards and has no keyCode.
        // The semantics are unclear so just abort and do nothing.
        return unknownKeyCode;
    case VirtualKey::Delete:
    case VirtualKey::DeleteRight:
        return kVK_ForwardDelete;
    case VirtualKey::Space:
        return kVK_Space;
    case VirtualKey::Semicolon:
        return kVK_ANSI_Semicolon;
    case VirtualKey::Equals:
        return kVK_ANSI_Equal;
    case VirtualKey::Return:
        return kVK_Return;
    case VirtualKey::NumberPad0:
        return kVK_ANSI_Keypad0;
    case VirtualKey::NumberPad1:
        return kVK_ANSI_Keypad1;
    case VirtualKey::NumberPad2:
        return kVK_ANSI_Keypad2;
    case VirtualKey::NumberPad3:
        return kVK_ANSI_Keypad3;
    case VirtualKey::NumberPad4:
        return kVK_ANSI_Keypad4;
    case VirtualKey::NumberPad5:
        return kVK_ANSI_Keypad5;
    case VirtualKey::NumberPad6:
        return kVK_ANSI_Keypad6;
    case VirtualKey::NumberPad7:
        return kVK_ANSI_Keypad7;
    case VirtualKey::NumberPad8:
        return kVK_ANSI_Keypad8;
    case VirtualKey::NumberPad9:
        return kVK_ANSI_Keypad9;
    case VirtualKey::NumberPadMultiply:
        return kVK_ANSI_KeypadMultiply;
    case VirtualKey::NumberPadAdd:
        return kVK_ANSI_KeypadPlus;
    case VirtualKey::NumberPadSubtract:
        return kVK_ANSI_KeypadMinus;
    case VirtualKey::NumberPadSeparator:
        // The 'Separator' key is only present on a few international keyboards.
        // It is usually mapped to the same character as Decimal ('.' or ',').
        [[fallthrough]];
    case VirtualKey::NumberPadDecimal:
        return kVK_ANSI_KeypadDecimal;
        // FIXME: this might be locale-dependent. See the above comment.
    case VirtualKey::NumberPadDivide:
        return kVK_ANSI_KeypadDivide;
    case VirtualKey::Function1:
        return kVK_F1;
    case VirtualKey::Function2:
        return kVK_F2;
    case VirtualKey::Function3:
        return kVK_F3;
    case VirtualKey::Function4:
        return kVK_F4;
    case VirtualKey::Function5:
        return kVK_F5;
    case VirtualKey::Function6:
        return kVK_F6;
    case VirtualKey::Function7:
        return kVK_F7;
    case VirtualKey::Function8:
        return kVK_F8;
    case VirtualKey::Function9:
        return kVK_F9;
    case VirtualKey::Function10:
        return kVK_F10;
    case VirtualKey::Function11:
        return kVK_F11;
    case VirtualKey::Function12:
        return kVK_F12;
    }
}

static NSEventModifierFlags eventModifierFlagsForVirtualKey(VirtualKey key)
{
    // Computes the modifiers changed by the virtual key when it is pressed or released.
    // The mapping from keys to modifiers is specified in the documentation for NSEvent.
    switch (key) {
    case VirtualKey::Shift:
    case VirtualKey::ShiftRight:
        return NSEventModifierFlagShift;

    case VirtualKey::Control:
    case VirtualKey::ControlRight:
        return NSEventModifierFlagControl;

    case VirtualKey::Alternate:
    case VirtualKey::AlternateRight:
        return NSEventModifierFlagOption;

    case VirtualKey::Meta:
    case VirtualKey::MetaRight:
        // The 'meta' keys (left and right) do not exist on Apple keyboards and are usually
        // mapped to the Command keys when using third-party keyboards.
    case VirtualKey::Command:
    case VirtualKey::CommandRight:
        return NSEventModifierFlagCommand;

    case VirtualKey::Help:
    case VirtualKey::PageUp:
    case VirtualKey::PageDown:
    case VirtualKey::End:
    case VirtualKey::Home:
        return NSEventModifierFlagFunction;

    case VirtualKey::LeftArrow:
    case VirtualKey::UpArrow:
    case VirtualKey::RightArrow:
    case VirtualKey::DownArrow:
        return NSEventModifierFlagNumericPad | NSEventModifierFlagFunction;

    case VirtualKey::Delete:
        return NSEventModifierFlagFunction;

    case VirtualKey::Clear:
    case VirtualKey::NumberPad0:
    case VirtualKey::NumberPad1:
    case VirtualKey::NumberPad2:
    case VirtualKey::NumberPad3:
    case VirtualKey::NumberPad4:
    case VirtualKey::NumberPad5:
    case VirtualKey::NumberPad6:
    case VirtualKey::NumberPad7:
    case VirtualKey::NumberPad8:
    case VirtualKey::NumberPad9:
    case VirtualKey::NumberPadMultiply:
    case VirtualKey::NumberPadAdd:
    case VirtualKey::NumberPadSubtract:
    case VirtualKey::NumberPadSeparator:
    case VirtualKey::NumberPadDecimal:
    case VirtualKey::NumberPadDivide:
        return NSEventModifierFlagNumericPad;

    case VirtualKey::Function1:
    case VirtualKey::Function2:
    case VirtualKey::Function3:
    case VirtualKey::Function4:
    case VirtualKey::Function5:
    case VirtualKey::Function6:
    case VirtualKey::Function7:
    case VirtualKey::Function8:
    case VirtualKey::Function9:
    case VirtualKey::Function10:
    case VirtualKey::Function11:
    case VirtualKey::Function12:
        return NSEventModifierFlagFunction;

    default:
        return 0;
    }
}

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, Variant<VirtualKey, CharKey>&& key)
{
    // FIXME: this function and the Automation protocol enum should probably adopt key names
    // from W3C UIEvents standard. For more details: https://w3c.github.io/uievents-code/

    bool isStickyModifier = false;
    NSEventModifierFlags changedModifiers = 0;
    unsigned short keyCode = unknownKeyCode;

    RetainPtr<NSString> characters;
    RetainPtr<NSString> unmodifiedCharacters;

    // FIXME: consider using AppKit SPI to normalize 'characters', i.e., changing * to Shift-8,
    // and passing that in to charactersIgnoringModifiers. We could hardcode this for ASCII if needed.
    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            isStickyModifier = virtualKeyHasStickyModifier(virtualKey);
            changedModifiers = eventModifierFlagsForVirtualKey(virtualKey);
            keyCode = keyCodeForVirtualKey(virtualKey);
            if (auto charCode = charCodeForVirtualKey(virtualKey))
                characters = adoptNS([[NSString alloc] initWithCharacters:&charCode.value() length:1]);
            if (auto charCodeIgnoringModifiers = charCodeIgnoringModifiersForVirtualKey(virtualKey))
                unmodifiedCharacters = adoptNS([[NSString alloc] initWithCharacters:&charCodeIgnoringModifiers.value() length:1]);
        },
        [&] (CharKey charKey) {
            keyCode = keyCodeForCharKey(charKey);
            characters = charKey.createNSString();
            unmodifiedCharacters = characters;
        }
    );

    switch (interaction) {
    case KeyboardInteraction::KeyPress:
    case KeyboardInteraction::InsertByKey:
        m_currentModifiers |= changedModifiers;
        break;
    case KeyboardInteraction::KeyRelease:
        m_currentModifiers &= ~changedModifiers;
        break;
    }

    // FIXME: this timestamp is not even close to matching native events. Find out how to get closer.
    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    RetainPtr window = page.platformWindow();
    NSInteger windowNumber = window.get().windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.get().frame.size.height);

    static constexpr auto characterTransformingModifiers = NSEventModifierFlagShift | NSEventModifierFlagOption;
    if (characters && (m_currentModifiers & characterTransformingModifiers)) {
        // `characters` will not automatically include the result of modifier keys when creating an NSEvent; AppKit
        // expects them to be pre-transformed. Event type is unimportant here as we are more interested in the events
        // ability to apply modifiers to its characters.
        characters = [[NSEvent keyEventWithType:NSEventTypeKeyDown location:eventPosition modifierFlags:0 timestamp:timestamp windowNumber:windowNumber context:nil characters:characters.get() charactersIgnoringModifiers:unmodifiedCharacters.get() isARepeat:NO keyCode:keyCode] charactersByApplyingModifiers:m_currentModifiers & characterTransformingModifiers];
    }

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    switch (interaction) {
    case KeyboardInteraction::KeyPress: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyDown;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters.get() charactersIgnoringModifiers:unmodifiedCharacters.get() isARepeat:NO keyCode:keyCode]];
        break;
    }
    case KeyboardInteraction::KeyRelease: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyUp;

        // When using a physical keyboard, if command is held down, releasing a non-modifier key doesn't send a KeyUp event.
        bool commandKeyHeldDown = m_currentModifiers & NSEventModifierFlagCommand;
        if (characters && commandKeyHeldDown)
            break;

        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters.get() charactersIgnoringModifiers:unmodifiedCharacters.get() isARepeat:NO keyCode:keyCode]];
        break;
    }
    case KeyboardInteraction::InsertByKey: {
        // Sticky modifiers should either be 'KeyPress' or 'KeyRelease'.
        ASSERT(!isStickyModifier);
        if (isStickyModifier)
            return;

        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyDown location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters.get() charactersIgnoringModifiers:unmodifiedCharacters.get() isARepeat:NO keyCode:keyCode]];
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyUp location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters.get() charactersIgnoringModifiers:unmodifiedCharacters.get() isARepeat:NO keyCode:keyCode]];
        break;
    }
    }

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

static BOOL characterIsProducedUsingShift(NSString *characters)
{
    ASSERT(characters.length == 1);

    // Per the WebDriver specification, keystrokes are assumed to be produced a standard 108-key US keyboard layout.
    // If this is no longer the case, then we'll need to use system keyboard SPI to decompose modifiers from keystrokes.
    unichar c = [characters characterAtIndex:0];
    if (c > 128)
        return NO;

    if (c >= 'A' && c <= 'Z')
        return YES;

    switch (c) {
    case '~':
    case '!':
    case '@':
    case '#':
    case '$':
    case '%':
    case '^':
    case '&':
    case '*':
    case '(':
    case ')':
    case '_':
    case '+':
    case '{':
    case '}':
    case '|':
    case ':':
    case '"':
    case '<':
    case '>':
    case '?':
        return YES;
    default:
        return NO;
    }
}

void WebAutomationSession::platformSimulateKeySequence(WebPageProxy& page, const String& keySequence)
{
    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    // Split the text into combining character sequences and send each separately.
    // This has no similarity to how keyboards work when inputting complex text.
    // This command is more similar to the 'insertText:' editing command, except
    // that this emits keyup/keydown/keypress events for roughly each character.
    // This API should move more towards that direction in the future.
    RetainPtr text = keySequence.createNSString();

    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    RetainPtr window = page.platformWindow();
    NSInteger windowNumber = window.get().windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.get().frame.size.height);

    [text enumerateSubstringsInRange:NSMakeRange(0, text.get().length) options:NSStringEnumerationByComposedCharacterSequences usingBlock:^(NSString *substring, NSRange substringRange, NSRange enclosingRange, BOOL *stop) {
    
        // For ASCII characters that are produced using Shift on a US-108 key keyboard layout,
        // WebDriver expects these to be delivered as [shift-down, key-down, key-up, shift-up]
        // keystrokes unless the shift key is already pressed down from an earlier interaction.
        BOOL shiftAlreadyPressed = m_currentModifiers & NSEventModifierFlagShift;
        BOOL shouldPressShift = !shiftAlreadyPressed && substringRange.length == 1 && characterIsProducedUsingShift(substring);
        NSEventModifierFlags modifiersForKeystroke = shouldPressShift ? m_currentModifiers | NSEventModifierFlagShift : m_currentModifiers;

        if (shouldPressShift)
            [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeFlagsChanged location:eventPosition modifierFlags:modifiersForKeystroke timestamp:timestamp windowNumber:windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:kVK_Shift]];

        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyDown location:eventPosition modifierFlags:modifiersForKeystroke timestamp:timestamp windowNumber:windowNumber context:nil characters:substring charactersIgnoringModifiers:substring isARepeat:NO keyCode:0]];
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyUp location:eventPosition modifierFlags:modifiersForKeystroke timestamp:timestamp windowNumber:windowNumber context:nil characters:substring charactersIgnoringModifiers:substring isARepeat:NO keyCode:0]];

        if (shouldPressShift)
            [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeFlagsChanged location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:kVK_Shift]];
    }];

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

void WebAutomationSession::platformSimulateWheelInteraction(WebPageProxy& page, const WebCore::IntPoint& locationInViewport, const WebCore::IntSize& delta)
{
    static constexpr auto scrollWheelCount = 2;
    auto cgScrollEvent = adoptCF(CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, scrollWheelCount, -delta.height(), -delta.width()));

    auto locationInWindow = viewportLocationToWindowLocation(locationInViewport, page);
    RetainPtr window = page.platformWindow();

    // Set the CGEvent location in flipped coords relative to the first screen, which compensates for the behavior of
    // +[NSEvent eventWithCGEvent:] when the event has no associated window. See <rdar://problem/17180591>.
    auto locationOnScreen = [window convertPointToScreen:locationInWindow];
    locationOnScreen = CGPointMake(locationOnScreen.x, NSScreen.screens.firstObject.frame.size.height - locationOnScreen.y);
    CGEventSetLocation(cgScrollEvent.get(), locationOnScreen);

    RetainPtr<NSEvent> scrollEvent = [[NSEvent eventWithCGEvent:cgScrollEvent.get()] _eventRelativeToWindow:window.get()];

    sendSynthesizedEventsToPage(page, @[scrollEvent.get()]);
}

#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

} // namespace WebKit

#endif // PLATFORM(MAC)
