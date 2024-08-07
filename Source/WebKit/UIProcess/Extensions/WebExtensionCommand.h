/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIObject.h"
#include "WebExtension.h"
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if defined(__OBJC__) && PLATFORM(IOS_FAMILY)
#include <UIKit/UIKeyCommand.h>
#endif

OBJC_CLASS WKWebExtensionCommand;

#if USE(APPKIT)
OBJC_CLASS NSEvent;
OBJC_CLASS NSMenuItem;
using CocoaMenuItem = NSMenuItem;
#else
OBJC_CLASS UIKeyCommand;
OBJC_CLASS UIMenuElement;
using CocoaMenuItem = UIMenuElement;
#endif

#if defined(__OBJC__) && PLATFORM(IOS_FAMILY)
@interface _WKWebExtensionKeyCommand : UIKeyCommand

+ (UIKeyCommand *)commandWithTitle:(NSString *)title image:(UIImage *)image input:(NSString *)input modifierFlags:(UIKeyModifierFlags)modifierFlags identifier:(NSString *)identifier;

@end
#endif // PLATFORM(IOS_FAMILY)

namespace WebKit {

class WebExtensionContext;
struct WebExtensionCommandParameters;

class WebExtensionCommand : public API::ObjectImpl<API::Object::Type::WebExtensionCommand>, public CanMakeWeakPtr<WebExtensionCommand> {
    WTF_MAKE_NONCOPYABLE(WebExtensionCommand);

public:
    template<typename... Args>
    static Ref<WebExtensionCommand> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionCommand(std::forward<Args>(args)...));
    }

    explicit WebExtensionCommand(WebExtensionContext&, const WebExtension::CommandData&);

    using ModifierFlags = WebExtension::ModifierFlags;

    bool operator==(const WebExtensionCommand&) const;

    WebExtensionCommandParameters parameters() const;

    WebExtensionContext* extensionContext() const;

    bool isActionCommand() const;

    const String& identifier() const { return m_identifier; }
    const String& description() const { return m_description; }

    const String& activationKey() const { return m_modifierFlags ? m_activationKey : nullString(); }
    bool setActivationKey(String);

    OptionSet<ModifierFlags> modifierFlags() const { return !m_activationKey.isEmpty() ? m_modifierFlags : OptionSet<ModifierFlags> { }; }
    void setModifierFlags(OptionSet<ModifierFlags> modifierFlags) { dispatchChangedEventSoonIfNeeded(); m_modifierFlags = modifierFlags; }

    String shortcutString() const;

    CocoaMenuItem *platformMenuItem() const;

#if PLATFORM(IOS_FAMILY)
    UIKeyCommand *keyCommand() const;
    bool matchesKeyCommand(UIKeyCommand *) const;
#endif

#if USE(APPKIT)
    bool matchesEvent(NSEvent *) const;
#endif

#ifdef __OBJC__
    WKWebExtensionCommand *wrapper() const { return (WKWebExtensionCommand *)API::ObjectImpl<API::Object::Type::WebExtensionCommand>::wrapper(); }
#endif

private:
    void dispatchChangedEventSoonIfNeeded();

    WeakPtr<WebExtensionContext> m_extensionContext;
    String m_identifier;
    String m_description;
    String m_activationKey;
    OptionSet<ModifierFlags> m_modifierFlags;
    String m_oldShortcut;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
