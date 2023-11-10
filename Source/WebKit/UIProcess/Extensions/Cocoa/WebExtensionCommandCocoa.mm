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
#import <wtf/BlockPtr.h>
#import <wtf/text/StringBuilder.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

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

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
