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

#include "PlatformWebView.h"

namespace TestWebKitAPI {

static const wchar_t* hostWindowClassName = L"org.WebKit.TestWebKitAPI.PlatformWebViewHostWindow";

static void registerWindowClass()
{
    static bool initialized;
    if (initialized)
        return;
    initialized = true;

    WNDCLASSEXW wndClass = {0};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = DefWindowProcW;
    wndClass.hCursor = LoadCursor(0, IDC_ARROW);
    wndClass.lpszClassName = hostWindowClassName;

    ::RegisterClassExW(&wndClass);
}

PlatformWebView::PlatformWebView(WKPageNamespaceRef namespaceRef)
{
    registerWindowClass();

    RECT viewRect = {0, 0, 800, 600};
    m_window = CreateWindowExW(0, hostWindowClassName, L"TestWebKitAPI", WS_OVERLAPPEDWINDOW, viewRect.left, viewRect.top, viewRect.right, viewRect.bottom, 0, 0, 0, 0);
    m_view = WKViewCreate(viewRect, namespaceRef, m_window);
}

PlatformWebView::~PlatformWebView()
{
    ::DestroyWindow(m_window);
    WKRelease(m_view);
}

WKPageRef PlatformWebView::page()
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::simulateSpacebarKeyPress()
{
    HWND window = WKViewGetWindow(m_view);

    // These offsets come from rom <http://msdn.microsoft.com/en-us/library/ms646280(VS.85).aspx>.
    static const size_t repeatCountBitOffset = 0;
    static const size_t scanCodeBitOffset = 16;
    static const size_t previousStateBitOffset = 30;
    static const size_t transitionStateBitOffset = 31;

    // These values match what happens when you press the spacebar in Notepad, as observed by Spy++.
    ::SendMessageW(window, WM_KEYDOWN, VK_SPACE, (1 << repeatCountBitOffset) | (39 << scanCodeBitOffset));
    ::SendMessageW(window, WM_CHAR, ' ', (1 << repeatCountBitOffset) | (39 << scanCodeBitOffset));
    ::SendMessageW(window, WM_KEYUP, VK_SPACE, (1 << repeatCountBitOffset) | (39 << scanCodeBitOffset) | (1 << previousStateBitOffset) | (1 << transitionStateBitOffset));
}

} // namespace TestWebKitAPI
