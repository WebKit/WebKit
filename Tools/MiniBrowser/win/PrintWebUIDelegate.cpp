/*
 * Copyright (C) 2009, 2013-2015 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Brent Fulgham. All Rights Reserved.
 * Copyright (C) 2013 Alex Christensen. All Rights Reserved.
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
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "stdafx.h"
#include "PrintWebUIDelegate.h"

#include "Common.h"
#include "MainWindow.h"
#include "WebKitLegacyBrowserWindow.h"
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <comip.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wininet.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

static const int MARGIN = 20;

HRESULT PrintWebUIDelegate::runJavaScriptAlertPanelWithMessage(_In_opt_ IWebView*, _In_ BSTR message)
{
    ::MessageBoxW(0, message, L"JavaScript Alert", MB_OK);
    return S_OK;
}

HRESULT PrintWebUIDelegate::runJavaScriptConfirmPanelWithMessage(_In_opt_ IWebView*, _In_ BSTR message, _Out_ BOOL* result)
{
    *result = ::MessageBoxW(0, message, L"JavaScript Confirm", MB_OKCANCEL) == IDOK;
    return S_OK;
}

HRESULT PrintWebUIDelegate::createWebViewWithRequest(_In_opt_ IWebView*, _In_opt_ IWebURLRequest* request, _COM_Outptr_opt_ IWebView** newWebView)
{
    if (!newWebView)
        return E_POINTER;
    *newWebView = nullptr;
    if (!request)
        return E_POINTER;

    auto& newWindow = MainWindow::create().leakRef();
    bool ok = newWindow.init(WebKitLegacyBrowserWindow::create, hInst);
    if (!ok)
        return E_FAIL;
    ShowWindow(newWindow.hwnd(), SW_SHOW);

    auto& newBrowserWindow = *static_cast<WebKitLegacyBrowserWindow*>(newWindow.browserWindow());
    *newWebView = newBrowserWindow.webView();
    IWebFramePtr frame;
    HRESULT hr;
    hr = (*newWebView)->mainFrame(&frame.GetInterfacePtr());
    if (FAILED(hr))
        return hr;
    hr = frame->loadRequest(request);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

static HWND getHandleFromWebView(IWebView* webView)
{
    COMPtr<IWebViewPrivate2> webViewPrivate;
    HRESULT hr = webView->QueryInterface(&webViewPrivate);
    if (FAILED(hr))
        return nullptr;

    HWND webViewWindow = nullptr;
    hr = webViewPrivate->viewWindow(&webViewWindow);
    if (FAILED(hr))
        return nullptr;

    return webViewWindow;
}

HRESULT PrintWebUIDelegate::webViewClose(_In_opt_ IWebView* webView)
{
    HWND hostWindow;
    HRESULT hr = webView->hostWindow(&hostWindow);
    if (FAILED(hr))
        return hr;

    ::DestroyWindow(hostWindow);

    if (hostWindow == m_modalDialogParent)
        m_modalDialogParent = nullptr;

    return S_OK;
}

HRESULT PrintWebUIDelegate::setFrame(_In_opt_ IWebView* webView, _In_ RECT* rect)
{
    if (m_modalDialogParent)
        ::MoveWindow(m_modalDialogParent, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, FALSE);

    ::MoveWindow(getHandleFromWebView(webView), 0, 0, rect->right - rect->left, rect->bottom - rect->top, FALSE);

    return S_OK;
}

HRESULT PrintWebUIDelegate::webViewFrame(_In_opt_ IWebView* webView, _Out_ RECT* rect)
{
    if (!::GetWindowRect(getHandleFromWebView(webView), rect))
        return E_FAIL;

    return S_OK;
}

HRESULT PrintWebUIDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualIID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG PrintWebUIDelegate::AddRef()
{
    return m_client.AddRef();
}

ULONG PrintWebUIDelegate::Release()
{
    return m_client.Release();
}

HRESULT PrintWebUIDelegate::webViewPrintingMarginRect(_In_opt_ IWebView* view, _Out_ RECT* rect)
{
    if (!view || !rect)
        return E_POINTER;

    IWebFramePtr mainFrame;
    if (FAILED(view->mainFrame(&mainFrame.GetInterfacePtr())))
        return E_FAIL;

    IWebFramePrivatePtr privateFrame;
    if (FAILED(mainFrame->QueryInterface(&privateFrame.GetInterfacePtr())))
        return E_FAIL;

    privateFrame->frameBounds(rect);

    rect->left += MARGIN;
    rect->top += MARGIN;
    HDC dc = ::GetDC(0);
    rect->right = (::GetDeviceCaps(dc, LOGPIXELSX) * 6.5) - MARGIN;
    rect->bottom = (::GetDeviceCaps(dc, LOGPIXELSY) * 11) - MARGIN;
    ::ReleaseDC(0, dc);

    return S_OK;
}

HRESULT PrintWebUIDelegate::willPerformDragSourceAction(_In_opt_ IWebView*, WebDragSourceAction, _In_ LPPOINT, _In_opt_ IDataObject*, _COM_Outptr_opt_ IDataObject** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT PrintWebUIDelegate::webViewHeaderHeight(_In_opt_ IWebView* webView, _Out_ float* height)
{
    if (!webView || !height)
        return E_POINTER;

    HDC dc = ::GetDC(0);

    TEXTMETRIC textMetric;
    ::GetTextMetrics(dc, &textMetric);
    ::ReleaseDC(0, dc);

    *height = 1.1 * textMetric.tmHeight;

    return S_OK;
}

HRESULT PrintWebUIDelegate::webViewFooterHeight(_In_opt_ IWebView* webView, _Out_ float* height)
{
    if (!webView || !height)
        return E_POINTER;

    HDC dc = ::GetDC(0);

    TEXTMETRIC textMetric;
    ::GetTextMetrics(dc, &textMetric);
    ::ReleaseDC(0, dc);

    *height = 1.1 * textMetric.tmHeight;

    return S_OK;
}

HRESULT PrintWebUIDelegate::drawHeaderInRect(_In_opt_ IWebView* webView, _In_ RECT* rect, ULONG_PTR drawingContext)
{
    if (!webView || !rect)
        return E_POINTER;

    // Turn off header for now.
    HDC dc = reinterpret_cast<HDC>(drawingContext);

    HGDIOBJ hFont = ::GetStockObject(ANSI_VAR_FONT);
    HGDIOBJ hOldFont = ::SelectObject(dc, hFont);

    LPCWSTR header = L"[Sample Header]";
    size_t length = wcslen(header);

    int rc = ::DrawTextW(dc, header, length, rect, DT_LEFT | DT_NOCLIP | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(dc, hOldFont);

    if (!rc)
        return E_FAIL;

    ::MoveToEx(dc, rect->left, rect->bottom, 0);
    HGDIOBJ hPen = ::GetStockObject(BLACK_PEN);
    HGDIOBJ hOldPen = ::SelectObject(dc, hPen);
    ::LineTo(dc, rect->right, rect->bottom);
    ::SelectObject(dc, hOldPen);

    return S_OK;
}

HRESULT PrintWebUIDelegate::drawFooterInRect(_In_opt_ IWebView* webView, _In_ RECT* rect, ULONG_PTR drawingContext, UINT pageIndex, UINT pageCount)
{
    if (!webView || !rect)
        return E_POINTER;

    HDC dc = reinterpret_cast<HDC>(drawingContext);

    HGDIOBJ hFont = ::GetStockObject(ANSI_VAR_FONT);
    HGDIOBJ hOldFont = ::SelectObject(dc, hFont);

    LPCWSTR footer = L"[Sample Footer]";
    size_t length = wcslen(footer);

    // Add a line, 1/10th inch above the footer text from left margin to right margin.
    ::MoveToEx(dc, rect->left, rect->top, 0);
    HGDIOBJ hPen = ::GetStockObject(BLACK_PEN);
    HGDIOBJ hOldPen = ::SelectObject(dc, hPen);
    ::LineTo(dc, rect->right, rect->top);
    ::SelectObject(dc, hOldPen);

    int rc = ::DrawTextW(dc, footer, length, rect, DT_LEFT | DT_NOCLIP | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(dc, hOldFont);

    if (!rc)
        return E_FAIL;

    return S_OK;
}

HRESULT PrintWebUIDelegate::canRunModal(_In_opt_ IWebView*, _Out_ BOOL* canRunBoolean)
{
    if (!canRunBoolean)
        return E_POINTER;
    *canRunBoolean = TRUE;
    return S_OK;
}

HRESULT PrintWebUIDelegate::runModal(_In_opt_ IWebView* webView)
{
    COMPtr<IWebView> protector(webView);

    auto modalDialogOwner = ::GetWindow(m_modalDialogParent, GW_OWNER);
    auto topLevelParent = ::GetAncestor(modalDialogOwner, GA_ROOT);

    ::EnableWindow(topLevelParent, FALSE);

    while (::IsWindow(getHandleFromWebView(webView))) {
#if USE(CF)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
        MSG msg;
        if (!::GetMessage(&msg, 0, 0, 0))
            break;

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);        
    }

    ::EnableWindow(topLevelParent, TRUE);

    return S_OK;
}

HRESULT PrintWebUIDelegate::createModalDialog(_In_opt_ IWebView* sender, _In_opt_ IWebURLRequest* request, _COM_Outptr_opt_ IWebView** newWebView)
{
    if (!newWebView)
        return E_POINTER;
    
    COMPtr<IWebView> webView;
    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, (void**)&webView);
    if (FAILED(hr))
        return hr;

    m_modalDialogParent = ::CreateWindow(L"STATIC", L"ModalDialog", WS_OVERLAPPED | WS_VISIBLE, 0, 0, 0, 0, getHandleFromWebView(sender), nullptr, nullptr, nullptr);

    hr = webView->setHostWindow(m_modalDialogParent);
    if (FAILED(hr))
        return hr;

    RECT clientRect = { 0, 0, 0, 0 };
    hr = webView->initWithFrame(clientRect, 0, _bstr_t(L""));
    if (FAILED(hr))
        return hr;

    COMPtr<IWebUIDelegate> uiDelegate;
    hr = sender->uiDelegate(&uiDelegate);
    if (FAILED(hr))
        return hr;

    webView->setUIDelegate(uiDelegate.get());

    *newWebView = webView.leakRef();

    return S_OK;
}
