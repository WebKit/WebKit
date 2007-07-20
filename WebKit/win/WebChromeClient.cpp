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
#include "WebChromeClient.h"

#include "WebElementPropertyBag.h"
#include "WebFrame.h"
#include "WebMutableURLRequest.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/NotImplemented.h>
#pragma warning(pop)

using namespace WebCore;

WebChromeClient::WebChromeClient(WebView* webView)
    : m_webView(webView)
{
}

void WebChromeClient::chromeDestroyed()
{
    delete this;
}

void WebChromeClient::setWindowRect(const FloatRect& r)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        RECT rect = IntRect(r);
        uiDelegate->setFrame(m_webView, &rect);
        uiDelegate->Release();
    }
}

FloatRect WebChromeClient::windowRect()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        RECT rect;
        HRESULT retval = uiDelegate->webViewFrame(m_webView, &rect);

        uiDelegate->Release();

        if (SUCCEEDED(retval))
            return rect;
    }

    return FloatRect();
}

FloatRect WebChromeClient::pageRect()
{
    RECT rect;
    m_webView->frameRect(&rect);
    return rect;
}

float WebChromeClient::scaleFactor()
{
    // Windows doesn't support UI scaling.
    return 1.0;
}

void WebChromeClient::focus()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewFocus(m_webView);
        uiDelegate->Release();
    }
}

void WebChromeClient::unfocus()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewUnfocus(m_webView);
        uiDelegate->Release();
    }
}

bool WebChromeClient::canTakeFocus(FocusDirection direction)
{
    IWebUIDelegate* uiDelegate = 0;
    BOOL bForward = (direction == FocusDirectionForward) ? TRUE : FALSE;
    BOOL result = FALSE;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->canTakeFocus(m_webView, bForward, &result);
        uiDelegate->Release();
    }

    return !!result;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    IWebUIDelegate* uiDelegate = 0;
    BOOL bForward = (direction == FocusDirectionForward) ? TRUE : FALSE;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->takeFocus(m_webView, bForward);
        uiDelegate->Release();
    }
}

Page* WebChromeClient::createWindow(Frame*, const FrameLoadRequest& frameLoadRequest)
{
    Page* page = 0;
    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance(frameLoadRequest.resourceRequest());

    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        IWebView* webView = 0;
        if (SUCCEEDED(uiDelegate->createWebViewWithRequest(m_webView, request, &webView))) {
            page = core(webView);
            webView->Release();
        }
    
        uiDelegate->Release();
    }

    request->Release();
    return page;
}

Page* WebChromeClient::createModalDialog(Frame*, const FrameLoadRequest&)
{
    Page* page = 0;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        notImplemented();
        uiDelegate->Release();
    }
    return page;
}

void WebChromeClient::show()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewShow(m_webView);
        uiDelegate->Release();
    }
}

bool WebChromeClient::canRunModal()
{
    bool result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        notImplemented();
        uiDelegate->Release();
    }
    return result;
}

void WebChromeClient::runModal()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        notImplemented();
        uiDelegate->Release();
    }
}

void WebChromeClient::setToolbarsVisible(bool visible)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setToolbarsVisible(m_webView, visible);
        uiDelegate->Release();
    }
}

bool WebChromeClient::toolbarsVisible()
{
    BOOL result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewAreToolbarsVisible(m_webView, &result);
        uiDelegate->Release();
    }
    return result != false;
}

void WebChromeClient::setStatusbarVisible(bool visible)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setStatusBarVisible(m_webView, visible);
        uiDelegate->Release();
    }
}

bool WebChromeClient::statusbarVisible()
{
    BOOL result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewIsStatusBarVisible(m_webView, &result);
        uiDelegate->Release();
    }
    return result != false;
}

void WebChromeClient::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::scrollbarsVisible()
{
    notImplemented();
    return false;
}

void WebChromeClient::setMenubarVisible(bool)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        notImplemented();
        uiDelegate->Release();
    }
}

bool WebChromeClient::menubarVisible()
{
    bool result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        notImplemented();
        uiDelegate->Release();
    }
    return result;
}

void WebChromeClient::setResizable(bool resizable)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setResizable(m_webView, resizable);
        uiDelegate->Release();
    }
}

void WebChromeClient::addMessageToConsole(const String& message, unsigned line, const String& url)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiPrivate;
        if (SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate)))
            uiPrivate->webViewAddMessageToConsole(m_webView, BString(message), line, BString(url), true);
    }
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        ui->Release();
        return true;
    }
    return false;
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    BOOL result = TRUE;
    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        WebFrame* webFrame = kit(frame);
        ui->runBeforeUnloadConfirmPanelWithMessage(m_webView, BString(message), webFrame, &result);
        ui->Release();
    }
    return !!result;
}

void WebChromeClient::closeWindowSoon()
{
    // We need to remove the parent WebView from WebViewSets here, before it actually
    // closes, to make sure that JavaScript code that executes before it closes
    // can't find it. Otherwise, window.open will select a closed WebView instead of 
    // opening a new one <rdar://problem/3572585>.

    // We also need to stop the load to prevent further parsing or JavaScript execution
    // after the window has torn down <rdar://problem/4161660>.
  
    // FIXME: This code assumes that the UI delegate will respond to a webViewClose
    // message by actually closing the WebView. Safari guarantees this behavior, but other apps might not.
    // This approach is an inherent limitation of not making a close execute immediately
    // after a call to window.close.

    m_webView->setGroupName(0);
    m_webView->stopLoading(0);
    m_webView->closeWindowSoon();
}

void WebChromeClient::runJavaScriptAlert(Frame*, const String& message)
{
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)))
        ui->runJavaScriptAlertPanelWithMessage(m_webView, BString(message));
}

bool WebChromeClient::runJavaScriptConfirm(Frame*, const String& message)
{
    BOOL result = FALSE;
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)))
        ui->runJavaScriptConfirmPanelWithMessage(m_webView, BString(message), &result);
    return !!result;
}

bool WebChromeClient::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    COMPtr<IWebUIDelegate> ui;
    if (FAILED(m_webView->uiDelegate(&ui)))
        return false;

    BSTR resultBSTR = 0;
    if (FAILED(ui->runJavaScriptTextInputPanelWithPrompt(m_webView, BString(message), BString(defaultValue), &resultBSTR)))
        return false;

    if (resultBSTR) {
        result = String(resultBSTR, SysStringLen(resultBSTR));
        SysFreeString(resultBSTR);
        return true;
    }

    return false;
}

void WebChromeClient::setStatusbarText(const String& statusText)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setStatusText(m_webView, BString(statusText));
    }
}

bool WebChromeClient::shouldInterruptJavaScript()
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiPrivate;
        if (SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            BOOL result;
            if (SUCCEEDED(uiPrivate->webViewShouldInterruptJavaScript(m_webView, &result)))
                return !!result;
        }
    }
    return false;
}

bool WebChromeClient::tabsToLinks() const
{
    BOOL enabled = FALSE;
    IWebPreferences* preferences;
    if (SUCCEEDED(m_webView->preferences(&preferences)))
        preferences->tabsToLinks(&enabled);

    return !!enabled;
}

IntRect WebChromeClient::windowResizerRect() const
{
    IntRect intRect;

    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        IWebUIDelegatePrivate* uiPrivate;
        if (SUCCEEDED(ui->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            RECT r;
            if (SUCCEEDED(uiPrivate->webViewResizerRect(m_webView, &r)))
                intRect = IntRect(r.left, r.top, r.right-r.left, r.bottom-r.top);
            uiPrivate->Release();
        }
        ui->Release();
    }
    return intRect;
}

void WebChromeClient::addToDirtyRegion(const IntRect& dirtyRect)
{
    m_webView->addToDirtyRegion(dirtyRect);
}

void WebChromeClient::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(core(m_webView->topLevelFrame()));

    m_webView->scrollBackingStore(core(m_webView->topLevelFrame())->view(), dx, dy, scrollViewRect, clipRect);
}

void WebChromeClient::updateBackingStore()
{
    ASSERT(core(m_webView->topLevelFrame()));

    m_webView->updateBackingStore(core(m_webView->topLevelFrame())->view(), 0, false);
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    COMPtr<WebElementPropertyBag> element;
    element.adoptRef(WebElementPropertyBag::createInstance(result));

    uiDelegate->mouseDidMoveOverElement(m_webView, element.get(), modifierFlags);
}

void WebChromeClient::setToolTip(const String& toolTip)
{
    m_webView->setToolTip(toolTip);
}

void WebChromeClient::print(Frame* frame)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    COMPtr<IWebUIDelegate2> uiDelegate2;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)))
        if (SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegate2, (void**)&uiDelegate2)))
            uiDelegate2->printFrame(m_webView, kit(frame));
}
