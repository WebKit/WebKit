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

#include "WebContext.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"

namespace WebKit {

PassRefPtr<WebFullScreenManagerProxy> WebFullScreenManagerProxy::create(WebPageProxy* page)
{
    return adoptRef(new WebFullScreenManagerProxy(page));
}

WebFullScreenManagerProxy::WebFullScreenManagerProxy(WebPageProxy* page)
    : m_page(page)
    , m_webView(0)
{
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
}

void WebFullScreenManagerProxy::setWebView(PlatformWebView* webView)
{
    m_webView = webView;
}

void WebFullScreenManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebFullScreenManagerProxyMessage(connection, messageID, arguments);
}

void WebFullScreenManagerProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{
    didReceiveSyncWebFullScreenManagerProxyMessage(connection, messageID, arguments, reply);
}

void WebFullScreenManagerProxy::willEnterFullScreen()
{
    m_page->process()->send(Messages::WebFullScreenManager::WillEnterFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    m_page->process()->send(Messages::WebFullScreenManager::DidEnterFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    m_page->process()->send(Messages::WebFullScreenManager::WillExitFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::didExitFullScreen()
{
    m_page->process()->send(Messages::WebFullScreenManager::DidExitFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    m_page->process()->send(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating), m_page->pageID());
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    m_page->process()->send(Messages::WebFullScreenManager::RequestExitFullScreen(), m_page->pageID());
}

void WebFullScreenManagerProxy::supportsFullScreen(bool withKeyboard, bool& supports)
{
    supports = !withKeyboard;
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
