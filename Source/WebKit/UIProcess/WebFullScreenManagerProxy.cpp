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

#include "APIFullscreenClient.h"
#include "WebAutomationSession.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/IntRect.h>

namespace WebKit {
using namespace WebCore;

WebFullScreenManagerProxy::WebFullScreenManagerProxy(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
    : m_page(page)
    , m_client(client)
{
    m_page.process().addMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page.pageID(), *this);
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
    m_page.process().removeMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page.pageID());
    m_client.closeFullScreenManager();
}

void WebFullScreenManagerProxy::willEnterFullScreen()
{
    m_page.fullscreenClient().willEnterFullscreen(&m_page);
    m_page.process().send(Messages::WebFullScreenManager::WillEnterFullScreen(), m_page.pageID());
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    m_page.fullscreenClient().didEnterFullscreen(&m_page);
    m_page.process().send(Messages::WebFullScreenManager::DidEnterFullScreen(), m_page.pageID());

    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession())
            automationSession->didEnterFullScreenForPage(m_page);
    }
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    m_page.fullscreenClient().willExitFullscreen(&m_page);
    m_page.process().send(Messages::WebFullScreenManager::WillExitFullScreen(), m_page.pageID());
}

void WebFullScreenManagerProxy::didExitFullScreen()
{
    m_page.fullscreenClient().didExitFullscreen(&m_page);
    m_page.process().send(Messages::WebFullScreenManager::DidExitFullScreen(), m_page.pageID());
    
    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession())
            automationSession->didExitFullScreenForPage(m_page);
    }
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    m_page.process().send(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating), m_page.pageID());
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    m_page.process().send(Messages::WebFullScreenManager::RequestExitFullScreen(), m_page.pageID());
}

void WebFullScreenManagerProxy::supportsFullScreen(bool withKeyboard, CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(IOS_FAMILY)
    completionHandler(!withKeyboard);
#else
    completionHandler(true);
#endif
}

void WebFullScreenManagerProxy::saveScrollPosition()
{
    m_page.process().send(Messages::WebFullScreenManager::SaveScrollPosition(), m_page.pageID());
}

void WebFullScreenManagerProxy::restoreScrollPosition()
{
    m_page.process().send(Messages::WebFullScreenManager::RestoreScrollPosition(), m_page.pageID());
}

void WebFullScreenManagerProxy::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page.process().send(Messages::WebFullScreenManager::SetFullscreenInsets(insets), m_page.pageID());
}

void WebFullScreenManagerProxy::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page.process().send(Messages::WebFullScreenManager::SetFullscreenAutoHideDuration(duration), m_page.pageID());
}

void WebFullScreenManagerProxy::setFullscreenControlsHidden(bool hidden)
{
    m_page.process().send(Messages::WebFullScreenManager::SetFullscreenControlsHidden(hidden), m_page.pageID());
}

void WebFullScreenManagerProxy::close()
{
    m_client.closeFullScreenManager();
}

bool WebFullScreenManagerProxy::isFullScreen()
{
    return m_client.isFullScreen();
}

void WebFullScreenManagerProxy::enterFullScreen()
{
    m_client.enterFullScreen();
}

void WebFullScreenManagerProxy::exitFullScreen()
{
    m_client.exitFullScreen();
}
    
void WebFullScreenManagerProxy::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_client.beganEnterFullScreen(initialFrame, finalFrame);
}

void WebFullScreenManagerProxy::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_client.beganExitFullScreen(initialFrame, finalFrame);
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
