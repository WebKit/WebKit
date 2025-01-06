/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "APIObject.h"
#include "APIViewClient.h"
#include "PageClientImpl.h"
#include "WKView.h"
#include "WebPageProxy.h"
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class PlayStationWebView : public API::ObjectImpl<API::Object::Type::View> {
    WTF_MAKE_TZONE_ALLOCATED(PlayStationWebView);
public:
#if USE(WPE_BACKEND_PLAYSTATION)
    static RefPtr<PlayStationWebView> create(struct wpe_view_backend*, const API::PageConfiguration&);
#else
    static RefPtr<PlayStationWebView> create(const API::PageConfiguration&);
#endif
    virtual ~PlayStationWebView();

    void setClient(std::unique_ptr<API::ViewClient>&&);

    WebPageProxy* page() { return m_page.get(); }

    void setViewSize(WebCore::IntSize);
    WebCore::IntSize viewSize() const { return m_viewSize; }

    void setViewState(OptionSet<WebCore::ActivityState>);
    OptionSet<WebCore::ActivityState> viewState() const { return m_viewStateFlags; }

#if USE(WPE_BACKEND_PLAYSTATION)
    struct wpe_view_backend* backend() { return m_backend; }
#endif

#if ENABLE(FULLSCREEN_API)
    void willEnterFullScreen();
    void didEnterFullScreen();
    void willExitFullScreen();
    void didExitFullScreen();
    void requestExitFullScreen();
#endif

    // Functions called by PageClientImpl
    void setViewNeedsDisplay(const WebCore::Region&);
#if ENABLE(FULLSCREEN_API)
    bool isFullScreen();
    void closeFullScreenManager();
    void enterFullScreen();
    void exitFullScreen();
    void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&);
    void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&);
#endif
    void setCursor(const WebCore::Cursor&);

private:
#if USE(WPE_BACKEND_PLAYSTATION)
    PlayStationWebView(struct wpe_view_backend*, const API::PageConfiguration&);
#else
    PlayStationWebView(const API::PageConfiguration&);
#endif

    std::unique_ptr<API::ViewClient> m_client;
    std::unique_ptr<WebKit::PageClientImpl> m_pageClient;
    RefPtr<WebPageProxy> m_page;
    OptionSet<WebCore::ActivityState> m_viewStateFlags;

    WebCore::IntSize m_viewSize;
#if USE(WPE_BACKEND_PLAYSTATION)
    struct wpe_view_backend* m_backend;
#endif
#if ENABLE(FULLSCREEN_API)
    bool m_isFullScreen { false };
#endif
};

} // namespace WebKit
