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

#include "WebExtensionTabIdentifier.h"
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSLocale;
OBJC_CLASS WKWebView;
OBJC_PROTOCOL(_WKWebExtensionTab);

namespace WebKit {

class WebExtensionContext;
class WebExtensionWindow;

class WebExtensionTab : public RefCounted<WebExtensionTab> {
    WTF_MAKE_NONCOPYABLE(WebExtensionTab);
    WTF_MAKE_FAST_ALLOCATED;

public:
    template<typename... Args>
    static Ref<WebExtensionTab> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionTab(std::forward<Args>(args)...));
    }

    explicit WebExtensionTab(const WebExtensionContext&, _WKWebExtensionTab *);

    enum class ChangedProperties : uint16_t {
        None       = 0,
        Audible    = 1 << 1,
        Loading    = 1 << 2,
        Muted      = 1 << 3,
        Pinned     = 1 << 4,
        ReaderMode = 1 << 5,
        Size       = 1 << 6,
        Title      = 1 << 7,
        URL        = 1 << 8,
        ZoomFactor = 1 << 9,
        All        = Audible | Loading | Muted | Pinned | ReaderMode | Size | Title | URL | ZoomFactor,
    };

    WebExtensionTabIdentifier identifier() const { return m_identifier; }
    WebExtensionContext* extensionContext() const { return m_extensionContext.get(); }

    bool operator==(const WebExtensionTab&) const;

    RefPtr<WebExtensionWindow> window() const;
    RefPtr<WebExtensionTab> parentTab() const;

    WKWebView *mainWebView() const;
    NSArray *webViews() const;

    String title() const;

    bool isSelected() const;
    bool isPinned() const;

    void toggleReaderMode();

    bool isReaderModeAvailable() const;
    bool isShowingReaderMode() const;

    void mute();
    void unmute();

    bool isAudible() const;
    bool isMuted() const;

    CGSize size() const;
    double zoomFactor() const;

    URL url() const;
    URL pendingURL() const;

    bool isLoadingComplete() const;

    void detectWebpageLocale(CompletionHandler<void(NSLocale *)>&&);

    void loadURL(URL);

    void reload();
    void reloadFromOrigin();

    void goBack();
    void goForward();

    void close();
    void select();

#ifdef __OBJC__
    _WKWebExtensionTab *delegate() const { return m_delegate.getAutoreleased(); }

    bool isValid() const { return m_extensionContext && m_delegate; }
#endif

private:
    WebExtensionTabIdentifier m_identifier;
    WeakPtr<WebExtensionContext> m_extensionContext;
    WeakObjCPtr<_WKWebExtensionTab> m_delegate;
    bool m_respondsToWindow : 1 { false };
    bool m_respondsToParentTab : 1 { false };
    bool m_respondsToMainWebView : 1 { false };
    bool m_respondsToWebViews : 1 { false };
    bool m_respondsToTabTitle : 1 { false };
    bool m_respondsToIsSelected : 1 { false };
    bool m_respondsToIsPinned : 1 { false };
    bool m_respondsToIsEphemeral : 1 { false };
    bool m_respondsToIsReaderModeAvailable : 1 { false };
    bool m_respondsToIsShowingReaderMode : 1 { false };
    bool m_respondsToToggleReaderMode : 1 { false };
    bool m_respondsToIsAudible : 1 { false };
    bool m_respondsToIsMuted : 1 { false };
    bool m_respondsToMute : 1 { false };
    bool m_respondsToUnmute : 1 { false };
    bool m_respondsToSize : 1 { false };
    bool m_respondsToZoomFactor : 1 { false };
    bool m_respondsToURL : 1 { false };
    bool m_respondsToPendingURL : 1 { false };
    bool m_respondsToIsLoadingComplete : 1 { false };
    bool m_respondsToDetectWebpageLocale : 1 { false };
    bool m_respondsToLoadURL : 1 { false };
    bool m_respondsToReload : 1 { false };
    bool m_respondsToReloadFromOrigin : 1 { false };
    bool m_respondsToGoBack : 1 { false };
    bool m_respondsToGoForward : 1 { false };
    bool m_respondsToClose : 1 { false };
    bool m_respondsToSelect : 1 { false };
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
