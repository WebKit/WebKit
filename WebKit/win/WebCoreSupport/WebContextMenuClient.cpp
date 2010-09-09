/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebContextMenuClient.h"

#include "WebElementPropertyBag.h"
#include "WebLocalizableStrings.h"
#include "WebView.h"

#include <WebCore/ContextMenu.h>
#include <WebCore/Event.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/NotImplemented.h>

#include <tchar.h>

using namespace WebCore;

WebContextMenuClient::WebContextMenuClient(WebView* webView)
    : m_webView(webView)
{
}

void WebContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

static bool isPreInspectElementTagSafari(IWebUIDelegate* uiDelegate)
{
    if (!uiDelegate)
        return false;

    TCHAR modulePath[MAX_PATH];
    DWORD length = ::GetModuleFileName(0, modulePath, _countof(modulePath));
    if (!length)
        return false;

    return String(modulePath, length).endsWith("Safari.exe", false);
}

static HMENU fixMenuReceivedFromOldSafari(IWebUIDelegate* uiDelegate, ContextMenu* originalMenu, HMENU menuFromClient)
{
    ASSERT_ARG(originalMenu, originalMenu);
    if (!isPreInspectElementTagSafari(uiDelegate))
        return menuFromClient;

    int count = ::GetMenuItemCount(originalMenu->platformDescription());
    if (count < 1)
        return menuFromClient;

    if (::GetMenuItemID(originalMenu->platformDescription(), count - 1) != WebMenuItemTagInspectElement)
        return menuFromClient;

    count = ::GetMenuItemCount(menuFromClient);
    if (count < 1)
        return menuFromClient;

    if (::GetMenuItemID(menuFromClient, count - 1) == WebMenuItemTagInspectElement)
        return menuFromClient;

    originalMenu->setPlatformDescription(menuFromClient);
    originalMenu->addInspectElementItem();
    return originalMenu->platformDescription();
}

HMENU WebContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return menu->platformDescription();

    ASSERT(uiDelegate);

    HMENU newMenu = 0;
    COMPtr<WebElementPropertyBag> propertyBag;
    propertyBag.adoptRef(WebElementPropertyBag::createInstance(menu->hitTestResult()));
    // FIXME: We need to decide whether to do the default before calling this delegate method
    if (FAILED(uiDelegate->contextMenuItemsForElement(m_webView, propertyBag.get(), (OLE_HANDLE)(ULONG64)menu->platformDescription(), (OLE_HANDLE*)&newMenu)))
        return menu->platformDescription();
    return fixMenuReceivedFromOldSafari(uiDelegate.get(), menu, newMenu);
}

void WebContextMenuClient::contextMenuItemSelected(ContextMenuItem* item, const ContextMenu* parentMenu)
{
    ASSERT(item->type() == ActionType || item->type() == CheckableActionType);

    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    ASSERT(uiDelegate);

    COMPtr<WebElementPropertyBag> propertyBag;
    propertyBag.adoptRef(WebElementPropertyBag::createInstance(parentMenu->hitTestResult()));
            
    uiDelegate->contextMenuItemSelected(m_webView, item->releasePlatformDescription(), propertyBag.get());
}

void WebContextMenuClient::downloadURL(const KURL& url)
{
    m_webView->downloadURL(url);
}

void WebContextMenuClient::searchWithGoogle(const Frame* frame)
{
    String searchString = frame->editor()->selectedText();
    searchString.stripWhiteSpace();
    String encoded = encodeWithURLEscapeSequences(searchString);
    encoded.replace("%20", "+");
    
    String url("http://www.google.com/search?q=");
    url.append(encoded);
    url.append("&ie=UTF-8&oe=UTF-8");

    if (Page* page = frame->page())
        page->mainFrame()->loader()->urlSelected(KURL(ParsedURLString, url), String(), 0, false, false, true, SendReferrer);
}

void WebContextMenuClient::lookUpInDictionary(Frame*)
{
    notImplemented();
}

void WebContextMenuClient::speak(const String&)
{
    notImplemented();
}

void WebContextMenuClient::stopSpeaking()
{
    notImplemented();
}

bool WebContextMenuClient::isSpeaking()
{
    notImplemented();
    return false;
}
