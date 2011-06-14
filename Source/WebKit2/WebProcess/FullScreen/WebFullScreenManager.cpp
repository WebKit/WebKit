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
#include "WebCoreArgumentCoders.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Color.h>
#include <WebCore/Element.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

WebFullScreenManager::WebFullScreenManager(WebPage* page)
    : m_page(page)
{
}
    
WebFullScreenManager::~WebFullScreenManager()
{
    
}

WebCore::Element* WebFullScreenManager::element() 
{ 
    return m_element.get(); 
}

void WebFullScreenManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebFullScreenManagerMessage(connection, messageID, arguments);
}

bool WebFullScreenManager::supportsFullScreen(bool withKeyboard)
{
    if (!m_page->corePage()->settings()->fullScreenEnabled())
        return false;

    return m_page->injectedBundleFullScreenClient().supportsFullScreen(m_page.get(), withKeyboard);

}

void WebFullScreenManager::enterFullScreenForElement(WebCore::Element* element)
{
    ASSERT(element);
    m_element = element;
    m_initialFrame = m_element->screenRect();
    m_page->injectedBundleFullScreenClient().enterFullScreenForElement(m_page.get(), element);
}

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element)
{
    ASSERT(element);
    ASSERT(m_element == element);
    m_page->injectedBundleFullScreenClient().exitFullScreenForElement(m_page.get(), element);
}

void WebFullScreenManager::beganEnterFullScreenAnimation()
{
    m_page->send(Messages::WebFullScreenManagerProxy::BeganEnterFullScreenAnimation());
}

void WebFullScreenManager::finishedEnterFullScreenAnimation(bool completed)
{
    m_page->send(Messages::WebFullScreenManagerProxy::FinishedEnterFullScreenAnimation(completed));
}

void WebFullScreenManager::beganExitFullScreenAnimation()
{
    m_page->send(Messages::WebFullScreenManagerProxy::BeganExitFullScreenAnimation());
}

void WebFullScreenManager::finishedExitFullScreenAnimation(bool completed)
{
    m_page->send(Messages::WebFullScreenManagerProxy::FinishedExitFullScreenAnimation(completed));
}
    
IntRect WebFullScreenManager::getFullScreenRect()
{
    IntRect rect;
    m_page->sendSync(Messages::WebFullScreenManagerProxy::GetFullScreenRect(), Messages::WebFullScreenManagerProxy::GetFullScreenRect::Reply(rect));
    return rect;
}

void WebFullScreenManager::willEnterFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitWillEnterFullScreenForElement(m_element.get());
    m_element->document()->setFullScreenRendererBackgroundColor(Color::transparent);
}

void WebFullScreenManager::didEnterFullScreen()
{
    ASSERT(m_element);
    m_element->document()->setFullScreenRendererBackgroundColor(Color::black);
    m_element->document()->webkitDidEnterFullScreenForElement(m_element.get());
}

void WebFullScreenManager::willExitFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitWillExitFullScreenForElement(m_element.get());
    m_element->document()->setFullScreenRendererBackgroundColor(Color::transparent);
}

void WebFullScreenManager::didExitFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitDidExitFullScreenForElement(m_element.get());
    m_element->document()->setFullScreenRendererBackgroundColor(Color::black);
}


} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
