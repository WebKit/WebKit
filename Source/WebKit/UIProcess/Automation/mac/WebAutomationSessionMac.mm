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

#import "WebAutomationSessionMacros.h"
#import "WebInspectorProxy.h"
#import "WebPageProxy.h"
#import "_WKAutomationSession.h"
#import <HIToolbox/Events.h>
#import <WebCore/IntPoint.h>
#import <WebCore/IntSize.h>
#import <WebCore/PlatformMouseEvent.h>
#import <objc/runtime.h>

using namespace WebCore;

namespace WebKit {

#pragma mark Commands for Platform: 'macOS'

void WebAutomationSession::inspectBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const bool* optionalEnableAutoCapturing, Ref<InspectBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (auto callback = m_pendingInspectorCallbacksPerPage.take(page->pageID()))
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    m_pendingInspectorCallbacksPerPage.set(page->pageID(), WTFMove(callback));

    // Don't bring the inspector to front since this may be done automatically.
    // We just want it loaded so it can pause if a breakpoint is hit during a command.
    if (page->inspector()) {
        page->inspector()->connect();

        // Start collecting profile information immediately so the entire session is captured.
        if (optionalEnableAutoCapturing && *optionalEnableAutoCapturing)
            page->inspector()->togglePageProfiling();
    }
}

#pragma mark AppKit Event Simulation Support

static const NSInteger synthesizedMouseEventMagicEventNumber = 0;
static const void *synthesizedAutomationEventAssociatedObjectKey = &synthesizedAutomationEventAssociatedObjectKey;

void WebAutomationSession::sendSynthesizedEventsToPage(WebPageProxy& page, NSArray *eventsToSend)
{
    NSWindow *window = page.platformWindow();

    for (NSEvent *event in eventsToSend) {
        // Take focus back in case the Inspector became focused while the prior command or
        // NSEvent was delivered to the window.
        [window becomeKeyWindow];

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

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, const WebCore::IntPoint& viewPosition, Inspector::Protocol::Automation::MouseInteraction interaction, Inspector::Protocol::Automation::MouseButton button, WebEvent::Modifiers keyModifiers)
{
    IntRect windowRect;
    page.rootViewToWindow(IntRect(viewPosition, IntSize()), windowRect);
    IntPoint windowPosition = windowRect.location();

    NSEventModifierFlags modifiers = 0;
    if (keyModifiers & WebEvent::MetaKey)
        modifiers |= NSEventModifierFlagCommand;
    if (keyModifiers & WebEvent::AltKey)
        modifiers |= NSEventModifierFlagOption;
    if (keyModifiers & WebEvent::ControlKey)
        modifiers |= NSEventModifierFlagControl;
    if (keyModifiers & WebEvent::ShiftKey)
        modifiers |= NSEventModifierFlagShift;
    if (keyModifiers & WebEvent::CapsLockKey)
        modifiers |= NSEventModifierFlagCapsLock;

    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;

    NSEventType downEventType = (NSEventType)0;
    NSEventType dragEventType = (NSEventType)0;
    NSEventType upEventType = (NSEventType)0;
    switch (button) {
    case Inspector::Protocol::Automation::MouseButton::None:
        downEventType = upEventType = dragEventType = NSEventTypeMouseMoved;
        break;
    case Inspector::Protocol::Automation::MouseButton::Left:
        downEventType = NSEventTypeLeftMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeLeftMouseUp;
        break;
    case Inspector::Protocol::Automation::MouseButton::Middle:
        downEventType = NSEventTypeOtherMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeOtherMouseUp;
        break;
    case Inspector::Protocol::Automation::MouseButton::Right:
        downEventType = NSEventTypeRightMouseDown;
        upEventType = NSEventTypeRightMouseUp;
        break;
    }

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    NSInteger eventNumber = synthesizedMouseEventMagicEventNumber;

    switch (interaction) {
    case Inspector::Protocol::Automation::MouseInteraction::Move:
        ASSERT(dragEventType);
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:dragEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:0 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::Down:
        ASSERT(downEventType);

        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::Up:
        ASSERT(upEventType);

        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::SingleClick:
        ASSERT(upEventType);
        ASSERT(downEventType);

        // Send separate down and up events. WebCore will see this as a single-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::DoubleClick:
        ASSERT(upEventType);
        ASSERT(downEventType);

        // Send multiple down and up events with proper click count.
        // WebCore will see this as a single-click event then double-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:2 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:2 pressure:0.0f]];
    }

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

static bool keyHasStickyModifier(Inspector::Protocol::Automation::VirtualKey key)
{
    // Returns whether the key's modifier flags should affect other events while pressed down.
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
    case Inspector::Protocol::Automation::VirtualKey::Control:
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
    case Inspector::Protocol::Automation::VirtualKey::Meta:
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return true;

    default:
        return false;
    }
}

static int keyCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey key)
{
    // The likely keyCode for the virtual key as defined in <HIToolbox/Events.h>.
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        return kVK_Shift;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        return kVK_Control;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        return kVK_Option;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        // The 'meta' key does not exist on Apple keyboards and is usually
        // mapped to the Command key when using third-party keyboards.
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return kVK_Command;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        return kVK_Help;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        return kVK_Delete;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        return kVK_Tab;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        return kVK_ANSI_KeypadClear;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        return kVK_ANSI_KeypadEnter;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        // The 'pause' key does not exist on Apple keyboards and has no keyCode.
        // The semantics are unclear so just abort and do nothing.
        return 0;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        // The 'cancel' key does not exist on Apple keyboards and has no keyCode.
        // According to the internet its functionality is similar to 'Escape'.
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        return kVK_Escape;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        return kVK_PageUp;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        return kVK_PageDown;
    case Inspector::Protocol::Automation::VirtualKey::End:
        return kVK_End;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return kVK_Home;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        return kVK_LeftArrow;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        return kVK_UpArrow;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        return kVK_RightArrow;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return kVK_DownArrow;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        // The 'insert' key does not exist on Apple keyboards and has no keyCode.
        // The semantics are unclear so just abort and do nothing.
        return 0;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return kVK_ForwardDelete;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        return kVK_Space;
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        return kVK_ANSI_Semicolon;
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        return kVK_ANSI_Equal;
    case Inspector::Protocol::Automation::VirtualKey::Return:
        return kVK_Return;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        return kVK_ANSI_Keypad0;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        return kVK_ANSI_Keypad1;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        return kVK_ANSI_Keypad2;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        return kVK_ANSI_Keypad3;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        return kVK_ANSI_Keypad4;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        return kVK_ANSI_Keypad5;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        return kVK_ANSI_Keypad6;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        return kVK_ANSI_Keypad7;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        return kVK_ANSI_Keypad8;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        return kVK_ANSI_Keypad9;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        return kVK_ANSI_KeypadMultiply;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        return kVK_ANSI_KeypadPlus;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        return kVK_ANSI_KeypadMinus;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        // The 'Separator' key is only present on a few international keyboards.
        // It is usually mapped to the same character as Decimal ('.' or ',').
        FALLTHROUGH;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        return kVK_ANSI_KeypadDecimal;
        // FIXME: this might be locale-dependent. See the above comment.
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        return kVK_ANSI_KeypadDivide;
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        return kVK_F1;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        return kVK_F2;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        return kVK_F3;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        return kVK_F4;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        return kVK_F5;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        return kVK_F6;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        return kVK_F7;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        return kVK_F8;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        return kVK_F9;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        return kVK_F10;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        return kVK_F11;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        return kVK_F12;
    }
}

static NSEventModifierFlags eventModifierFlagsForVirtualKey(Inspector::Protocol::Automation::VirtualKey key)
{
    // Computes the modifiers changed by the virtual key when it is pressed or released.
    // The mapping from keys to modifiers is specified in the documentation for NSEvent.
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        return NSEventModifierFlagShift;

    case Inspector::Protocol::Automation::VirtualKey::Control:
        return NSEventModifierFlagControl;

    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        return NSEventModifierFlagOption;

    case Inspector::Protocol::Automation::VirtualKey::Meta:
        // The 'meta' key does not exist on Apple keyboards and is usually
        // mapped to the Command key when using third-party keyboards.
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return NSEventModifierFlagCommand;

    case Inspector::Protocol::Automation::VirtualKey::Help:
        return NSEventModifierFlagHelp | NSEventModifierFlagFunction;

    case Inspector::Protocol::Automation::VirtualKey::PageUp:
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
    case Inspector::Protocol::Automation::VirtualKey::End:
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return NSEventModifierFlagFunction;

    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return NSEventModifierFlagNumericPad | NSEventModifierFlagFunction;

    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return NSEventModifierFlagFunction;

    case Inspector::Protocol::Automation::VirtualKey::Clear:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        return NSEventModifierFlagNumericPad;

    case Inspector::Protocol::Automation::VirtualKey::Function1:
    case Inspector::Protocol::Automation::VirtualKey::Function2:
    case Inspector::Protocol::Automation::VirtualKey::Function3:
    case Inspector::Protocol::Automation::VirtualKey::Function4:
    case Inspector::Protocol::Automation::VirtualKey::Function5:
    case Inspector::Protocol::Automation::VirtualKey::Function6:
    case Inspector::Protocol::Automation::VirtualKey::Function7:
    case Inspector::Protocol::Automation::VirtualKey::Function8:
    case Inspector::Protocol::Automation::VirtualKey::Function9:
    case Inspector::Protocol::Automation::VirtualKey::Function10:
    case Inspector::Protocol::Automation::VirtualKey::Function11:
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        return NSEventModifierFlagFunction;

    default:
        return 0;
    }
}

void WebAutomationSession::platformSimulateKeyStroke(WebPageProxy& page, Inspector::Protocol::Automation::KeyboardInteractionType interaction, Inspector::Protocol::Automation::VirtualKey key)
{
    // FIXME: this function and the Automation protocol enum should probably adopt key names
    // from W3C UIEvents standard. For more details: https://w3c.github.io/uievents-code/

    bool isStickyModifier = keyHasStickyModifier(key);
    NSEventModifierFlags changedModifiers = eventModifierFlagsForVirtualKey(key);
    int keyCode = keyCodeForVirtualKey(key);

    // FIXME: consider using AppKit SPI to normalize 'characters', i.e., changing * to Shift-8,
    // and passing that in to charactersIgnoringModifiers. We could hardcode this for ASCII if needed.
    std::optional<unichar> charCode = charCodeForVirtualKey(key);
    std::optional<unichar> charCodeIgnoringModifiers = charCodeIgnoringModifiersForVirtualKey(key);
    NSString *characters = charCode ? [NSString stringWithCharacters:&charCode.value() length:1] : nil;
    NSString *unmodifiedCharacters = charCodeIgnoringModifiers ? [NSString stringWithCharacters:&charCodeIgnoringModifiers.value() length:1] : nil;

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    // FIXME: this timestamp is not even close to matching native events. Find out how to get closer.
    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.frame.size.height);

    switch (interaction) {
    case Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyDown;
        m_currentModifiers |= changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:unmodifiedCharacters isARepeat:NO keyCode:keyCode]];
        break;
    }
    case Inspector::Protocol::Automation::KeyboardInteractionType::KeyRelease: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyUp;
        m_currentModifiers &= ~changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:m_currentModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:unmodifiedCharacters isARepeat:NO keyCode:keyCode]];
        break;
    }
    case Inspector::Protocol::Automation::KeyboardInteractionType::InsertByKey: {
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

} // namespace WebKit

#endif // PLATFORM(MAC)
