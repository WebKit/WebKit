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
#include "WebFullScreenManager.h"

#if ENABLE(FULLSCREEN_API)

#include "Connection.h"
#include "MessageID.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Page.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return adoptRef(new WebFullScreenManager(page));
}

WebFullScreenManager::WebFullScreenManager(WebPage* page)
    : m_page(page)
{
}

void WebFullScreenManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebFullScreenManagerMessage(connection, messageID, arguments);
}

bool WebFullScreenManager::supportsFullScreen()
{
    if (!m_page->corePage()->settings()->fullScreenEnabled())
        return false;

    bool supports = true;
    WebProcess::shared().connection()->sendSync(Messages::WebFullScreenManagerProxy::SupportsFullScreen(), supports, 0);
    return supports;
}

void WebFullScreenManager::enterFullScreenForElement(WebCore::Element* element)
{
    ASSERT(element);
    m_element = element;
    WebProcess::shared().connection()->send(Messages::WebFullScreenManagerProxy::EnterFullScreen(), 0);
}

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element)
{
    ASSERT(element);
    ASSERT(m_element == element);
    WebProcess::shared().connection()->send(Messages::WebFullScreenManagerProxy::ExitFullScreen(), 0);
}

void WebFullScreenManager::willEnterFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitWillEnterFullScreenForElement(m_element.get());
}

void WebFullScreenManager::didEnterFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitDidEnterFullScreenForElement(m_element.get());
}

void WebFullScreenManager::willExitFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitWillExitFullScreenForElement(m_element.get());
}

void WebFullScreenManager::didExitFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitDidExitFullScreenForElement(m_element.get());
}


} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
