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
    if (m_oldShortcut)
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

bool WebExtensionCommand::setActivationKey(RefPtr<StringImpl> activationKey, SuppressEvents suppressEvents)
{
    if (isEmpty(activationKey)) {
        m_activationKey = nullString();
        return true;
    }

    if (length(activationKey) > 1)
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

    if ([createNSString(activationKey) rangeOfCharacterFromSet:notAllowedCharacterSet].location != NSNotFound)
        return false;

    if (suppressEvents == SuppressEvents::No)
        dispatchChangedEventSoonIfNeeded();

#if PLATFORM(IOS_FAMILY)
    m_activationKey = convertToASCIIUppercase(activationKey);
#else
    m_activationKey = convertToASCIILowercase(activationKey);
#endif

    return true;
}

RefPtr<StringImpl> WebExtensionCommand::shortcutString() const
{
    auto flags = modifierFlags();
    auto key = activationKey();

    if (!flags || isEmpty(key))
        return emptyString();

    static NeverDestroyed<HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>> specialKeyMap = HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> {
        { ","_s, "Comma"_s },
        { "."_s, "Period"_s },
        { " "_s, "Space"_s },
        { StringImpl::create(@"\uF704"), "F1"_s },
        { StringImpl::create(@"\uF705"), "F2"_s },
        { StringImpl::create(@"\uF706"), "F3"_s },
        { StringImpl::create(@"\uF707"), "F4"_s },
        { StringImpl::create(@"\uF708"), "F5"_s },
        { StringImpl::create(@"\uF709"), "F6"_s },
        { StringImpl::create(@"\uF70A"), "F7"_s },
        { StringImpl::create(@"\uF70B"), "F8"_s },
        { StringImpl::create(@"\uF70C"), "F9"_s },
        { StringImpl::create(@"\uF70D"), "F10"_s },
        { StringImpl::create(@"\uF70E"), "F11"_s },
        { StringImpl::create(@"\uF70F"), "F12"_s },
        { StringImpl::create(@"\uF727"), "Insert"_s },
        { StringImpl::create(@"\uF728"), "Delete"_s },
        { StringImpl::create(@"\uF729"), "Home"_s },
        { StringImpl::create(@"\uF72B"), "End"_s },
        { StringImpl::create(@"\uF72C"), "PageUp"_s },
        { StringImpl::create(@"\uF72D"), "PageDown"_s },
        { StringImpl::create(@"\uF700"), "Up"_s },
        { StringImpl::create(@"\uF701"), "Down"_s },
        { StringImpl::create(@"\uF702"), "Left"_s },
        { StringImpl::create(@"\uF703"), "Right"_s }
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

    if (auto specialKey = specialKeyMap.get().get(key); !isEmpty(specialKey))
        stringBuilder.append(specialKey);
    else
        stringBuilder.append(convertToASCIIUppercase(key));

    return stringBuilder.toString();
}

RefPtr<StringImpl> WebExtensionCommand::userVisibleShortcut() const
{
    auto flags = modifierFlags();
    auto key = activationKey();

    if (!flags || isEmpty(key))
        return emptyString();

#if PLATFORM(MAC)
    NSKeyboardShortcut *shortcut = [NSKeyboardShortcut shortcutWithKeyEquivalent:createNSString(key).get() modifierMask:flags.toRaw()];
    return StringImpl::create(shortcut.localizedDisplayName ?: @"");
#else
    static NeverDestroyed<HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>> specialKeyMap = HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> {
        { ","_s, ","_s },
        { "."_s, "."_s },
        { " "_s, "Space"_s },
        { StringImpl::create(@"\uF704"), "F1"_s },
        { StringImpl::create(@"\uF705"), "F2"_s },
        { StringImpl::create(@"\uF706"), "F3"_s },
        { StringImpl::create(@"\uF707"), "F4"_s },
        { StringImpl::create(@"\uF708"), "F5"_s },
        { StringImpl::create(@"\uF709"), "F6"_s },
        { StringImpl::create(@"\uF70A"), "F7"_s },
        { StringImpl::create(@"\uF70B"), "F8"_s },
        { StringImpl::create(@"\uF70C"), "F9"_s },
        { StringImpl::create(@"\uF70D"), "F10"_s },
        { StringImpl::create(@"\uF70E"), "F11"_s },
        { StringImpl::create(@"\uF70F"), "F12"_s },
        { StringImpl::create(@"\uF727"), "Ins"_s },
        { StringImpl::create(@"\uF728"), StringImpl::create(@"⌫") },
        { StringImpl::create(@"\uF729"), StringImpl::create(@"↖︎") },
        { StringImpl::create(@"\uF72B"), StringImpl::create(@"↘︎") },
        { StringImpl::create(@"\uF72C"), StringImpl::create(@"⇞") },
        { StringImpl::create(@"\uF72D"), StringImpl::create(@"⇟") },
        { StringImpl::create(@"\uF700"), StringImpl::create(@"↑") },
        { StringImpl::create(@"\uF701"), StringImpl::create(@"↓") },
        { StringImpl::create(@"\uF702"), StringImpl::create(@"←") },
        { StringImpl::create(@"\uF703"), StringImpl::create(@"→") }
    };

    StringBuilder stringBuilder;

    if (flags.contains(ModifierFlags::Control))
        stringBuilder.append(StringImpl::create(@"⌃").get());

    if (flags.contains(ModifierFlags::Option))
        stringBuilder.append(StringImpl::create(@"⌥").get());

    if (flags.contains(ModifierFlags::Shift))
        stringBuilder.append(StringImpl::create(@"⇧").get());

    if (flags.contains(ModifierFlags::Command))
        stringBuilder.append(StringImpl::create(@"⌘").get());

    if (auto specialKey = specialKeyMap.get().get(key); !isEmpty(specialKey))
        stringBuilder.append(specialKey);
    else
        stringBuilder.append(convertToASCIIUppercase(key));

    return stringBuilder.toString();
#endif
}

CocoaMenuItem *WebExtensionCommand::platformMenuItem() const
{
#if USE(APPKIT)
    auto *result = [[_WKWebExtensionMenuItem alloc] initWithTitle:createNSString(description()).get() handler:makeBlockPtr([this, protectedThis = Ref { *this }](id sender) mutable {
        if (RefPtr context = extensionContext())
            context->performCommand(const_cast<WebExtensionCommand&>(*this), WebExtensionContext::UserTriggered::Yes);
    }).get()];

    result.keyEquivalent = createNSString(activationKey()).get();
    result.keyEquivalentModifierMask = modifierFlags().toRaw();
    if (RefPtr context = extensionContext())
        result.image = toCocoaImage(protect(context->extension())->icon(WebCore::FloatSize(16, 16)));

    return result;
#else
    return [UIAction actionWithTitle:createNSString(description()).get() image:nil identifier:nil handler:makeBlockPtr([this, protectedThis = Ref { *this }](UIAction *) mutable {
        if (RefPtr context = extensionContext())
            context->performCommand(const_cast<WebExtensionCommand&>(*this), WebExtensionContext::UserTriggered::Yes);
    }).get()];
#endif
}

#if PLATFORM(IOS_FAMILY)
UIKeyCommand *WebExtensionCommand::keyCommand() const
{
    if (isEmpty(activationKey()))
        return nil;

    return [_WKWebExtensionKeyCommand commandWithTitle:createNSString(description()).get() image:nil input:createNSString(activationKey()).get() modifierFlags:modifierFlags().toRaw() identifier:createNSString(identifier()).get()];
}

bool WebExtensionCommand::matchesKeyCommand(UIKeyCommand *keyCommand) const
{
    return keyCommand.modifierFlags == modifierFlags().toRaw() && [keyCommand.input isEqual:createNSString(activationKey()).get()] && [keyCommand.propertyList[@"identifier"] isEqual:createNSString(identifier()).get()];
}
#endif

#if USE(APPKIT)
bool WebExtensionCommand::matchesEvent(NSEvent *event) const
{
    if (event.type != NSEventTypeKeyDown || event.isARepeat)
        return false;

    if (isEmpty(activationKey()))
        return false;

    auto expectedModifierFlags = modifierFlags().toRaw();
    if ((event.modifierFlags & expectedModifierFlags) != expectedModifierFlags)
        return false;

    static NeverDestroyed<HashMap<RefPtr<StringImpl>, uint16_t>> specialKeyMap = HashMap<RefPtr<StringImpl>, uint16_t> {
        { ","_s, kVK_ANSI_Comma },
        { "."_s, kVK_ANSI_Period },
        { " "_s, kVK_Space },
        { StringImpl::create(@"\uF704"), kVK_F1 },
        { StringImpl::create(@"\uF705"), kVK_F2 },
        { StringImpl::create(@"\uF706"), kVK_F3 },
        { StringImpl::create(@"\uF707"), kVK_F4 },
        { StringImpl::create(@"\uF708"), kVK_F5 },
        { StringImpl::create(@"\uF709"), kVK_F6 },
        { StringImpl::create(@"\uF70A"), kVK_F7 },
        { StringImpl::create(@"\uF70B"), kVK_F8 },
        { StringImpl::create(@"\uF70C"), kVK_F9 },
        { StringImpl::create(@"\uF70D"), kVK_F10 },
        { StringImpl::create(@"\uF70E"), kVK_F11 },
        { StringImpl::create(@"\uF70F"), kVK_F12 },
        // Insert (\uF727) is not present on Apple keyboards.
        { StringImpl::create(@"\uF728"), kVK_ForwardDelete },
        { StringImpl::create(@"\uF729"), kVK_Home },
        { StringImpl::create(@"\uF72B"), kVK_End },
        { StringImpl::create(@"\uF72C"), kVK_PageUp },
        { StringImpl::create(@"\uF72D"), kVK_PageDown },
        { StringImpl::create(@"\uF700"), kVK_UpArrow },
        { StringImpl::create(@"\uF701"), kVK_DownArrow },
        { StringImpl::create(@"\uF702"), kVK_LeftArrow },
        { StringImpl::create(@"\uF703"), kVK_RightArrow }
    };

    if (equalIgnoringASCIICase(activationKey(), StringImpl::create(event.charactersIgnoringModifiers).get()))
        return true;

    if (auto mappedKeyCode = specialKeyMap.get().get(activationKey()))
        return mappedKeyCode == event.keyCode;

    return false;
}
#endif // USE(APPKIT)

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
