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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionCommand.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionContext.h"
#import "WebExtensionMenuItem.h"
#import <wtf/BlockPtr.h>
#import <wtf/text/StringBuilder.h>

#if USE(APPKIT)
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

#if PLATFORM(IOS_FAMILY)
@implementation _WKWebExtensionKeyCommand

+ (instancetype)commandWithTitle:(NSString *)title image:(UIImage *)image input:(NSString *)input modifierFlags:(UIKeyModifierFlags)modifierFlags handler:(WebExtensionKeyCommandHandlerBlock)handler
{
    RELEASE_ASSERT(title.length);
    RELEASE_ASSERT(input.length);
    RELEASE_ASSERT(handler);

    auto *command = [self commandWithTitle:title image:image action:@selector(_performWithTarget:) input:input modifierFlags:modifierFlags propertyList:nil];
    if (!command)
        return nil;

    command->_handler = [handler copy];

    return command;
}

- (id)copyWithZone:(NSZone *)zone
{
    _WKWebExtensionKeyCommand *copy = [super copyWithZone:zone];
    copy->_handler = [_handler copy];
    return copy;
}

- (void)performWithSender:(id)sender target:(id)target
{
    [super performWithSender:sender target:self];
}

- (id)_resolvedTargetFromFirstTarget:(id)firstTarget sender:(id)sender
{
    return nil;
}

- (void)_performWithTarget:(id)target
{
    ASSERT(_handler);
    if (_handler)
        _handler();
}

@end
#endif // PLATFORM(IOS_FAMILY)

namespace WebKit {

void WebExtensionCommand::dispatchChangedEventSoonIfNeeded()
{
    // Already scheduled to fire if old shortcut is set.
    if (!m_oldShortcut.isNull())
        return;

    m_oldShortcut = shortcutString();

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        RefPtr context = extensionContext();
        if (!context)
            return;

        context->fireCommandChangedEventIfNeeded(*this, m_oldShortcut);

        m_oldShortcut = nullString();
    }).get());
}

bool WebExtensionCommand::setActivationKey(String activationKey)
{
    if (activationKey.isEmpty()) {
        m_activationKey = nullString();
        return true;
    }

    if (activationKey.length() > 1)
        return false;

    static NSCharacterSet *notAllowedCharacterSet;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        auto *allowedCharacterSet = [NSMutableCharacterSet alphanumericCharacterSet];
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF704, 12)];
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF727, 3)];
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF72B, 3)];
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF700, 4)];
        [allowedCharacterSet addCharactersInString:@",. "];

        notAllowedCharacterSet = allowedCharacterSet.invertedSet;
    });

    if ([(NSString *)activationKey rangeOfCharacterFromSet:notAllowedCharacterSet].location != NSNotFound)
        return false;

    dispatchChangedEventSoonIfNeeded();

    m_activationKey = activationKey.convertToASCIILowercase();

    return true;
}

String WebExtensionCommand::shortcutString() const
{
    auto flags = modifierFlags();
    auto key = activationKey();

    if (!flags || key.isEmpty())
        return emptyString();

    static NeverDestroyed<HashMap<String, String>> specialKeyMap = HashMap<String, String> {
        { ","_s, "Comma"_s },
        { "."_s, "Period"_s },
        { " "_s, "Space"_s },
        { @"\uF704", "F1"_s },
        { @"\uF705", "F2"_s },
        { @"\uF706", "F3"_s },
        { @"\uF707", "F4"_s },
        { @"\uF708", "F5"_s },
        { @"\uF709", "F6"_s },
        { @"\uF70A", "F7"_s },
        { @"\uF70B", "F8"_s },
        { @"\uF70C", "F9"_s },
        { @"\uF70D", "F10"_s },
        { @"\uF70E", "F11"_s },
        { @"\uF70F", "F12"_s },
        { @"\uF727", "Insert"_s },
        { @"\uF728", "Delete"_s },
        { @"\uF729", "Home"_s },
        { @"\uF72B", "End"_s },
        { @"\uF72C", "PageUp"_s },
        { @"\uF72D", "PageDown"_s },
        { @"\uF700", "Up"_s },
        { @"\uF701", "Down"_s },
        { @"\uF702", "Left"_s },
        { @"\uF703", "Right"_s }
    };

    StringBuilder stringBuilder;

    if (flags.contains(ModifierFlags::Control))
        stringBuilder.append("MacCtrl"_s);

    if (flags.contains(ModifierFlags::Option)) {
        if (!stringBuilder.isEmpty())
            stringBuilder.append('+');
        stringBuilder.append("Alt"_s);
    }

    if (flags.contains(ModifierFlags::Shift)) {
        if (!stringBuilder.isEmpty())
            stringBuilder.append('+');
        stringBuilder.append("Shift"_s);
    }

    if (flags.contains(ModifierFlags::Command)) {
        if (!stringBuilder.isEmpty())
            stringBuilder.append('+');
        stringBuilder.append("Command"_s);
    }

    if (!stringBuilder.isEmpty())
        stringBuilder.append('+');

    if (auto specialKey = specialKeyMap.get().get(key); !specialKey.isEmpty())
        stringBuilder.append(specialKey);
    else
        stringBuilder.append(key.convertToASCIIUppercase());

    return stringBuilder.toString();
}

CocoaMenuItem *WebExtensionCommand::platformMenuItem() const
{
#if USE(APPKIT)
    auto *result = [[_WKWebExtensionMenuItem alloc] initWithTitle:description() handler:makeBlockPtr([this, protectedThis = Ref { *this }](id sender) mutable {
        if (RefPtr context = extensionContext())
            context->performCommand(const_cast<WebExtensionCommand&>(*this), WebExtensionContext::UserTriggered::Yes);
    }).get()];

    result.keyEquivalent = activationKey();
    result.keyEquivalentModifierMask = modifierFlags().toRaw();

    return result;
#else
    return [UIAction actionWithTitle:description() image:nil identifier:nil handler:makeBlockPtr([this, protectedThis = Ref { *this }](UIAction *) mutable {
        if (RefPtr context = extensionContext())
            context->performCommand(const_cast<WebExtensionCommand&>(*this), WebExtensionContext::UserTriggered::Yes);
    }).get()];
#endif
}

#if PLATFORM(IOS_FAMILY)
UIKeyCommand *WebExtensionCommand::keyCommand() const
{
    if (activationKey().isEmpty())
        return nil;

    return [_WKWebExtensionKeyCommand commandWithTitle:description() image:nil input:activationKey() modifierFlags:modifierFlags().toRaw() handler:makeBlockPtr([this, protectedThis = Ref { *this }]() mutable {
        if (RefPtr context = extensionContext())
            context->performCommand(const_cast<WebExtensionCommand&>(*this), WebExtensionContext::UserTriggered::Yes);
    }).get()];
}
#endif

#if USE(APPKIT)
bool WebExtensionCommand::matchesEvent(NSEvent *event) const
{
    if (event.type != NSEventTypeKeyDown || event.isARepeat)
        return false;

    if (activationKey().isEmpty())
        return false;

    auto expectedModifierFlags = modifierFlags().toRaw();
    if ((event.modifierFlags & expectedModifierFlags) != expectedModifierFlags)
        return false;

    static NeverDestroyed<HashMap<String, uint16_t>> specialKeyMap = HashMap<String, uint16_t> {
        { ","_s, kVK_ANSI_Comma },
        { "."_s, kVK_ANSI_Period },
        { " "_s, kVK_Space },
        { @"\uF704", kVK_F1 },
        { @"\uF705", kVK_F2 },
        { @"\uF706", kVK_F3 },
        { @"\uF707", kVK_F4 },
        { @"\uF708", kVK_F5 },
        { @"\uF709", kVK_F6 },
        { @"\uF70A", kVK_F7 },
        { @"\uF70B", kVK_F8 },
        { @"\uF70C", kVK_F9 },
        { @"\uF70D", kVK_F10 },
        { @"\uF70E", kVK_F11 },
        { @"\uF70F", kVK_F12 },
        // Insert (\uF727) is not present on Apple keyboards.
        { @"\uF728", kVK_ForwardDelete },
        { @"\uF729", kVK_Home },
        { @"\uF72B", kVK_End },
        { @"\uF72C", kVK_PageUp },
        { @"\uF72D", kVK_PageDown },
        { @"\uF700", kVK_UpArrow },
        { @"\uF701", kVK_DownArrow },
        { @"\uF702", kVK_LeftArrow },
        { @"\uF703", kVK_RightArrow }
    };

    if (equalIgnoringASCIICase(activationKey(), String(event.charactersIgnoringModifiers)))
        return true;

    if (auto mappedKeyCode = specialKeyMap.get().get(activationKey()))
        return mappedKeyCode == event.keyCode;

    return false;
}
#endif // USE(APPKIT)

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
