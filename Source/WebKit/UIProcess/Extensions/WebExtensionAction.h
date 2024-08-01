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
#include "CocoaImage.h"
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSError;
OBJC_CLASS WKWebExtensionAction;
OBJC_CLASS WKWebView;
OBJC_CLASS _WKWebExtensionActionWebView;
OBJC_CLASS _WKWebExtensionActionWebViewDelegate;

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIViewController;
OBJC_CLASS _WKWebExtensionActionViewController;
#endif

#if PLATFORM(MAC)
OBJC_CLASS NSPopover;
OBJC_CLASS _WKWebExtensionActionPopover;
#endif

namespace WebKit {

class WebExtensionContext;
class WebExtensionTab;
class WebExtensionWindow;

class WebExtensionAction : public API::ObjectImpl<API::Object::Type::WebExtensionAction>, public CanMakeWeakPtr<WebExtensionAction> {
    WTF_MAKE_NONCOPYABLE(WebExtensionAction);

public:
    template<typename... Args>
    static Ref<WebExtensionAction> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionAction(std::forward<Args>(args)...));
    }

    explicit WebExtensionAction(WebExtensionContext&);
    explicit WebExtensionAction(WebExtensionContext&, WebExtensionTab&);
    explicit WebExtensionAction(WebExtensionContext&, WebExtensionWindow&);

    enum class FallbackWhenEmpty { No, Yes };

#if PLATFORM(MAC)
    enum class Appearance : uint8_t { Default, Light, Dark, Both };
#endif

    bool operator==(const WebExtensionAction&) const;

    WebExtensionContext* extensionContext() const;
    WebExtensionTab* tab() { return m_tab.get(); }
    WebExtensionWindow* window() { return m_window.get(); }

    void clearCustomizations();
    void clearBlockedResourceCount();

    void propertiesDidChange();

    CocoaImage *icon(CGSize);
    void setIconsDictionary(NSDictionary *);

    String label(FallbackWhenEmpty = FallbackWhenEmpty::Yes) const;
    void setLabel(String);

    String badgeText() const;
    void setBadgeText(String);

    bool hasUnreadBadgeText() const;
    void setHasUnreadBadgeText(bool);

    void incrementBlockedResourceCount(ssize_t amount);

    bool isEnabled() const;
    void setEnabled(std::optional<bool>);

    bool presentsPopup() const { return !popupPath().isEmpty(); }
    bool canProgrammaticallyPresentPopup() const;

    String popupPath() const;
    void setPopupPath(String);

    NSString *popupWebViewInspectionName();
    void setPopupWebViewInspectionName(const String&);

#if PLATFORM(IOS_FAMILY)
    UIViewController *popupViewController();
#endif

#if PLATFORM(MAC)
    NSPopover *popupPopover();

    Appearance popupPopoverAppearance() const { return m_popoverAppearance; }
    void setPopupPopoverAppearance(Appearance);
#endif

    WKWebView *popupWebView();
    bool hasPopupWebView() const { return !!m_popupWebView; }

    bool presentsPopupWhenReady() const { return m_presentsPopupWhenReady; }
    bool popupPresented() const { return m_popupPresented; }

    void presentPopupWhenReady();
    void popupDidFinishDocumentLoad();
    void readyToPresentPopup();
    void popupSizeDidChange();
    void closePopup();

    NSArray *platformMenuItems() const;

#ifdef __OBJC__
    WKWebExtensionAction *wrapper() const { return (WKWebExtensionAction *)API::ObjectImpl<API::Object::Type::WebExtensionAction>::wrapper(); }
#endif

private:
    WebExtensionAction* fallbackAction() const;

#if PLATFORM(MAC)
    void detectPopoverColorScheme();
#endif

    WeakPtr<WebExtensionContext> m_extensionContext;
    RefPtr<WebExtensionTab> m_tab;
    RefPtr<WebExtensionWindow> m_window;

#if PLATFORM(IOS_FAMILY)
    RetainPtr<_WKWebExtensionActionViewController> m_popupViewController;
#endif

#if PLATFORM(MAC)
    RetainPtr<_WKWebExtensionActionPopover> m_popupPopover;
    Appearance m_popoverAppearance { Appearance::Default };
#endif

    RetainPtr<_WKWebExtensionActionWebView> m_popupWebView;
    RetainPtr<_WKWebExtensionActionWebViewDelegate> m_popupWebViewDelegate;
    String m_customPopupPath;
    String m_popupWebViewInspectionName;

    RetainPtr<NSDictionary> m_customIcons;
    String m_customLabel;
    String m_customBadgeText;
    ssize_t m_blockedResourceCount { 0 };
    std::optional<bool> m_customEnabled;
    std::optional<bool> m_hasUnreadBadgeText;
    bool m_presentsPopupWhenReady : 1 { false };
    bool m_popupPresented : 1 { false };
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
