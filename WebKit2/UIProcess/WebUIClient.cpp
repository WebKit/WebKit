/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebUIClient.h"

#include "ImmutableDictionary.h"
#include "NativeWebKeyboardEvent.h"
#include "WKAPICast.h"
#include "WebNumber.h"
#include "WebPageProxy.h"
#include <WebCore/FloatRect.h>
#include <WebCore/IntSize.h>
#include <string.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

WebUIClient::WebUIClient()
{
    initialize(0);
}

void WebUIClient::initialize(const WKPageUIClient* client)
{
    if (client && !client->version)
        m_pageUIClient = *client;
    else 
        memset(&m_pageUIClient, 0, sizeof(m_pageUIClient));
}

PassRefPtr<WebPageProxy> WebUIClient::createNewPage(WebPageProxy* page, const WindowFeatures& windowFeatures, WebEvent::Modifiers modifiers, WebMouseEvent::Button button)
{
    if (!m_pageUIClient.createNewPage)
        return 0;

    ImmutableDictionary::MapType map;
    if (windowFeatures.xSet)
        map.set("x", WebDouble::create(windowFeatures.x));
    if (windowFeatures.ySet)
        map.set("y", WebDouble::create(windowFeatures.y));
    if (windowFeatures.widthSet)
        map.set("width", WebDouble::create(windowFeatures.width));
    if (windowFeatures.heightSet)
        map.set("height", WebDouble::create(windowFeatures.height));
    map.set("menuBarVisible", WebBoolean::create(windowFeatures.menuBarVisible));
    map.set("statusBarVisible", WebBoolean::create(windowFeatures.statusBarVisible));
    map.set("toolBarVisible", WebBoolean::create(windowFeatures.toolBarVisible));
    map.set("scrollbarsVisible", WebBoolean::create(windowFeatures.scrollbarsVisible));
    map.set("resizable", WebBoolean::create(windowFeatures.resizable));
    map.set("fullscreen", WebBoolean::create(windowFeatures.fullscreen));
    map.set("dialog", WebBoolean::create(windowFeatures.dialog));
    RefPtr<ImmutableDictionary> featuresMap = ImmutableDictionary::adopt(map);

    return adoptRef(toImpl(m_pageUIClient.createNewPage(toAPI(page), toAPI(featuresMap.get()), toAPI(modifiers), toAPI(button), m_pageUIClient.clientInfo)));
} 

void WebUIClient::showPage(WebPageProxy* page)
{
    if (!m_pageUIClient.showPage)
        return;
    
    m_pageUIClient.showPage(toAPI(page), m_pageUIClient.clientInfo);
}

void WebUIClient::close(WebPageProxy* page)
{
    if (!m_pageUIClient.close)
        return;
    
    m_pageUIClient.close(toAPI(page), m_pageUIClient.clientInfo);
}

void WebUIClient::runJavaScriptAlert(WebPageProxy* page, const String& message, WebFrameProxy* frame)
{
    if (!m_pageUIClient.runJavaScriptAlert)
        return;
    
    m_pageUIClient.runJavaScriptAlert(toAPI(page), toAPI(message.impl()), toAPI(frame), m_pageUIClient.clientInfo);
}

bool WebUIClient::runJavaScriptConfirm(WebPageProxy* page, const String& message, WebFrameProxy* frame)
{
    if (!m_pageUIClient.runJavaScriptConfirm)
        return false;

    return m_pageUIClient.runJavaScriptConfirm(toAPI(page), toAPI(message.impl()), toAPI(frame), m_pageUIClient.clientInfo);
}

String WebUIClient::runJavaScriptPrompt(WebPageProxy* page, const String& message, const String& defaultValue, WebFrameProxy* frame)
{
    if (!m_pageUIClient.runJavaScriptPrompt)
        return String();

    WebString* string = toImpl(m_pageUIClient.runJavaScriptPrompt(toAPI(page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), m_pageUIClient.clientInfo));
    if (!string)
        return String();

    String result = string->string();
    string->deref();

    return result;
}

void WebUIClient::setStatusText(WebPageProxy* page, const String& text)
{
    if (!m_pageUIClient.setStatusText)
        return;

    m_pageUIClient.setStatusText(toAPI(page), toAPI(text.impl()), m_pageUIClient.clientInfo);
}

void WebUIClient::mouseDidMoveOverElement(WebPageProxy* page, WebEvent::Modifiers modifiers, APIObject* userData)
{
    if (!m_pageUIClient.mouseDidMoveOverElement)
        return;

    m_pageUIClient.mouseDidMoveOverElement(toAPI(page), toAPI(modifiers), toAPI(userData), m_pageUIClient.clientInfo);
}

void WebUIClient::didNotHandleKeyEvent(WebPageProxy* page, const NativeWebKeyboardEvent& event)
{
    if (!m_pageUIClient.didNotHandleKeyEvent)
        return;
    m_pageUIClient.didNotHandleKeyEvent(toAPI(page), event.nativeEvent(), m_pageUIClient.clientInfo);
}

bool WebUIClient::toolbarsAreVisible(WebPageProxy* page)
{
    if (!m_pageUIClient.toolbarsAreVisible)
        return true;
    return m_pageUIClient.toolbarsAreVisible(toAPI(page), m_pageUIClient.clientInfo);

}
void WebUIClient::setToolbarsAreVisible(WebPageProxy* page, bool visible)
{
    if (!m_pageUIClient.setToolbarsAreVisible)
        return;
    m_pageUIClient.setToolbarsAreVisible(toAPI(page), visible, m_pageUIClient.clientInfo);
}

bool WebUIClient::menuBarIsVisible(WebPageProxy* page)
{
    if (!m_pageUIClient.menuBarIsVisible)
        return true;
    return m_pageUIClient.menuBarIsVisible(toAPI(page), m_pageUIClient.clientInfo);
}

void WebUIClient::setMenuBarIsVisible(WebPageProxy* page, bool visible)
{
    if (!m_pageUIClient.setMenuBarIsVisible)
        return;
    m_pageUIClient.setMenuBarIsVisible(toAPI(page), visible, m_pageUIClient.clientInfo);
}

bool WebUIClient::statusBarIsVisible(WebPageProxy* page)
{
    if (!m_pageUIClient.statusBarIsVisible)
        return true;
    return m_pageUIClient.statusBarIsVisible(toAPI(page), m_pageUIClient.clientInfo);
}

void WebUIClient::setStatusBarIsVisible(WebPageProxy* page, bool visible)
{
    if (!m_pageUIClient.setStatusBarIsVisible)
        return;
    m_pageUIClient.setStatusBarIsVisible(toAPI(page), visible, m_pageUIClient.clientInfo);
}

bool WebUIClient::isResizable(WebPageProxy* page)
{
    if (!m_pageUIClient.isResizable)
        return true;
    return m_pageUIClient.isResizable(toAPI(page), m_pageUIClient.clientInfo);
}

void WebUIClient::setIsResizable(WebPageProxy* page, bool resizable)
{
    if (!m_pageUIClient.setIsResizable)
        return;
    m_pageUIClient.setIsResizable(toAPI(page), resizable, m_pageUIClient.clientInfo);
}

void WebUIClient::setWindowFrame(WebPageProxy* page, const FloatRect& frame)
{
    if (!m_pageUIClient.setWindowFrame)
        return;

    m_pageUIClient.setWindowFrame(toAPI(page), toAPI(frame), m_pageUIClient.clientInfo);
}

FloatRect WebUIClient::windowFrame(WebPageProxy* page)
{
    if (!m_pageUIClient.getWindowFrame)
        return FloatRect();

    return toImpl(m_pageUIClient.getWindowFrame(toAPI(page), m_pageUIClient.clientInfo));
}

bool WebUIClient::canRunBeforeUnloadConfirmPanel()
{
    return m_pageUIClient.runBeforeUnloadConfirmPanel;
}

bool WebUIClient::runBeforeUnloadConfirmPanel(WebPageProxy* page, const String& message, WebFrameProxy* frame)
{
    if (!m_pageUIClient.runBeforeUnloadConfirmPanel)
        return true;

    return m_pageUIClient.runBeforeUnloadConfirmPanel(toAPI(page), toAPI(message.impl()), toAPI(frame), m_pageUIClient.clientInfo);
}

void WebUIClient::didDraw(WebPageProxy* page)
{
    if (!m_pageUIClient.didDraw)
        return;

    m_pageUIClient.didDraw(toAPI(page), m_pageUIClient.clientInfo);
}

void WebUIClient::pageDidScroll(WebPageProxy* page)
{
    if (!m_pageUIClient.pageDidScroll)
        return;

    m_pageUIClient.pageDidScroll(toAPI(page), m_pageUIClient.clientInfo);
}

} // namespace WebKit
