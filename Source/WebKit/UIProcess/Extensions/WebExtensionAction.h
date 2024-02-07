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

#include "APIObject.h"
#include "CocoaImage.h"
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSError;
OBJC_CLASS WKWebView;
OBJC_CLASS _WKWebExtensionAction;
OBJC_CLASS _WKWebExtensionActionWebView;
OBJC_CLASS _WKWebExtensionActionWebViewDelegate;

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

    enum class LoadOnFirstAccess { No, Yes };
    enum class FallbackWhenEmpty { No, Yes };

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

    WKWebView *popupWebView(LoadOnFirstAccess = LoadOnFirstAccess::Yes);
    void presentPopupWhenReady();
    void readyToPresentPopup();
    void popupSizeDidChange();
    void popupDidClose();
    void closePopupWebView();

    NSArray *platformMenuItems() const;

#ifdef __OBJC__
    _WKWebExtensionAction *wrapper() const { return (_WKWebExtensionAction *)API::ObjectImpl<API::Object::Type::WebExtensionAction>::wrapper(); }
#endif

private:
    WebExtensionAction* fallbackAction() const;

    WeakPtr<WebExtensionContext> m_extensionContext;
    RefPtr<WebExtensionTab> m_tab;
    RefPtr<WebExtensionWindow> m_window;

    RetainPtr<_WKWebExtensionActionWebView> m_popupWebView;
    RetainPtr<_WKWebExtensionActionWebViewDelegate> m_popupWebViewDelegate;
    String m_customPopupPath;

    RetainPtr<NSDictionary> m_customIcons;
    String m_customLabel;
    String m_customBadgeText;
    ssize_t m_blockedResourceCount { 0 };
    std::optional<bool> m_customEnabled;
    std::optional<bool> m_hasUnreadBadgeText;
    bool m_popupPresented : 1 { false };
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
