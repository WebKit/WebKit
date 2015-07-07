/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebFullScreenManagerProxy.h"

#if ENABLE(FULLSCREEN_API)

#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/IntRect.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebFullScreenManagerProxy> WebFullScreenManagerProxy::create(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
{
    return adoptRef(new WebFullScreenManagerProxy(page, client));
}

WebFullScreenManagerProxy::WebFullScreenManagerProxy(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
    : m_page(&page)
    , m_client(&client)
{
    m_page->process().addMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page->pageID(), *this);
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
}

void WebFullScreenManagerProxy::willEnterFullScreen()
{
    m_page->process().send(Messages::WebFullScreenManager::WillEnterFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    m_page->process().send(Messages::WebFullScreenManager::DidEnterFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    m_page->process().send(Messages::WebFullScreenManager::WillExitFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::didExitFullScreen()
{
    m_page->process().send(Messages::WebFullScreenManager::DidExitFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    m_page->process().send(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating), m_page->pageID());
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    m_page->process().send(Messages::WebFullScreenManager::RequestExitFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::supportsFullScreen(bool withKeyboard, bool& supports)
{
    supports = !withKeyboard;
}

void WebFullScreenManagerProxy::saveScrollPosition()
{
    m_page->process().send(Messages::WebFullScreenManager::SaveScrollPosition(), m_page->pageID());
}

void WebFullScreenManagerProxy::restoreScrollPosition()
{
    m_page->process().send(Messages::WebFullScreenManager::RestoreScrollPosition(), m_page->pageID());
}

void WebFullScreenManagerProxy::invalidate()
{
    m_page->process().removeMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page->pageID());

    if (!m_client)
        return;

    m_client->closeFullScreenManager();
    m_client = nullptr;
}

void WebFullScreenManagerProxy::close()
{
    if (!m_client)
        return;
    m_client->closeFullScreenManager();
}

bool WebFullScreenManagerProxy::isFullScreen()
{
    if (!m_client)
        return false;
    return m_client->isFullScreen();
}

void WebFullScreenManagerProxy::enterFullScreen()
{
    if (!m_client)
        return;
    m_client->enterFullScreen();
}

void WebFullScreenManagerProxy::exitFullScreen()
{
    if (!m_client)
        return;
    m_client->exitFullScreen();
}
    
void WebFullScreenManagerProxy::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    if (!m_client)
        return;
    m_client->beganEnterFullScreen(initialFrame, finalFrame);
}

void WebFullScreenManagerProxy::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    if (!m_client)
        return;
    m_client->beganExitFullScreen(initialFrame, finalFrame);
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
