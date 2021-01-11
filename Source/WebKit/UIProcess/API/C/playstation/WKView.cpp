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

#include "config.h"
#include "WKView.h"

#include "APIPageConfiguration.h"
#include "APIViewClient.h"
#include "PlayStationWebView.h"
#include "WKAPICast.h"
#include "WKSharedAPICast.h"
#include <WebCore/Cursor.h>

namespace API {
template<> struct ClientTraits<WKViewClientBase> {
    typedef std::tuple<WKViewClientV0> Versions;
};
}

WKCursorType toWKCursorType(const WebCore::Cursor& cursor)
{
    switch (cursor.type()) {
    case WebCore::Cursor::Hand:
        return kWKCursorTypeHand;
    case WebCore::Cursor::None:
        return kWKCursorTypeNone;
    case WebCore::Cursor::Pointer:
    default:
        return kWKCursorTypePointer;
    }
}

WKViewRef WKViewCreate(WKPageConfigurationRef configuration)
{
    return WebKit::toAPI(WebKit::PlayStationWebView::create(*WebKit::toImpl(configuration)).leakRef());
}

WKPageRef WKViewGetPage(WKViewRef view)
{
    return WebKit::toAPI(WebKit::toImpl(view)->page());
}

void WKViewSetSize(WKViewRef view, WKSize viewSize)
{
    WebKit::toImpl(view)->setViewSize(WebKit::toIntSize(viewSize));
}

static void setViewActivityStateFlag(WKViewRef view, WebCore::ActivityState::Flag flag, bool set)
{
    auto viewState = WebKit::toImpl(view)->viewState();
    if (set)
        viewState.add(flag);
    else
        viewState.remove(flag);
    WebKit::toImpl(view)->setViewState(viewState);
}

void WKViewSetFocus(WKViewRef view, bool focused)
{
    setViewActivityStateFlag(view, WebCore::ActivityState::IsFocused, focused);
}

void WKViewSetActive(WKViewRef view, bool active)
{
    setViewActivityStateFlag(view, WebCore::ActivityState::WindowIsActive, active);
}

void WKViewSetVisible(WKViewRef view, bool visible)
{
    setViewActivityStateFlag(view, WebCore::ActivityState::IsVisible, visible);
}

void WKViewWillEnterFullScreen(WKViewRef view)
{
#if ENABLE(FULLSCREEN_API)
    WebKit::toImpl(view)->willEnterFullScreen();
#endif
}

void WKViewDidEnterFullScreen(WKViewRef view)
{
#if ENABLE(FULLSCREEN_API)
    WebKit::toImpl(view)->didEnterFullScreen();
#endif
}

void WKViewWillExitFullScreen(WKViewRef view)
{
#if ENABLE(FULLSCREEN_API)
    WebKit::toImpl(view)->willExitFullScreen();
#endif
}

void WKViewDidExitFullScreen(WKViewRef view)
{
#if ENABLE(FULLSCREEN_API)
    WebKit::toImpl(view)->didExitFullScreen();
#endif
}

void WKViewRequestExitFullScreen(WKViewRef view)
{
#if ENABLE(FULLSCREEN_API)
    WebKit::toImpl(view)->requestExitFullScreen();
#endif
}

bool WKViewIsFullScreen(WKViewRef view)
{
#if ENABLE(FULLSCREEN_API)
    return WebKit::toImpl(view)->isFullScreen();
#else
    return false;
#endif
}

void WKViewSetViewClient(WKViewRef view, const WKViewClientBase* client)
{
    class ViewClient final : public API::Client<WKViewClientBase>, public API::ViewClient {
    public:
        explicit ViewClient(const WKViewClientBase* client)
        {
            initialize(client);
        }

    private:
        void setViewNeedsDisplay(WebKit::PlayStationWebView& view, const WebCore::Region& region) final
        {
            if (!m_client.setViewNeedsDisplay)
                return;
            m_client.setViewNeedsDisplay(WebKit::toAPI(&view), WebKit::toAPI(region.bounds()), m_client.base.clientInfo);
        }

        void enterFullScreen(WebKit::PlayStationWebView& view)
        {
            if (!m_client.enterFullScreen)
                return;
            m_client.enterFullScreen(WebKit::toAPI(&view), m_client.base.clientInfo);
        }
        
        void exitFullScreen(WebKit::PlayStationWebView& view)
        {
            if (!m_client.exitFullScreen)
                return;
            m_client.exitFullScreen(WebKit::toAPI(&view), m_client.base.clientInfo);
        }
        
        void closeFullScreen(WebKit::PlayStationWebView& view)
        {
            if (!m_client.closeFullScreen)
                return;
            m_client.closeFullScreen(WebKit::toAPI(&view), m_client.base.clientInfo);
        }
        
        void beganEnterFullScreen(WebKit::PlayStationWebView& view, const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame)
        {
            if (!m_client.beganEnterFullScreen)
                return;
            m_client.beganEnterFullScreen(WebKit::toAPI(&view), WebKit::toAPI(initialFrame), WebKit::toAPI(finalFrame), m_client.base.clientInfo);
        }
        
        void beganExitFullScreen(WebKit::PlayStationWebView& view, const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame)
        {
            if (!m_client.beganExitFullScreen)
                return;
            m_client.beganExitFullScreen(WebKit::toAPI(&view), WebKit::toAPI(initialFrame), WebKit::toAPI(finalFrame), m_client.base.clientInfo);
        }

        void setCursor(WebKit::PlayStationWebView& view, const WebCore::Cursor& cursor) final
        {
            if (!m_client.setCursor)
                return;
            m_client.setCursor(WebKit::toAPI(&view), toWKCursorType(cursor), m_client.base.clientInfo);
        }
    };

    WebKit::toImpl(view)->setClient(makeUnique<ViewClient>(client));
}
