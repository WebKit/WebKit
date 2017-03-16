/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "NativeWebKeyboardEvent.h"
#import "WebAutomationSessionMacros.h"
#import "WebPageProxy.h"
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/WebEvent.h>

using namespace WebCore;

namespace WebKit {

void WebAutomationSession::sendSynthesizedEventsToPage(WebPageProxy& page, NSArray *eventsToSend)
{
    // 'eventsToSend' contains WebCore::WebEvent instances. Use a wrapper type specific to the event type.
    for (::WebEvent *event in eventsToSend) {
        switch (event.type) {
        case WebEventMouseDown:
        case WebEventMouseUp:
        case WebEventMouseMoved:
        case WebEventScrollWheel:
        case WebEventTouchBegin:
        case WebEventTouchChange:
        case WebEventTouchEnd:
        case WebEventTouchCancel:
            notImplemented();
            break;

        case WebEventKeyDown:
        case WebEventKeyUp:
            page.handleKeyboardEvent(NativeWebKeyboardEvent(event));
            break;
        }
    }
}

#pragma mark Commands for Platform: 'iOS'

void WebAutomationSession::platformSimulateKeyStroke(WebPageProxy& page, Inspector::Protocol::Automation::KeyboardInteractionType interaction, Inspector::Protocol::Automation::VirtualKey key)
{
    // The modifiers changed by the virtual key when it is pressed or released.
    WebEventFlags changedModifiers = 0;

    // UIKit does not send key codes for virtual keys even for a hardware keyboard.
    // Instead, it sends single unichars and WebCore maps these to "windows" key codes.
    // Synthesize a single unichar such that the correct key code is inferred.
    std::optional<unichar> charCode = std::nullopt;

    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
        changedModifiers |= WebEventFlagMaskShift;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Control:
        changedModifiers |= WebEventFlagMaskControl;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
        changedModifiers |= WebEventFlagMaskAlternate;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Meta:
        // The 'meta' key does not exist on Apple keyboards and is usually
        // mapped to the Command key when using third-party keyboards.
    case Inspector::Protocol::Automation::VirtualKey::Command:
        changedModifiers |= WebEventFlagMaskCommand;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        charCode = NSHelpFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        charCode = NSBackspaceCharacter;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        charCode = NSTabCharacter;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        charCode = NSClearLineFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        charCode = NSEnterCharacter;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        charCode = NSPauseFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        // The 'cancel' key does not exist on Apple keyboards and has no keycode.
        // According to the internet its functionality is similar to 'Escape'.
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        charCode = 0x1B;
        break;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        charCode = NSPageUpFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        charCode = NSPageDownFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::End:
        charCode = NSEndFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        charCode = NSHomeFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        charCode = NSLeftArrowFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        charCode = NSUpArrowFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        charCode = NSRightArrowFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        charCode = NSDownArrowFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        charCode = NSInsertFunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        charCode = NSDeleteCharacter;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        charCode = ' ';
        break;
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        charCode = ';';
        break;
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        charCode = '=';
        break;
    case Inspector::Protocol::Automation::VirtualKey::Return:
        charCode = NSCarriageReturnCharacter;
        break;

    // On iOS, it seems to be irrelevant in later processing whether the number came from number pad or not.
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        charCode = '0';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        charCode = '1';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        charCode = '2';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        charCode = '3';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        charCode = '4';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        charCode = '5';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        charCode = '6';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        charCode = '7';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        charCode = '8';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        charCode = '9';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        charCode = '*';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        charCode = '+';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        charCode = '-';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        // The 'Separator' key is only present on a few international keyboards.
        // It is usually mapped to the same character as Decimal ('.' or ',').
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        charCode = '.';
        break;
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        charCode = '/';
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        charCode = NSF1FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        charCode = NSF2FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        charCode = NSF3FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        charCode = NSF4FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        charCode = NSF5FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        charCode = NSF6FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        charCode = NSF7FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        charCode = NSF8FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        charCode = NSF9FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        charCode = NSF10FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        charCode = NSF11FunctionKey;
        break;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        charCode = NSF12FunctionKey;
        break;
    }

    ASSERT(changedModifiers || interaction == Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress);

    // FIXME: consider using UIKit SPI to normalize 'characters', i.e., changing * to Shift-8,
    // and passing that in to charactersIgnoringModifiers. This is probably not worth the trouble
    // unless it causes an actual behavioral difference.
    NSString *characters = charCode ? [NSString stringWithCharacters:&charCode.value() length:1] : nil;
    BOOL isTabKey = charCode && charCode.value() == NSTabCharacter;

    // This is used as WebEvent.keyboardFlags, which are only used if we need to
    // send this event back to UIKit to be interpreted by the keyboard / input manager.
    // Just ignore this for now; we can fix it if there's an actual behavioral difference.
    NSUInteger inputFlags = 0;

    // Provide an empty keyCode so that WebCore infers it from the charCode.
    uint16_t keyCode = 0;

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    switch (interaction) {
    case Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress: {
        m_currentModifiers |= changedModifiers;

        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:characters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
        break;
    }
    case Inspector::Protocol::Automation::KeyboardInteractionType::KeyRelease: {
        m_currentModifiers &= ~changedModifiers;

        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:characters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
        break;
    }
    case Inspector::Protocol::Automation::KeyboardInteractionType::InsertByKey: {
        // Modifiers only change with KeyPress or KeyRelease, this code path is for single characters.
        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:characters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:characters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
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
    BOOL isTabKey = [text isEqualToString:@"\t"];

    [text enumerateSubstringsInRange:NSMakeRange(0, text.length) options:NSStringEnumerationByComposedCharacterSequences usingBlock:^(NSString *substring, NSRange substringRange, NSRange enclosingRange, BOOL *stop) {
        [eventsToBeSent addObject:[[::WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:substring charactersIgnoringModifiers:substring modifiers:m_currentModifiers isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:isTabKey]];
        [eventsToBeSent addObject:[[::WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:substring charactersIgnoringModifiers:substring modifiers:m_currentModifiers isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:isTabKey]];
    }];

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

} // namespace WebKit

#endif // PLATFORM(IOS)
