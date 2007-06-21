/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInspectorClient.h"

#include "WebInspectorClient.h"
#include "WebKit.h"
#include "WebMutableURLRequest.h"
#include "WebNodeHighlight.h"
#include "WebView.h"

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/Element.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameView.h>
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>
#include <WebCore/RenderObject.h>
#pragma warning(pop)

#include <tchar.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

static LPCTSTR kWebInspectorWindowClassName = TEXT("WebInspectorWindowClass");
static ATOM registerWindowClass();
static LPCTSTR kWebInspectorPointerProp = TEXT("WebInspectorPointer");

static const unsigned defaultAttachedHeight = 300;

WebInspectorClient::WebInspectorClient(WebView* webView)
    : m_inspectedWebView(webView)
    , m_hwnd(0)
    , m_webViewHwnd(0)
    , m_originalWebViewWndProc(0)
    , m_attached(false)
{
    ASSERT(m_inspectedWebView);

    m_inspectedWebView->viewWindow((OLE_HANDLE*)&m_inspectedWebViewHwnd);

    // FIXME: Implement window size/position save/restore
#if 0
    [self setWindowFrameAutosaveName:@"Web Inspector"];
#endif
}

WebInspectorClient::~WebInspectorClient()
{
    if (m_hwnd)
        ::DestroyWindow(m_hwnd);
}

void WebInspectorClient::inspectorDestroyed()
{
    delete this;
}

Page* WebInspectorClient::createPage()
{
    if (m_webView)
        return core(m_webView.get());

    ASSERT(!m_hwnd);

    registerWindowClass();

    m_hwnd = ::CreateWindowEx(0, kWebInspectorWindowClassName, 0, WS_OVERLAPPEDWINDOW,
                              0, 0, 0, 0,
                              0, 0, 0, 0);
    if (!m_hwnd)
        return 0;

    ::SetProp(m_hwnd, kWebInspectorPointerProp, reinterpret_cast<HANDLE>(this));

    m_webView.adoptRef(WebView::createInstance());

    if (FAILED(m_webView->setHostWindow((OLE_HANDLE)(ULONG64)m_hwnd)))
        return 0;

    RECT rect = {0};
    if (FAILED(m_webView->initWithFrame(rect, 0, 0)))
        return 0;

    m_webView->setProhibitsMainFrameScrolling();

    if (FAILED(m_webView->viewWindow(reinterpret_cast<OLE_HANDLE*>(&m_webViewHwnd))))
        return 0;

    COMPtr<WebMutableURLRequest> request;
    request.adoptRef(WebMutableURLRequest::createInstance());

    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, ::CFBundleCopyResourceURL(::CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit")), CFSTR("inspector"), CFSTR("html"), CFSTR("inspector")));
    if (!htmlURLRef)
        return 0;

    CFStringRef urlStringRef = ::CFURLGetString(htmlURLRef.get());
    if (FAILED(request->initWithURL(BString(urlStringRef), WebURLRequestUseProtocolCachePolicy, 60)))
        return 0;

    if (FAILED(m_webView->topLevelFrame()->loadRequest(request.get())))
        return 0;

    return core(m_webView.get());
}

void WebInspectorClient::showWindow()
{
    if (!m_hwnd)
        return;

    updateWindowTitle();
    ::SetWindowPos(m_hwnd, HWND_TOP, 60, 200, 750, 650, SWP_SHOWWINDOW);
    m_inspectedWebView->page()->inspectorController()->setWindowVisible(true);
}

void WebInspectorClient::closeWindow()
{
    if (!m_webView)
        return;

    ::ShowWindow(m_hwnd, SW_HIDE);
    m_inspectedWebView->page()->inspectorController()->setWindowVisible(false);
}

bool WebInspectorClient::windowVisible()
{
    return !!::IsWindowVisible(m_hwnd);
}

void WebInspectorClient::attachWindow()
{
    ASSERT(m_hwnd);
    ASSERT(m_webView);
    ASSERT(!m_attached);
    ASSERT(m_inspectedWebViewHwnd);

    if (!m_originalWebViewWndProc) {
        ::SetProp(m_inspectedWebViewHwnd, kWebInspectorPointerProp, reinterpret_cast<HANDLE>(this));
#pragma warning(disable: 4244 4312)
        m_originalWebViewWndProc = (WNDPROC)::SetWindowLongPtr(m_inspectedWebViewHwnd, GWLP_WNDPROC, (LONG_PTR)SubclassedWebViewWndProc);
    }

    HWND hostWindow;
    if (FAILED(m_inspectedWebView->hostWindow((OLE_HANDLE*)&hostWindow)))
        return;

    m_webView->setHostWindow((OLE_HANDLE)(ULONG64)hostWindow);
    ::ShowWindow(m_hwnd, SW_HIDE);
    m_attached = true;

    ::SendMessage(hostWindow, WM_SIZE, 0, 0);

    if (m_highlight && m_highlight->visible())
        m_highlight->updateWindow();
}

void WebInspectorClient::detachWindow()
{
    ASSERT(m_attached);
    ASSERT(m_originalWebViewWndProc);

    ::SetWindowLongPtr(m_inspectedWebViewHwnd, GWLP_WNDPROC, (LONG_PTR)m_originalWebViewWndProc);
    ::RemoveProp(m_inspectedWebViewHwnd, kWebInspectorPointerProp);
    m_originalWebViewWndProc = 0;

    m_attached = false;

    m_webView->setHostWindow((OLE_HANDLE)(ULONG64)m_hwnd);
    ::ShowWindow(m_hwnd, SW_SHOW);
    ::SendMessage(m_hwnd, WM_SIZE, 0, 0);

    HWND hostWindow;
    if (SUCCEEDED(m_inspectedWebView->hostWindow((OLE_HANDLE*)&hostWindow)))
        ::SendMessage(hostWindow, WM_SIZE, 0, 0);

    if (m_highlight && m_highlight->visible())
        m_highlight->updateWindow();
}

void WebInspectorClient::highlight(Node* node)
{
    ASSERT_ARG(node, node);

    HWND hwnd;
    if (FAILED(m_inspectedWebView->viewWindow((OLE_HANDLE*)&hwnd)))
        return;
    RECT rect;
    ::GetClientRect(hwnd, &rect);
    IntRect webViewRect(rect);

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return;
    IntRect nodeRect(renderer->absoluteBoundingBoxRect());

    if (!webViewRect.contains(nodeRect) && !nodeRect.contains(webViewRect)) {
        Element* element;
        if (node->isElementNode())
            element = static_cast<Element*>(node);
        else
            element = static_cast<Element*>(node->parent());
        element->scrollIntoViewIfNeeded();
    }

    IntSize offset = m_inspectedWebView->page()->mainFrame()->view()->scrollOffset();
    nodeRect.move(-offset);

    if (!m_highlight)
        m_highlight.set(new WebNodeHighlight(hwnd));

    m_highlight->highlight(nodeRect);
}

void WebInspectorClient::hideHighlight()
{
    if (m_highlight)
        m_highlight->hide();
}

void WebInspectorClient::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void WebInspectorClient::updateWindowTitle()
{
    // FIXME: The series of appends should be replaced with a call to String::format()
    // when it can be figured out how to get the unicode em-dash to show up.
    String title = "Web Inspector ";
    title.append((UChar)0x2014); // em-dash
    title.append(' ');
    title.append(m_inspectedURL);
    ::SetWindowText(m_hwnd, title.charactersWithNullTermination());
}

LRESULT WebInspectorClient::onSize(WPARAM, LPARAM)
{
    RECT rect;
    ::GetClientRect(m_hwnd, &rect);

    ::SetWindowPos(m_webViewHwnd, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);

    return 0;
}

LRESULT WebInspectorClient::onClose(WPARAM, LPARAM)
{
    ::ShowWindow(m_hwnd, SW_HIDE);
    m_inspectedWebView->page()->inspectorController()->setWindowVisible(false);

    hideHighlight();

    return 0;
}

void WebInspectorClient::onWebViewWindowPosChanging(WPARAM, LPARAM lParam)
{
    ASSERT(m_attached);

    WINDOWPOS* windowPos = reinterpret_cast<WINDOWPOS*>(lParam);
    ASSERT_ARG(lParam, windowPos);

    if (windowPos->flags & SWP_NOSIZE)
        return;

    windowPos->cy -= defaultAttachedHeight;

    ::SetWindowPos(m_webViewHwnd, 0, windowPos->x, windowPos->y + windowPos->cy, windowPos->cx, defaultAttachedHeight, SWP_NOZORDER);
}

static LRESULT CALLBACK WebInspectorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebInspectorClient* client = reinterpret_cast<WebInspectorClient*>(::GetProp(hwnd, kWebInspectorPointerProp));
    if (!client)
        return ::DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            return client->onSize(wParam, lParam);
        case WM_CLOSE:
            return client->onClose(wParam, lParam);
        default:
            break;
    }

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SubclassedWebViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebInspectorClient* client = reinterpret_cast<WebInspectorClient*>(::GetProp(hwnd, kWebInspectorPointerProp));
    ASSERT(client);

    switch (msg) {
        case WM_WINDOWPOSCHANGING:
            client->onWebViewWindowPosChanging(wParam, lParam);
        default:
            break;
    }

    return ::CallWindowProc(client->m_originalWebViewWndProc, hwnd, msg, wParam, lParam);
}

static ATOM registerWindowClass()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = 0;
    wcex.lpfnWndProc    = WebInspectorWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = 0;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebInspectorWindowClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;

    return ::RegisterClassEx(&wcex);
}
