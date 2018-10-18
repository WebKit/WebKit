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

#if PLATFORM(IOS_FAMILY)

#import "NativeWebKeyboardEvent.h"
#import "WebAutomationSessionMacros.h"
#import "WebPageProxy.h"
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/WebEvent.h>

namespace WebKit {
using namespace WebCore;

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

void WebAutomationSession::platformSimulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, WTF::Variant<VirtualKey, CharKey>&& key)
{
    // The modifiers changed by the virtual key when it is pressed or released.
    WebEventFlags changedModifiers = 0;

    // UIKit does not send key codes for virtual keys even for a hardware keyboard.
    // Instead, it sends single unichars and WebCore maps these to "windows" key codes.
    // Synthesize a single unichar such that the correct key code is inferred.
    std::optional<unichar> charCode;
    std::optional<unichar> charCodeIgnoringModifiers;

    // Figure out the effects of sticky modifiers.
    WTF::switchOn(key,
        [&] (VirtualKey virtualKey) {
            charCode = charCodeForVirtualKey(virtualKey);
            charCodeIgnoringModifiers = charCodeIgnoringModifiersForVirtualKey(virtualKey);

            switch (virtualKey) {
            case VirtualKey::Shift:
                changedModifiers |= WebEventFlagMaskShift;
                break;
            case VirtualKey::Control:
                changedModifiers |= WebEventFlagMaskControl;
                break;
            case VirtualKey::Alternate:
                changedModifiers |= WebEventFlagMaskAlternate;
                break;
            case VirtualKey::Meta:
                // The 'meta' key does not exist on Apple keyboards and is usually
                // mapped to the Command key when using third-party keyboards.
            case VirtualKey::Command:
                changedModifiers |= WebEventFlagMaskCommand;
                break;
            default:
                break;
            }
        },
        [&] (CharKey charKey) {
            charCode = (unichar)charKey;
            charCodeIgnoringModifiers = (unichar)charKey;
        }
    );

    // FIXME: consider using UIKit SPI to normalize 'characters', i.e., changing * to Shift-8,
    // and passing that in to charactersIgnoringModifiers. This is probably not worth the trouble
    // unless it causes an actual behavioral difference.
    NSString *characters = charCode ? [NSString stringWithCharacters:&charCode.value() length:1] : nil;
    NSString *unmodifiedCharacters = charCodeIgnoringModifiers ? [NSString stringWithCharacters:&charCodeIgnoringModifiers.value() length:1] : nil;
    BOOL isTabKey = charCode && charCode.value() == NSTabCharacter;

    // This is used as WebEvent.keyboardFlags, which are only used if we need to
    // send this event back to UIKit to be interpreted by the keyboard / input manager.
    // Just ignore this for now; we can fix it if there's an actual behavioral difference.
    NSUInteger inputFlags = 0;

    // Provide an empty keyCode so that WebCore infers it from the charCode.
    uint16_t keyCode = 0;

    auto eventsToBeSent = adoptNS([[NSMutableArray alloc] init]);

    switch (interaction) {
    case KeyboardInteraction::KeyPress: {
        m_currentModifiers |= changedModifiers;

        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:unmodifiedCharacters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
        break;
    }
    case KeyboardInteraction::KeyRelease: {
        m_currentModifiers &= ~changedModifiers;

        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:unmodifiedCharacters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
        break;
    }
    case KeyboardInteraction::InsertByKey: {
        // Modifiers only change with KeyPress or KeyRelease, this code path is for single characters.
        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:unmodifiedCharacters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
        [eventsToBeSent addObject:[[[::WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:characters charactersIgnoringModifiers:unmodifiedCharacters modifiers:m_currentModifiers isRepeating:NO withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:isTabKey] autorelease]];
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
        auto keyDownEvent = adoptNS([[::WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:substring charactersIgnoringModifiers:substring modifiers:m_currentModifiers isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:isTabKey]);
        [eventsToBeSent addObject:keyDownEvent.get()];
        auto keyUpEvent = adoptNS([[::WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:substring charactersIgnoringModifiers:substring modifiers:m_currentModifiers isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:isTabKey]);
        [eventsToBeSent addObject:keyUpEvent.get()];
    }];

    sendSynthesizedEventsToPage(page, eventsToBeSent.get());
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
