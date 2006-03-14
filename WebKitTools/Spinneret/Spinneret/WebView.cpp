/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "stdafx.h"
#include "config.h"

#include "WebFrame.h"

#include "WebView.h"
#include "Resource.h"
#include "FrameView.h"
#include "MouseEvent.h"
#include "IntRect.h"

using namespace WebCore;

namespace WebKit {

class WebView::WebViewPrivate {
public:
    WebViewPrivate() {}
    ~WebViewPrivate()
    {
        delete mainFrame;
    }

    WebFrame* mainFrame;
    HWND windowHandle;
};

const LPCWSTR kWebViewWindowClassName = L"WebViewWindowClass";

LRESULT CALLBACK WebViewWndProc(HWND, UINT, WPARAM, LPARAM);

static ATOM registerWebViewWithInstance(HINSTANCE hInstance)
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = WebViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 4; // 4 bytes for the WebView pointer
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebViewWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex);
}

// FIXME: This should eventually just use the DLL instance, I think.
WebView* WebView::createWebView(HINSTANCE hInstance, HWND parent)
{
    // Save away our instace handle for WebCore to use.
    Widget::instanceHandle = hInstance;

    registerWebViewWithInstance(hInstance);

    HWND hWnd = CreateWindow(kWebViewWindowClassName, 0, WS_CHILD | WS_HSCROLL | WS_VSCROLL,
       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, parent, 0, hInstance, 0);

    if (!hWnd)
        return 0;

    WebView* newWebView = new WebView(hWnd);
    SetWindowLongPtr(hWnd, 0, (LONG)newWebView);
    return newWebView;
}

WebView::WebView(HWND hWnd)
{
    d = new WebViewPrivate();
    d->windowHandle = hWnd;
    d->mainFrame = new WebFrame("dummy", this);
    d->mainFrame->loadHTMLString("<p style=\"background-color: #00FF00\">Testing</p><img src=\"http://webkit.opendarwin.org/images/icon-gold.png\" alt=\"Face\"><div style=\"border: solid blue\">div with blue border</div><ul><li>foo<li>bar<li>baz</ul>");
}

WebView::~WebView()
{
    delete d;
}

HWND WebView::windowHandle()
{
    return d->windowHandle;
}

WebFrame* WebView::mainFrame()
{
    return d->mainFrame;
}

void WebView::mouseMoved(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    MouseEvent mouseEvent(hWnd, wParam, lParam, 0);
    d->mainFrame->viewImpl()->viewportMouseMoveEvent(&mouseEvent);
}

void WebView::mouseDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    MouseEvent mouseEvent(hWnd, wParam, lParam, 1);
    d->mainFrame->viewImpl()->viewportMousePressEvent(&mouseEvent);
}

void WebView::mouseUp(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    MouseEvent mouseEvent(hWnd, wParam, lParam, 1);
    d->mainFrame->viewImpl()->viewportMouseReleaseEvent(&mouseEvent);
}

void WebView::mouseDoubleClick(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    MouseEvent mouseEvent(hWnd, wParam, lParam, 2);
    d->mainFrame->viewImpl()->viewportMouseReleaseEvent(&mouseEvent);
}

#define LINE_SCROLL_SIZE 30

static int calculateScrollDelta(WPARAM wParam, int oldPosition, int pageSize)
{
    switch (LOWORD(wParam)) {
        case SB_PAGEUP: 
            return -(pageSize - LINE_SCROLL_SIZE); 
         case SB_PAGEDOWN: 
            return (pageSize - LINE_SCROLL_SIZE); 
        case SB_LINEUP: 
            return -LINE_SCROLL_SIZE;
        case SB_LINEDOWN: 
            return LINE_SCROLL_SIZE;
        case SB_THUMBPOSITION: 
            return HIWORD(wParam) - oldPosition; 
    }
    return 0;
}

static int scrollMessageForKey(WPARAM keyCode)
{
    switch (keyCode) {
    case VK_UP:
        return SB_LINEUP;
    case VK_PRIOR: 
        return SB_PAGEUP;
    case VK_NEXT:
        return SB_PAGEDOWN;
    case VK_DOWN:
        return SB_LINEDOWN;
    case VK_HOME:
        return SB_TOP;
    case VK_END:
        return SB_BOTTOM;
    }
    return -1;
}

LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    WebView* webview = (WebView*)GetWindowLongPtr(hWnd, 0);
    switch (message)
    {
    case WM_PAINT:
        webview->mainFrame()->paint();
        break;
    case WM_DESTROY:
        // Do nothing?
        break;
    case WM_MOUSEMOVE:
        webview->mouseMoved(hWnd, wParam, lParam);
        break;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        webview->mouseDown(hWnd, wParam, lParam);
        break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        webview->mouseUp(hWnd, wParam, lParam);
        break;
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
        webview->mouseDoubleClick(hWnd, wParam, lParam);
        break;
    case WM_HSCROLL: {
        ScrollView* view = webview->mainFrame()->impl()->view();
        view->scrollBy(calculateScrollDelta(wParam, view->contentsX(), view->visibleWidth()), 0);
        break;
    }
    case WM_VSCROLL: {
        ScrollView* view = webview->mainFrame()->impl()->view();
        view->scrollBy(0, calculateScrollDelta(wParam, view->contentsY(), view->visibleHeight()));
        break;
    }
    case WM_KEYDOWN: {
        WORD wScrollNotify = scrollMessageForKey(wParam);
        if (wScrollNotify != -1)
            SendMessage(hWnd, WM_VSCROLL, MAKELONG(wScrollNotify, 0), 0L);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


};
