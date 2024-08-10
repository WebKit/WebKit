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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "CocoaImage.h"
#include "WebExtension.h"
#include "WebExtensionCommand.h"
#include "WebExtensionMenuItemContextType.h"
#include "WebExtensionMenuItemType.h"
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSArray;

#if USE(APPKIT)
OBJC_CLASS NSMenuItem;
using CocoaMenuItem = NSMenuItem;
#else
OBJC_CLASS UIMenuElement;
using CocoaMenuItem = UIMenuElement;
#endif

#if defined(__OBJC__) && USE(APPKIT)
using WebExtensionMenuItemHandlerBlock = void (^)(id);

@interface _WKWebExtensionMenuItem : NSMenuItem

- (instancetype)initWithTitle:(NSString *)title handler:(WebExtensionMenuItemHandlerBlock)block;

@property (nonatomic, copy) WebExtensionMenuItemHandlerBlock handler;

@end
#endif // USE(APPKIT)

namespace WebKit {

class WebExtensionCommand;
class WebExtensionContext;
struct WebExtensionMenuItemContextParameters;
struct WebExtensionMenuItemParameters;

class WebExtensionMenuItem : public RefCounted<WebExtensionMenuItem>, public CanMakeWeakPtr<WebExtensionMenuItem> {
    WTF_MAKE_NONCOPYABLE(WebExtensionMenuItem);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionMenuItem);

public:
    template<typename... Args>
    static Ref<WebExtensionMenuItem> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionMenuItem(std::forward<Args>(args)...));
    }

    using MenuItemVector = Vector<Ref<WebExtensionMenuItem>>;

    static NSArray *matchingPlatformMenuItems(const MenuItemVector&, const WebExtensionMenuItemContextParameters&, size_t limit = 0);

    bool operator==(const WebExtensionMenuItem&) const;

    WebExtensionMenuItemParameters minimalParameters() const;

    WebExtensionContext* extensionContext() const;

    bool matches(const WebExtensionMenuItemContextParameters&) const;

    void update(const WebExtensionMenuItemParameters&);

    WebExtensionMenuItemType type() const { return m_type; }
    const String& identifier() const { return m_identifier; }
    const String& title() const { return m_title; }

    WebExtensionCommand* command() const { return m_command.get(); }

    CocoaImage *icon(CGSize) const;

    bool isChecked() const { return m_checked; }
    void setChecked(bool checked) { ASSERT(isCheckedType(type())); m_checked = checked; }

    bool toggleCheckedIfNeeded(const WebExtensionMenuItemContextParameters&);

    bool isEnabled() const { return m_enabled; }
    bool isVisible() const { return m_visible; }

    const WebExtension::MatchPatternSet& documentPatterns() const { return m_documentPatterns; }
    const WebExtension::MatchPatternSet& targetPatterns() const { return m_targetPatterns; }
    OptionSet<WebExtensionMenuItemContextType> contexts() const { return m_contexts; }

    WebExtensionMenuItem* parentMenuItem() const { return m_parentMenuItem.get(); }
    const MenuItemVector& submenuItems() const { return m_submenuItems; }

    void addSubmenuItem(WebExtensionMenuItem&);
    void removeSubmenuItem(WebExtensionMenuItem&);

private:
    explicit WebExtensionMenuItem(WebExtensionContext&, const WebExtensionMenuItemParameters&);

    static String removeAmpersands(const String&);

    enum class ForceUnchecked : bool { No, Yes };
    CocoaMenuItem *platformMenuItem(const WebExtensionMenuItemContextParameters&, ForceUnchecked = ForceUnchecked::No) const;

    WeakPtr<WebExtensionContext> m_extensionContext;

    WebExtensionMenuItemType m_type;
    String m_identifier;
    String m_title;

    RefPtr<WebExtensionCommand> m_command;
    RetainPtr<NSDictionary> m_icons;

    bool m_checked : 1 { false };
    bool m_enabled : 1 { true };
    bool m_visible : 1 { true };

    WebExtension::MatchPatternSet m_documentPatterns;
    WebExtension::MatchPatternSet m_targetPatterns;
    OptionSet<WebExtensionMenuItemContextType> m_contexts;

    WeakPtr<WebExtensionMenuItem> m_parentMenuItem;
    MenuItemVector m_submenuItems;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
