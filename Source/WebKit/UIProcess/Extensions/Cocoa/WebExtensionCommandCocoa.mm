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

#import "CocoaHelpers.h"
#import "WebExtension.h"
#import "WebExtensionContext.h"
#import "WebExtensionMenuItem.h"
#import <wtf/BlockPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/StringBuilder.h>

#if USE(APPKIT)
#import "AppKitSPI.h"
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

#if PLATFORM(IOS_FAMILY)
@implementation _WKWebExtensionKeyCommand

+ (UIKeyCommand *)commandWithTitle:(NSString *)title image:(UIImage *)image input:(NSString *)input modifierFlags:(UIKeyModifierFlags)modifierFlags identifier:(NSString *)identifier
{
    RELEASE_ASSERT(title);
    RELEASE_ASSERT(input);
    RELEASE_ASSERT(identifier);

    auto *propertyList = @{
        @"title": title,
        @"activation": input,
        @"identifier": identifier,
    };

    auto *command = [UIKeyCommand commandWithTitle:title image:image action:@selector(performWebExtensionCommandForKeyCommand:) input:input modifierFlags:modifierFlags propertyList:propertyList];
    if (!command)
        return nil;

    return command;
}

- (void)performWebExtensionCommandForKeyCommand:(id)sender
{
    // To be handled by the first responder instead of this class.
    // This method exists just to make this symbol available.

    // FIXME: Would put ASSERT_NOT_REACHED() here but some compilers are warning the function is "noreturn".
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

    dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        RefPtr context = extensionContext();
        if (!context)
            return;

        context->fireCommandChangedEventIfNeeded(*this, m_oldShortcut);

        m_oldShortcut = nullString();
    }).get());
}

bool WebExtensionCommand::setActivationKey(String activationKey, SuppressEvents suppressEvents)
{
    if (activationKey.isEmpty()) {
        m_activationKey = nullString();
        return true;
    }

    if (activationKey.length() > 1)
        return false;

    static NSCharacterSet *notAllowedCharacterSet = [] {
        auto *allowedCharacterSet = [NSMutableCharacterSet alphanumericCharacterSet];
        // F1-F12.
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF704, 12)];
        // Insert, Delete, Home.
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF727, 3)];
        // End, Page Up, Page Down.
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF72B, 3)];
        // Up, Down, Left, Right.
        [allowedCharacterSet addCharactersInRange:NSMakeRange(0xF700, 4)];
        [allowedCharacterSet addCharactersInString:@",. "];

        return allowedCharacterSet.invertedSet;
    }();

    if ([activationKey.createNSString() rangeOfCharacterFromSet:notAllowedCharacterSet].location != NSNotFound)
        return false;

    if (suppressEvents == SuppressEvents::No)
        dispatchChangedEventSoonIfNeeded();

#if PLATFORM(IOS_FAMILY)
    m_activationKey = activationKey.convertToASCIIUppercase();
#else
    m_activationKey = activationKey.convertToASCIILowercase();
#endif

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

String WebExtensionCommand::userVisibleShortcut() const
{
    auto flags = modifierFlags();
    auto key = activationKey();

    if (!flags || key.isEmpty())
        return emptyString();

#if PLATFORM(MAC)
    NSKeyboardShortcut *shortcut = [NSKeyboardShortcut shortcutWithKeyEquivalent:key.createNSString().get() modifierMask:flags.toRaw()];
    return shortcut.localizedDisplayName ?: @"";
#else
    static NeverDestroyed<HashMap<String, String>> specialKeyMap = HashMap<String, String> {
        { ","_s, ","_s },
        { "."_s, "."_s },
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
        { @"\uF727", "Ins"_s },
        { @"\uF728", @"⌫" },
        { @"\uF729", @"↖︎" },
        { @"\uF72B", @"↘︎" },
        { @"\uF72C", @"⇞" },
        { @"\uF72D", @"⇟" },
        { @"\uF700", @"↑" },
        { @"\uF701", @"↓" },
        { @"\uF702", @"←" },
        { @"\uF703", @"→" }
    };

    StringBuilder stringBuilder;

    if (flags.contains(ModifierFlags::Control))
        stringBuilder.append(String(@"⌃"));

    if (flags.contains(ModifierFlags::Option))
        stringBuilder.append(String(@"⌥"));

    if (flags.contains(ModifierFlags::Shift))
        stringBuilder.append(String(@"⇧"));

    if (flags.contains(ModifierFlags::Command))
        stringBuilder.append(String(@"⌘"));

    if (auto specialKey = specialKeyMap.get().get(key); !specialKey.isEmpty())
        stringBuilder.append(specialKey);
    else
        stringBuilder.append(key.convertToASCIIUppercase());

    return stringBuilder.toString();
#endif
}

CocoaMenuItem *WebExtensionCommand::platformMenuItem() const
{
#if USE(APPKIT)
    auto *result = [[_WKWebExtensionMenuItem alloc] initWithTitle:description().createNSString().get() handler:makeBlockPtr([this, protectedThis = Ref { *this }](id sender) mutable {
        if (RefPtr context = extensionContext())
            context->performCommand(const_cast<WebExtensionCommand&>(*this), WebExtensionContext::UserTriggered::Yes);
    }).get()];

    result.keyEquivalent = activationKey().createNSString().get();
    result.keyEquivalentModifierMask = modifierFlags().toRaw();
    if (RefPtr context = extensionContext())
        result.image = toCocoaImage(context->protectedExtension()->icon(WebCore::FloatSize(16, 16)));

    return result;
#else
    return [UIAction actionWithTitle:description().createNSString().get() image:nil identifier:nil handler:makeBlockPtr([this, protectedThis = Ref { *this }](UIAction *) mutable {
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

    return [_WKWebExtensionKeyCommand commandWithTitle:description().createNSString().get() image:nil input:activationKey().createNSString().get() modifierFlags:modifierFlags().toRaw() identifier:identifier().createNSString().get()];
}

bool WebExtensionCommand::matchesKeyCommand(UIKeyCommand *keyCommand) const
{
    return keyCommand.modifierFlags == modifierFlags().toRaw() && [keyCommand.input isEqual:activationKey().createNSString().get()] && [keyCommand.propertyList[@"identifier"] isEqual:identifier().createNSString().get()];
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
