/*
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "PlatformWebView.h"

#include <WebCore/HWndDC.h>
#include <cairo.h>
#include <cstdio>
#include <windows.h>
#include <wtf/RunLoop.h>
#include <wtf/win/GDIObject.h>


namespace WTR {

static LPCWSTR hostWindowClassName = L"WTRWebViewHostWindow";
static LPCWSTR testRunnerWindowName = L"WebKitTestRunner";

static void registerWindowClass()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    WNDCLASSEXW wndClass { };
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = DefWindowProc;
    wndClass.hCursor = LoadCursor(0, IDC_ARROW);
    wndClass.hInstance = GetModuleHandle(0);
    wndClass.lpszClassName = hostWindowClassName;

    ::RegisterClassExW(&wndClass);
}

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration, const TestOptions& options)
    : m_window(nullptr)
    , m_windowIsKey(true)
    , m_options(options)
{
    registerWindowClass();

    m_window = ::CreateWindowEx(
        WS_EX_TOOLWINDOW,
        hostWindowClassName,
        testRunnerWindowName,
        WS_POPUP,
        CW_USEDEFAULT,
        0,
        0,
        0,
        0,
        0,
        GetModuleHandle(0),
        0);
    RECT viewRect = { };
    m_view = WKViewCreate(viewRect, configuration, m_window);
    WKViewSetIsInWindow(m_view, true);

    ShowWindow(m_window, SW_SHOW);
}

PlatformWebView::~PlatformWebView()
{
    if (::IsWindow(m_window))
        ::DestroyWindow(m_window);
}

void PlatformWebView::resizeTo(unsigned width, unsigned height, WebViewSizingMode)
{
    WKRect frame = { };
    frame.size.width = width;
    frame.size.height = height;
    setWindowFrame(frame);
}

WKPageRef PlatformWebView::page()
{
    return WKViewGetPage(m_view);
}

void PlatformWebView::focus()
{
    ::SetFocus(::WKViewGetWindow(m_view));
}

WKRect PlatformWebView::windowFrame()
{
    WKRect wkFrame { };
    RECT r;

    if (::GetWindowRect(m_window, &r)) {
        wkFrame.origin.x = r.left;
        wkFrame.origin.y = r.top;
        wkFrame.size.width = r.right - r.left;
        wkFrame.size.height = r.bottom - r.top;
    }

    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect frame, WebViewSizingMode)
{
    ::SetWindowPos(
        WKViewGetWindow(m_view),
        0,
        0,
        0,
        frame.size.width,
        frame.size.height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);

    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    if (m_options.shouldShowWebView)
        flags |= SWP_NOMOVE;
    ::SetWindowPos(
        m_window,
        0,
        -frame.size.width,
        -frame.size.height,
        frame.size.width,
        frame.size.height,
        flags);
}

void PlatformWebView::didInitializeClients()
{
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::addToWindow()
{
}

void PlatformWebView::removeFromWindow()
{
}

void PlatformWebView::setWindowIsKey(bool windowIsKey)
{
    m_windowIsKey = windowIsKey;
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

#if USE(CAIRO)
static cairo_surface_t* generateCairoSurfaceFromBitmap(BITMAP bitmapTag)
{
    cairo_surface_t* image = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32,
        bitmapTag.bmWidth,
        bitmapTag.bmHeight);

    unsigned char* src = (unsigned char*)bitmapTag.bmBits;
    int srcPitch = bitmapTag.bmWidthBytes;
    unsigned char* dst = cairo_image_surface_get_data(image);
    int dstPitch = cairo_image_surface_get_stride(image);

    for (int y = 0; y < bitmapTag.bmHeight; y++) {
        memcpy(dst, src, dstPitch);
        src += srcPitch;
        dst += dstPitch;
    }

    return image;
}

cairo_surface_t* PlatformWebView::windowSnapshotImage()
{
    RECT windowRect;
    ::GetClientRect(m_window, &windowRect);
    LONG width = windowRect.right - windowRect.left;
    LONG height = windowRect.bottom - windowRect.top;

    WebCore::HWndDC windowDC(m_window);
    auto memoryDC = adoptGDIObject(::CreateCompatibleDC(windowDC));

    BITMAPINFO bitmapInfo { };
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage = width * height * 4;

    void* bitsPointer = 0;
    auto bitmap = adoptGDIObject(::CreateDIBSection(windowDC, &bitmapInfo, DIB_RGB_COLORS, &bitsPointer, 0, 0));
    if (!bitmap)
        return nullptr;

    ::SelectObject(memoryDC.get(), bitmap.get());
    ::SendMessage(m_window, WM_PRINT, reinterpret_cast<WPARAM>(memoryDC.get()), PRF_CLIENT | PRF_CHILDREN | PRF_OWNED);

    BITMAP bitmapTag { };
    GetObject(bitmap.get(), sizeof(bitmapTag), &bitmapTag);
    ASSERT(bitmapTag.bmBitsPixel == 32);

    return generateCairoSurfaceFromBitmap(bitmapTag);
}
#endif

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
}

void PlatformWebView::setNavigationGesturesEnabled(bool)
{
}

void PlatformWebView::forceWindowFramesChanged()
{
}

bool PlatformWebView::drawsBackground() const
{
    return false;
}

void PlatformWebView::setDrawsBackground(bool)
{
}

void PlatformWebView::setEditable(bool)
{
}

} // namespace WTR
