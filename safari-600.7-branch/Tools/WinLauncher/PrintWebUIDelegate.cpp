/*
 * Copyright (C) 2009, 2013-2014 Apple Inc. All Rights Reserved.
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

#include <WebKit/WebKitCOMAPI.h>
#include <comip.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wininet.h>

static const int MARGIN = 20;

HRESULT STDMETHODCALLTYPE PrintWebUIDelegate::runJavaScriptAlertPanelWithMessage(IWebView*, BSTR message)
{
    ::MessageBoxW(0, message, L"JavaScript Alert", MB_OK);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PrintWebUIDelegate::runJavaScriptConfirmPanelWithMessage(IWebView*, BSTR message, BOOL* result)
{
    *result = ::MessageBoxW(0, message, L"JavaScript Confirm", MB_OKCANCEL) == IDOK;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PrintWebUIDelegate::createWebViewWithRequest(IWebView*, IWebURLRequest* request, IWebView**)
{
    if (!request)
        return E_POINTER;

    TCHAR executablePath[MAX_PATH];
    DWORD length = ::GetModuleFileName(GetModuleHandle(0), executablePath, ARRAYSIZE(executablePath));
    if (!length)
        return E_FAIL;

    _bstr_t url;
    HRESULT hr = request->URL(&url.GetBSTR());
    if (FAILED(hr))
        return E_FAIL;

    std::wstring command = std::wstring(L"\"") + executablePath + L"\" " + (const wchar_t*)url;

    PROCESS_INFORMATION processInformation;
    STARTUPINFOW startupInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    if (!::CreateProcessW(0, (LPWSTR)command.c_str(), 0, 0, 0, 0, 0, 0, &startupInfo, &processInformation))
        return E_FAIL;

    return S_OK;
}

HRESULT PrintWebUIDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualIID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG PrintWebUIDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG PrintWebUIDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

typedef _com_ptr_t<_com_IIID<IWebFrame, &__uuidof(IWebFrame)>> IWebFramePtr;
typedef _com_ptr_t<_com_IIID<IWebFramePrivate, &__uuidof(IWebFramePrivate)>> IWebFramePrivatePtr;

HRESULT PrintWebUIDelegate::webViewPrintingMarginRect(IWebView* view, RECT* rect)
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

HRESULT PrintWebUIDelegate::webViewHeaderHeight(IWebView* webView, float* height)
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

HRESULT PrintWebUIDelegate::webViewFooterHeight(IWebView* webView, float* height)
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

HRESULT PrintWebUIDelegate::drawHeaderInRect(IWebView* webView, RECT* rect, ULONG_PTR drawingContext)
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

HRESULT PrintWebUIDelegate::drawFooterInRect(IWebView* webView, RECT* rect, ULONG_PTR drawingContext, UINT pageIndex, UINT pageCount)
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
