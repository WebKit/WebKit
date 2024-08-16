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

#include "WebExtensionError.h"
#include "WebExtensionWindowIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/Forward.h>
#include <wtf/Identified.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakObjCPtr.h>

OBJC_PROTOCOL(WKWebExtensionWindow);

#ifdef __OBJC__
#import "WKWebExtensionWindow.h"
#endif

namespace WebKit {

class WebExtensionContext;
class WebExtensionTab;
struct WebExtensionTabQueryParameters;
struct WebExtensionWindowParameters;

enum class WebExtensionWindowTypeFilter : uint8_t {
    Normal = 1 << 0,
    Popup  = 1 << 1,
};

static constexpr OptionSet<WebExtensionWindowTypeFilter> allWebExtensionWindowTypeFilters()
{
    return {
        WebExtensionWindowTypeFilter::Normal,
        WebExtensionWindowTypeFilter::Popup
    };
}

class WebExtensionWindow : public RefCounted<WebExtensionWindow>, public CanMakeWeakPtr<WebExtensionWindow>, public Identified<WebExtensionWindowIdentifier> {
    WTF_MAKE_NONCOPYABLE(WebExtensionWindow);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionWindow);

public:
    template<typename... Args>
    static Ref<WebExtensionWindow> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionWindow(std::forward<Args>(args)...));
    }

    explicit WebExtensionWindow(const WebExtensionContext&, WKWebExtensionWindow*);

    enum class Type : uint8_t {
        Normal,
        Popup,
    };

    using TypeFilter = WebExtensionWindowTypeFilter;

    enum class State : uint8_t {
        Normal,
        Minimized,
        Maximized,
        Fullscreen,
    };

    enum class PopulateTabs : bool { No, Yes };
    enum class SkipValidation : bool { No, Yes };

    using TabVector = Vector<Ref<WebExtensionTab>>;

    WebExtensionWindowParameters parameters(PopulateTabs = PopulateTabs::No) const;
    WebExtensionWindowParameters minimalParameters() const;

    WebExtensionContext* extensionContext() const;

    bool operator==(const WebExtensionWindow&) const;

    bool matches(OptionSet<TypeFilter>) const;
    bool matches(const WebExtensionTabQueryParameters&, std::optional<WebPageProxyIdentifier> = std::nullopt) const;

    bool extensionHasAccess() const;

    TabVector tabs(SkipValidation = SkipValidation::No) const;
    RefPtr<WebExtensionTab> activeTab(SkipValidation = SkipValidation::No) const;

    Type type() const;

    State state() const;
    void setState(State, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    bool isOpen() const;
    void didOpen() { ASSERT(!m_isOpen); m_isOpen = true; }
    void didClose() { ASSERT(m_isOpen); m_isOpen = false; }

    bool isFocused() const;
    bool isFrontmost() const;
    void focus(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    bool isPrivate() const;

    // Returns the frame using top-down coordinates.
    CGRect normalizedFrame() const;

    // Handles the frame in the screen's native coordinate system.
    CGRect frame() const;
    void setFrame(CGRect, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

#if PLATFORM(MAC)
    CGRect screenFrame() const;
#endif

    void close(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

#ifdef __OBJC__
    WKWebExtensionWindow *delegate() const { return m_delegate.getAutoreleased(); }

    bool isValid() const { return m_extensionContext && m_delegate; }
#endif

private:
    WeakPtr<WebExtensionContext> m_extensionContext;
    WeakObjCPtr<WKWebExtensionWindow> m_delegate;
    bool m_isOpen : 1 { false };
    mutable bool m_private : 1 { false };
    mutable bool m_cachedPrivate : 1 { false };
    bool m_respondsToTabs : 1 { false };
    bool m_respondsToActiveTab : 1 { false };
    bool m_respondsToWindowType : 1 { false };
    bool m_respondsToWindowState : 1 { false };
    bool m_respondsToSetWindowState : 1 { false };
    bool m_respondsToIsPrivate : 1 { false };
    bool m_respondsToFrame : 1 { false };
    bool m_respondsToSetFrame : 1 { false };
    bool m_respondsToScreenFrame : 1 { false };
    bool m_respondsToFocus : 1 { false };
    bool m_respondsToClose : 1 { false };
};

#ifdef __OBJC__
WKWebExtensionWindowType toAPI(WebExtensionWindow::Type);
WKWebExtensionWindowState toAPI(WebExtensionWindow::State);
#endif

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
