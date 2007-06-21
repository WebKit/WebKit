/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebNodeHighlight.h"

#pragma warning(push, 0)
#include <WebCore/Color.h>
#include <WebCore/GraphicsContext.h>
#pragma warning(pop)

using namespace WebCore;

static LPCTSTR kOverlayWindowClassName = TEXT("WebNodeHighlightWindowClass");
static ATOM registerOverlayClass();
static LPCTSTR kWebNodeHighlightPointerProp = TEXT("WebNodeHighlightPointer");

WebNodeHighlight::WebNodeHighlight(HWND webView)
    : m_webView(webView)
    , m_overlay(0)
{
}

WebNodeHighlight::~WebNodeHighlight()
{
    if (m_overlay) {
        ::RemoveProp(m_overlay, kWebNodeHighlightPointerProp);
        ::DestroyWindow(m_overlay);
    }

    removeSubclass();
}

void WebNodeHighlight::highlight(const IntRect& rect)
{
    if (!m_overlay) {
        registerOverlayClass();

        m_overlay = ::CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, kOverlayWindowClassName, 0, WS_POPUP | WS_VISIBLE,
                                     0, 0, 0, 0,
                                     m_webView, 0, 0, 0);
        if (!m_overlay)
            return;

        ::SetProp(m_overlay, kWebNodeHighlightPointerProp, reinterpret_cast<HANDLE>(this));
        ::SetWindowPos(m_overlay, m_webView, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        m_subclassedWindow = ::GetAncestor(m_webView, GA_ROOT);
        ::SetProp(m_subclassedWindow, kWebNodeHighlightPointerProp, reinterpret_cast<HANDLE>(this));
#pragma warning(disable: 4244 4312)
        m_originalWndProc = (WNDPROC)::SetWindowLongPtr(m_subclassedWindow, GWLP_WNDPROC, (LONG_PTR)SubclassedWndProc);
    }

    m_rect = rect;
    updateWindow();
    ::ShowWindow(m_overlay, SW_SHOW);
}

void WebNodeHighlight::hide()
{
    if (m_overlay)
        ::ShowWindow(m_overlay, SW_HIDE);
}

bool WebNodeHighlight::visible() const
{
    return m_overlay && ::IsWindowVisible(m_overlay);
}

void WebNodeHighlight::updateWindow()
{
    ASSERT(m_overlay);

    HDC hdc = ::CreateCompatibleDC(::GetDC(m_overlay));
    if (!hdc)
        return;

    RECT webViewRect;
    ::GetWindowRect(m_webView, &webViewRect);

    SIZE size;
    size.cx = webViewRect.right - webViewRect.left;
    size.cy = webViewRect.bottom - webViewRect.top;

    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth         = size.cx;
    bitmapInfo.bmiHeader.biHeight        = -size.cy;
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = 32;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    void* pixels = 0;
    HBITMAP hbmp = ::CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
    if (!hbmp) {
        DWORD error = ::GetLastError();
        error++;
        return;
    }

    ::SelectObject(hdc, hbmp);

    GraphicsContext context(hdc);

    context.clipOut(m_rect);

    FloatRect overlayRect(webViewRect);
    overlayRect.setLocation(FloatPoint(0, 0));
    context.fillRect(overlayRect, Color(0, 0, 0, 128));

    IntRect outlineRect(m_rect);
    outlineRect.inflate(1);
    context.fillRect(outlineRect, Color::white);

    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    POINT srcPoint;
    srcPoint.x = 0;
    srcPoint.y = 0;

    POINT dstPoint;
    dstPoint.x = webViewRect.left;
    dstPoint.y = webViewRect.top;

    ::UpdateLayeredWindow(m_overlay, ::GetDC(0), &dstPoint, &size, hdc, &srcPoint, 0, &bf, ULW_ALPHA);

    ::DeleteObject(hbmp);
    ::DeleteDC(hdc);
}

void WebNodeHighlight::removeSubclass()
{
    if (!m_subclassedWindow)
        return;

    ::RemoveProp(m_subclassedWindow, kWebNodeHighlightPointerProp);
#pragma warning(disable: 4244 4312)
    ::SetWindowLongPtr(m_subclassedWindow, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);

    m_subclassedWindow = 0;
    m_originalWndProc = 0;
}

static ATOM registerOverlayClass()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = 0;
    wcex.lpfnWndProc    = OverlayWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = 0;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kOverlayWindowClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;

    return ::RegisterClassEx(&wcex);
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebNodeHighlight* highlight = reinterpret_cast<WebNodeHighlight*>(::GetProp(hwnd, kWebNodeHighlightPointerProp));
    if (!highlight)
        return ::DefWindowProc(hwnd, msg, wParam, lParam);

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SubclassedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebNodeHighlight* highlight = reinterpret_cast<WebNodeHighlight*>(::GetProp(hwnd, kWebNodeHighlightPointerProp));
    ASSERT(highlight);

    switch (msg) {
        case WM_WINDOWPOSCHANGED:
            if (highlight->visible())
                highlight->updateWindow();
            break;
        case WM_DESTROY:
            highlight->removeSubclass();
            break;
        default:
            break;
    }

    return ::CallWindowProc(highlight->m_originalWndProc, hwnd, msg, wParam, lParam);
}
