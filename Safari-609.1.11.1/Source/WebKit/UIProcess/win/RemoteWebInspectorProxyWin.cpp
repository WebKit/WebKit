/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "RemoteWebInspectorProxy.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "APIPageConfiguration.h"
#include "WebInspectorProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebView.h"
#include <WebCore/IntRect.h>

namespace WebKit {

static LPCTSTR RemoteWebInspectorProxyPointerProp = TEXT("RemoteWebInspectorProxyPointer");
const LPCWSTR RemoteWebInspectorProxyClassName = L"RemoteWebInspectorProxyClass";

LRESULT CALLBACK RemoteWebInspectorProxy::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RemoteWebInspectorProxy* client = reinterpret_cast<RemoteWebInspectorProxy*>(::GetProp(hwnd, RemoteWebInspectorProxyPointerProp));

    switch (msg) {
    case WM_SIZE:
        return client->sizeChange();
    case WM_CLOSE:
        return client->onClose();
    default:
        break;
    }

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

static ATOM registerWindowClass()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = 0;
    wcex.lpfnWndProc    = RemoteWebInspectorProxy::WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = 0;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = RemoteWebInspectorProxyClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;
    return ::RegisterClassEx(&wcex);
}

LRESULT RemoteWebInspectorProxy::sizeChange()
{
    if (!m_webView)
        return 0;

    RECT rect;
    ::GetClientRect(m_frontendHandle, &rect);
    ::SetWindowPos(m_webView->window(), 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
    return 0;
}

LRESULT RemoteWebInspectorProxy::onClose()
{
    ::ShowWindow(m_frontendHandle, SW_HIDE);
    frontendDidClose();
    return 0;
}

WebPageProxy* RemoteWebInspectorProxy::platformCreateFrontendPageAndWindow()
{
    RefPtr<WebPreferences> preferences = WebPreferences::create(String(), "WebKit2.", "WebKit2.");
    preferences->setAllowFileAccessFromFileURLs(true);

#if ENABLE(DEVELOPER_MODE)
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif

    RefPtr<WebPageGroup> pageGroup = WebPageGroup::create(inspectorPageGroupIdentifierForPage(nullptr));

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&inspectorProcessPool(inspectorLevelForPage(nullptr)));
    pageConfiguration->setPreferences(preferences.get());
    pageConfiguration->setPageGroup(pageGroup.get());

    WebCore::IntRect rect(60, 200, 1500, 1000);
    registerWindowClass();
    m_frontendHandle = ::CreateWindowEx(0, RemoteWebInspectorProxyClassName, 0, WS_OVERLAPPEDWINDOW,
        rect.x(), rect.y(), rect.width(), rect.height(), 0, 0, 0, 0);

    ::SetProp(m_frontendHandle, RemoteWebInspectorProxyPointerProp, reinterpret_cast<HANDLE>(this));
    ShowWindow(m_frontendHandle, SW_SHOW);

    RECT r;
    ::GetClientRect(m_frontendHandle, &r);
    m_webView = WebView::create(r, pageConfiguration, m_frontendHandle);
    return m_webView->page();
}

void RemoteWebInspectorProxy::platformResetState() { }
void RemoteWebInspectorProxy::platformBringToFront() { }
void RemoteWebInspectorProxy::platformSave(const String&, const String&, bool, bool) { }
void RemoteWebInspectorProxy::platformAppend(const String&, const String&) { }
void RemoteWebInspectorProxy::platformSetSheetRect(const WebCore::FloatRect&) { }
void RemoteWebInspectorProxy::platformStartWindowDrag() { }
void RemoteWebInspectorProxy::platformOpenInNewTab(const String&) { }
void RemoteWebInspectorProxy::platformShowCertificate(const WebCore::CertificateInfo&) { }

void RemoteWebInspectorProxy::platformCloseFrontendPageAndWindow()
{
    ::DestroyWindow(m_frontendHandle);
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
