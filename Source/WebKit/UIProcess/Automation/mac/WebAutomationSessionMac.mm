/*
 * Copyright (C) 2016, 2017 Apple Inc. All rights reserved.
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
#import "WebInspectorProxy.h"
#import "WebPageProxy.h"
#import "_WKAutomationSession.h"
#import <Carbon/Carbon.h>
#import <WebCore/IntPoint.h>
#import <WebCore/IntSize.h>
#import <WebCore/PlatformMouseEvent.h>
#import <objc/runtime.h>

namespace WebKit {
using namespace WebCore;

#pragma mark Commands for 'PLATFORM(MAC)'

void WebAutomationSession::inspectBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, Optional<bool>&& enableAutoCapturing, Ref<InspectBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (auto callback = m_pendingInspectorCallbacksPerPage.take(page->identifier()))
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    m_pendingInspectorCallbacksPerPage.set(page->identifier(), WTFMove(callback));

    // Don't bring the inspector to front since this may be done automatically.
    // We just want it loaded so it can pause if a breakpoint is hit during a command.
    if (page->inspector()) {
        page->inspector()->show();

        // Start collecting profile information immediately so the entire session is captured.
        if (enableAutoCapturing && *enableAutoCapturing)
            page->inspector()->togglePageProfiling();
    }
}

#pragma mark AppKit Event Simulation Support

static const NSInteger synthesizedMouseEventMagicEventNumber = 0;
static const void *synthesizedAutomationEventAssociatedObjectKey = &synthesizedAutomationEventAssociatedObjectKey;

void WebAutomationSession::sendSynthesizedEventsToPage(WebPageProxy& page, NSArray *eventsToSend)
{
    NSWindow *window = page.platformWindow();
    [window makeKeyAndOrderFront:nil];
    page.makeFirstResponder();

    for (NSEvent *event in eventsToSend) {
        LOG(Automation, "Sending event[%p] to window[%p]: %@", event, window, event);

        // Take focus back in case the Inspector became focused while the prior command or
        // NSEvent was delivered to the window.
        [window makeKeyAndOrderFront:nil];
        page.makeFirstResponder();

        markEventAsSynthesizedForAutomation(event);
        [window sendEvent:event];
    }
}

void WebAutomationSession::markEventAsSynthesizedForAutomation(NSEvent *event)
{
    objc_setAssociatedObject(event, &synthesizedAutomationEventAssociatedObjectKey, m_sessionIdentifier, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

bool WebAutomationSession::wasEventSynthesizedForAutomation(NSEvent *event)
{
    NSString *senderSessionIdentifier = objc_getAssociatedObject(event, &synthesizedAutomationEventAssociatedObjectKey);
    if ([senderSessionIdentifier isEqualToString:m_sessionIdentifier])
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

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

static WebMouseEvent::Button automationMouseButtonToPlatformMouseButton(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:   return WebMouseEvent::LeftButton;
    case MouseButton::Middle: return WebMouseEvent::MiddleButton;
    case MouseButton::Right:  return WebMouseEvent::RightButton;
    case MouseButton::None:   return WebMouseEvent::NoButton;
    default: ASSERT_NOT_REACHED();
    }
}

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, MouseButton button, const WebCore::IntPoint& locationInViewport, OptionSet<WebEvent::Modifier> keyModifiers)
{
    IntRect windowRect;

    IntPoint locationInView = locationInViewport + IntPoint(0, page.topContentInset());
    page.rootViewToWindow(IntRect(locationInView, IntSize()), windowRect);
    IntPoint locationInWindow = windowRect.location();

    NSEventModifierFlags modifiers = 0;
    if (keyModifiers.contains(WebEvent::Modifier::MetaKey))
        modifiers |= NSEventModifierFlagCommand;
    if (keyModifiers.contains(WebEvent::Modifier::AltKey))
        modifiers |= NSEventModifierFlagOption;
    if (keyModifiers.contains(WebEvent::Modifier::ControlKey))
        modifiers |= NSEventModifierFlagControl;
    if (keyModifiers.contains(WebEvent::Modifier::ShiftKey))
        modifiers |= NSEventModifierFlagShift;
    if (keyModifiers.contains(WebEvent::Modifier::CapsLockKey))
        modifiers |= NSEventModifierFlagCapsLock;

    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;

    NSEventType downEventType = (NSEventType)0;
    NSEventType dragEventType = (NSEventType)0;
    NSEventType upEventType = (NSEventType)0;
    switch (automationMouseButtonToPlatformMouseButton(button)) {
    case WebMouseEvent::NoButton:
        downEventType = upEventType = dragEventType = NSEventTypeMouseMoved;
        break;
    case WebMouseEvent::LeftButton:
        downEventType = NSEventTypeLeftMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeLeftMouseUp;
        break;
    case WebMouseEvent::MiddleButton:
        downEventType = NSEventTypeOtherMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeOtherMouseUp;
        break;
    case WebMouseEvent::RightButton:
        downEventType = NSEventTypeRightMouseDown;
        upEventType = NSEventTypeRightMouseUp;
        break;
    }

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    NSInteger eventNumber = synthesizedMouseEventMagicEventNumber;

    switch (interaction) {
    case MouseInteraction::Move:
        ASSERT(dragEventType);
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:dragEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:0 pressure:0.0f]];
        break;
    case MouseInteraction::Down:
        ASSERT(downEventType);

        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        break;
    case MouseInteraction::Up:
        ASSERT(upEventType);

        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        break;
    case MouseInteraction::SingleClick:
        ASSERT(upEventType);
        ASSERT(downEventType);

        // Send separate down and up events. WebCore will see this as a single-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        break;
    case MouseInteraction::DoubleClick:
        ASSERT(upEventType);
        ASSERT(downEventType);

        // Send multiple down and up events with proper click count.
        // WebCore will see this as a single-click event then double-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:2 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:2 pressure:0.0f]];
    }

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

OptionSet<WebEvent::Modifier> WebAutomationSession::platformWebModifiersFromRaw(unsigned modifiers)
{
    OptionSet<WebEvent::Modifier> webModifiers;

    if (modifiers & NSEventModifierFlagCommand)
        webModifiers.add(WebEvent::Modifier::MetaKey);
    if (modifiers & NSEventModifierFlagOption)
        webModifiers.add(WebEvent::Modifier::AltKey);
    if (modifiers & NSEventModifierFlagControl)
        webModifiers.add(WebEvent::Modifier::ControlKey);
    if (modifiers & NSEventModifierFlagShift)
        webModifiers.add(WebEvent::Modifier::ShiftKey);
    if (modifiers & NSEventModifierFlagCapsLock)
        webModifiers.add(WebEvent::Modifier::CapsLockKey);

    return webModifiers;
}

#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
static bool virtualKeyHasStickyModifier(VirtualKey key)
{
    // Returns whether the key's modifier flags should affect other events while pressed down.
    switch (key) {
    case VirtualKey::Shift:
    case VirtualKey::Control:
    case VirtualKey::Alternate:
    case VirtualKey::Meta:
    case VirtualKey::Command:
        return true;

    default:
        return false;
    }
}

static int keyCodeForCharKey(CharKey key)
{
    switch (key) {
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

    return 0;
}

static int keyCodeForVirtualKey(VirtualKey key)
{
    // The likely keyCode for the virtual key as defined in <HIToolbox/Events.h>.
    switch (key) {
    case VirtualKey::Shift:
        return kVK_Shift;
    case VirtualKey::Control:
        return kVK_Control;
    case VirtualKey::Alternate:
        return kVK_Option;
    case VirtualKey::Meta:
        // The 'meta' key does not exist on Apple keyboards and is usually
        // mapped to the Command key when using third-party keyboards.
    case VirtualKey::Command:
        return kVK_Command;
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
        return 0;
    case VirtualKey::Cancel:
        // The 'cancel' key does not exist on Apple keyboards and has no keyCode.
        // According to the internet its functionality is similar to 'Escape'.
    case VirtualKey::Escape:
        return kVK_Escape;
    case VirtualKey::PageUp:
        return kVK_PageUp;
    case VirtualKey::PageDown:
        return kVK_PageDown;
    case VirtualKey::End:
        return kVK_End;
    case VirtualKey::Home:
        return kVK_Home;
    case VirtualKey::LeftArrow:
        return kVK_LeftArrow;
    case VirtualKey::UpArrow:
        return kVK_UpArrow;
    case VirtualKey::RightArrow:
        return kVK_RightArrow;
    case VirtualKey::DownArrow:
        return kVK_DownArrow;
    case VirtualKey::Insert:
        // The 'insert' key does not exist on Apple keyboards and has no keyCode.
        // The semantics are unclear so just abort and do nothing.
        return 0;
    case VirtualKey::Delete:
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
        FALLTHROUGH;
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
        return NSEventModifierFlagShift;

    case VirtualKey::Control:
        return NSEventModifierFlagControl;

    case VirtualKey::Alternate:
        return NSEventModifierFlagOption;

    case VirtualKey::Meta:
        // The 'meta' key does not exist on Apple keyboards and is usually
        // mapped to the Command key when using third-party keyboards.
    case VirtualKey::Command:
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

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, WTF::Variant<VirtualKey, CharKey>&& key)
{
    // FIXME: this function and the Automation protocol enum should probably adopt key names
    // from W3C UIEvents standard. For more details: https://w3c.github.io/uievents-code/

    bool isStickyModifier = false;
    NSEventModifierFlags changedModifiers = 0;
    int keyCode = 0;
    Optional<unichar> charCode;
    Optional<unichar> charCodeIgnoringModifiers;

    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            isStickyModifier = virtualKeyHasStickyModifier(virtualKey);
            changedModifiers = eventModifierFlagsForVirtualKey(virtualKey);
            keyCode = keyCodeForVirtualKey(virtualKey);
            charCode = charCodeForVirtualKey(virtualKey);
            charCodeIgnoringModifiers = charCodeIgnoringModifiersForVirtualKey(virtualKey);
        },
        [&] (CharKey charKey) {
            keyCode = keyCodeForCharKey(charKey);
            charCode = (unichar)charKey;
            charCodeIgnoringModifiers = (unichar)charKey;
        }
    );

    // FIXME: consider using AppKit SPI to normalize 'characters', i.e., changing * to Shift-8,
    // and passing that in to charactersIgnoringModifiers. We could hardcode this for ASCII if needed.
    NSString *characters = charCode ? [NSString stringWithCharacters:&charCode.value() length:1] : nil;
    NSString *unmodifiedCharacters = charCodeIgnoringModifiers ? [NSString stringWithCharacters:&charCodeIgnoringModifiers.value() length:1] : nil;

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    // FIXME: this timestamp is not even close to matching native events. Find out how to get closer.
    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.frame.size.height);

    switch (interaction) {
    case KeyboardInteraction::KeyPress: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyDown;
        m_currentModifiers |= changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:unmodifiedCharacters isARepeat:NO keyCode:keyCode]];
        break;
    }
    case KeyboardInteraction::KeyRelease: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyUp;
        m_currentModifiers &= ~changedModifiers;

        // When using a physical keyboard, if command is held down, releasing a non-modifier key doesn't send a KeyUp event.
        bool commandKeyHeldDown = m_currentModifiers & NSEventModifierFlagCommand;
        if (characters && commandKeyHeldDown)
            break;

        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:unmodifiedCharacters isARepeat:NO keyCode:keyCode]];
        break;
    }
    case KeyboardInteraction::InsertByKey: {
        // Sticky modifiers should either be 'KeyPress' or 'KeyRelease'.
        ASSERT(!isStickyModifier);
        if (isStickyModifier)
            return;

        m_currentModifiers |= changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyDown location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:unmodifiedCharacters isARepeat:NO keyCode:keyCode]];
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyUp location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:unmodifiedCharacters isARepeat:NO keyCode:keyCode]];
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
    NSString *text = keySequence;

    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.frame.size.height);

    [text enumerateSubstringsInRange:NSMakeRange(0, text.length) options:NSStringEnumerationByComposedCharacterSequences usingBlock:^(NSString *substring, NSRange substringRange, NSRange enclosingRange, BOOL *stop) {
    
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

} // namespace WebKit

#endif // PLATFORM(MAC)
