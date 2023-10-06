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

#include "WebExtensionWindowIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>

OBJC_PROTOCOL(_WKWebExtensionWindow);

#ifdef __OBJC__
#import "_WKWebExtensionWindow.h"
#endif

namespace WebKit {

class WebExtensionContext;
class WebExtensionTab;
struct WebExtensionTabQueryParameters;
struct WebExtensionWindowParameters;

class WebExtensionWindow : public RefCounted<WebExtensionWindow>, public CanMakeWeakPtr<WebExtensionWindow> {
    WTF_MAKE_NONCOPYABLE(WebExtensionWindow);
    WTF_MAKE_FAST_ALLOCATED;

public:
    template<typename... Args>
    static Ref<WebExtensionWindow> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionWindow(std::forward<Args>(args)...));
    }

    explicit WebExtensionWindow(const WebExtensionContext&, _WKWebExtensionWindow*);

    enum class Type : uint8_t {
        Normal,
        Popup,
    };

    enum class TypeFilter : uint8_t {
        None   = 0,
        Normal = 1 << 0,
        Popup  = 1 << 1,
        All    = Normal | Popup,
    };

    enum class State : uint8_t {
        Normal,
        Minimized,
        Maximized,
        Fullscreen,
    };

    enum class PopulateTabs : bool { No, Yes };

    using Error = std::optional<String>;
    using TabVector = Vector<Ref<WebExtensionTab>>;

    WebExtensionWindowIdentifier identifier() const { return m_identifier; }
    WebExtensionWindowParameters parameters(PopulateTabs = PopulateTabs::No) const;
    WebExtensionWindowParameters minimalParameters() const;

    WebExtensionContext* extensionContext() const;

    bool operator==(const WebExtensionWindow&) const;

    bool matches(OptionSet<TypeFilter>) const;
    bool matches(const WebExtensionTabQueryParameters&, std::optional<WebPageProxyIdentifier> = std::nullopt) const;

    bool extensionHasAccess() const;

    TabVector tabs() const;
    RefPtr<WebExtensionTab> activeTab() const;

    Type type() const;

    State state() const;
    void setState(State, CompletionHandler<void(Error)>&&);

    bool isFocused() const;
    bool isFrontmost() const;
    void focus(CompletionHandler<void(Error)>&&);

    bool isPrivate() const;

    // Returns the frame using top-down coordinates.
    CGRect normalizedFrame() const;

    // Handles the frame in the screen's native coordinate system.
    CGRect frame() const;
    void setFrame(CGRect, CompletionHandler<void(Error)>&&);

    CGRect screenFrame() const;

    void close(CompletionHandler<void(Error)>&&);

#ifdef __OBJC__
    _WKWebExtensionWindow *delegate() const { return m_delegate.getAutoreleased(); }

    bool isValid() const { return m_extensionContext && m_delegate; }
#endif

private:
    WebExtensionWindowIdentifier m_identifier;
    WeakPtr<WebExtensionContext> m_extensionContext;
    WeakObjCPtr<_WKWebExtensionWindow> m_delegate;
    mutable bool m_private : 1 { false };
    mutable bool m_cachedPrivate : 1 { false };
    bool m_respondsToTabs : 1 { false };
    bool m_respondsToActiveTab : 1 { false };
    bool m_respondsToWindowType : 1 { false };
    bool m_respondsToWindowState : 1 { false };
    bool m_respondsToSetWindowState : 1 { false };
    bool m_respondsToIsUsingPrivateBrowsing : 1 { false };
    bool m_respondsToFrame : 1 { false };
    bool m_respondsToSetFrame : 1 { false };
    bool m_respondsToScreenFrame : 1 { false };
    bool m_respondsToFocus : 1 { false };
    bool m_respondsToClose : 1 { false };
};

#ifdef __OBJC__
_WKWebExtensionWindowType toAPI(WebExtensionWindow::Type);
_WKWebExtensionWindowState toAPI(WebExtensionWindow::State);
#endif

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebExtensionWindow::TypeFilter> {
    using values = EnumValues<
        WebKit::WebExtensionWindow::TypeFilter,
        WebKit::WebExtensionWindow::TypeFilter::None,
        WebKit::WebExtensionWindow::TypeFilter::Normal,
        WebKit::WebExtensionWindow::TypeFilter::Popup,
        WebKit::WebExtensionWindow::TypeFilter::All
    >;
};

} // namespace WTF

#endif // ENABLE(WK_WEB_EXTENSIONS)
