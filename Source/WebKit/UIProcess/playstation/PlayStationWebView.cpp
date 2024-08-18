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
#include "PlayStationWebView.h"

#include "APIPageConfiguration.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "WebProcessPool.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(WPE_BACKEND_PLAYSTATION)
#include <wpe/playstation.h>
#endif

namespace WebKit {

#if USE(WPE_BACKEND_PLAYSTATION)

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlayStationWebView);

RefPtr<PlayStationWebView> PlayStationWebView::create(struct wpe_view_backend* backend, const API::PageConfiguration& configuration)
{
    return adoptRef(*new PlayStationWebView(backend, configuration));
}

PlayStationWebView::PlayStationWebView(struct wpe_view_backend* backend, const API::PageConfiguration& conf)
    : m_pageClient(makeUniqueWithoutRefCountedCheck<PageClientImpl>(*this))
    , m_viewStateFlags { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsFocused, WebCore::ActivityState::IsVisible, WebCore::ActivityState::IsInWindow }
    , m_backend(backend)
{
    auto configuration = conf.copy();
    auto& pool = configuration->processPool();
    m_page = pool.createWebPage(*m_pageClient, WTFMove(configuration));

    wpe_view_backend_initialize(m_backend);

    auto& openerInfo = m_page->configuration().openerInfo();
    m_page->initializeWebPage(openerInfo ? openerInfo->site : Site(aboutBlankURL()));
}

#else

RefPtr<PlayStationWebView> PlayStationWebView::create(const API::PageConfiguration& configuration)
{
    return adoptRef(*new PlayStationWebView(configuration));
}

PlayStationWebView::PlayStationWebView(const API::PageConfiguration& conf)
    : m_pageClient(makeUniqueWithoutRefCountedCheck<PageClientImpl>(*this))
    , m_viewStateFlags { WebCore::ActivityState::WindowIsActive, WebCore::ActivityState::IsFocused, WebCore::ActivityState::IsVisible, WebCore::ActivityState::IsInWindow }
{
    auto configuration = conf.copy();
    auto& pool = configuration->processPool();
    m_page = pool.createWebPage(*m_pageClient, WTFMove(configuration));

    auto& openerInfo = m_page->configuration().openerInfo();
    m_page->initializeWebPage(openerInfo ? openerInfo->site : Site(aboutBlankURL()));
}

#endif // USE(WPE_BACKEND_PLAYSTATION)

PlayStationWebView::~PlayStationWebView()
{
}

void PlayStationWebView::setClient(std::unique_ptr<API::ViewClient>&& client)
{
    if (!client)
        m_client = makeUnique<API::ViewClient>();
    else
        m_client = WTFMove(client);
}

void PlayStationWebView::setViewSize(WebCore::IntSize viewSize)
{
    m_viewSize = viewSize;
}

void PlayStationWebView::setViewState(OptionSet<WebCore::ActivityState> flags)
{
    auto changedFlags = m_viewStateFlags ^ flags;
    m_viewStateFlags = flags;

    if (changedFlags)
        m_page->activityStateDidChange(changedFlags);
}

void PlayStationWebView::setViewNeedsDisplay(const WebCore::Region& region)
{
    if (m_client)
        m_client->setViewNeedsDisplay(*this, region);
}

#if ENABLE(FULLSCREEN_API)
void PlayStationWebView::willEnterFullScreen()
{
    m_isFullScreen = true;
    m_page->fullScreenManager()->willEnterFullScreen();
}

void PlayStationWebView::didEnterFullScreen()
{
    m_page->fullScreenManager()->didEnterFullScreen();
}

void PlayStationWebView::willExitFullScreen()
{
    m_page->fullScreenManager()->willExitFullScreen();
}

void PlayStationWebView::didExitFullScreen()
{
    m_page->fullScreenManager()->didExitFullScreen();
    m_isFullScreen = false;
}

void PlayStationWebView::requestExitFullScreen()
{
    if (isFullScreen())
        m_page->fullScreenManager()->requestExitFullScreen();
}

void PlayStationWebView::closeFullScreenManager()
{
    if (m_client && isFullScreen())
        m_client->closeFullScreen(*this);
    m_isFullScreen = false;
}

bool PlayStationWebView::isFullScreen()
{
    return m_isFullScreen;
}

void PlayStationWebView::enterFullScreen()
{
    if (m_client && !isFullScreen())
        m_client->enterFullScreen(*this);
}

void PlayStationWebView::exitFullScreen()
{
    if (m_client && isFullScreen())
        m_client->exitFullScreen(*this);
}

void PlayStationWebView::beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame)
{
    if (m_client)
        m_client->beganEnterFullScreen(*this, initialFrame, finalFrame);
}

void PlayStationWebView::beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame)
{
    if (m_client)
        m_client->beganExitFullScreen(*this, initialFrame, finalFrame);
}
#endif

void PlayStationWebView::setCursor(const WebCore::Cursor& cursor)
{
    if (m_client)
        m_client->setCursor(*this, cursor);
}

} // namespace WebKit
