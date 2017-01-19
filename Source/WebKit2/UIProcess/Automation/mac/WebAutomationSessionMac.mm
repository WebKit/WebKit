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

void WebAutomationSession::resizeWindowOfBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const Inspector::InspectorObject& sizeObject)
{
    float width;
    if (!sizeObject.getDouble(WTF::ASCIILiteral("width"), width))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'width' parameter was not found or invalid.");

    float height;
    if (!sizeObject.getDouble(WTF::ASCIILiteral("height"), height))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'height' parameter was not found or invalid.");

    if (width < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'width' parameter had an invalid value.");

    if (height < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'height' parameter had an invalid value.");

    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::FloatRect originalFrame;
    page->getWindowFrame(originalFrame);

    WebCore::FloatRect newFrame = WebCore::FloatRect(originalFrame.location(), WebCore::FloatSize(width, height));
    if (newFrame == originalFrame)
        return;

    page->setWindowFrame(newFrame);

    // If nothing changed at all, it's probably fair to report that something went wrong.
    // (We can't assume that the requested frame size will be honored exactly, however.)
    WebCore::FloatRect updatedFrame;
    page->getWindowFrame(updatedFrame);
    if (originalFrame == updatedFrame)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The window size was expected to have changed, but did not.");
}

void WebAutomationSession::moveWindowOfBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const Inspector::InspectorObject& positionObject)
{
    float x;
    if (!positionObject.getDouble(WTF::ASCIILiteral("x"), x))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'x' parameter was not found or invalid.");

    float y;
    if (!positionObject.getDouble(WTF::ASCIILiteral("y"), y))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'y' parameter was not found or invalid.");

    if (x < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'x' parameter had an invalid value.");

    if (y < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'y' parameter had an invalid value.");

    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::FloatRect originalFrame;
    page->getWindowFrame(originalFrame);

    WebCore::FloatRect newFrame = WebCore::FloatRect(WebCore::FloatPoint(x, y), originalFrame.size());
    if (newFrame == originalFrame)
        return;

    page->setWindowFrame(newFrame);

    // If nothing changed at all, it's probably fair to report that something went wrong.
    // (We can't assume that the requested frame size will be honored exactly, however.)
    WebCore::FloatRect updatedFrame;
    page->getWindowFrame(updatedFrame);
    if (originalFrame == updatedFrame)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The window position was expected to have changed, but did not.");
}

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
        [window makeKeyAndOrderFront:nil];

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

    NSEventType downEventType;
    NSEventType dragEventType;
    NSEventType upEventType;
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
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:dragEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:0 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::Down:
        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::Up:
        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::SingleClick:
        // Send separate down and up events. WebCore will see this as a single-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::DoubleClick:
        // Send multiple down and up events with proper click count.
        // WebCore will see this as a single-click event then double-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:1 pressure:0.0f]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:2 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:2 pressure:0.0f]];
    }

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

void WebAutomationSession::platformSimulateKeyStroke(WebPageProxy& page, Inspector::Protocol::Automation::KeyboardInteractionType interaction, Inspector::Protocol::Automation::VirtualKey key)
{
    // If true, the key's modifier flags should affect other events while pressed down.
    bool isStickyModifier = false;
    // The modifiers changed by the virtual key when it is pressed or released.
    // The mapping from keys to modifiers is specified in the documentation for NSEvent.
    NSEventModifierFlags changedModifiers = 0;
    // The likely keyCode for the virtual key as defined in <HIToolbox/Events.h>.
    int keyCode = 0;
    // Typical characters produced by the virtual key, if any.
    NSString *characters = @"";

    // FIXME: this function and the Automation protocol enum should probably adopt key names
    // from W3C UIEvents standard. For more details: https://w3c.github.io/uievents-code/

    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        isStickyModifier = true;
        changedModifiers |= NSEventModifierFlagShift;
        keyCode = kVK_Shift;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        isStickyModifier = true;
        changedModifiers |= NSEventModifierFlagControl;
        keyCode = kVK_Control;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        isStickyModifier = true;
        changedModifiers |= NSEventModifierFlagOption;
        keyCode = kVK_Option;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        // The 'meta' key does not exist on Apple keyboards and is usually
        // mapped to the Command key when using third-party keyboards.
    case Inspector::Protocol::Automation::VirtualKey::Command:
        isStickyModifier = true;
        changedModifiers |= NSEventModifierFlagCommand;
        keyCode = kVK_Command;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        changedModifiers |= NSEventModifierFlagHelp | NSEventModifierFlagFunction;
        keyCode = kVK_Help;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        keyCode = kVK_Delete;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        keyCode = kVK_Tab;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_KeypadClear;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        keyCode = kVK_ANSI_KeypadEnter;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        // The 'pause' key does not exist on Apple keyboards and has no keycode.
        // The semantics are unclear so just abort and do nothing.
        return;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        // The 'cancel' key does not exist on Apple keyboards and has no keycode.
        // According to the internet its functionality is similar to 'Escape'.
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        keyCode = kVK_Escape;
        break;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_PageUp;
        break;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_PageDown;
        break;
    case Inspector::Protocol::Automation::VirtualKey::End:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_End;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_Home;
        break;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        changedModifiers |= NSEventModifierFlagNumericPad | NSEventModifierFlagFunction;
        keyCode = kVK_LeftArrow;
        break;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        changedModifiers |= NSEventModifierFlagNumericPad | NSEventModifierFlagFunction;
        keyCode = kVK_UpArrow;
        break;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        changedModifiers |= NSEventModifierFlagNumericPad | NSEventModifierFlagFunction;
        keyCode = kVK_RightArrow;
        break;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        changedModifiers |= NSEventModifierFlagNumericPad | NSEventModifierFlagFunction;
        keyCode = kVK_DownArrow;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        // The 'insert' key does not exist on Apple keyboards and has no keycode.
        // The semantics are unclear so just abort and do nothing.
        return;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_ForwardDelete;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        keyCode = kVK_Space;
        characters = @" ";
        break;
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        keyCode = kVK_ANSI_Semicolon;
        characters = @";";
        break;
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        keyCode = kVK_ANSI_Equal;
        characters = @"=";
        break;
    case Inspector::Protocol::Automation::VirtualKey::Return:
        keyCode = kVK_Return;
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad0;
        characters = @"0";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad1;
        characters = @"1";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad2;
        characters = @"2";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad3;
        characters = @"3";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad4;
        characters = @"4";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad5;
        characters = @"5";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad6;
        characters = @"6";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad7;
        characters = @"7";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad8;
        characters = @"8";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_Keypad9;
        characters = @"9";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_KeypadMultiply;
        characters = @"*";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_KeypadPlus;
        characters = @"+";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_KeypadMinus;
        characters = @"-";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        changedModifiers |= NSEventModifierFlagNumericPad;
        // The 'Separator' key is only present on a few international keyboards.
        // It is usually mapped to the same character as Decimal ('.' or ',').
        FALLTHROUGH;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_KeypadDecimal;
        // FIXME: this might be locale-dependent. See the above comment.
        characters = @".";
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        changedModifiers |= NSEventModifierFlagNumericPad;
        keyCode = kVK_ANSI_KeypadDivide;
        characters = @"/";
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F1;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F2;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F3;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F4;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F5;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F6;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F7;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F8;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F9;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F10;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F11;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        changedModifiers |= NSEventModifierFlagFunction;
        keyCode = kVK_F12;
        break;
    }

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    ASSERT(isStickyModifier || interaction == Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress);

    NSEventModifierFlags existingModifiers = [NSEvent modifierFlags];
    NSEventModifierFlags updatedModifiers = 0;
    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.frame.size.height);

    switch (interaction) {
    case Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyDown;
        updatedModifiers = existingModifiers | changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:updatedModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode]];
        break;
    }
    case Inspector::Protocol::Automation::KeyboardInteractionType::KeyRelease: {
        NSEventType eventType = isStickyModifier ? NSEventTypeFlagsChanged : NSEventTypeKeyUp;
        updatedModifiers = existingModifiers & ~changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:eventType location:eventPosition modifierFlags:updatedModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode]];
        break;
    }
    case Inspector::Protocol::Automation::KeyboardInteractionType::InsertByKey: {
        // Sticky modifiers should either be 'KeyPress' or 'KeyRelease'.
        ASSERT(!isStickyModifier);
        if (isStickyModifier)
            return;

        updatedModifiers = existingModifiers | changedModifiers;
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyDown location:eventPosition modifierFlags:updatedModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode]];
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyUp location:eventPosition modifierFlags:updatedModifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode]];
        break;
    }
    }

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
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

    NSEventModifierFlags modifiers = [NSEvent modifierFlags];
    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;
    NSPoint eventPosition = NSMakePoint(0, window.frame.size.height);

    [text enumerateSubstringsInRange:NSMakeRange(0, text.length) options:NSStringEnumerationByComposedCharacterSequences usingBlock:^(NSString *substring, NSRange substringRange, NSRange enclosingRange, BOOL *stop) {
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyDown location:eventPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:substring charactersIgnoringModifiers:substring isARepeat:NO keyCode:0]];
        [eventsToBeSent addObject:[NSEvent keyEventWithType:NSEventTypeKeyUp location:eventPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil characters:substring charactersIgnoringModifiers:substring isARepeat:NO keyCode:0]];
    }];

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

} // namespace WebKit

#endif // PLATFORM(MAC)
