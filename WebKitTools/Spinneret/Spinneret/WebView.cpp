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

#include "WebView.h"
#include "Resource.h"

#include "FrameWin.h"
#include "FrameView.h"
#include "Page.h"
#include "render_frames.h"
#include "GraphicsContext.h"


using namespace WebCore;

namespace WebKit {

// FIXME: we need to hang a pointer off the HWND
WebView* gSharedWebViewHack = 0;

class WebView::WebViewPrivate {
public:
    WebViewPrivate() {}
    ~WebViewPrivate()
    {
        delete frame;
    }

    FrameWin* frame;
    HWND windowHandle;
};

const LPCWSTR kWebViewWindowClassName = L"WebViewWindowClass";

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

static ATOM registerWebViewWithInstance(HINSTANCE hInstance)
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW; // CS_DBLCLKS?
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebViewWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex);
}

// FIXME: This should eventually just use the DLL instance, I think.
WebView* WebView::createWebView(HINSTANCE hInstance, HWND parent)
{
   registerWebViewWithInstance(hInstance);

   HWND hWnd = CreateWindow(kWebViewWindowClassName, 0, WS_CHILD | WS_BORDER,
       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, parent, 0, hInstance, 0);
   
   if (!hWnd)
      return 0;

   gSharedWebViewHack = new WebView(hWnd);
   return gSharedWebViewHack;
}

WebView::WebView(HWND hWnd)
{
    d = new WebViewPrivate();
    d->windowHandle = hWnd;

    Page *page = new Page();
    d->frame = new FrameWin(page, 0);
    FrameView *frameView = new FrameView(d->frame);
    d->frame->setView(frameView);

    d->frame->begin();
    d->frame->write("<img src=\"data:image/gif;base64,R0lGODdhMAAwAPAAAAAAAP///ywAAAAAMAAwAAAC8IyPqcvt3wCcDkiLc7C0qwyGHhSWpjQu5yqmCYsapyuvUUlvONmOZtfzgFzByTB10QgxOR0TqBQejhRNzOfkVJ+5YiUqrXF5Y5lKh/DeuNcP5yLWGsEbtLiOSpa/TPg7JpJHxyendzWTBfX0cxOnKPjgBzi4diinWGdkF8kjdfnycQZXZeYGejmJlZeGl9i2icVqaNVailT6F5iJ90m6mvuTS4OK05M0vDk0Q4XUtwvKOzrcd3iq9uisF81M1OIcR7lEewwcLp7tuNNkM3uNna3F2JQFo97Vriy/Xl4/f1cf5VWzXyym7PHhhx4dbgYKAAA7\" alt=\"Larry\"><div style=\"border: 1px black\">foo</div><ul><li>foo<li>bar<li>baz</ul>");
    d->frame->end();
}

WebView::~WebView()
{
    delete d;
}

void WebView::drawRect(const PAINTSTRUCT& ps)
{
    GraphicsContext context(ps.hdc);
    d->frame->paint(&context, ps.rcPaint);
}

HWND WebView::windowHandle()
{
    return d->windowHandle;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    HDC hdc;

    switch (message)
    {
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_PAINT:
        PAINTSTRUCT ps;
        hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code here...
        gSharedWebViewHack->drawRect(ps);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        // Do nothing?
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


};
